#include "dock-layouts.h"
#include <QMainWindow>
#include <QDockWidget>
#include <QSaveFile>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QCryptographicHash>
#include <QSet>
#include <QUuid>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QShowEvent>
#include <QTimer>
#include <QLabel>
#include <algorithm>
#include <utility>

namespace augment {
namespace {
constexpr int maxLayouts=32;
constexpr int maxName=80;
constexpr int maxState=512*1024;
constexpr int maxFile=24*1024*1024;

QByteArray hash(const QByteArray &state) {
    return QCryptographicHash::hash(state,QCryptographicHash::Sha256).toHex();
}
bool uniqueNames(const QList<DockLayoutSnapshot> &items) {
    QSet<QString> names,ids;
    for(const auto &s:items){
        const QString name=s.name.toCaseFolded();
        if(names.contains(name)||ids.contains(s.id))return false;
        names.insert(name);ids.insert(s.id);
    }
    return true;
}
} // namespace

DockLayouts::DockLayouts(QString configPath,QMainWindow *main)
    :path_(std::move(configPath)),main_(main){reload();}

bool DockLayouts::validName(const QString &name){
    if(name!=name.trimmed()||name.isEmpty()||name.size()>maxName)return false;
    for(const QChar c:name)if(c.isNull()||c.isLowSurrogate()||c.isHighSurrogate()||c.category()==QChar::Other_Control)return false;
    return true;
}

QStringList DockLayouts::dockIds()const{
    if(!main_)return {};
    QStringList names;
    for(auto *dock:main_->findChildren<QDockWidget*>()){
        if(dock->objectName().isEmpty()||
           (main_->dockWidgetArea(dock)==Qt::NoDockWidgetArea && !dock->isFloating()))continue;
        names.append(dock->objectName());
    }
    names.sort();
    if(QSet<QString>(names.begin(),names.end()).size()!=names.size())return {};
    return names;
}

bool DockLayouts::write(const QList<DockLayoutSnapshot> &snapshots)const{
    if(snapshots.size()>maxLayouts||!uniqueNames(snapshots))return false;
    QJsonArray array;
    for(const auto &s:snapshots){
        if(!validName(s.name)||QUuid(s.id).isNull()||
           s.state.isEmpty()||s.state.size()>maxState||s.requiredDocks.isEmpty())return false;
        QJsonArray docks;
        for(const auto &id:s.requiredDocks)docks.append(id);
        array.append(QJsonObject{{"id",s.id},{"name",s.name},
                                 {"state",QString::fromLatin1(s.state.toBase64())},
                                 {"sha256",QString::fromLatin1(hash(s.state))},
                                 {"docks",docks}});
    }
    const QByteArray bytes=QJsonDocument(QJsonObject{{"version",1},{"layouts",array}})
        .toJson(QJsonDocument::Indented);
    if(bytes.size()>maxFile)return false;
    if(!QDir().mkpath(QFileInfo(path_).absolutePath()))return false;
    QSaveFile file(path_);
    if(!file.open(QIODevice::WriteOnly))return false;
    if(file.write(bytes)!=bytes.size()){file.cancelWriting();return false;}
    return file.commit();
}

bool DockLayouts::reload(){
    QFile file(path_);
    if(!file.exists()){presets_.clear();return true;}
    if(!file.open(QIODevice::ReadOnly)||file.size()>maxFile)return false;
    QJsonParseError error;
    const auto doc=QJsonDocument::fromJson(file.readAll(),&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject())return false;
    const auto root=doc.object();
    if(root.value("version").toInt()!=1||!root.value("layouts").isArray())return false;
    const auto array=root.value("layouts").toArray();
    if(array.size()>maxLayouts)return false;
    QList<DockLayoutSnapshot> parsed;
    for(const auto &entry:array){
        if(!entry.isObject())return false;
        const auto row=entry.toObject();
        for(const auto &key:{"id","name","state","sha256"})
            if(!row.value(key).isString())return false;
        if(!row.value("docks").isArray())return false;
        DockLayoutSnapshot snapshot;
        snapshot.id=row.value("id").toString();
        snapshot.name=row.value("name").toString();
        const QString encoded=row.value("state").toString();
        if(encoded.size()>maxState*2)return false;
        snapshot.state=QByteArray::fromBase64(encoded.toLatin1());
        if(snapshot.state.isEmpty()||snapshot.state.size()>maxState||
           snapshot.state.toBase64()!=encoded.toLatin1()||
           hash(snapshot.state)!=row.value("sha256").toString().toLatin1()||
           !validName(snapshot.name)||QUuid(snapshot.id).isNull())return false;
        QSet<QString> dockSet;
        for(const auto &v:row.value("docks").toArray()){
            if(!v.isString()||v.toString().isEmpty()||v.toString().size()>256||
               dockSet.contains(v.toString()))return false;
            dockSet.insert(v.toString());
            snapshot.requiredDocks.append(v.toString());
        }
        if(dockSet.isEmpty())return false;
        parsed.append(snapshot);
    }
    if(!uniqueNames(parsed))return false;
    presets_=parsed;
    return true;
}

QString DockLayouts::capture(const QString &name){
    const QString trimmed=name.trimmed();
    if(!validName(trimmed)||!main_||presets_.size()>=maxLayouts)return {};
    for(const auto &item:presets_)if(item.name.compare(trimmed,Qt::CaseInsensitive)==0)return {};
    const QStringList ids=dockIds();
    const QByteArray state=main_->saveState();
    if(ids.isEmpty()||state.isEmpty()||state.size()>maxState)return {};
    DockLayoutSnapshot snapshot{QUuid::createUuid().toString(QUuid::WithoutBraces),trimmed,ids,state};
    auto next=presets_;next.append(snapshot);
    if(!write(next))return {};
    presets_=next;
    return snapshot.id;
}

bool DockLayouts::replace(const QString &id){
    if(!main_)return false;
    const auto ids=dockIds();
    const auto state=main_->saveState();
    if(ids.isEmpty()||state.isEmpty()||state.size()>maxState)return false;
    auto next=presets_;
    for(auto &snapshot:next)if(snapshot.id==id){
        snapshot.state=state;snapshot.requiredDocks=ids;
        if(!write(next))return false;
        presets_=next;return true;
    }
    return false;
}

bool DockLayouts::rename(const QString &id,const QString &name){
    const QString trimmed=name.trimmed();
    if(!validName(trimmed))return false;
    auto next=presets_;
    for(const auto &snapshot:next)
        if(snapshot.id!=id&&snapshot.name.compare(trimmed,Qt::CaseInsensitive)==0)return false;
    for(auto &snapshot:next)if(snapshot.id==id){
        snapshot.name=trimmed;
        if(!write(next))return false;
        presets_=next;return true;
    }
    return false;
}

bool DockLayouts::remove(const QString &id){
    auto next=presets_;
    for(int i=0;i<next.size();++i)if(next[i].id==id){
        next.removeAt(i);
        if(!write(next))return false;
        presets_=next;return true;
    }
    return false;
}

bool DockLayouts::restore(const QString &id){
    if(!main_)return false;
    for(const auto &snapshot:presets_)if(snapshot.id==id){
        const auto available=dockIds();
        for(const auto &needed:snapshot.requiredDocks)
            if(!available.contains(needed))return false;
        const QByteArray previous=main_->saveState();
        if(previous.isEmpty())return false;
        if(!main_->restoreState(snapshot.state)){
            main_->restoreState(previous);
            return false;
        }
        undoState_=previous;
        return true;
    }
    return false;
}

bool DockLayouts::undo(){
    if(!main_||undoState_.isEmpty())return false;
    const QByteArray previous=main_->saveState();
    if(previous.isEmpty()||!main_->restoreState(undoState_)){
        main_->restoreState(previous);
        return false;
    }
    undoState_.clear();
    return true;
}

QWidget *DockLayouts::createPage(QWidget *owner,std::function<void(QWidget*)> checkpoints,std::function<void()> openCollectionManager,std::function<void()> changed){
    if(!main_)return nullptr;
    class Page final:public QWidget {
    public:
        using QWidget::QWidget;
        std::function<void()> refresh;
    protected:
        void showEvent(QShowEvent *event)override{
            QWidget::showEvent(event);
            if(refresh)refresh();
        }
    };
    auto *page=new Page(owner);
    page->setObjectName("aftermarket-dock-layouts-page");
    auto *root=new QVBoxLayout(page);
    auto *help=new QLabel("Save and switch native OBS dock arrangements. "
        "These snapshots do not change scenes, audio, or recording settings.",page);
    help->setWordWrap(true);root->addWidget(help);
    auto *list=new QListWidget(page);
    list->setObjectName("aftermarket-layout-list");
    root->addWidget(list,1);
    auto refresh=[this,list]{
        const QString selected=list->currentItem()?
            list->currentItem()->data(Qt::UserRole).toString():QString{};
        list->clear();
        for(const auto &s:presets_){
            auto *item=new QListWidgetItem(s.name,list);
            item->setData(Qt::UserRole,s.id);
            if(s.id==selected)list->setCurrentItem(item);
        }
        if(!list->currentItem()&&list->count())list->setCurrentRow(0);
    };
    auto selected=[list]{
        return list->currentItem()?list->currentItem()->data(Qt::UserRole).toString():QString{};
    };
    auto row1=new QHBoxLayout;
    auto *save=new QPushButton("Save current…",page);
    auto *load=new QPushButton("Switch to layout",page);
    auto *undoBtn=new QPushButton("Undo switch",page);
    row1->addWidget(save);row1->addWidget(load);row1->addWidget(undoBtn);
    root->addLayout(row1);
    auto row2=new QHBoxLayout;
    auto *renameBtn=new QPushButton("Rename…",page);
    auto *replaceBtn=new QPushButton("Update from current",page);
    auto *deleteBtn=new QPushButton("Delete",page);
    row2->addWidget(renameBtn);row2->addWidget(replaceBtn);row2->addWidget(deleteBtn);
    root->addLayout(row2);
    if(checkpoints){
        auto *checkpointBtn=new QPushButton("Workspace checkpoints…",page);
        checkpointBtn->setObjectName("aftermarket-workspace-checkpoints");
        root->addWidget(checkpointBtn);
        QObject::connect(checkpointBtn,&QPushButton::clicked,page,[checkpoints,page]{checkpoints(page);});
    }
    if(openCollectionManager){
        auto *managerBtn=new QPushButton("Open Scene Collection Manager…",page);
        managerBtn->setObjectName("aftermarket-open-collection-manager");
        root->addWidget(managerBtn);
        QObject::connect(managerBtn,&QPushButton::clicked,page,[openCollectionManager,page]{
            QTimer::singleShot(0,page,openCollectionManager);
        });
    }
    QObject::connect(save,&QPushButton::clicked,page,[=]{
        bool ok=false;
        const QString name=QInputDialog::getText(page,"Save dock layout","Layout name:",
            QLineEdit::Normal,QString{},&ok);
        if(!ok)return;
        if(capture(name).isEmpty())
            QMessageBox::warning(page,"Layout not saved",
                "Choose a unique name and ensure OBS docks are available.");
        else if(changed)changed();
        refresh();
    });
    QObject::connect(load,&QPushButton::clicked,page,[=]{
        if(selected().isEmpty())return;
        if(!restore(selected()))
            QMessageBox::warning(page,"Cannot switch layout",
                "The saved dock layout is unavailable or needs docks that are not loaded. "
                "The current layout was preserved.");
        undoBtn->setEnabled(canUndo());
    });
    QObject::connect(undoBtn,&QPushButton::clicked,page,[=]{
        if(!undo())
            QMessageBox::warning(page,"Undo unavailable",
                "The previous dock arrangement could not be restored.");
        undoBtn->setEnabled(canUndo());
    });
    QObject::connect(renameBtn,&QPushButton::clicked,page,[=]{
        const QString id=selected();
        if(id.isEmpty())return;
        const auto it=std::find_if(presets_.begin(),presets_.end(),
            [&id](const auto &s){return s.id==id;});
        if(it==presets_.end())return;
        bool ok=false;
        const auto name=QInputDialog::getText(page,"Rename layout","Layout name:",
            QLineEdit::Normal,it->name,&ok);
        if(ok){
            if(!rename(id,name))QMessageBox::warning(page,"Rename failed",
                "The layout name must be unique.");
            else if(changed)changed();
        }
        refresh();
    });
    QObject::connect(replaceBtn,&QPushButton::clicked,page,[=]{
        const QString id=selected();
        if(id.isEmpty())return;
        if(QMessageBox::question(page,"Update dock layout",
            "Replace this saved arrangement with the current OBS dock layout?")!=QMessageBox::Yes)return;
        if(!replace(id))QMessageBox::warning(page,"Update failed","Could not save the current dock layout.");
        refresh();
    });
    QObject::connect(deleteBtn,&QPushButton::clicked,page,[=]{
        const QString id=selected();
        if(id.isEmpty())return;
        if(QMessageBox::question(page,"Delete dock layout",
            "Delete this saved dock arrangement?")!=QMessageBox::Yes)return;
        if(!remove(id))QMessageBox::warning(page,"Delete failed","Could not delete the layout.");
        else if(changed)changed();
        refresh();
    });
    QObject::connect(list,&QListWidget::itemDoubleClicked,page,[=](QListWidgetItem *item){
        if(item&&!restore(item->data(Qt::UserRole).toString()))
            QMessageBox::warning(page,"Cannot switch layout","One or more saved docks are missing.");
        undoBtn->setEnabled(canUndo());
    });
    page->refresh=[this,refresh,undoBtn]{refresh();undoBtn->setEnabled(canUndo());};
    page->refresh();
    return page;
}
} // namespace augment
