#pragma once
#include "dock-layouts.h"
#include <QMap>
#include <QDateTime>
#include <functional>

namespace augment {
struct CheckpointScene {
    QString collection;
    QByteArray bytes;
};
struct CompanionBackupResult {
    bool verified=false;
    QString filename,message;
};
struct WorkspaceCheckpoint {
    QString id,name,collection;
    QDateTime created;
    QByteArray scene;
    DockLayoutSnapshot layout;
    QMap<QString,QByteArray> settings;
    QStringList missing;
};
class WorkspaceCheckpoints final {
public:
    using SceneCapture=std::function<CheckpointScene()>;
    WorkspaceCheckpoints(QString configDirectory,QMainWindow *main,SceneCapture scene);
    QString capture(const QString &name,QString *error=nullptr);
    WorkspaceCheckpoint read(const QString &id,QString *error=nullptr)const;
    QList<WorkspaceCheckpoint> entries(int *invalid=nullptr)const;
    bool restoreLayout(const QString &id,QString *error=nullptr);
    bool exportScene(const QString &id,const QString &destination,QString *error=nullptr)const;
    QString exportRecovery(const QString &id,const QString &parent,QString *error=nullptr)const;
    using CompanionBackup=std::function<CompanionBackupResult(const WorkspaceCheckpoint&)>;
    static CompanionBackupResult verifyCompanionBackup(const QString &directory,const QByteArray &scene,
                                                         std::function<bool()> dispatch);
    void showManager(QWidget *owner,CompanionBackup companion={});
private:
    QString directory_,storage_;
    QPointer<QMainWindow> main_;
    SceneCapture scene_;
    QStringList dockIds()const;
};
}
