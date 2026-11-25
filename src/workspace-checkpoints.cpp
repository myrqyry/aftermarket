#include "workspace-checkpoints.h"
#include <QMainWindow>
#include <QDockWidget>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QUuid>
#include <QSet>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QTemporaryDir>
#include <QCheckBox>
#include <QRegularExpression>
#include <algorithm>

namespace augment {
namespace {
constexpr qint64 maxScene=32*1024*1024,maxSettings=24*1024*1024,maxState=512*1024,maxBundle=80*1024*1024;
const QStringList files={"rules.json","action-strip.json","cue-deck.json","dock-layouts.json",
    "front-audio-controls.json","audio-device-visibility.json","dock-title-settings.ini","source-picker.ini"};
void fail(QString *error,const QString &message){if(error)*error=message;}
bool validId(const QString &id){return !QUuid(id).isNull()&&QUuid(id).toString(QUuid::WithoutBraces)==id;}
bool validName(const QString &name){
    if(name.isEmpty()||name!=name.trimmed()||name.size()>80)return false;
    for(auto c:name)if(c.category()==QChar::Other_Control||c.isNull())return false;
    return true;
}
QByteArray digest(const QByteArray &bytes){return QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex();}
QByteArray encoded(const QByteArray &bytes){return bytes.toBase64();}
bool decode(const QJsonValue &v,QByteArray &bytes,qint64 limit){
    if(!v.isString()||v.toString().size()>((limit+2)/3)*4)return false;
    auto b64=v.toString().toLatin1();bytes=QByteArray::fromBase64(b64);
    return bytes.size()<=limit&&bytes.toBase64()==b64;
}
bool validScene(const QByteArray &bytes,const QString &collection){
    if(bytes.isEmpty()||bytes.size()>maxScene||collection.isEmpty())return false;
    const auto doc=QJsonDocument::fromJson(bytes);
    return doc.isObject()&&doc.object().value("name").toString()==collection&&doc.object().value("sources").isArray();
}
bool readBounded(const QString &path,QByteArray &out,qint64 limit){
    QFileInfo info(path);
    if(info.isSymLink()||!info.isFile()||info.size()>limit)return false;
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly))return false;
    out=f.read(limit+1);return f.error()==QFile::NoError&&out.size()<=limit;
}
QJsonArray strings(const QStringList &list){QJsonArray a;for(const auto &s:list)a.append(s);return a;}
bool readStrings(const QJsonValue &v,QStringList &list){
    if(!v.isArray())return false;
    for(auto item:v.toArray()){
        if(!item.isString()||item.toString().isEmpty()||item.toString().size()>256||list.contains(item.toString()))return false;
        list.append(item.toString());
    }
    return true;
}
QByteArray payloadBytes(const QJsonObject &obj){return QJsonDocument(obj).toJson(QJsonDocument::Compact);}
}
WorkspaceCheckpoints::WorkspaceCheckpoints(QString configDirectory,QMainWindow *main,SceneCapture scene)
    :directory_(std::move(configDirectory)),storage_(QDir(directory_).filePath("workspace-checkpoints")),
     main_(main),scene_(std::move(scene)){}
QStringList WorkspaceCheckpoints::dockIds()const{
    QStringList ids;if(!main_)return ids;
    for(auto *dock:main_->findChildren<QDockWidget*>()){
        if(dock->objectName().isEmpty()||
           (main_->dockWidgetArea(dock)==Qt::NoDockWidgetArea&&!dock->isFloating()))continue;
        if(ids.contains(dock->objectName()))return {};
        ids.append(dock->objectName());
    }
    ids.sort();return ids;
}
QString WorkspaceCheckpoints::capture(const QString &name,QString *error){
    if(error)error->clear();
    const auto title=name.trimmed();
    if(!validName(title)||!main_||!scene_){fail(error,"A name and active OBS workspace are required.");return {};}
    QDir storage(storage_);
    if(QFileInfo(storage_).isSymLink()||!QDir().mkpath(storage_)){fail(error,"Checkpoint folder unavailable.");return {};}
    if(storage.entryList({"*.json"},QDir::Files).size()>=32){fail(error,"Checkpoint limit (32) reached. Archive old bundles outside this folder first.");return {};}
    for(const auto &entry:entries())if(entry.name.compare(title,Qt::CaseInsensitive)==0){fail(error,"Choose a unique checkpoint name.");return {};}
    const auto scene=scene_();
    if(!main_||!validScene(scene.bytes,scene.collection)){fail(error,"OBS did not provide a verified scene collection save.");return {};}
    const auto ids=dockIds();const auto state=main_->saveState();
    if(ids.isEmpty()||state.isEmpty()||state.size()>maxState){fail(error,"Native dock state unavailable.");return {};}
    QJsonObject settings;QStringList missing;qint64 total=0;
    for(const auto &name:files){
        const auto path=QDir(directory_).filePath(name);
        QFileInfo info(path);
        if(!info.exists()&&!info.isSymLink()){missing.append(name);continue;}
        QByteArray bytes;
        if(!readBounded(path,bytes,maxSettings-total)){fail(error,"Could not read "+name);return {};}
        total+=bytes.size();settings.insert(name,QString::fromLatin1(encoded(bytes)));
    }
    const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject payload{{"id",id},{"name",title},{"collection",scene.collection},
        {"created",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"scene",QString::fromLatin1(encoded(scene.bytes))},{"layout",QString::fromLatin1(encoded(state))},
        {"docks",strings(ids)},{"settings",settings},{"missing",strings(missing)}};
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"payload",payload},
        {"sha256",QString::fromLatin1(digest(payloadBytes(payload)))}}).toJson(QJsonDocument::Indented);
    QSaveFile file(storage.filePath(id+".json"));
    if(bytes.size()>maxBundle||!file.open(QIODevice::WriteOnly)||
       !file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner)||
       file.write(bytes)!=bytes.size()||!file.commit()){
        fail(error,"Could not securely save checkpoint.");return {};
    }
    return id;
}
WorkspaceCheckpoint WorkspaceCheckpoints::read(const QString &id,QString *error)const{
    if(error)error->clear();
    auto invalid=[&]{fail(error,"Checkpoint is missing, corrupt or unsupported.");return WorkspaceCheckpoint{};};
    QByteArray raw;
    if(!validId(id)||QFileInfo(storage_).isSymLink()||!readBounded(QDir(storage_).filePath(id+".json"),raw,maxBundle))return invalid();
    const auto doc=QJsonDocument::fromJson(raw);const auto root=doc.object();
    const auto payload=root.value("payload").toObject();
    if(root.value("version").toInt()!=1||payload.value("id").toString()!=id||
       digest(payloadBytes(payload))!=root.value("sha256").toString().toLatin1())return invalid();
    WorkspaceCheckpoint out;out.id=id;out.name=payload.value("name").toString();
    out.collection=payload.value("collection").toString();out.created=QDateTime::fromString(payload.value("created").toString(),Qt::ISODateWithMs);
    if(!validName(out.name)||!out.created.isValid()||!decode(payload.value("scene"),out.scene,maxScene)||
       !validScene(out.scene,out.collection)||!decode(payload.value("layout"),out.layout.state,maxState)||
       out.layout.state.isEmpty()||!readStrings(payload.value("docks"),out.layout.requiredDocks)||
       out.layout.requiredDocks.isEmpty()||!readStrings(payload.value("missing"),out.missing)||
       !payload.value("settings").isObject())return invalid();
    out.layout.id=id;out.layout.name=out.name;
    const auto settings=payload.value("settings").toObject();qint64 total=0;
    for(auto it=settings.begin();it!=settings.end();++it){
        QByteArray bytes;
        if(!files.contains(it.key())||out.missing.contains(it.key())||!decode(it.value(),bytes,maxSettings-total))return invalid();
        total+=bytes.size();out.settings.insert(it.key(),bytes);
    }
    for(auto name:out.missing)if(!files.contains(name))return invalid();
    for(auto name:files)if(!out.settings.contains(name)&&!out.missing.contains(name))return invalid();
    return out;
}
QList<WorkspaceCheckpoint> WorkspaceCheckpoints::entries(int *invalid)const{
    if(invalid)*invalid=0;
    QList<WorkspaceCheckpoint> list;
    if(QFileInfo(storage_).isSymLink())return list;
    for(const auto &entry:QDir(storage_).entryInfoList({"*.json"},QDir::Files,QDir::Name)){
        auto item=read(entry.completeBaseName());
        if(item.id.isEmpty()){if(invalid)++*invalid;continue;}
        // List view retains metadata only; load the verified payload on demand.
        item.scene.clear();item.layout.state.clear();
        for(auto &bytes:item.settings)bytes.clear();
        list.append(std::move(item));
    }
    std::sort(list.begin(),list.end(),[](const auto &a,const auto &b){return a.created>b.created;});
    return list;
}
bool WorkspaceCheckpoints::restoreLayout(const QString &id,QString *error){
    const auto saved=read(id,error);
    if(saved.id.isEmpty()||!main_)return false;
    const auto available=dockIds();
    for(const auto &dock:saved.layout.requiredDocks)if(!available.contains(dock)){fail(error,"Required dock unavailable: "+dock);return false;}
    const auto previous=main_->saveState();
    if(previous.isEmpty()||!main_->restoreState(saved.layout.state)){
        if(!previous.isEmpty())main_->restoreState(previous);
        fail(error,"Dock restoration failed.");return false;
    }
    return true;
}
bool WorkspaceCheckpoints::exportScene(const QString &id,const QString &destination,QString *error)const{
    const auto saved=read(id,error);if(saved.id.isEmpty())return false;
    QFile file(destination);
    if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)){fail(error,"Choose a new export file; existing files are never overwritten.");return false;}
    if(!file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner)||
       file.write(saved.scene)!=saved.scene.size()||!file.flush()){
        file.close();file.remove();fail(error,"Could not export scene collection.");return false;
    }
    return true;
}
QString WorkspaceCheckpoints::exportRecovery(const QString &id,const QString &parent,QString *error)const{
    const auto saved=read(id,error);if(saved.id.isEmpty())return {};
    const QDir dir(parent);
    const auto target=dir.filePath("checkpoint-"+id);
    if(!dir.exists()||QFileInfo::exists(target)||QFileInfo(target).isSymLink()){
        fail(error,"Choose a directory without this recovery export.");return {};
    }
    QTemporaryDir stage(dir.filePath(".checkpoint-XXXXXX"));
    if(!stage.isValid()||!QDir(stage.path()).mkdir("aftermarket")){
        fail(error,"Could not create recovery staging directory.");return {};
    }
    const QDir output(stage.path());
    auto put=[&](const QString &name,const QByteArray &bytes){
        QSaveFile file(output.filePath(name));
        return file.open(QIODevice::WriteOnly)&&
            file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner)&&
            file.write(bytes)==bytes.size()&&file.commit();
    };
    const auto layout=QJsonDocument(QJsonObject{{"version",1},{"layouts",QJsonArray{
        QJsonObject{{"id",saved.id},{"name",saved.name},
        {"state",QString::fromLatin1(saved.layout.state.toBase64())},
        {"sha256",QString::fromLatin1(digest(saved.layout.state))},{"docks",strings(saved.layout.requiredDocks)}}}}}).toJson();
    const QByteArray instructions=QString(
        "Workspace checkpoint: %1\nCollection: %2\nCaptured: %3\n\n"
        "Import scene.json through OBS Scene Collection > Import. Do not overwrite a live collection.\n"
        "The aftermarket directory contains captured settings, not media or plugins. To recover individual settings, "
        "close OBS first, back up the current Aftermarket config folder, then copy only the files you intend to restore. "
        "Missing files were not present at capture: %4. Do not delete current files automatically.\n"
        "dock-layout.json contains a native layout preset; use the checkpoint manager's Restore docks only action "
        "in the original installation. Do not overwrite an existing layout library with this preset.\n"
        "This folder can contain private source settings. Keep it local.\n"
    ).arg(saved.name,saved.collection,saved.created.toString(Qt::ISODate),saved.missing.join(", ")).toUtf8();
    if(!put("scene.json",saved.scene)||!put("dock-layout.json",layout)||!put("RECOVERY.txt",instructions)){
        fail(error,"Could not write recovery files.");return {};
    }
    for(auto it=saved.settings.begin();it!=saved.settings.end();++it)
        if(!put("aftermarket/"+it.key(),it.value())){fail(error,"Could not export "+it.key());return {};}
    if(QFileInfo::exists(target)||QFileInfo(target).isSymLink()||!QDir().rename(stage.path(),target)){
        fail(error,"Could not publish recovery folder; existing files were preserved.");return {};
    }
    stage.setAutoRemove(false);
    return target;
}
CompanionBackupResult WorkspaceCheckpoints::verifyCompanionBackup(const QString &directory,const QByteArray &scene,
                                                                   std::function<bool()> dispatch){
    const QDir dir(directory);
    auto expected=QJsonDocument::fromJson(scene).object();
    if(directory.isEmpty()||QFileInfo(directory).isSymLink()||expected.isEmpty()||
       !expected.value("sources").isArray()||!dispatch)
        return {false,{},"Backup destination or scene unavailable."};
    expected.remove("name");
    const auto existing=dir.entryList({"*.json"},QDir::Files|QDir::NoSymLinks);
    if(!dispatch())return {false,{},"Scene Collection Manager backup action unavailable."};
    QStringList matching;
    for(const auto &name:dir.entryList({"*.json"},QDir::Files|QDir::NoSymLinks)){
        if(existing.contains(name))continue;
        QByteArray raw;
        if(!readBounded(dir.filePath(name),raw,maxScene))continue;
        auto actual=QJsonDocument::fromJson(raw).object();
        actual.remove("name");
        if(actual==expected)matching.append(name);
    }
    if(matching.size()!=1)return {false,{},"Backup requested, but a new matching file could not be verified. Workspace checkpoint is still safe."};
    return {true,matching.first(),"Scene Collection Manager backup verified: "+matching.first()};
}
void WorkspaceCheckpoints::showManager(QWidget *owner,CompanionBackup companion){
    QDialog dialog(owner);dialog.setWindowTitle("Aftermarket · Workspace checkpoints");dialog.resize(560,450);
    auto *root=new QVBoxLayout(&dialog);
    auto *help=new QLabel("Capture a named scene collection, dock arrangement and Aftermarket settings. "
        "Checkpoints may contain private source settings. Keep them local. Full workspace restore is not yet available.",&dialog);
    help->setWordWrap(true);root->addWidget(help);
    auto *list=new QListWidget(&dialog);list->setObjectName("aftermarket-checkpoint-list");root->addWidget(list,1);
    auto *details=new QLabel(&dialog);details->setWordWrap(true);root->addWidget(details);
    auto *status=new QLabel(&dialog);status->setWordWrap(true);root->addWidget(status);
    auto *alsoBackup=new QCheckBox("Also create a Scene Collection Manager backup",&dialog);
    alsoBackup->setObjectName("aftermarket-companion-backup");
    alsoBackup->setVisible(bool(companion));alsoBackup->setChecked(false);
    alsoBackup->setToolTip("Uses the installed plugin and its configured backup retention. Optional; the workspace checkpoint is independent.");
    root->addWidget(alsoBackup);

    auto *actions=new QHBoxLayout;root->addLayout(actions);
    auto *captureBtn=new QPushButton("Create checkpoint…",&dialog);captureBtn->setObjectName("aftermarket-create-checkpoint");
    auto *restoreBtn=new QPushButton("Restore docks only",&dialog);
    auto *exportBtn=new QPushButton("Export scene…",&dialog);
    auto *recoveryBtn=new QPushButton("Export recovery folder…",&dialog);
    actions->addWidget(captureBtn);actions->addWidget(restoreBtn);actions->addWidget(exportBtn);
    auto *bottom=new QHBoxLayout;root->addLayout(bottom);
    auto *folder=new QPushButton("Open checkpoint folder",&dialog);bottom->addWidget(folder);bottom->addWidget(recoveryBtn);bottom->addStretch();
    auto *close=new QPushButton("Close",&dialog);bottom->addWidget(close);
    QList<WorkspaceCheckpoint> current;
    auto selected=[list]{return list->currentItem()?list->currentItem()->data(Qt::UserRole).toString():QString{};};
    auto updateDetails=[&]{
        const auto id=selected();restoreBtn->setEnabled(!id.isEmpty());exportBtn->setEnabled(!id.isEmpty());recoveryBtn->setEnabled(!id.isEmpty());
        for(const auto &item:current)if(item.id==id){
            details->setText(QString("Collection: %1\nCaptured: %2\n%3 docks · %4 settings files · %5 absent/default files")
                .arg(item.collection,item.created.toLocalTime().toString(Qt::ISODate))
                .arg(item.layout.requiredDocks.size()).arg(item.settings.size()).arg(item.missing.size()));return;
        }
        details->clear();
    };
    auto refresh=[&]{
        int invalid=0;current=entries(&invalid);list->clear();
        for(const auto &item:current){
            auto *row=new QListWidgetItem(item.name+" — "+item.collection,list);row->setData(Qt::UserRole,item.id);
        }
        if(list->count())list->setCurrentRow(0);
        status->setText(invalid?QString("%1 invalid checkpoint(s) left untouched.").arg(invalid):QString{});
        updateDetails();
    };
    QObject::connect(list,&QListWidget::currentItemChanged,&dialog,[&]{updateDetails();});
    QObject::connect(captureBtn,&QPushButton::clicked,&dialog,[&]{
        bool ok=false;const auto name=QInputDialog::getText(&dialog,"Create checkpoint","Checkpoint name:",QLineEdit::Normal,{},&ok);
        if(!ok)return;
        QString error;const auto id=capture(name,&error);
        QString result=id.isEmpty()?error:"Checkpoint saved.";
        if(!id.isEmpty()&&companion&&alsoBackup->isChecked()){
            const auto saved=read(id,&error);
            if(!saved.id.isEmpty())result+=" "+companion(saved).message;
            else result+=" Companion backup skipped: "+error;
        }
        refresh();status->setText(result);
        for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toString()==id)list->setCurrentRow(i);
    });
    QObject::connect(restoreBtn,&QPushButton::clicked,&dialog,[&]{
        if(QMessageBox::question(&dialog,"Restore docks only","Change dock arrangement? Scenes and settings will remain unchanged.")!=QMessageBox::Yes)return;
        QString error;status->setText(restoreLayout(selected(),&error)?"Dock arrangement restored.":error);
    });
    QObject::connect(exportBtn,&QPushButton::clicked,&dialog,[&]{
        const auto destination=QFileDialog::getSaveFileName(&dialog,"Export scene for OBS import","scene-checkpoint.json","OBS scene collection (*.json)");
        if(destination.isEmpty())return;
        QString error;status->setText(exportScene(selected(),destination,&error)?"Scene exported. Import it using OBS Scene Collection → Import.":error);
    });
    QObject::connect(recoveryBtn,&QPushButton::clicked,&dialog,[&]{
        const auto parent=QFileDialog::getExistingDirectory(&dialog,"Choose recovery export location");
        if(parent.isEmpty())return;
        QString error;const auto path=exportRecovery(selected(),parent,&error);
        status->setText(path.isEmpty()?error:"Recovery folder exported: "+path);
    });
    QObject::connect(folder,&QPushButton::clicked,&dialog,[&]{
        if(QDir().mkpath(storage_))QDesktopServices::openUrl(QUrl::fromLocalFile(storage_));
    });
    QObject::connect(close,&QPushButton::clicked,&dialog,&QDialog::accept);
    refresh();dialog.exec();
}
}
