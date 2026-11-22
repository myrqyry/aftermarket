#include "audio-patchbay.h"
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QToolButton>
#include <QPushButton>
#include <QTimer>
#include <QSignalBlocker>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QPalette>
#include <QFontMetrics>
#include <algorithm>

namespace augment {
class AudioPatchbay::Canvas final : public QWidget {
public:
    QVector<RouteSource> routes;
    QString selected;
    std::function<void(const QString &)> onSource;
    std::function<void(int)> onTrack;
    Canvas(QWidget *parent=nullptr):QWidget(parent) {
        setObjectName("aftermarket-route-canvas");
        setMinimumWidth(310);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::MinimumExpanding);
    }
    void setRoutes(const QVector<RouteSource> &value,const QString &current) {
        if(routes==value&&selected==current)return;
        routes=value;selected=current;
        setMinimumHeight(qMax(365,80+qMax(routes.size(),6)*54));
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        auto fg=palette().color(QPalette::Text);
        auto accent=palette().color(QPalette::Highlight);
        auto muted=palette().color(QPalette::Mid);
        auto base=palette().color(QPalette::Button);
        auto light=palette().color(QPalette::HighlightedText);
        const int rx=width()-50;
        p.setPen(fg);
        p.drawText(QRect(12,8,165,25),Qt::AlignLeft|Qt::AlignVCenter,"Audio sources");
        p.drawText(QRect(rx-18,8,65,25),Qt::AlignLeft|Qt::AlignVCenter,"Tracks");
        for(int i=0;i<6;++i){
            const int y=60+i*54;
            p.setPen(QPen(muted,1));
            p.setBrush(base);
            p.drawRoundedRect(QRectF(rx-20,y-17,65,34),12,12);
            p.setPen(fg);
            p.drawText(QRectF(rx-20,y-17,65,34),Qt::AlignCenter,QString::number(i+1));
        }
        for(int index=0;index<routes.size();++index){
            const auto &src=routes.at(index);
            const bool active=src.id==selected;
            const int y=60+index*54;
            p.setPen(QPen(active?accent:muted,1.5));
            p.setBrush(active?accent.lighter(130):base);
            p.drawRoundedRect(QRectF(10,y-19,159,38),12,12);
            p.setPen(active?light:fg);
            const auto name=p.fontMetrics().elidedText(src.name,Qt::ElideRight,136);
            p.drawText(QRectF(18,y-18,137,36),Qt::AlignVCenter|Qt::AlignLeft,name);
            for(int t=0;t<6;++t)if(src.tracks&(1u<<t)){
                const int endpoint=60+t*54;
                QPainterPath cable(QPointF(169,y));
                const double mid=169+(rx-169)*0.5;
                cable.cubicTo(mid,y,mid,endpoint,rx-20,endpoint);
                QColor wire=active?accent:muted;
                wire.setAlpha(active?225:115);
                p.setPen(QPen(wire,active?2.8:1.3,Qt::SolidLine,Qt::RoundCap));
                p.setBrush(Qt::NoBrush);
                p.drawPath(cable);
            }
        }
        if(routes.isEmpty()){
            p.setPen(muted);
            p.drawText(QRect(15,78,width()-25,90),
                Qt::AlignHCenter|Qt::TextWordWrap,"No active OBS audio sources");
        }
    }
    void mousePressEvent(QMouseEvent *event) override {
        if(event->button()!=Qt::LeftButton){QWidget::mousePressEvent(event);return;}
        const QPoint pos=event->position().toPoint();
        if(pos.x()>=10&&pos.x()<=169){
            const int index=(pos.y()-33)/54;
            if(index>=0&&index<routes.size()){
                const int y=60+index*54;
                if(qAbs(pos.y()-y)<=20&&onSource)onSource(routes.at(index).id);
            }
        } else if(pos.x()>=width()-75){
            for(int t=0;t<6;++t)
                if(qAbs(pos.y()-(60+t*54))<=20&&onTrack){onTrack(t+1);break;}
        }
        event->accept();
    }
};
AudioPatchbay::AudioPatchbay(Query q,ToggleTrack t,ChangeMonitoring m,QWidget *parent)
    :QWidget(parent),query(std::move(q)),toggle(std::move(t)),monitor(std::move(m)){
    setObjectName("aftermarket-audio-patchbay");
    setWindowTitle("Routing");
    auto *root=new QVBoxLayout(this);
    root->setContentsMargins(5,5,5,5);root->setSpacing(5);
    auto *header=new QHBoxLayout;
    auto *heading=new QLabel("Audio patchbay",this);
    auto font=heading->font();font.setBold(true);heading->setFont(font);
    header->addWidget(heading);
    header->addStretch();
    auto *refreshButton=new QPushButton("Refresh",this);
    refreshButton->setToolTip("Read live OBS source routing");
    header->addWidget(refreshButton);
    root->addLayout(header);
    auto *selector=new QHBoxLayout;
    selector->addWidget(new QLabel("Source",this));
    sources=new QComboBox(this);sources->setObjectName("aftermarket-route-source");
    sources->setAccessibleName("Choose an OBS audio source");
    selector->addWidget(sources,1);
    root->addLayout(selector);
    auto *scroll=new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    canvas=new Canvas(scroll);
    canvas->onSource=[this](const QString &id){selectSource(id);};
    canvas->onTrack=[this](int track){toggleTrack(track);};
    scroll->setWidget(canvas);
    root->addWidget(scroll,1);
    auto *trackRow=new QHBoxLayout;
    trackRow->addWidget(new QLabel("Route to",this));
    for(int i=0;i<6;++i){
        auto *button=new QToolButton(this);
        button->setObjectName(QStringLiteral("aftermarket-track-%1").arg(i+1));
        button->setCheckable(true);
        button->setText(QString::number(i+1));
        button->setToolTip(QStringLiteral("Toggle OBS audio track %1 for selected source").arg(i+1));
        button->setAccessibleName(button->toolTip());
        button->setMinimumSize(27,27);
        tracks[i]=button;
        trackRow->addWidget(button,1);
        connect(button,&QToolButton::clicked,this,[this,i]{toggleTrack(i+1);});
    }
    root->addLayout(trackRow);
    auto *monitorRow=new QHBoxLayout;
    monitorRow->addWidget(new QLabel("Monitoring",this));
    monitoring=new QComboBox(this);monitoring->setObjectName("aftermarket-monitor");
    monitoring->addItems({"Off","Monitor Only","Monitor and Output"});
    monitoring->setAccessibleName("Native OBS monitoring mode");
    monitorRow->addWidget(monitoring,1);
    root->addLayout(monitorRow);
    status=new QLabel(this);status->setObjectName("aftermarket-route-status");
    status->setWordWrap(true);root->addWidget(status);
    connect(refreshButton,&QPushButton::clicked,this,[this]{refresh();});
    connect(sources,&QComboBox::currentIndexChanged,this,[this](int index){
        if(index>=0)selectSource(sources->itemData(index).toString());
    });
    connect(monitoring,&QComboBox::currentIndexChanged,this,[this](int mode){
        if(mode>=0)setMonitoring(mode);
    });
    timer=new QTimer(this);timer->setInterval(1400);
    connect(timer,&QTimer::timeout,this,[this]{if(isVisible())refresh();});
    timer->start();
    refresh();
}
int AudioPatchbay::routeCount() const {return routes.size();}
QString AudioPatchbay::selectedId() const {return selected;}
void AudioPatchbay::refresh(){
    const auto latest=query?query():routes;
    if(latest==routes&&!selected.isEmpty())return; // keep open native comboboxes stable
    routes=latest;
    auto exists=[this](const QString &id){
        return std::any_of(routes.begin(),routes.end(),[&](const RouteSource &r){return r.id==id;});
    };
    if(!exists(selected))selected=routes.isEmpty()?QString{}:routes.first().id;
    syncControls();
}
void AudioPatchbay::syncControls(){
    const QSignalBlocker sourceBlock(sources),monitorBlock(monitoring);
    sources->clear();
    for(const auto &r:routes)sources->addItem(r.name,r.id);
    int chosen=-1;
    for(int i=0;i<routes.size();++i)if(routes[i].id==selected){chosen=i;break;}
    sources->setCurrentIndex(chosen);
    sources->setEnabled(chosen>=0);
    for(int i=0;i<6;++i){
        const QSignalBlocker blocker(tracks[i]);
        tracks[i]->setEnabled(chosen>=0);
        tracks[i]->setChecked(chosen>=0&&(routes.at(chosen).tracks&(1u<<i)));
    }
    monitoring->setEnabled(chosen>=0);
    monitoring->setCurrentIndex(chosen<0?-1:routes.at(chosen).monitor);
    canvas->setRoutes(routes,selected);
}
bool AudioPatchbay::selectSource(const QString &id){
    const auto found=std::find_if(routes.begin(),routes.end(),
        [&](const RouteSource &route){return route.id==id;});
    if(found==routes.end())return false;
    selected=id;syncControls();return true;
}
bool AudioPatchbay::toggleTrack(int track){
    if(selected.isEmpty()||track<1||track>6||!toggle)return false;
    const bool ok=toggle(selected,track);
    if(ok){refresh();status->setText(QStringLiteral("Updated track %1 in OBS").arg(track));}
    else{refresh();status->setText("Source unavailable; OBS routing unchanged");}
    return ok;
}
bool AudioPatchbay::setMonitoring(int mode){
    if(selected.isEmpty()||mode<0||mode>2||!monitor)return false;
    const bool ok=monitor(selected,mode);
    refresh();
    status->setText(ok?"Updated native OBS monitoring":"Source unavailable; monitoring unchanged");
    return ok;
}
}
