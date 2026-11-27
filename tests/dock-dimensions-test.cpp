#include "dock-dimensions.h"
#include "panels.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QTemporaryDir>
#include <QMenu>
#include <QAction>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(int argc,char**argv){
 QApplication app(argc,argv);app.setStyle("Fusion");
 QMainWindow main;main.setCentralWidget(new QLabel("Canvas"));
 auto *dock=new QDockWidget("Test",&main);dock->setObjectName("testDock");
 auto *content=new QWidget;auto *layout=new QVBoxLayout(content);
 auto *label=new QLabel("Content");label->setMinimumSize(180,120);layout->addWidget(label);
 dock->setWidget(content);dock->setMinimumSize(90,70);dock->setMaximumSize(800,650);
 main.addDockWidget(Qt::LeftDockWidgetArea,dock);
 auto *other=new QDockWidget("Other",&main);other->setObjectName("otherDock");other->setWidget(new QLabel("Other"));main.addDockWidget(Qt::RightDockWidgetArea,other);
 main.resize(1000,700);main.show();app.processEvents();
 const auto originalMin=dock->minimumSize(),originalMax=dock->maximumSize();
 QTemporaryDir dir;CHECK(dir.isValid());augment::Panels panels;
 {
  augment::DockDimensions sizes(&main,&panels,dir.filePath("dock.ini"));sizes.observe(dock);sizes.observe(other);
  auto floor=sizes.minimum(dock);
  CHECK(floor.width()>=180&&floor.height()>120);
  CHECK(sizes.define(dock,QSize(1,1),true,false));
  CHECK(dock->minimumWidth()==floor.width()&&dock->maximumWidth()==floor.width());
  CHECK(dock->minimumHeight()==originalMin.height()&&dock->maximumHeight()==originalMax.height());
  CHECK(!panels.canCollapse(dock));CHECK(!panels.toggle(dock));
  CHECK(other->maximumWidth()!=other->minimumWidth());
  label->setMinimumWidth(300);layout->invalidate();content->updateGeometry();
  for(int i=0;i<8;i++)app.processEvents();
  CHECK(dock->minimumWidth()>=300&&dock->maximumWidth()==dock->minimumWidth());
  label->setMinimumWidth(180);layout->invalidate();content->updateGeometry();
  for(int i=0;i<8;i++)app.processEvents();
  CHECK(dock->minimumWidth()==floor.width()&&dock->maximumWidth()==floor.width());
  CHECK(sizes.define(dock,QSize(350,300),true,true));
  CHECK(dock->minimumSize()==QSize(350,300)&&dock->maximumSize()==QSize(350,300));
  app.processEvents();CHECK(dock->size()==QSize(350,300));
  QMenu menu;sizes.appendActions(&menu,dock);
  CHECK(menu.findChild<QAction*>("aftermarket-dock-dimensions-action"));
  auto *unlock=menu.findChild<QAction*>("aftermarket-dock-unlock-dimensions");
  CHECK(unlock&&unlock->isEnabled());unlock->trigger();app.processEvents();
  CHECK(dock->minimumSize()==originalMin&&dock->maximumSize()==originalMax);
  CHECK(sizes.define(dock,QSize(360,300),true,false));
 }
 CHECK(dock->minimumSize()==originalMin&&dock->maximumSize()==originalMax);
 {
  augment::DockDimensions restored(&main,&panels,dir.filePath("dock.ini"));
  restored.observe(dock);app.processEvents();
  CHECK(dock->minimumWidth()==360&&dock->maximumWidth()==360);
  CHECK(dock->minimumHeight()==originalMin.height());
  CHECK(restored.unlock(dock));
  CHECK(dock->minimumSize()==originalMin&&dock->maximumSize()==originalMax);
  CHECK(panels.toggle(dock));CHECK(!restored.define(dock,QSize(400,300),true,true));CHECK(panels.toggle(dock));
 }
 return 0;
}
