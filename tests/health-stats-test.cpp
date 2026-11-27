#include "health-stats.h"
#include "menu-settings-corner.h"
#include "action-strip.h"
#include "menu-bar-compact.h"
#include <QStatusBar>
#include <QMenuBar>
#include <QApplication>
#include <QMainWindow>
#include <QToolButton>
#include <QTemporaryDir>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<" "<<#x<<"\n";return 1;}}while(0)
using namespace augment;
int main(int argc,char**argv){
 QApplication app(argc,argv);
 HealthPolicy policy;HealthSample s;s.rendered=100;s.renderMissed=7;s.encoded=100;s.encodeSkipped=3;
 s.stream=true;s.streamId=1;s.networkDropped=10;s.obsCpu=20;s.systemCpu=30;s.gpu=50;s.renderBudget=25;
 CHECK(policy.update(s).isEmpty()); // historical counters aren't new trouble
 s.renderMissed++;s.rendered+=60;CHECK(policy.update(s).contains("Rendering lag"));
 for(int i=0;i<5;i++)CHECK(!policy.update(s).isEmpty());
 CHECK(policy.update(s).isEmpty());
 s.networkDropped++;CHECK(policy.update(s).contains("Network drops"));
 s.stream=false;CHECK(!policy.update(s).contains("Network drops"));
 s.stream=true;s.streamId=2;s.networkDropped=100;CHECK(policy.update(s).isEmpty());
 s.reconnecting=true;CHECK(policy.update(s).contains("Reconnecting"));
 s.stream=false;s.reconnecting=false;policy.update(s);
 s.gpu=95;CHECK(policy.update(s).isEmpty());CHECK(policy.update(s).isEmpty());CHECK(policy.update(s).contains("GPU load"));
 s.gpu.reset();CHECK(!policy.update(s).contains("GPU load"));
 s.systemCpu=94;policy.update(s);policy.update(s);CHECK(policy.update(s).contains("System CPU"));
 s.systemCpu=50;s.encodeSkipped++;CHECK(policy.update(s).contains("Encoding lag"));
 s.encodeSkipped=0;s.encoded=0;policy.reset();CHECK(policy.update(s).isEmpty());
 QTemporaryDir tmp;QMainWindow main;main.menuBar()->setNativeMenuBar(false);main.menuBar()->addMenu("File");main.resize(800,400);main.show();HealthSample sample;
 QAction settings("Settings",&main);MenuSettingsCorner corner(main.menuBar(),&settings);
 auto*host=main.findChild<QWidget*>("aftermarket-menu-settings-host");CHECK(host);
 auto*gear=host->findChild<QToolButton*>("aftermarket-menu-settings");CHECK(gear);
 {
  HealthStats widget(&main,tmp.filePath("health.ini"),[&]{return sample;},false);
  auto*b=main.findChild<QToolButton*>("aftermarket-health-stats");CHECK(b&&b->isHidden()&&widget.enabled());
  widget.poll();sample.renderMissed=1;widget.poll();CHECK(!b->isHidden()&&b->text().contains("Rendering"));
  CHECK(widget.placement()==HealthPlacement::StatusBar);
  CHECK(widget.setPlacement(HealthPlacement::MenuBar));app.processEvents();
  CHECK(b->parentWidget()==host&&b->isVisible()&&b->geometry().right()<gear->geometry().left());
  CHECK(widget.setPlacement(HealthPlacement::Both));app.processEvents();
  auto*top=host->findChild<QToolButton*>("aftermarket-health-stats-menu");
  CHECK(top&&top->isVisible()&&b->parentWidget()==main.statusBar()&&b->isVisible());
  for(int i=0;i<6;i++)widget.poll();app.processEvents();
  CHECK(b->isHidden()&&top->isHidden());
  sample.renderMissed++;widget.poll();CHECK(!top->isHidden());
  CHECK(widget.setPlacement(HealthPlacement::MenuBar));app.processEvents();
  CHECK(b->parentWidget()==host&&!b->isHidden()&&top->isHidden());
  ActionStrip strip(tmp.filePath("strip.json"));
  CHECK(strip.setEnabled(main.menuBar(),true));app.processEvents();
  CHECK(b->parentWidget()==host&&b->isVisible()&&gear->isVisible());
  MenuBarCompact compact;
  CHECK(compact.setEnabled(main.menuBar(),true));app.processEvents();
  CHECK(b->isVisible()&&gear->isVisible());
  CHECK(compact.setEnabled(main.menuBar(),false));
  CHECK(strip.setEnabled(main.menuBar(),false));app.processEvents();
  CHECK(b->parentWidget()==host&&b->isVisible()&&gear->isVisible());
  CHECK(widget.setEnabled(false)&&b->isHidden());
 }
 CHECK(!main.findChild<QToolButton*>("aftermarket-health-stats"));
 {HealthStats widget(&main,tmp.filePath("health.ini"),[&]{return sample;},false);CHECK(!widget.enabled()&&widget.placement()==HealthPlacement::MenuBar);}
 return 0;
}
