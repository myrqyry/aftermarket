#include "front-audio-controls.h"
#include <QApplication>
#include <QWidget>
#include <QBoxLayout>
#include <QGridLayout>
#include <QAbstractButton>
#include <QSlider>
#include <QDial>
#include <QSpinBox>
#include <QLabel>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QFile>
#include <QSignalBlocker>
#include <QPointer>
#include <QSet>
#include <QMap>
#include <QLayout>
#include <QtMath>
#include <algorithm>

namespace augment {
namespace {
constexpr int maxOffsetMs=999999;
constexpr int dialOffsetMs=2000; // coarse ±2s; numeric spin edits all offsets
QString sourceName(QWidget *volume){
    for(auto *button:volume->findChildren<QAbstractButton*>())
        if(button->inherits("VolumeName")||button->objectName()=="VolumeName")
            return button->text();
    return {};
}
bool volumeControl(QWidget *widget){
    return widget&&(widget->inherits("VolumeControl")||
                     widget->objectName()=="VolumeControl");
}
}
struct FrontAudioControls::State {
    struct Row {
        QPointer<QWidget> volume,controls;
        QPointer<QSlider> panSlider;
        QPointer<QDial> panDial,offsetDial;
        QPointer<QSpinBox> offsetSpin;
        QPointer<QLabel> offsetLabel;
        QString id;
    };
    QString file;
    Query query;
    SetPan pan;
    SetOffset offset;
    std::function<void()> persist;
    QTimer saveDebounce;
    QPointer<QWidget> native;
    QList<Row> rows;
    QTimer poll;
    bool useKnobs=false;
    bool busy=false;
    bool load(){
        QFile f(file);
        if(!f.exists())return true;
        if(!f.open(QIODevice::ReadOnly)||f.size()>4096)return false;
        const auto document=QJsonDocument::fromJson(f.readAll());
        if(!document.isObject())return false;
        const auto object=document.object();
        if(object.value("version").toInt()!=1||!object.value("knobs").isBool())return false;
        useKnobs=object.value("knobs").toBool();
        return true;
    }
    bool save(){
        QSaveFile f(file);
        if(!f.open(QIODevice::WriteOnly))return false;
        const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"knobs",useKnobs}})
            .toJson(QJsonDocument::Indented);
        return f.write(bytes)==bytes.size()&&f.commit();
    }
    void remove(Row &r){
        if(r.controls)delete r.controls.data();
    }
    void apply(Row &r,const AudioPropertyState &source){
        if(!r.controls)return;
        const int panValue=qBound(0,qRound(source.balance*100),100);
        if(r.panSlider&&!r.panSlider->isSliderDown()){
            const QSignalBlocker guard(r.panSlider);
            r.panSlider->setValue(panValue);
        }
        if(r.panDial&&!r.panDial->isSliderDown()){
            const QSignalBlocker guard(r.panDial);
            r.panDial->setValue(panValue);
        }
        if(r.offsetSpin&&!r.offsetSpin->hasFocus()){
            const QSignalBlocker guard(r.offsetSpin);
            r.offsetSpin->setValue(qBound(-maxOffsetMs,source.offsetMs,maxOffsetMs));
        }
        if(r.offsetDial&&!r.offsetDial->isSliderDown()){
            const QSignalBlocker guard(r.offsetDial);
            r.offsetDial->setValue(qBound(-dialOffsetMs,source.offsetMs,dialOffsetMs));
        }
        if(r.panSlider)r.panSlider->setEnabled(source.stereo);
        if(r.panDial)r.panDial->setEnabled(source.stereo);
        if(r.panSlider)r.panSlider->setVisible(!useKnobs);
        if(r.panDial)r.panDial->setVisible(useKnobs);
        if(r.offsetDial)r.offsetDial->setVisible(useKnobs);
        if(r.offsetLabel)r.offsetLabel->setVisible(!useKnobs);
        if(r.controls)r.controls->setMinimumHeight(useKnobs?57:48);
        if(r.offsetSpin){
            r.offsetSpin->setMinimumWidth(useKnobs?51:58);
            r.offsetSpin->setMaximumWidth(useKnobs?58:79);
        }
    }
};
FrontAudioControls::FrontAudioControls(QString settingsFile,Query q,SetPan p,
                                       SetOffset o,std::function<void()> persist,QObject *parent)
    :QObject(parent),state(new State){
    state->file=std::move(settingsFile);
    state->query=std::move(q);
    state->pan=std::move(p);
    state->offset=std::move(o);
    state->persist=std::move(persist);
    state->saveDebounce.setInterval(320);
    state->saveDebounce.setSingleShot(true);
    connect(&state->saveDebounce,&QTimer::timeout,this,[this]{
        if(state->persist)state->persist();
    });
    state->load();
    state->poll.setInterval(900);
    connect(&state->poll,&QTimer::timeout,this,[this]{
        if(state->native&&state->native->isVisible())refresh();
    });
}
FrontAudioControls::~FrontAudioControls(){detach();}
bool FrontAudioControls::attach(QWidget *nativeMixer){
    if(state->native||!nativeMixer)return false;
    state->native=nativeMixer;
    refresh();
    state->poll.start();
    return true;
}
void FrontAudioControls::detach(){
    state->poll.stop();
    for(auto &row:state->rows)state->remove(row);
    state->rows.clear();
    state->native.clear();
}
bool FrontAudioControls::knobs() const{return state->useKnobs;}
bool FrontAudioControls::setKnobs(bool enabled){
    if(state->useKnobs==enabled)return false;
    state->useKnobs=enabled;
    for(auto &row:state->rows)if(row.controls){
        if(row.panSlider)row.panSlider->setVisible(!enabled);
        if(row.panDial)row.panDial->setVisible(enabled);
        if(row.offsetDial)row.offsetDial->setVisible(enabled);
        if(row.offsetLabel)row.offsetLabel->setVisible(!enabled);
        if(row.controls)row.controls->setMinimumHeight(enabled?57:48);
        if(row.offsetSpin){
            row.offsetSpin->setMinimumWidth(enabled?51:58);
            row.offsetSpin->setMaximumWidth(enabled?58:79);
        }
    }
    state->save();
    return true;
}
int FrontAudioControls::rowCount()const{return state->rows.size();}
void FrontAudioControls::refresh(){
    if(!state->native||!state->query||state->busy)return;
    state->busy=true;
    const auto available=state->query();
    QMap<QString,AudioPropertyState> byName;
    QSet<QString> ambiguous;
    for(const auto &src:available){
        if(src.id.isEmpty()||src.name.isEmpty())continue;
        if(byName.contains(src.name))ambiguous.insert(src.name);
        byName.insert(src.name,src);
    }
    for(const auto &name:ambiguous)byName.remove(name);
    QList<QWidget*> candidates;
    for(auto *widget:state->native->findChildren<QWidget*>())
        if(volumeControl(widget)&&!widget->property("aftermarket-foreign").toBool())
            candidates.append(widget);
    QSet<QWidget*> seen;
    for(auto *widget:candidates){
        const QString name=sourceName(widget);
        if(!byName.contains(name))continue;
        const auto src=byName.value(name);
        if(!widget->layout())continue;
        State::Row *existing=nullptr;
        for(auto &r:state->rows)if(r.volume==widget){
            existing=&r;
            break;
        }
        if(existing&&existing->id!=src.id){
            state->remove(*existing);
            existing->controls.clear();
        }
        if(!existing||!existing->controls){
            auto *controls=new QWidget(widget);
            controls->setObjectName("aftermarket-source-adjustments");
            controls->setProperty("aftermarket-source-uuid",src.id);
            controls->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
            // OBS 32.x often uses narrow vertical VolumeControl columns.
            // Keep each adjustment on its own row instead of growing those columns.
            controls->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
            controls->setMinimumHeight(48); // slider rows stay usable in narrow OBS columns
            auto *outer=new QVBoxLayout(controls);
            outer->setContentsMargins(1,0,1,0);
            outer->setSpacing(1);
            auto *panRow=new QHBoxLayout;
            panRow->setContentsMargins(0,0,0,0);
            panRow->setSpacing(2);
            auto *panLabel=new QLabel("Pan",controls);
            QFont panFont=panLabel->font();
            panFont.setWeight(QFont::Normal);
            if(panFont.pointSizeF()>0)panFont.setPointSizeF(qMax(7.0,panFont.pointSizeF()-0.75));
            panLabel->setFont(panFont);
            panLabel->setMaximumWidth(34);
            panRow->addWidget(panLabel);
            auto *pan=new QSlider(Qt::Horizontal,controls);
            pan->setObjectName("aftermarket-pan-slider");
            pan->setRange(0,100);
            pan->setMinimumWidth(34);
            pan->setFixedHeight(20);
            pan->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
            pan->setToolTip("Stereo pan · L 0 · Center 50 · R 100");
            panRow->addWidget(pan,1);
            auto *panDial=new QDial(controls);
            panDial->setObjectName("aftermarket-pan-dial");
            panDial->setRange(0,100);
            panDial->setNotchesVisible(false);
            panDial->setFixedSize(26,26);
            panDial->setToolTip(pan->toolTip());
            panRow->addWidget(panDial);
            outer->addLayout(panRow);
            auto *syncRow=new QHBoxLayout;
            syncRow->setContentsMargins(0,0,0,0);
            syncRow->setSpacing(2);
            auto *offsetLabel=new QLabel("Δ",controls);
            offsetLabel->setFixedWidth(14);
            offsetLabel->setToolTip("Native OBS sync offset (milliseconds)");
            syncRow->addWidget(offsetLabel);
            auto *offsetDial=new QDial(controls);
            offsetDial->setObjectName("aftermarket-offset-dial");
            offsetDial->setRange(-dialOffsetMs,dialOffsetMs);
            offsetDial->setFixedSize(26,26);
            offsetDial->setToolTip("Coarse sync offset · ±2000 ms · precise value at right");
            syncRow->addWidget(offsetDial);
            auto *offset=new QSpinBox(controls);
            offset->setObjectName("aftermarket-offset-ms");
            offset->setRange(-maxOffsetMs,maxOffsetMs);
            offset->setSingleStep(10);
            offset->setFixedHeight(23);
            offset->setSuffix("ms");
            offset->setMinimumWidth(58);
            offset->setMaximumWidth(79);
            offset->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
            offset->setToolTip("Precise OBS audio sync offset in milliseconds");
            syncRow->addWidget(offset,1);
            outer->addLayout(syncRow);
            bool inserted=false;
            if(auto *box=qobject_cast<QBoxLayout*>(widget->layout())){
                if(box->direction()==QBoxLayout::TopToBottom||
                   box->direction()==QBoxLayout::BottomToTop){
                    box->addWidget(controls);
                    inserted=true;
                }
            }else if(auto *grid=qobject_cast<QGridLayout*>(widget->layout())){
                grid->addWidget(controls,grid->rowCount(),0,1,qMax(1,grid->columnCount()));
                inserted=true;
            }
            if(!inserted){delete controls;continue;}
            controls->show();
            const QString id=src.id;
            connect(pan,&QSlider::valueChanged,this,[this,id](int v){
                if(!state->pan)return;
                if(state->pan(id,v/100.0))state->saveDebounce.start();
                refresh();
            });
            connect(panDial,&QDial::valueChanged,this,[this,id](int v){
                if(!state->pan)return;
                if(state->pan(id,v/100.0))state->saveDebounce.start();
                refresh();
            });
            connect(offset,&QSpinBox::valueChanged,this,[this,id](int ms){
                if(!state->offset)return;
                if(state->offset(id,ms))state->saveDebounce.start();
                refresh();
            });
            connect(offsetDial,&QDial::valueChanged,this,[this,id](int ms){
                if(!state->offset)return;
                if(state->offset(id,ms))state->saveDebounce.start();
                refresh();
            });
            State::Row row;
            row.volume=widget;row.controls=controls;row.id=id;
            row.panSlider=pan;row.panDial=panDial;row.offsetSpin=offset;
            row.offsetDial=offsetDial;row.offsetLabel=offsetLabel;
            if(existing)*existing=row;else state->rows.append(row);
            existing=nullptr;
            for(auto &record:state->rows)if(record.volume==widget)existing=&record;
        }
        if(existing)state->apply(*existing,src);
        seen.insert(widget);
    }
    for(int i=state->rows.size()-1;i>=0;--i)
        if(!state->rows[i].volume||!seen.contains(state->rows[i].volume)){
            state->remove(state->rows[i]);
            state->rows.removeAt(i);
        }
    state->busy=false;
}
}
