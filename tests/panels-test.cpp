#include "panels.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(false)
int main(int argc,char **argv) {
 QApplication app(argc,argv);app.setStyle("Fusion");
 QMainWindow main;main.setCentralWidget(new QLabel("Preview",&main));
 auto *a=new QDockWidget("Sources",&main);a->setObjectName("sources");
 auto *body=new QLabel("Native contents",a);body->setMinimumSize(100,120);a->setWidget(body);
 auto *b=new QDockWidget("Scenes",&main);b->setWidget(new QLabel("Scenes",b));
 main.addDockWidget(Qt::LeftDockWidgetArea,a);main.splitDockWidget(a,b,Qt::Vertical);
 main.resize(640,480);main.show();app.processEvents();
 augment::Panels panels;
 const int height=a->height(),max=a->maximumHeight(),min=a->minimumHeight(),width=a->width(),minWidth=a->minimumWidth();
 REQUIRE(panels.canCollapse(a));REQUIRE(panels.toggle(a));app.processEvents();
 REQUIRE(panels.collapsed(a)&&body->isHidden()&&!a->isHidden());
 REQUIRE(a->height()<height/2); REQUIRE(a->width()>=width);
 REQUIRE(panels.toggle(a));app.processEvents();
 REQUIRE(!panels.collapsed(a)&&a->widget()==body&&!body->isHidden());
 REQUIRE(a->maximumHeight()==max&&a->minimumHeight()==min&&a->height()>height/2); REQUIRE(a->minimumWidth()==minWidth);
 const QPointF point(12,4);
 QMouseEvent click(QEvent::MouseButtonRelease,point,a->mapToGlobal(point.toPoint()),Qt::MiddleButton,Qt::NoButton,Qt::NoModifier);
 QApplication::sendEvent(a,&click);app.processEvents();
 REQUIRE(panels.collapsed(a));
 panels.restoreAll();app.processEvents();REQUIRE(!body->isHidden()&&!panels.collapsed(a));
 main.tabifyDockWidget(a,b);app.processEvents();REQUIRE(!panels.canCollapse(a));
 main.removeDockWidget(b);b->hide();app.processEvents();
 REQUIRE(panels.canCollapse(a));REQUIRE(panels.toggle(a));a->hide();app.processEvents();
 REQUIRE(!panels.collapsed(a)&&!body->isHidden());
 std::cout<<"dock height, native content identity, restore, header gesture, tab guard passed\n";
}
