#include "cue-deck.h"
#include <QDialog>
#include <QFormLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QSaveFile>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QSignalBlocker>
#include <QCryptographicHash>
#include <cmath>
#include <QUuid>
#include <QRegularExpression>
#include <algorithm>

namespace augment {
namespace {
constexpr int kMaxCues=32,kMaxSteps=48,kMaxWaitMs=30000,kMaxTotalWait=120000;
bool validName(const QString &name){
    const auto s=name.trimmed();
    return !s.isEmpty()&&s.size()<=80&&!s.contains('\n')&&!s.contains('\r');
}
bool validId(const QString &id){
    static const QRegularExpression pattern(
        "^[0-9a-fA-F]{8}-(?:[0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}$");
    return pattern.match(id).hasMatch();
}
QString newId(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
bool validStepsData(const QList<CueStep> &steps){
    if(steps.size()>kMaxSteps)return false;
    int wait=0;
    for(const auto &step:steps){
        if(step.type=="wait"){
            if(!step.target.isEmpty()||step.delayMs<1||step.delayMs>kMaxWaitMs)return false;
            wait+=step.delayMs;
            if(wait>kMaxTotalWait)return false;
        }else if(step.type=="hotkey"){
            if(step.delayMs!=0||!step.target.startsWith("hotkey:v1:")||
               step.target.size()>2048)return false;
        }else if(step.type=="layout"){
            if(step.delayMs!=0||step.target.isEmpty()||step.target.size()>256)return false;
        }else return false;
    }
    return true;
}
QString stepLabel(const CueStep &step,const QList<CueChoice> &layouts,
                  const QList<CueChoice> &hotkeys){
    if(step.type=="wait")return QString("Wait %1 ms").arg(step.delayMs);
    const auto &choices=step.type=="layout"?layouts:hotkeys;
    for(const auto &choice:choices)if(choice.id==step.target)
        return (step.type=="layout"?"Layout · ":"Hotkey · ")+
            (choice.group.isEmpty()?QString{}:choice.group+" / ")+choice.name;
    return (step.type=="layout"?"Layout unavailable · ":"Hotkey unavailable · ")+step.target.left(80);
}
QString choose(QWidget *owner,const QString &title,const CueDeck::Choices &provider){
    const auto options=provider?provider():QList<CueChoice>{};
    if(options.isEmpty()){QMessageBox::information(owner,title,"No available actions. Check the OBS plugin and source state.");return {};}
    QStringList labels;
    for(const auto &item:options){
        QString label=(item.group.isEmpty()?QString{}:item.group+" / ")+item.name;
        if(labels.contains(label)){
            // Stable identities live in data, not in gigantic visible JSON
            // hotkey binding strings that stretch native dialogs off screen.
            const auto hash=QCryptographicHash::hash(item.id.toUtf8(),
                QCryptographicHash::Sha256).toHex().left(7);
            label+=" · "+QString::fromLatin1(hash);
        }
        labels.append(label);
    }
    bool ok=false;
    const auto value=QInputDialog::getItem(owner,title,"Action",labels,0,false,&ok);
    const int index=labels.indexOf(value);
    return ok&&index>=0?options[index].id:QString{};
}
}

CueDeck::CueDeck(QString path,Choices layouts,Dispatch applyLayout,
                 Choices hotkeys,Dispatch triggerHotkey,QObject *parent)
    :QObject(parent),path_(std::move(path)),layouts_(std::move(layouts)),
     hotkeys_(std::move(hotkeys)),applyLayout_(std::move(applyLayout)),
     triggerHotkey_(std::move(triggerHotkey)){
    timer_.setSingleShot(true);
    QObject::connect(&timer_,&QTimer::timeout,this,[this]{advance();});
    if(!reload()&&QFile::exists(path_))status_="Cue file invalid; presets not loaded";
}
CueDeck::~CueDeck(){cancel();closeManager();}
bool CueDeck::validSteps(const QList<CueStep> &steps)const{return validStepsData(steps);}
bool CueDeck::reload(){
    if(running_)return false;
    QFile f(path_);
    if(!f.exists())return true;
    if(!f.open(QIODevice::ReadOnly)||f.size()>256*1024)return false;
    QJsonParseError error;
    const auto doc=QJsonDocument::fromJson(f.readAll(),&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject())return false;
    const auto root=doc.object();
    if(root.value("version").toInt()!=1||!root.value("cues").isArray())return false;
    const auto raw=root.value("cues").toArray();
    if(raw.size()>kMaxCues)return false;
    QList<Cue> parsed;
    QSet<QString> ids,names;
    for(const auto &value:raw){
        if(!value.isObject())return false;
        const auto obj=value.toObject();
        const auto id=obj.value("id").toString(),name=obj.value("name").toString();
        if(!validId(id)||!validName(name)||!obj.value("steps").isArray())return false;
        if(ids.contains(id)||names.contains(name.trimmed().toCaseFolded()))return false;
        ids.insert(id);names.insert(name.trimmed().toCaseFolded());
        Cue cue{id,name.trimmed(),{}};
        const auto items=obj.value("steps").toArray();
        if(items.size()>kMaxSteps)return false;
        for(const auto &item:items){
            if(!item.isObject())return false;
            const auto step=item.toObject();
            if(!step.value("type").isString()||!step.value("target").isString()||
               !step.value("delayMs").isDouble())return false;
            const auto delay=step.value("delayMs").toDouble();
            if(!std::isfinite(delay)||delay<0||delay>kMaxWaitMs||
               std::floor(delay)!=delay)return false;
            cue.steps.append({step.value("type").toString(),
                    step.value("target").toString(),int(delay)});
        }
        if(!validSteps(cue.steps))return false;
        parsed.append(cue);
    }
    cues_=parsed;
    changed();
    return true;
}
bool CueDeck::save() const{
    QJsonArray array;
    for(const auto &cue:cues_){
        QJsonArray steps;
        for(const auto &item:cue.steps)
            steps.append(QJsonObject{{"type",item.type},{"target",item.target},
                                     {"delayMs",item.delayMs}});
        array.append(QJsonObject{{"id",cue.id},{"name",cue.name},{"steps",steps}});
    }
    QSaveFile f(path_);
    if(!f.open(QIODevice::WriteOnly))return false;
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"cues",array}})
        .toJson(QJsonDocument::Indented);
    return f.write(bytes)==bytes.size()&&f.commit();
}
QString CueDeck::addCue(const QString &name){
    const QString clean=name.trimmed();
    if(!validName(clean)||cues_.size()>=kMaxCues)return {};
    for(const auto &cue:cues_)
        if(cue.name.compare(clean,Qt::CaseInsensitive)==0)return {};
    const QString id=newId();
    cues_.append({id,clean,{}});
    if(!save()){cues_.removeLast();return {};}
    changed();
    return id;
}
bool CueDeck::updateCue(const QString &id,const QString &name,const QList<CueStep> &steps){
    const QString clean=name.trimmed();
    if(running_||!validName(clean)||!validSteps(steps))return false;
    for(const auto &cue:cues_)if(cue.id!=id&&cue.name.compare(clean,Qt::CaseInsensitive)==0)return false;
    for(auto &cue:cues_)if(cue.id==id){
        const auto old=cue;
        cue.name=clean;cue.steps=steps;
        if(!save()){cue=old;return false;}
        changed();
        return true;
    }
    return false;
}
bool CueDeck::removeCue(const QString &id){
    if(running_)return false;
    for(int i=0;i<cues_.size();++i)if(cues_[i].id==id){
        const auto prior=cues_;
        cues_.removeAt(i);
        if(!save()){cues_=prior;return false;}
        changed();return true;
    }
    return false;
}
bool CueDeck::preflight(const Cue &cue){
    if(cue.steps.isEmpty()){announce("Cue has no actions");return false;}
    const auto layouts=layouts_?layouts_():QList<CueChoice>{};
    const auto keys=hotkeys_?hotkeys_():QList<CueChoice>{};
    for(const auto &step:cue.steps){
        if(step.type=="wait")continue;
        const auto &list=step.type=="layout"?layouts:keys;
        const bool present=std::any_of(list.begin(),list.end(),[&](const CueChoice &choice){
            return choice.id==step.target;
        });
        if(!present){announce("Unavailable "+step.type+" action: "+step.target);return false;}
        if(step.type=="layout"&&!applyLayout_){announce("Layout dispatch unavailable");return false;}
        if(step.type=="hotkey"&&!triggerHotkey_){announce("Hotkey dispatch unavailable");return false;}
    }
    return true;
}
bool CueDeck::run(const QString &id){
    if(running_)return false;
    for(const auto &cue:cues_)if(cue.id==id){
        if(!preflight(cue))return false;
        activeSteps_=cue.steps;
        cursor_=0;dispatched_=0;running_=true;
        announce("Running · "+cue.name);
        advance();
        return true;
    }
    announce("Cue not found");return false;
}
void CueDeck::advance(){
    while(running_&&cursor_<activeSteps_.size()){
        const auto step=activeSteps_[cursor_++];
        if(step.type=="wait"){timer_.start(step.delayMs);return;}
        const bool ok=step.type=="layout"?applyLayout_(step.target):triggerHotkey_(step.target);
        if(!ok){
            timer_.stop();running_=false;activeSteps_.clear();
            announce("Dispatch failed · "+step.type+" "+step.target);
            return;
        }
        ++dispatched_;
    }
    if(running_){
        running_=false;activeSteps_.clear();
        announce(QString("Completed · %1 action(s) dispatched (effects not confirmed)").arg(dispatched_));
    }
}
void CueDeck::cancel(){
    if(!running_)return;
    timer_.stop();running_=false;activeSteps_.clear();
    announce(QString("Stopped · %1 action(s) already dispatched").arg(dispatched_));
}
void CueDeck::announce(const QString &status){status_=status;changed();}
void CueDeck::changed(){if(refreshManager_)refreshManager_();}
bool CueDeck::editCue(QWidget *owner,const QString &id){
    Cue initial;
    if(!id.isEmpty()){
        auto it=std::find_if(cues_.begin(),cues_.end(),[&](const Cue &c){return c.id==id;});
        if(it==cues_.end())return false;
        initial=*it;
    }
    QDialog dialog(owner);
    dialog.setWindowTitle(id.isEmpty()?"New Cue":"Edit Cue");
    dialog.resize(580,490);
    auto *root=new QVBoxLayout(&dialog);
    auto *name=new QLineEdit(initial.name,&dialog);
    name->setObjectName("aftermarket-cue-name");
    name->setPlaceholderText("Cue name");
    root->addWidget(name);
    QList<CueStep> steps=initial.steps;
    auto *list=new QListWidget(&dialog);
    list->setObjectName("aftermarket-cue-steps");
    root->addWidget(list,1);
    auto rebuild=[&]{
        const int old=list->currentRow();list->clear();
        const auto layouts=layouts_?layouts_():QList<CueChoice>{};
        const auto hotkeys=hotkeys_?hotkeys_():QList<CueChoice>{};
        for(int i=0;i<steps.size();++i)
            list->addItem(QString("%1. ").arg(i+1)+stepLabel(steps[i],layouts,hotkeys));
        if(!steps.isEmpty())list->setCurrentRow(qBound(0,old,steps.size()-1));
    };
    auto *buttons=new QHBoxLayout();
    auto *addLayout=new QPushButton("+ Layout",&dialog);
    auto *addHotkey=new QPushButton("+ Hotkey",&dialog);
    auto *addWait=new QPushButton("+ Wait",&dialog);
    auto *up=new QPushButton("Up",&dialog),*down=new QPushButton("Down",&dialog);
    auto *remove=new QPushButton("Remove",&dialog);
    for(auto *b:{addLayout,addHotkey,addWait,up,down,remove})buttons->addWidget(b);
    root->addLayout(buttons);
    QObject::connect(addLayout,&QPushButton::clicked,&dialog,[&]{
        const auto target=choose(&dialog,"Add layout action",layouts_);
        if(!target.isEmpty()&&steps.size()<kMaxSteps){steps.append({"layout",target,0});rebuild();}
    });
    QObject::connect(addHotkey,&QPushButton::clicked,&dialog,[&]{
        const auto target=choose(&dialog,"Add OBS or plugin hotkey",hotkeys_);
        if(!target.isEmpty()&&steps.size()<kMaxSteps){steps.append({"hotkey",target,0});rebuild();}
    });
    QObject::connect(addWait,&QPushButton::clicked,&dialog,[&]{
        bool ok=false;
        const auto duration=QInputDialog::getInt(&dialog,"Wait between actions",
            "Milliseconds",250,1,kMaxWaitMs,25,&ok);
        if(ok&&steps.size()<kMaxSteps){steps.append({"wait",{},duration});rebuild();}
    });
    QObject::connect(up,&QPushButton::clicked,&dialog,[&]{
        const int i=list->currentRow();
        if(i<1)return;
        steps.swapItemsAt(i,i-1);rebuild();list->setCurrentRow(i-1);
    });
    QObject::connect(down,&QPushButton::clicked,&dialog,[&]{
        const int i=list->currentRow();
        if(i<0||i+1>=steps.size())return;
        steps.swapItemsAt(i,i+1);rebuild();list->setCurrentRow(i+1);
    });
    QObject::connect(remove,&QPushButton::clicked,&dialog,[&]{
        const int i=list->currentRow();
        if(i>=0&&i<steps.size()){steps.removeAt(i);rebuild();}
    });
    auto *hint=new QLabel("A hotkey dispatch can start an OBS effect or change what's on air. "
                          "Cue completion means actions were issued—not that an effect finished.",&dialog);
    hint->setWordWrap(true);root->addWidget(hint);
    auto *bottom=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);
    root->addWidget(bottom);
    QObject::connect(bottom,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    QObject::connect(bottom,&QDialogButtonBox::accepted,&dialog,[&]{
        if(!validName(name->text())||!validSteps(steps)){
            QMessageBox::warning(&dialog,"Invalid cue","Check the cue name, actions, and total wait duration.");
            return;
        }
        const bool ok=id.isEmpty()?(!addCue(name->text()).isEmpty()):updateCue(id,name->text(),steps);
        if(!ok){QMessageBox::warning(&dialog,"Save failed","Cue could not be saved, or its name already exists.");return;}
        if(id.isEmpty()){
            const QString created=cues_.last().id;
            if(!updateCue(created,name->text(),steps)){
                removeCue(created);
                QMessageBox::warning(&dialog,"Save failed","Cue actions could not be saved.");
                return;
            }
        }
        dialog.accept();
    });
    rebuild();
    return dialog.exec()==QDialog::Accepted;
}
QWidget *CueDeck::createPage(QWidget *parent){
    if(manager_)return manager_;
    auto *dialog=new QWidget(parent);
    manager_=dialog;
    dialog->setObjectName("aftermarket-cue-deck-manager");
    auto *root=new QVBoxLayout(dialog);
    root->setContentsMargins(6,6,6,6);root->setSpacing(5);
    auto *title=new QLabel("Sequence native layouts, OBS actions and delays.",dialog);
    title->setWordWrap(true);root->addWidget(title);
    auto *split=new QSplitter(Qt::Horizontal,dialog);split->setChildrenCollapsible(false);
    auto *cueGroup=new QGroupBox("Saved cues",split);
    auto *cueLayout=new QVBoxLayout(cueGroup);cueLayout->setContentsMargins(6,4,6,4);
    auto *list=new QListWidget(cueGroup);
    list->setObjectName("aftermarket-cue-list");list->setTextElideMode(Qt::ElideRight);
    cueLayout->addWidget(list,1);
    auto *stepGroup=new QGroupBox("Cue steps",split);
    auto *stepLayout=new QVBoxLayout(stepGroup);stepLayout->setContentsMargins(6,4,6,4);
    auto *detail=new QListWidget(stepGroup);
    detail->setObjectName("aftermarket-cue-step-preview");
    detail->setSelectionMode(QAbstractItemView::NoSelection);
    detail->setFocusPolicy(Qt::NoFocus);
    detail->setWordWrap(true);detail->setTextElideMode(Qt::ElideNone);
    detail->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detail->setAccessibleName("Steps of selected cue");
    stepLayout->addWidget(detail,1);
    split->addWidget(cueGroup);split->addWidget(stepGroup);split->setSizes({240,380});
    root->addWidget(split,1);
    auto *status=new QLabel(status_,dialog);
    status->setObjectName("aftermarket-cue-status");
    status->setWordWrap(true);root->addWidget(status);
    auto *buttons=new QHBoxLayout();
    auto *create=new QPushButton("New cue",dialog);
    auto *edit=new QPushButton("Edit",dialog);
    auto *remove=new QPushButton("Delete",dialog);
    auto *run=new QPushButton("Run cue",dialog);
    run->setObjectName("aftermarket-cue-run");
    auto *stop=new QPushButton("Stop",dialog);
    stop->setObjectName("aftermarket-cue-stop");
    for(auto *b:{create,edit,remove})buttons->addWidget(b);
    buttons->addStretch();buttons->addWidget(run);buttons->addWidget(stop);
    root->addLayout(buttons);
    QPointer<QWidget> safe=dialog;
    auto selected=[list](){auto *item=list->currentItem();return item?item->data(Qt::UserRole).toString():QString{};};
    refreshManager_=[this,safe,list,detail,status,edit,remove,run,stop]{
        if(!safe)return;
        const QString current=list->currentItem()?list->currentItem()->data(Qt::UserRole).toString():QString{};
        const QSignalBlocker block(list);
        list->clear();int row=-1;
        for(int i=0;i<cues_.size();++i){
            const auto &cue=cues_[i];
            auto *item=new QListWidgetItem(QString("%1   ·   %2 steps").arg(cue.name).arg(cue.steps.size()),list);
            item->setData(Qt::UserRole,cue.id);
            if(cue.id==current)row=i;
        }
        if(row<0&&list->count())row=0;
        if(row>=0)list->setCurrentRow(row);
        status->setText(status_);
        const QString id=list->currentItem()?list->currentItem()->data(Qt::UserRole).toString():QString{};
        auto it=std::find_if(cues_.begin(),cues_.end(),[&](const Cue &cue){return cue.id==id;});
        QStringList labels;
        if(it!=cues_.end()){
            const auto layouts=layouts_?layouts_():QList<CueChoice>{};
            const auto hotkeys=hotkeys_?hotkeys_():QList<CueChoice>{};
            for(const auto &step:it->steps)labels<<stepLabel(step,layouts,hotkeys);
        }
        detail->clear();
        for(int i=0;i<labels.size();++i){
            auto *step=new QListWidgetItem(QString("%1.  %2").arg(i+1).arg(labels.at(i)),detail);
            step->setToolTip(labels.at(i));
        }
        edit->setEnabled(!running_&&it!=cues_.end());
        remove->setEnabled(!running_&&it!=cues_.end());
        run->setEnabled(!running_&&it!=cues_.end()&&!it->steps.isEmpty());
        stop->setEnabled(running_);
    };
    QObject::connect(list,&QListWidget::itemSelectionChanged,dialog,[this]{changed();});
    QObject::connect(create,&QPushButton::clicked,dialog,[this,dialog]{
        if(!running_)editCue(dialog);
    });
    QObject::connect(edit,&QPushButton::clicked,dialog,[this,dialog,selected]{
        const auto id=selected();if(!running_&&!id.isEmpty())editCue(dialog,id);
    });
    QObject::connect(remove,&QPushButton::clicked,dialog,[this,dialog,selected]{
        const auto id=selected();
        if(running_||id.isEmpty())return;
        if(QMessageBox::question(dialog,"Delete cue","Delete the selected cue?",
                QMessageBox::Yes|QMessageBox::No)==QMessageBox::Yes)removeCue(id);
    });
    QObject::connect(run,&QPushButton::clicked,dialog,[this,selected]{
        if(!running_)this->run(selected());
    });
    QObject::connect(stop,&QPushButton::clicked,dialog,[this]{cancel();});
    QObject::connect(dialog,&QObject::destroyed,this,[this]{refreshManager_={};manager_=nullptr;});
    changed();
    return dialog;
}
void CueDeck::closeManager(){delete manager_.data();}
void CueDeck::refreshPage(){changed();}
} // namespace augment
