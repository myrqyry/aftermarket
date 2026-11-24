#include "action-strip.h"
#include "dock-layouts.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QTemporaryDir>
#include <QToolButton>
#include <iostream>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(int argc,char**argv){
    QApplication app(argc,argv);
    QTemporaryDir dir; REQUIRE(dir.isValid());
    QMainWindow main;main.resize(980,640);
    main.menuBar()->addMenu("File");
    main.setCentralWidget(new QLabel("OBS preview"));
    auto *scenes=new QDockWidget("Scenes",&main);
    scenes->setObjectName("scenesDock");
    scenes->setWidget(new QLabel("Scenes"));
    main.addDockWidget(Qt::LeftDockWidgetArea,scenes);
    main.show();app.processEvents();
    augment::DockLayouts layouts(dir.filePath("layouts.json"),&main);
    const QString id=layouts.capture("Streaming");
    REQUIRE(!id.isEmpty());
    main.addDockWidget(Qt::RightDockWidgetArea,scenes);
    const auto choices=[&]{
        QList<augment::LayoutChoice> result;
        for(const auto &s:layouts.snapshots())result.append({s.id,s.name});
        return result;
    };
    const auto apply=[&](const QString &target){return layouts.restore(target);};
    const QString file=dir.filePath("strip.json");
    augment::ActionStrip strip(file,{},{},{},nullptr,choices,apply);
    REQUIRE(strip.addItem({"switch","layout",id,""}));
    REQUIRE(!strip.addItem({"missing","layout","bogus",""}));
    REQUIRE(strip.setEnabled(main.menuBar(),true));
    app.processEvents();
    auto *button=strip.widget()->findChild<QToolButton*>("aftermarket-strip-switch");
    REQUIRE(button);
    REQUIRE(button->text()=="Streaming");
    REQUIRE(strip.trigger("switch"));
    REQUIRE(main.dockWidgetArea(scenes)==Qt::LeftDockWidgetArea);
    REQUIRE(layouts.rename(id,"Editing"));
    strip.refresh();
    REQUIRE(button->text()=="Editing");
    REQUIRE(layouts.remove(id));
    strip.refresh();
    REQUIRE(!button->isEnabled());
    REQUIRE(!strip.trigger("switch"));
    REQUIRE(!strip.addItem({"bad","layout",id+"extra",""}));
    REQUIRE(strip.setEnabled(main.menuBar(),false));
    augment::ActionStrip restored(file,{},{},{},nullptr,choices,apply);
    REQUIRE(restored.items().size()==1);
    REQUIRE(restored.items().first().target==id);
    REQUIRE(!restored.trigger("switch"));
    std::cout<<"AFTERMARKET_LAYOUT_STRIP_REAL_DOCK_PASS=1\n";
}
