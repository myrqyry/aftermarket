#include "menu-bar-compact.h"
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QShortcut>
#include <QKeyEvent>
#include <QPointer>
#include <QLabel>
#include <QWidget>
#include <QTimer>
#include <iostream>

#define REQUIRE(cond) do { if(!(cond)){std::cerr<<"FAILED "<<__LINE__<<": "<<#cond<<"\\n";return 1;} } while(false)

int main(int argc,char **argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");
    QMainWindow window;
    window.resize(780,530);
    auto *bar=window.menuBar();
    auto *file=bar->addMenu("&File");file->setObjectName("menuFile");
    auto *edit=bar->addMenu("&Edit");edit->setObjectName("menuEdit");
    auto *tools=bar->addMenu("&Tools");tools->setObjectName("menuTools");
    int invoked=0;
    auto *open=file->addAction("Native &Open",[&]{++invoked;});
    open->setShortcut(QKeySequence("Ctrl+Shift+O"));
    auto *originalCorner=new QLabel("Existing corner",bar);
    bar->setCornerWidget(originalCorner);
    window.show();app.processEvents();
    const QList<QAction*> original={file->menuAction(),edit->menuAction(),tools->menuAction()};
    augment::MenuBarCompact compact;
    REQUIRE(!compact.enabled());
    REQUIRE(compact.setEnabled(bar,true));
    app.processEvents();
    REQUIRE(compact.enabled());
    auto *hamburger=compact.hamburgerMenu();
    REQUIRE(hamburger);
    REQUIRE(bar->actions().size()==1);
    REQUIRE(bar->actions().first()==hamburger->menuAction());
    REQUIRE(hamburger->actions()==original);
    REQUIRE(hamburger->minimumWidth()>=175); // text + submenu indicator stay distinct
    REQUIRE(hamburger->actions().at(0)->menu()==file);
    REQUIRE(bar->cornerWidget()==originalCorner);
    auto *hotkey=bar->findChild<QShortcut*>();
    REQUIRE(hotkey&&hotkey->key()==QKeySequence("Alt+M"));
    QPointer<QShortcut> safeHotkey(hotkey);
    QKeyEvent shortcutKey(QEvent::KeyPress,Qt::Key_M,Qt::AltModifier,"m");
    QApplication::sendEvent(&window,&shortcutKey);app.processEvents();
    REQUIRE(hamburger->isVisible());
    QKeyEvent releaseKey(QEvent::KeyRelease,Qt::Key_M,Qt::NoModifier,"m");
    QApplication::sendEvent(&window,&releaseKey);app.processEvents();
    REQUIRE(hamburger->isVisible());
    hamburger->hide();app.processEvents();
    REQUIRE(file->actions().contains(open));
    open->trigger();REQUIRE(invoked==1);
    REQUIRE(!compact.setEnabled(bar,true)); // idempotent has no side effects
    REQUIRE(hamburger->actions()==original);

    // Other OBS plugins can append menu tabs after Aftermarket is enabled.
    auto *plugin=bar->addMenu("Plugin &Extras");
    auto *pluginAction=plugin->addAction("Original command",[&]{++invoked;});
    QPointer<QAction> pluginMenuAction=plugin->menuAction();
    app.processEvents();
    REQUIRE(bar->actions().size()==1);
    REQUIRE(hamburger->actions().size()==4);
    REQUIRE(hamburger->actions().last()==pluginMenuAction);
    pluginAction->trigger();REQUIRE(invoked==2);

    REQUIRE(compact.setEnabled(bar,false));
    app.processEvents();
    REQUIRE(!compact.enabled());
    REQUIRE(!safeHotkey); // remove the plugin shortcut when returning to OBS
    REQUIRE(bar->actions()==QList<QAction*>({original[0],original[1],original[2],pluginMenuAction}));
    REQUIRE(bar->cornerWidget()==originalCorner);
    REQUIRE(file->menuAction()==original[0]);
    REQUIRE(!bar->actions().contains(hamburger->menuAction()));

    REQUIRE(compact.setEnabled(bar,true));app.processEvents();
    // Original action destruction is not allowed to leave a dangling menu item.
    delete plugin;app.processEvents();
    REQUIRE(hamburger->actions().size()==3);
    REQUIRE(compact.setEnabled(bar,false));app.processEvents();
    REQUIRE(bar->actions()==original);
    REQUIRE(open->shortcut()==QKeySequence("Ctrl+Shift+O"));
    REQUIRE(bar->cornerWidget()==originalCorner);

    // With an existing unrelated action in the bar, it too must be retained.
    auto *extra=new QAction("Top-level action",bar);
    bar->addAction(extra);
    REQUIRE(compact.setEnabled(bar,true));app.processEvents();
    REQUIRE(hamburger->actions().last()==extra);
    REQUIRE(compact.setEnabled(bar,false));app.processEvents();
    REQUIRE(bar->actions().last()==extra);

    // Teardown while compact restores OBS menus, including their identity.
    REQUIRE(compact.setEnabled(bar,true));app.processEvents();
    // class itself restores on destruction (verified below with separate owner)
    compact.setEnabled(bar,false);
    {
        augment::MenuBarCompact temporary;
        REQUIRE(temporary.setEnabled(bar,true));
    }
    app.processEvents();
    REQUIRE(bar->actions().size()==4);
    REQUIRE(bar->actions().at(0)==original[0]);
    REQUIRE(bar->actions().last()==extra);

    // Plugin unload restores and destroys every plugin-owned popup.
    QMainWindow spare;
    auto *spareFile=spare.menuBar()->addMenu("&File");
    {
        augment::MenuBarCompact scoped;
        REQUIRE(scoped.setEnabled(spare.menuBar(),true));
    }
    REQUIRE(spare.menuBar()->actions().size()==1);
    REQUIRE(spare.menuBar()->actions().first()==spareFile->menuAction());
    REQUIRE(!spare.menuBar()->findChild<QMenu*>("aftermarket-hamburger-menu"));
    // An unrelated menu bar remains untouched.
    QMenuBar unrelated;
    REQUIRE(!compact.setEnabled(&unrelated,true));
    REQUIRE(unrelated.actions().isEmpty());
    std::cout<<"compact native menu bar verified\n";
    return 0;
}
