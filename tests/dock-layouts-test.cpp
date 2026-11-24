#include "dock-layouts.h"
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QMainWindow>
#include <QTemporaryDir>
#include <iostream>

#define REQUIRE(expr) do { if(!(expr)) { std::cerr << "FAIL " << __LINE__ << ": " << #expr << "\n"; return 1; } } while(false)

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QMainWindow main;
    main.resize(960, 640);
    main.setCentralWidget(new QLabel("Native OBS preview", &main));
    auto *scenes=new QDockWidget("Scenes", &main);
    scenes->setObjectName("scenesDock");
    scenes->setWidget(new QLabel("Scene list", scenes));
    main.addDockWidget(Qt::LeftDockWidgetArea, scenes);
    auto *audio=new QDockWidget("Audio Mixer", &main);
    audio->setObjectName("mixerDock");
    audio->setWidget(new QLabel("Mixer", audio));
    main.addDockWidget(Qt::BottomDockWidgetArea, audio);
    main.show(); app.processEvents();

    augment::DockLayouts layouts(dir.filePath("dock-layouts.json"), &main);
    REQUIRE(layouts.snapshots().isEmpty());
    REQUIRE(!layouts.capture("   ").size());
    const QString edit=layouts.capture("Editing");
    REQUIRE(!edit.isEmpty());
    REQUIRE(!layouts.capture("Editing").size());
    REQUIRE(layouts.snapshots().size()==1);
    REQUIRE(layouts.snapshots().front().id==edit);
    main.addDockWidget(Qt::RightDockWidgetArea, scenes);
    main.addDockWidget(Qt::RightDockWidgetArea, audio);
    app.processEvents();
    const QString streaming=layouts.capture("Streaming");
    REQUIRE(!streaming.isEmpty());
    REQUIRE(layouts.restore(edit));
    app.processEvents();
    REQUIRE(main.dockWidgetArea(scenes)==Qt::LeftDockWidgetArea);
    REQUIRE(main.dockWidgetArea(audio)==Qt::BottomDockWidgetArea);
    REQUIRE(layouts.canUndo());
    REQUIRE(layouts.undo());
    REQUIRE(!layouts.canUndo());
    REQUIRE(!layouts.undo());
    app.processEvents();
    REQUIRE(main.dockWidgetArea(scenes)==Qt::RightDockWidgetArea);
    REQUIRE(main.dockWidgetArea(audio)==Qt::RightDockWidgetArea);

    REQUIRE(layouts.rename(edit,"Desk Work"));
    REQUIRE(!layouts.rename(streaming,"Desk Work"));
    REQUIRE(layouts.snapshots().front().name=="Desk Work");
    REQUIRE(layouts.replace(edit)); // replace only explicit snapshot
    REQUIRE(layouts.restore(edit));
    REQUIRE(main.dockWidgetArea(scenes)==Qt::RightDockWidgetArea);

    // Snapshot ID, name, and opaque Qt state survive a fresh manager instance.
    augment::DockLayouts loaded(dir.filePath("dock-layouts.json"), &main);
    REQUIRE(loaded.snapshots().size()==2);
    REQUIRE(loaded.snapshots().front().id==edit);
    REQUIRE(loaded.snapshots().front().name=="Desk Work");
    REQUIRE(!loaded.restore("unknown-layout"));
    // A layout requiring an absent dock must leave the entire live layout alone.
    main.removeDockWidget(audio);
    main.addDockWidget(Qt::LeftDockWidgetArea, scenes);
    const QByteArray before=main.saveState();
    REQUIRE(!loaded.restore(edit));
    REQUIRE(main.saveState()==before);
    main.addDockWidget(Qt::BottomDockWidgetArea,audio);
    REQUIRE(loaded.restore(streaming));
    REQUIRE(main.dockWidgetArea(audio)==Qt::RightDockWidgetArea);

    // A corrupt save file must not discard the currently loaded in-memory presets.
    QFile invalid(dir.filePath("dock-layouts.json"));
    REQUIRE(invalid.open(QIODevice::WriteOnly));
    invalid.write("{this isn't json");invalid.close();
    REQUIRE(!loaded.reload());
    REQUIRE(loaded.snapshots().size()==2);
    REQUIRE(loaded.remove(streaming));
    REQUIRE(loaded.snapshots().size()==1);
    augment::DockLayouts reopened(dir.filePath("dock-layouts.json"), &main);
    REQUIRE(reopened.snapshots().size()==1);
    REQUIRE(reopened.snapshots().first().id==edit);
    std::cout << "DOCK_LAYOUTS_PERSISTENCE_RESTORE_ROLLBACK_PASS=1\n";
}
