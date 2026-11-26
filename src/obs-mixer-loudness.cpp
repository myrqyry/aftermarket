#include "obs-mixer-loudness.h"
#include "mixer-loudness.h"
#include "ebur128.h"
#include <obs.h>
#include <QPainter>
#include <QMenu>
#include <QActionGroup>
#include <QSettings>
#include <QTimer>
#include <QToolTip>
#include <QHelpEvent>
#include <QContextMenuEvent>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>
#include <cstring>
namespace augment {
namespace {
using Clock=std::chrono::steady_clock;
constexpr size_t Capacity=16,BlockFrames=2048,Channels=8;
std::vector<int> channelMap(speaker_layout layout){
    using V=std::vector<int>;
    switch(layout){
    case SPEAKERS_MONO:return V{EBUR128_CENTER};
    case SPEAKERS_STEREO:return V{EBUR128_LEFT,EBUR128_RIGHT};
    case SPEAKERS_2POINT1:return V{EBUR128_LEFT,EBUR128_RIGHT,EBUR128_UNUSED};
    case SPEAKERS_4POINT0:return V{EBUR128_LEFT,EBUR128_RIGHT,EBUR128_CENTER,EBUR128_Mp180};
    case SPEAKERS_4POINT1:return V{EBUR128_LEFT,EBUR128_RIGHT,EBUR128_CENTER,EBUR128_UNUSED,EBUR128_Mp180};
    case SPEAKERS_5POINT1:return V{EBUR128_LEFT,EBUR128_RIGHT,EBUR128_CENTER,EBUR128_UNUSED,EBUR128_LEFT_SURROUND,EBUR128_RIGHT_SURROUND};
    case SPEAKERS_7POINT1:return V{EBUR128_LEFT,EBUR128_RIGHT,EBUR128_CENTER,EBUR128_UNUSED,EBUR128_Mp135,EBUR128_Mm135,EBUR128_Mp090,EBUR128_Mm090};
    default:return {};
    }
}
class Tap {
    struct Block {std::array<float,BlockFrames*Channels> data;size_t frames=0;uint64_t sequence=0;};
    std::array<Block,Capacity> queue_;
    std::atomic<uint64_t> head_{0},tail_{0},sequence_{0},drops_{0};
    std::atomic<bool> stop_{false};
    std::thread worker_;
    std::mutex snapshotMutex_;
    LoudnessReading snapshot_;
    Clock::time_point updated_{};
    audio_t *audio_=nullptr;
    unsigned track_,channels_=0;
    bool attached_=false;
    static void receive(void *opaque,size_t,struct audio_data *data){
        auto &s=*static_cast<Tap*>(opaque);
        const uint64_t seq=s.sequence_.fetch_add(1,std::memory_order_relaxed)+1;
        const auto head=s.head_.load(std::memory_order_relaxed);
        if(!data||!data->frames||data->frames>BlockFrames||head-s.tail_.load(std::memory_order_acquire)>=Capacity){
            ++s.drops_;return;
        }
        for(unsigned c=0;c<s.channels_;++c)if(!data->data[c]){++s.drops_;return;}
        auto &b=s.queue_[head%Capacity];b.frames=data->frames;b.sequence=seq;
        for(size_t f=0;f<b.frames;++f)for(unsigned c=0;c<s.channels_;++c){
            const float x=reinterpret_cast<const float*>(data->data[c])[f];
            b.data[f*s.channels_+c]=std::isfinite(x)?x:0.f;
        }
        s.head_.store(head+1,std::memory_order_release);
    }
public:
    explicit Tap(unsigned track):track_(track){
        audio_=obs_get_audio();
        if(!audio_||track_>=MAX_AUDIO_MIXES)return;
        auto *info=audio_output_get_info(audio_);
        if(!info||info->format!=AUDIO_FORMAT_FLOAT_PLANAR)return;
        auto map=channelMap(info->speakers);
        channels_=static_cast<unsigned>(map.size());
        if(channels_!=audio_output_get_channels(audio_)||!channels_)return;
        auto meter=std::make_unique<LoudnessMeter>(info->samples_per_sec,channels_,std::move(map));
        if(!meter->valid())return;
        try {
            worker_=std::thread([this,meter=std::move(meter)]{
                uint64_t last=0,seenDrops=0;auto lastRead=Clock::now();
                while(!stop_.load(std::memory_order_acquire)){
                    bool changed=false;
                    // Each block owns its slot until processing completes.
                    // A discontinuity invalidates integrated history; restart
                    // instead of reporting a deceptively continuous measurement.
                    const auto losses=drops_.load(std::memory_order_acquire);
                    if(losses!=seenDrops){
                        seenDrops=losses;meter->reset();
                        tail_.store(head_.load(std::memory_order_acquire),std::memory_order_release);
                        last=0;std::lock_guard<std::mutex> lock(snapshotMutex_);snapshot_={};updated_={};
                    }
                    auto tail=tail_.load(std::memory_order_relaxed);
                    const auto head=head_.load(std::memory_order_acquire);
                    for(;tail<head;++tail){
                        const auto &b=queue_[tail%Capacity];
                        if(last&&b.sequence!=last+1)meter->reset();
                        last=b.sequence;
                        if(!meter->add(b.data.data(),b.frames))meter->reset();
                        changed=true;tail_.store(tail+1,std::memory_order_release);
                    }
                    const auto now=Clock::now();
                    if(changed&&now-lastRead>=std::chrono::milliseconds(100)){
                        auto reading=meter->read();
                        {std::lock_guard<std::mutex> lock(snapshotMutex_);snapshot_=reading;updated_=now;}
                        lastRead=now;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            });
        }catch(...){return;}
        attached_=audio_output_connect(audio_,track_,nullptr,receive,this);
        if(!attached_){stop_.store(true,std::memory_order_release);if(worker_.joinable())worker_.join();}
    }
    ~Tap(){
        if(attached_&&obs_get_audio()==audio_)audio_output_disconnect(audio_,track_,receive,this);
        stop_.store(true,std::memory_order_release);
        if(worker_.joinable())worker_.join();
    }
    bool available()const{return attached_;}
    LoudnessReading read(){
        std::lock_guard<std::mutex> lock(snapshotMutex_);
        if(Clock::now()-updated_>std::chrono::seconds(2))return {};
        return snapshot_;
    }
    uint64_t dropped()const{return drops_.load(std::memory_order_relaxed);}
};
const std::array<const char*,5> labels{"M","S","I","LRA","TP"};
const std::array<const char*,5> names{"Momentary (400 ms)","Short-term (3 s)","Integrated (since reset)","Loudness range","True peak (maximum since reset)"};
std::array<double,5> values(const LoudnessReading &r){return {r.momentary,r.shortTerm,r.integrated,r.range,r.truePeak};}
QString measurement(double value,int i){
    return std::isfinite(value)?QString::number(value,'f',1)+ (i==3?" LU":i==4?" dBTP":" LUFS"):QStringLiteral("—");
}
QColor indicator(double value,int i,int target,const QPalette &palette){
    if(!std::isfinite(value))return palette.color(QPalette::Mid);
    if(i==3)return palette.color(QPalette::WindowText);
    if(i==4){if(value>=0)return QColor("#e45f65");if(value>=-1)return QColor("#d8ab55");return QColor("#63bb86");}
    if(value>target+4)return QColor("#e45f65");
    if(value>target+2)return QColor("#d8ab55");
    if(value>=target-2)return QColor("#63bb86");
    return palette.color(QPalette::WindowText);
}
}
struct ObsMixerLoudness::State {
    QString file;
    int track=1,target=-14;
    bool enabled=true;
    std::unique_ptr<Tap> tap;
    LoudnessReading reading;
    QTimer *timer=nullptr;
};
ObsMixerLoudness::ObsMixerLoudness(const QString &settingsFile,QWidget *parent):QWidget(parent),s_(new State){
    s_->file=settingsFile;
    QSettings prefs(settingsFile,QSettings::IniFormat);
    s_->track=qBound(1,prefs.value("track",1).toInt(),MAX_AUDIO_MIXES);
    s_->target=qBound(-36,prefs.value("target",-14).toInt(),-6);
    s_->enabled=prefs.value("enabled",true).toBool();
    setObjectName("aftermarket-mixer-loudness");
    setAccessibleName("Output loudness");
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    setMinimumWidth(105);setMaximumWidth(190);setFixedHeight(20);
    setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);
    s_->timer=new QTimer(this);
    connect(s_->timer,&QTimer::timeout,this,[this]{refresh();});
    s_->timer->start(250);
    if(!s_->enabled)hide();restart();
}
ObsMixerLoudness::~ObsMixerLoudness()=default;
void ObsMixerLoudness::restart(){
    s_->tap.reset();
    s_->reading={};
    if(s_->enabled&&obs_get_audio())s_->tap=std::make_unique<Tap>(s_->track-1);
    refresh();
}
void ObsMixerLoudness::refresh(){
    s_->reading=s_->tap?s_->tap->read():LoudnessReading{};
    const auto vals=values(s_->reading);
    QString info=QString("Track %1 · target %2 LUFS").arg(s_->track).arg(s_->target);
    if(!s_->tap||!s_->tap->available())info+=" · measurement unavailable";
    else if(s_->tap->dropped())info+=" · history restarted after dropped audio";
    for(int i=0;i<5;++i)info+="\n"+QString::fromLatin1(names[i])+": "+measurement(vals[i],i);
    info+="\nGreen: near target / peak below -1 dBTP; amber: above target / peak near full scale; red: >4 LU above target / peak at or above full scale. LRA is informational. Right-click for options.";
    setAccessibleDescription(info);
    setProperty("track",s_->track);
    setProperty("frames",QVariant::fromValue<qulonglong>(s_->reading.frames));
    setProperty("integrated",s_->reading.integrated);
    setProperty("truePeak",s_->reading.truePeak);
    if(isVisible())update();
}
void ObsMixerLoudness::paintEvent(QPaintEvent*){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
    auto font=p.font();font.setPixelSize(10);p.setFont(font);
    const auto fg=palette().color(QPalette::WindowText);
    const auto vals=values(s_->reading);
    const int step=width()/5;
    for(int i=0;i<5;++i){
        const int x=i*step;
        p.setPen(Qt::NoPen);p.setBrush(indicator(vals[i],i,s_->target,palette()));
        p.drawEllipse(QPointF(x+4,height()/2.0),2.6,2.6);
        p.setPen(fg);
        const QString text=(step<28&&i==3)?"R":QString::fromLatin1(labels[i]);
        p.drawText(QRect(x+9,0,step-9,height()),Qt::AlignVCenter|Qt::AlignLeft,text);
    }
}
bool ObsMixerLoudness::event(QEvent *e){
    if(e->type()==QEvent::ToolTip){
        auto *h=static_cast<QHelpEvent*>(e);
        QToolTip::showText(h->globalPos(),accessibleDescription(),this);return true;
    }
    return QWidget::event(e);
}
void ObsMixerLoudness::contextMenuEvent(QContextMenuEvent *e){
    auto *menu=new QMenu(this);menu->setAttribute(Qt::WA_DeleteOnClose);
    populateMenu(menu);menu->popup(e->globalPos());
}
void ObsMixerLoudness::populateMenu(QMenu *menu){
    auto save=[this]{
        QSettings prefs(s_->file,QSettings::IniFormat);
        prefs.setValue("track",s_->track);prefs.setValue("target",s_->target);prefs.setValue("enabled",s_->enabled);
    };
    auto *enabled=menu->addAction("Show loudness dots");enabled->setObjectName("aftermarket-loudness-enabled");enabled->setCheckable(true);enabled->setChecked(s_->enabled);
    connect(enabled,&QAction::triggered,this,[this,save](bool checked){s_->enabled=checked;setVisible(checked);save();restart();});
    auto *tracks=menu->addMenu("Measure output track");
    auto *trackGroup=new QActionGroup(tracks);trackGroup->setExclusive(true);
    for(int i=1;i<=MAX_AUDIO_MIXES;++i){
        auto *a=tracks->addAction(QString("Track %1").arg(i));a->setObjectName(QString("aftermarket-loudness-track-%1").arg(i));a->setCheckable(true);a->setChecked(i==s_->track);trackGroup->addAction(a);
        connect(a,&QAction::triggered,this,[this,save,i]{if(s_->track!=i){s_->track=i;save();restart();}});
    }
    auto *targets=menu->addMenu("Loudness target");
    auto *targetGroup=new QActionGroup(targets);targetGroup->setExclusive(true);
    for(int value:{-23,-16,-14}){
        auto *a=targets->addAction(QString("%1 LUFS").arg(value));a->setCheckable(true);a->setChecked(value==s_->target);targetGroup->addAction(a);
        connect(a,&QAction::triggered,this,[this,save,value]{s_->target=value;save();refresh();});
    }
    menu->addSeparator();
    auto *reset=menu->addAction("Reset loudness history");
    reset->setObjectName("aftermarket-loudness-reset");reset->setEnabled(s_->enabled);
    connect(reset,&QAction::triggered,this,[this]{restart();});
}
}
