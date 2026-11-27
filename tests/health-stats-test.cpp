#include "health-stats.h"
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
 QTemporaryDir tmp;QMainWindow main;main.show();HealthSample sample;
 {
  HealthStats widget(&main,tmp.filePath("health.ini"),[&]{return sample;},false);
  auto*b=main.findChild<QToolButton*>("aftermarket-health-stats");CHECK(b&&b->isHidden()&&widget.enabled());
  widget.poll();sample.renderMissed=1;widget.poll();CHECK(!b->isHidden()&&b->text().contains("Rendering"));
  CHECK(widget.setEnabled(false)&&b->isHidden());
 }
 CHECK(!main.findChild<QToolButton*>("aftermarket-health-stats"));
 {HealthStats widget(&main,tmp.filePath("health.ini"),[&]{return sample;},false);CHECK(!widget.enabled());}
 return 0;
}
