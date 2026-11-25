#include "property-preset-footer.h"
#include "property-presets.h"
#include <obs.hpp>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QBoxLayout>
#include <QComboBox>
#include <QToolButton>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QListWidget>
#include <QPointer>
#include <QTimer>
#include <QMap>
#include <QSignalBlocker>
#include <QPushButton>
#include <functional>
Q_DECLARE_METATYPE(OBSSource);
namespace augment {
namespace {
QString utf8(const char *s){return s?QString::fromUtf8(s):QString{};}
QString typeOf(obs_source_t *s){
    if(!s)return {};
    return (obs_source_get_type(s)==OBS_SOURCE_TYPE_FILTER?QStringLiteral("filter:"):QStringLiteral("input:"))+utf8(obs_source_get_id(s));
}
OBSSource titleTarget(QDialog *dialog,bool filters){
    const auto format=utf8(obs_frontend_get_locale_string(filters?"Basic.Filters.Title":"Basic.PropertiesWindow"));
    if(!format.contains("%1"))return {};
    struct Lookup{QString format,title;OBSSource result;int count=0;};
    Lookup lookup{format,dialog->windowTitle(),{},0};
    auto match=[](void *p,obs_source_t *s){
        auto &l=*static_cast<Lookup*>(p);
        if(!obs_source_removed(s)&&l.format.arg(utf8(obs_source_get_name(s)))==l.title){
            if(l.result==s)return true;
            ++l.count;l.result=s;
        }
        return true;
    };
    obs_enum_sources(match,&lookup);
    // Scene sources aren't included by all OBS source enumerators.
    if(filters)obs_enum_scenes(match,&lookup);
    return lookup.count==1?lookup.result:OBSSource{};
}
OBSSource listTarget(QListWidget *list){
    if(!list||!list->currentItem())return {};
    const auto v=list->currentItem()->data(Qt::UserRole);
    if(v.metaType()!=QMetaType::fromType<OBSSource>())return {};
    return v.value<OBSSource>();
}
QWidget *visibleView(QDialog *dialog){
    QWidget *view=nullptr;
    for(auto *w:dialog->findChildren<QWidget*>()){
        if(!w->inherits("OBSPropertiesView")||!w->isVisible())continue;
        if(view)return nullptr;
        view=w;
    }
    return view;
}
QByteArray settingsOf(obs_source_t *source){
    OBSDataAutoRelease data=obs_source_get_settings(source);
    return data?QByteArray(obs_data_get_json(data)):QByteArray{};
}
bool supports(obs_source_t *s){
    if(!s||obs_source_removed(s))return false;
    if(obs_source_get_type(s)!=OBS_SOURCE_TYPE_INPUT&&obs_source_get_type(s)!=OBS_SOURCE_TYPE_FILTER)return false;
    // Deferred native property editors require a separate transaction path.
    auto *props=obs_source_properties(s);
    const bool okay=props&&!(obs_properties_get_flags(props)&OBS_PROPERTIES_DEFER_UPDATE);
    if(props)obs_properties_destroy(props);
    return okay;
}
struct Footer:QWidget {
    Footer(QDialog *owner):QWidget(owner),dialog(owner){
        setObjectName("aftermarket-property-presets");
        auto *layout=new QHBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(3);
        combo=new QComboBox(this);combo->setObjectName("aftermarket-preset-select");
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setMinimumContentsLength(8);combo->setMaximumWidth(190);
        load=new QToolButton(this);load->setObjectName("aftermarket-preset-load");load->setText("Load");load->setToolTip("Apply selected preset");
        save=new QToolButton(this);save->setObjectName("aftermarket-preset-save");save->setText("Save");save->setToolTip("Save current properties as a new preset");
        more=new QToolButton(this);more->setObjectName("aftermarket-preset-more");more->setText("⋮");more->setPopupMode(QToolButton::InstantPopup);more->setToolTip("Manage presets");
        menu=new QMenu(more);more->setMenu(menu);
        update=menu->addAction("Update selected preset…");rename=menu->addAction("Rename…");remove=menu->addAction("Delete…");
        menu->addSeparator();undo=menu->addAction("Undo last preset load");
        layout->addWidget(combo);layout->addWidget(load);layout->addWidget(save);layout->addWidget(more);
        setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    }
    QPointer<QDialog> dialog;
    QPointer<QWidget> view;
    QPointer<QListWidget> lastList;
    QComboBox *combo;QToolButton *load,*save,*more;QMenu *menu;QAction *update,*rename,*remove,*undo;
    QString uuid,type,ownerUuid,undoUuid;QByteArray previous,lastApplied;
    bool filter=false;
    OBSSource target(){
        if(!dialog||!view||visibleView(dialog)!=view)return {};
        OBSSource parent=titleTarget(dialog,filter);
        if(!parent||utf8(obs_source_get_uuid(parent))!=ownerUuid)return {};
        if(!filter)return utf8(obs_source_get_uuid(parent))==uuid?parent:OBSSource{};
        auto *a=dialog->findChild<QListWidget*>("asyncFilters");
        auto *b=dialog->findChild<QListWidget*>("effectFilters");
        OBSSource x=listTarget(a),y=listTarget(b),result;
        if(x&&y){
            if(lastList==a)result=x;else if(lastList==b)result=y;
        }else result=x?x:y;
        if(!result||obs_filter_get_parent(result)!=parent||utf8(obs_source_get_uuid(result))!=uuid)return {};
        return result;
    }
    void refresh(PropertyPresets &store,const QString &selection={}){
        QString wanted=selection.isEmpty()?combo->currentData().toString():selection;
        QSignalBlocker block(combo);combo->clear();combo->addItem("Presets…",QString{});
        for(const auto &name:store.names(type))combo->addItem(name,name);
        int i=combo->findData(wanted);combo->setCurrentIndex(i<0?0:i);
        buttons();
    }
    void buttons(){
        bool bound=!uuid.isEmpty()&&!type.isEmpty();
        bool chosen=bound&&!combo->currentData().toString().isEmpty();
        combo->setEnabled(bound);save->setEnabled(bound);load->setEnabled(chosen);update->setEnabled(chosen);
        rename->setEnabled(chosen);remove->setEnabled(chosen);undo->setEnabled(bound&&uuid==undoUuid&&!previous.isEmpty());
    }
    bool apply(const QByteArray &bytes){
        auto source=target();if(!source||!supports(source)||typeOf(source)!=type)return false;
        OBSDataAutoRelease data=obs_data_create_from_json(bytes.constData());
        if(!data)return false;
        // Flush any pending native widget undo callback before changing settings.
        QPointer<QWidget> guarded=view;
        QMetaObject::invokeMethod(view,"ReloadProperties",Qt::DirectConnection);
        if(!guarded||!target()||obs_source_removed(source))return false;
        obs_source_reset_settings(source,data);
        QMetaObject::invokeMethod(guarded,"ReloadProperties",Qt::DirectConnection);
        QMetaObject::invokeMethod(guarded,"SignalChanged",Qt::DirectConnection);
        if(filter)obs_frontend_save();
        return true;
    }
};
}
struct PropertyPresetFooter::State{
    explicit State(QString path):store(std::move(path)){}
    PropertyPresets store;
    QList<QPointer<Footer>> footers;
    bool queued=false;
};
PropertyPresetFooter::PropertyPresetFooter(QString path,QObject *parent):QObject(parent),state(new State(std::move(path))){
    qApp->installEventFilter(this);schedule();
}
PropertyPresetFooter::~PropertyPresetFooter(){
    qApp->removeEventFilter(this);
    for(auto &f:state->footers)if(f)delete f.data();
}
void PropertyPresetFooter::schedule(){
    if(state->queued)return;state->queued=true;
    QTimer::singleShot(0,this,[this]{state->queued=false;scan();});
}
bool PropertyPresetFooter::eventFilter(QObject *obj,QEvent *event){
    auto *w=qobject_cast<QWidget*>(obj);
    if(!w)return false;
    const auto kind=event->type();
    if(kind==QEvent::Show||kind==QEvent::Hide||kind==QEvent::WindowTitleChange){
        if(w->inherits("OBSBasicProperties")||w->inherits("OBSBasicFilters")||w->inherits("OBSPropertiesView"))schedule();
    }
    if(kind==QEvent::StyleChange&&w->objectName()=="buttonBox")schedule();
    if(kind==QEvent::FocusIn){
        auto *list=qobject_cast<QListWidget*>(w);
        if(list&&(list->objectName()=="asyncFilters"||list->objectName()=="effectFilters")){
            for(auto &f:state->footers)if(f&&list->window()==f->dialog){f->lastList=list;schedule();}
        }
    }
    return false;
}
void PropertyPresetFooter::scan(){
    for(auto it=state->footers.begin();it!=state->footers.end();)if(it->isNull())it=state->footers.erase(it);else ++it;
    for(auto *window:QApplication::topLevelWidgets()){
        auto *dialog=qobject_cast<QDialog*>(window);
        if(!dialog||!dialog->isVisible())continue;
        const bool filter=dialog->inherits("OBSBasicFilters");
        if(!filter&&!dialog->inherits("OBSBasicProperties"))continue;
        auto *box=dialog->findChild<QDialogButtonBox*>("buttonBox");
        auto *layout=box?qobject_cast<QBoxLayout*>(box->layout()):nullptr;
        if(!layout||!box->button(filter?QDialogButtonBox::Close:QDialogButtonBox::Ok))continue;
        Footer *f=nullptr;for(auto &item:state->footers)if(item&&item->dialog==dialog){f=item;break;}
        if(!f){
            f=new Footer(dialog);f->filter=filter;state->footers.append(f);
            for(auto *list:{dialog->findChild<QListWidget*>("asyncFilters"),dialog->findChild<QListWidget*>("effectFilters")}){
                if(!list)continue;
                if(list->hasFocus())f->lastList=list;
                connect(list,&QListWidget::currentRowChanged,f,[this,f,list](int){f->lastList=list;schedule();});
            }
            auto refreshAll=[this,f](const QString &name){
                for(auto &entry:state->footers)if(entry)entry->refresh(state->store,entry==f?name:QString{});
            };
            auto warn=[f]{if(!f->dialog)return;auto *box=new QMessageBox(QMessageBox::Warning,"Presets","The preset could not be saved or applied. The target may have changed or the preset file may be invalid.",QMessageBox::Ok,f);box->setAttribute(Qt::WA_DeleteOnClose);box->open();};
            // Heap-owned asynchronous prompt; recovery/removal destroys it with the footer.
            auto askName=[f](const QString &title,const QString &initial,std::function<void(QString)> callback){
                auto *prompt=new QInputDialog(f);prompt->setAttribute(Qt::WA_DeleteOnClose);prompt->setWindowTitle(title);
                prompt->setLabelText("Preset name (settings may include local paths or credentials)");prompt->setTextValue(initial);
                QObject::connect(prompt,&QInputDialog::textValueSelected,f,[callback](const QString &value){callback(value.trimmed());});
                prompt->open();
            };
            connect(f->combo,qOverload<int>(&QComboBox::currentIndexChanged),f,[f]{f->buttons();});
            connect(f->save,&QToolButton::clicked,f,[this,f,askName,refreshAll,warn]{
                auto source=f->target();if(!source)return;
                auto type=f->type,uuid=f->uuid;
                askName("Save preset",{},[this,f,type,uuid,refreshAll,warn](QString name){
                    auto target=f->target();if(!target||f->uuid!=uuid||f->type!=type)return;
                    if(!state->store.save(type,name,settingsOf(target),false)){warn();return;}
                    refreshAll(name);
                });
            });
            connect(f->load,&QToolButton::clicked,f,[this,f,warn]{
                auto target=f->target();if(!target)return;
                auto data=state->store.read(f->type,f->combo->currentData().toString());
                auto previous=settingsOf(target);
                if(data.isEmpty()||!f->apply(data)){warn();return;}
                f->undoUuid=f->uuid;f->previous=previous;f->lastApplied=settingsOf(target);f->buttons();
            });
            connect(f->undo,&QAction::triggered,f,[f,warn]{
                if(f->uuid!=f->undoUuid||f->previous.isEmpty())return;
                auto target=f->target();
                if(!target||settingsOf(target)!=f->lastApplied){f->previous.clear();f->buttons();warn();return;}
                if(!f->apply(f->previous)){warn();return;}
                f->previous.clear();f->buttons();
            });
            connect(f->rename,&QAction::triggered,f,[this,f,askName,refreshAll,warn]{
                const auto type=f->type,old=f->combo->currentData().toString();
                askName("Rename preset",old,[this,f,type,old,refreshAll,warn](QString name){
                    if(f->type!=type||name==old)return;
                    if(!state->store.rename(type,old,name)){warn();return;}
                    refreshAll(name);
                });
            });
            // Destructive operations require an explicit confirmation; the
            // callback is disconnected if recovery destroys the footer.
            auto confirm=[f](QString title,QString message,std::function<void()> callback){
                auto *prompt=new QMessageBox(QMessageBox::Question,title,message,QMessageBox::Yes|QMessageBox::No,f);
                prompt->setAttribute(Qt::WA_DeleteOnClose);prompt->setTextFormat(Qt::PlainText);prompt->setDefaultButton(QMessageBox::No);
                QObject::connect(prompt,&QMessageBox::finished,f,[callback](int result){if(result==QMessageBox::Yes)callback();});
                prompt->open();
            };
            connect(f->update,&QAction::triggered,f,[this,f,confirm,refreshAll,warn]{
                const auto type=f->type,uuid=f->uuid,name=f->combo->currentData().toString();
                confirm("Update preset","Replace the saved settings for "+name+"?",[this,f,type,uuid,name,refreshAll,warn]{
                    auto target=f->target();if(!target||f->uuid!=uuid||f->type!=type)return;
                    if(!state->store.save(type,name,settingsOf(target),true)){warn();return;}
                    refreshAll(name);
                });
            });
            connect(f->remove,&QAction::triggered,f,[this,f,confirm,refreshAll,warn]{
                const auto type=f->type,name=f->combo->currentData().toString();
                confirm("Delete preset","Delete "+name+"?",[this,f,type,name,refreshAll,warn]{
                    if(f->type!=type)return;
                    if(!state->store.remove(type,name)){warn();return;}
                    refreshAll({});
                });
            });
        }
        if(layout->indexOf(f)<0){
            QAbstractButton *defaults=box->button(QDialogButtonBox::RestoreDefaults);
            if(!defaults)for(auto *button:box->buttons())if(box->buttonRole(button)==QDialogButtonBox::ResetRole){defaults=button;break;}
            int index=defaults?layout->indexOf(defaults):-1;
            layout->insertWidget(index<0?0:index+1,f);
        }
        auto *view=visibleView(dialog);
        OBSSource target=titleTarget(dialog,filter);
        const auto ownerId=target?utf8(obs_source_get_uuid(target)):QString{};
        if(f->ownerUuid.isEmpty())f->ownerUuid=ownerId;
        if(ownerId!=f->ownerUuid)target={};
        if(filter){
            auto *a=dialog->findChild<QListWidget*>("asyncFilters");
            auto *b=dialog->findChild<QListWidget*>("effectFilters");
            auto x=listTarget(a),y=listTarget(b);OBSSource selected;
            if(x&&y){if(f->lastList==a)selected=x;else if(f->lastList==b)selected=y;}
            else selected=x?x:y;
            if(!selected||!target||obs_filter_get_parent(selected)!=target)target={};else target=selected;
        }
        const auto uuid=target?utf8(obs_source_get_uuid(target)):QString{};
        if(f->view!=view||f->uuid!=uuid){
            f->view=view;f->uuid.clear();f->type.clear();
            if(view&&supports(target)){f->uuid=uuid;f->type=typeOf(target);}
            f->refresh(state->store);
        }
        f->setVisible(!f->uuid.isEmpty());
    }
}
}
