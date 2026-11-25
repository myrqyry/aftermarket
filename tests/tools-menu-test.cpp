#include "tools-menu.h"
#include "menu-bar-compact.h"
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QPointer>
#include <iostream>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(int argc,char **argv){
 QApplication app(argc,argv);
 QMainWindow window;auto *bar=window.menuBar();
 auto *tools=bar->addMenu("Tools");tools->setObjectName("menuTools");
 int nativeInvoked=0;
 auto *native=tools->addAction("Source Dock",[&]{++nativeInvoked;});
 auto *anchor=tools->addAction("Aftermarket");
 auto *other=tools->addAction("Third party");
 QPointer<QAction> safeAnchor=anchor;
 {
  using Menu=augment::ToolsMenu;using Cmd=Menu::Command;
  Menu menu(anchor);
  REQUIRE(tools->actions()==QList<QAction*>({native,anchor,other}));
  REQUIRE(anchor->menu()&&anchor->menu()->objectName()=="aftermarket-tools-menu");
  auto *find=menu.action(Cmd::Find);
  auto *resume=menu.action(Cmd::Resume);
  int invoked=0;QObject::connect(find,&QAction::triggered,[&]{++invoked;});
  menu.setState(true,false);
  REQUIRE(!find->isEnabled()&&!resume->isVisible());
  menu.setState(true,true);
  REQUIRE(find->isEnabled()&&find->isVisible());
  REQUIRE(menu.action(Cmd::Layouts)->isEnabled());
  find->trigger();REQUIRE(invoked==1);
  augment::MenuBarCompact compact;
  REQUIRE(compact.setEnabled(bar,true));app.processEvents();
  REQUIRE(compact.hamburgerMenu()->actions().contains(tools->menuAction()));
  REQUIRE(tools->actions().contains(anchor));
  REQUIRE(compact.setEnabled(bar,false));
  menu.setState(false,true);
  REQUIRE(resume->isEnabled()&&resume->isVisible()&&anchor->isEnabled());
  for(auto *a:anchor->menu()->actions())if(a!=resume)REQUIRE(!a->isVisible());
  menu.setState(true,true);
  REQUIRE(menu.action(Cmd::Find)==find&&find->isVisible()&&find->isEnabled()&&!resume->isVisible());
  native->trigger();REQUIRE(nativeInvoked==1);
 }
 REQUIRE(!safeAnchor&&tools->actions()==QList<QAction*>({native,other}));
 std::cout<<"TOOLS_MENU_PASS\n";
}
