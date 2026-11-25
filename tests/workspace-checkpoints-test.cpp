#include "workspace-checkpoints.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <iostream>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(false)
static bool write(const QString &path,const QByteArray &bytes){
    QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();
}
int main(int argc,char **argv){
    QApplication app(argc,argv);
    QTemporaryDir temp;REQUIRE(temp.isValid());
    QMainWindow main;main.setCentralWidget(new QLabel("preview"));
    auto *dock=new QDockWidget("Mixer",&main);dock->setObjectName("mixerDock");
    dock->setWidget(new QLabel("audio",dock));main.addDockWidget(Qt::LeftDockWidgetArea,dock);
    main.show();app.processEvents();
    const auto dir=temp.path();
    REQUIRE(write(dir+"/front-audio-controls.json",R"({"version":1,"knobs":false,"channelOrder":["source-uuid"]})"));
    REQUIRE(write(dir+"/action-strip.json",R"({"version":1,"enabled":true,"items":[]})"));
    REQUIRE(write(dir+"/unrelated-secret.json","not for backup"));
    const QByteArray scene=R"({"name":"Test collection","sources":[{"name":"Microphone"}],"scene_order":[]})";
    bool available=true;
    augment::WorkspaceCheckpoints store(dir,&main,[&]{
        return available?augment::CheckpointScene{"Test collection",scene}:augment::CheckpointScene{};
    });
    QString error;
    REQUIRE(store.capture(" ",&error).isEmpty());
    auto id=store.capture("Before edits",&error);
    REQUIRE(!id.isEmpty());
    REQUIRE(store.entries().size()==1);
    auto saved=store.read(id,&error);
    REQUIRE(saved.collection=="Test collection"&&saved.scene==scene&&saved.name=="Before edits");
    REQUIRE(saved.settings.value("front-audio-controls.json").contains("source-uuid"));
    REQUIRE(saved.settings.contains("action-strip.json")&&!saved.settings.contains("unrelated-secret.json"));
    REQUIRE(saved.missing.contains("cue-deck.json"));
    REQUIRE(!saved.layout.state.isEmpty()&&saved.layout.requiredDocks.contains("mixerDock"));
    REQUIRE(store.capture("before edits",&error).isEmpty());
    available=false;REQUIRE(store.capture("Unavailable",&error).isEmpty());
    REQUIRE(store.entries().size()==1);
    REQUIRE(write(dir+"/front-audio-controls.json",R"({"version":1,"knobs":true})"));
    main.addDockWidget(Qt::RightDockWidgetArea,dock);
    augment::WorkspaceCheckpoints reopened(dir,&main,{});
    REQUIRE(reopened.entries().size()==1&&reopened.read(id,&error).scene==scene);
    REQUIRE(reopened.restoreLayout(id,&error));
    REQUIRE(main.dockWidgetArea(dock)==Qt::LeftDockWidgetArea);
    QFile config(dir+"/front-audio-controls.json");REQUIRE(config.open(QIODevice::ReadOnly));
    REQUIRE(config.readAll().contains("true")); // restore layout only
    const auto out=dir+"/export.json";
    REQUIRE(reopened.exportScene(id,out,&error));
    QFile f(out);REQUIRE(f.open(QIODevice::ReadOnly)&&f.readAll()==scene);
    REQUIRE(!reopened.exportScene(id,out,&error)); // never overwrite export
    const auto recovery=reopened.exportRecovery(id,dir,&error);
    REQUIRE(!recovery.isEmpty());
    QFile exportedScene(recovery+"/scene.json");
    REQUIRE(exportedScene.open(QIODevice::ReadOnly)&&exportedScene.readAll()==scene);
    QFile exportedSetting(recovery+"/aftermarket/front-audio-controls.json");
    REQUIRE(exportedSetting.open(QIODevice::ReadOnly)&&exportedSetting.readAll().contains("source-uuid"));
    REQUIRE(!QFile::exists(recovery+"/aftermarket/unrelated-secret.json"));
    REQUIRE(QFile::exists(recovery+"/dock-layout.json")&&QFile::exists(recovery+"/RECOVERY.txt"));
    REQUIRE(reopened.exportRecovery(id,dir,&error).isEmpty());

    REQUIRE(reopened.read("../outside",&error).id.isEmpty());
    const auto checkpoint=dir+"/workspace-checkpoints/"+id+".json";
    QFile raw(checkpoint);REQUIRE(raw.open(QIODevice::ReadOnly));auto bytes=raw.readAll();raw.close();
    REQUIRE(bytes.contains("Before edits"));bytes.replace("Before edits","Corrupted");
    REQUIRE(write(checkpoint,bytes));
    REQUIRE(reopened.read(id,&error).id.isEmpty());
    REQUIRE(reopened.entries().isEmpty());
    std::cout<<"WORKSPACE_CHECKPOINT_PASS\n";
}
