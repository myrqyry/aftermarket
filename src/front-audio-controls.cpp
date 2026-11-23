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
#include <QMenu>
#include <QToolBar>
#include <QPushButton>
#include <QToolButton>
#include <QAbstractButton>
#include <QWidgetAction>
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
        QPointer<QLabel> panLabel,offsetLabel;
        QString id;
    };
    QString file;
    Query query;
    SetPan pan;
    SetOffset offset;
    std::function<void()> persist;
    QTimer saveDebounce;
    QPointer<QWidget> native;
    QPointer<QToolBar> footer;
    QPointer<QToolButton> menuButton;
    QPointer<QPushButton> hiddenButton,optionsButton;
    QPointer<QToolButton> layoutButton,advancedButton;
    QPointer<QMenu> nativeOptions;
    QString nativeOptionsTitle;
    bool footerWasHidden=false;
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
    void findAndHideFooter(){
        if(!native)return;
        // Match the actual OBS AudioMixer toolbar, never arbitrary dock
        // toolbars. Unknown third-party buttons leave the native row intact.
        for(auto *candidate:native->findChildren<QToolBar*>(
                QString(),Qt::FindDirectChildrenOnly)){
            QPushButton *hidden=nullptr,*options=nullptr;
            QToolButton *orientation=nullptr,*advanced=nullptr;
            int recognized=0,unknown=0;
            for(auto *child:candidate->findChildren<QAbstractButton*>(
                    QString(),Qt::FindDirectChildrenOnly)){
                if(child->objectName()=="qt_toolbar_ext_button")continue;
                if(auto *push=qobject_cast<QPushButton*>(child)){
                    if(push->text().contains("hidden",Qt::CaseInsensitive)){
                        hidden=push;++recognized;
                    }else if(push->text()=="Options"){options=push;++recognized;}
                    else ++unknown;
                }else if(auto *tool=qobject_cast<QToolButton*>(child)){
                    if(tool->text().contains("Advanced Audio Properties",
                                           Qt::CaseInsensitive)){
                        advanced=tool;++recognized;
                    }else if(!orientation){orientation=tool;++recognized;}
                    else ++unknown;
                }else ++unknown;
            }
            if(recognized!=4||unknown||!hidden||!options||
               !orientation||!advanced)continue;
            footer=candidate;
            hiddenButton=hidden;optionsButton=options;
            layoutButton=orientation;advancedButton=advanced;
            footerWasHidden=footer->isHidden();
            // OBS owns the QMenu and checkboxes; reuse them as a submenu
            // rather than cloning native actions or modifying preferences.
            for(auto *menu:native->findChildren<QMenu*>(
                    QString(),Qt::FindDirectChildrenOnly)){
                bool isMixerOptions=false;
                for(auto *action:menu->actions())
                    if(action->text().contains("Show hidden sources",
                                                Qt::CaseInsensitive))
                        isMixerOptions=true;
                if(!isMixerOptions)
                    for(auto *check:menu->findChildren<QAbstractButton*>())
                        if(check->text().contains("Show hidden sources",
                                                   Qt::CaseInsensitive))
                            isMixerOptions=true;
                if(isMixerOptions){
                    nativeOptions=menu;
                    nativeOptionsTitle=menu->title();
                    break;
                }
            }
            // Keep the original visible until the native title menu has
            // actually been placed. Floating/overfull titles use the footer.
            break;
        }
    }
    void syncFooterVisibility(){
        if(!footer)return;
        if(footerWasHidden||(menuButton&&menuButton->isVisible()))
            footer->hide();
        else footer->show();
    }
    void restoreFooter(QObject *owner){
        if(menuButton)menuButton->removeEventFilter(owner);
        menuButton.clear();
        if(nativeOptions){
            nativeOptions->setTitle(nativeOptionsTitle);
            nativeOptions.clear();
        }
        if(footer&&!footerWasHidden)footer->show();
        footer.clear();
        hiddenButton.clear();optionsButton.clear();
        layoutButton.clear();advancedButton.clear();
    }
    void syncLayout(Row &r){
        if(!r.controls)return;
        const bool narrow=r.volume&&r.volume->width()<145;
        if(r.panLabel)r.panLabel->setVisible(!narrow);
        if(r.offsetLabel)r.offsetLabel->setVisible(!narrow);
        if(r.panSlider)r.panSlider->setVisible(!useKnobs&&!narrow);
        if(r.panDial)r.panDial->setVisible(useKnobs||narrow);
        if(r.offsetDial)r.offsetDial->setVisible(useKnobs&&!narrow);
        if(r.offsetSpin){
            r.offsetSpin->setMinimumWidth(narrow?46:54);
            r.offsetSpin->setMaximumWidth(narrow?58:78);
        }
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
        syncLayout(r);
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
    state->findAndHideFooter();
    refresh();
    state->poll.start();
    return true;
}
void FrontAudioControls::detach(){
    state->poll.stop();
    state->restoreFooter(this);
    for(auto &row:state->rows)state->remove(row);
    state->rows.clear();
    state->native.clear();
}
bool FrontAudioControls::knobs() const{return state->useKnobs;}
bool FrontAudioControls::setKnobs(bool enabled){
    if(state->useKnobs==enabled)return false;
    state->useKnobs=enabled;
    for(auto &row:state->rows)state->syncLayout(row);
    state->save();
    return true;
}
int FrontAudioControls::rowCount()const{return state->rows.size();}
bool FrontAudioControls::setMenuButton(QToolButton *button){
    if(!state->native||!state->footer||!button)return false;
    if(state->menuButton)state->menuButton->removeEventFilter(this);
    state->menuButton=button;
    button->installEventFilter(this);
    state->syncFooterVisibility();
    return true;
}
bool FrontAudioControls::eventFilter(QObject *watched,QEvent *event){
    if(watched==state->menuButton&&
       (event->type()==QEvent::Show||event->type()==QEvent::Hide)){
        QTimer::singleShot(0,this,[this]{state->syncFooterVisibility();});
    }
    return QObject::eventFilter(watched,event);
}
void FrontAudioControls::populateSettingsMenu(QMenu *menu){
    if(!menu||!state->query)return;
    if(state->footer){
        auto addProxy=[menu](const QString &label,QAbstractButton *button){
            QPointer<QAbstractButton> safe=button;
            auto *action=menu->addAction(label);
            action->setEnabled(bool(safe));
            QObject::connect(action,&QAction::triggered,menu,
                             [safe]{if(safe)safe->click();});
            return action;
        };
        addProxy(state->hiddenButton?
                 state->hiddenButton->text():QStringLiteral("Hidden sources"),
                 state->hiddenButton);
        addProxy("Switch mixer layout",state->layoutButton);
        addProxy("Advanced Audio Properties",state->advancedButton);
        if(state->nativeOptions){
            // QMenu::menuAction is OBS-owned and not deleted by our popup.
            if(state->nativeOptions->title().isEmpty())
                state->nativeOptions->setTitle("Mixer options");
            menu->addMenu(state->nativeOptions);
        }else addProxy("Mixer options",state->optionsButton);
        menu->addSeparator();
    }
    auto *channels=menu->addMenu("Channel settings");
    channels->setObjectName("aftermarket-channel-settings");
    const auto sources=state->query();
    if(sources.isEmpty()){
        channels->setEnabled(false);
        return;
    }
    for(const auto &source:sources){
        if(source.id.isEmpty()||source.name.isEmpty())continue;
        auto *sub=channels->addMenu(source.name);
        sub->setObjectName("aftermarket-channel-"+source.id);
        auto *row=new QWidget(sub);
        auto *layout=new QHBoxLayout(row);
        layout->setContentsMargins(7,4,7,4);
        layout->setSpacing(6);
        layout->addWidget(new QLabel("Pan",row));
        auto *pan=new QSpinBox(row);
        pan->setObjectName("aftermarket-menu-pan");
        pan->setRange(0,100);
        pan->setSuffix("%");
        pan->setValue(qBound(0,qRound(source.balance*100),100));
        pan->setEnabled(source.stereo);
        pan->setToolTip(source.stereo?"Native OBS balance: 0 left, 50 center, 100 right":
                                      "Pan unavailable for this mono source");
        layout->addWidget(pan);
        layout->addWidget(new QLabel("Sync",row));
        auto *offset=new QSpinBox(row);
        offset->setObjectName("aftermarket-menu-offset");
        offset->setRange(-maxOffsetMs,maxOffsetMs);
        offset->setSingleStep(10);
        offset->setSuffix("ms");
        offset->setValue(source.offsetMs);
        layout->addWidget(offset);
        auto *action=new QWidgetAction(sub);
        action->setDefaultWidget(row);
        sub->addAction(action);
        const QString uuid=source.id;
        connect(pan,&QSpinBox::valueChanged,this,[this,uuid](int v){
            if(state->pan&&state->pan(uuid,v/100.0)){
                state->saveDebounce.start();
                refresh();
            }
        });
        connect(offset,&QSpinBox::valueChanged,this,[this,uuid](int v){
            if(state->offset&&state->offset(uuid,v)){
                state->saveDebounce.start();
                refresh();
            }
        });
    }
}
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
            // One horizontal strip avoids stacking two additional control
            // rows under each OBS rotary/fader, including narrow vertical docks.
            controls->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
            controls->setMinimumHeight(30);
            controls->setMaximumHeight(34);
            auto *outer=new QHBoxLayout(controls);
            outer->setContentsMargins(1,1,1,1);
            outer->setSpacing(2);
            auto *panLabel=new QLabel("Pan",controls);
            QFont panFont=panLabel->font();
            panFont.setWeight(QFont::Normal);
            if(panFont.pointSizeF()>0)
                panFont.setPointSizeF(qMax(7.0,panFont.pointSizeF()-0.75));
            panLabel->setFont(panFont);
            panLabel->setMaximumWidth(25);
            outer->addWidget(panLabel);
            auto *pan=new QSlider(Qt::Horizontal,controls);
            pan->setObjectName("aftermarket-pan-slider");
            pan->setRange(0,100);
            pan->setMinimumWidth(22);
            pan->setFixedHeight(20);
            pan->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
            pan->setToolTip("Stereo pan · L 0 · Center 50 · R 100");
            outer->addWidget(pan,1);
            auto *panDial=new QDial(controls);
            panDial->setObjectName("aftermarket-pan-dial");
            panDial->setRange(0,100);
            panDial->setNotchesVisible(false);
            panDial->setFixedSize(24,24);
            panDial->setToolTip(pan->toolTip());
            outer->addWidget(panDial);
            auto *offsetLabel=new QLabel("Δ",controls);
            offsetLabel->setFixedWidth(12);
            offsetLabel->setToolTip("Native OBS sync offset (milliseconds)");
            outer->addWidget(offsetLabel);
            auto *offsetDial=new QDial(controls);
            offsetDial->setObjectName("aftermarket-offset-dial");
            offsetDial->setRange(-dialOffsetMs,dialOffsetMs);
            offsetDial->setFixedSize(24,24);
            offsetDial->setToolTip("Coarse sync offset · ±2000 ms · precise value at right");
            outer->addWidget(offsetDial);
            auto *offset=new QSpinBox(controls);
            offset->setObjectName("aftermarket-offset-ms");
            offset->setRange(-maxOffsetMs,maxOffsetMs);
            offset->setSingleStep(10);
            offset->setFixedHeight(23);
            offset->setSuffix("ms");
            offset->setMinimumWidth(46);
            offset->setMaximumWidth(78);
            offset->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
            offset->setToolTip("Precise OBS audio sync offset in milliseconds");
            outer->addWidget(offset);
            bool inserted=false;
            if(auto *box=qobject_cast<QBoxLayout*>(widget->layout())){
                // Native OBS uses either stacked or side-by-side VolumeControl
                // layouts; preserve its direction and existing child widgets.
                box->addWidget(controls);
                inserted=true;
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
            row.offsetDial=offsetDial;row.panLabel=panLabel;row.offsetLabel=offsetLabel;
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
