#include "augment.h"
#include <QApplication>
#include <QLabel>
#include <QToolTip>
#include <QTemporaryDir>
#include <QFile>
#include <QHelpEvent>
#include <QScreen>
#include <QEventLoop>
#include <QTimer>
#include <iostream>
#define CHECK(x) do {if(!(x)){std::cerr<<"line "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
static void wait(int ms=40){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
static QLabel *tip(){
 for(auto *w:QApplication::topLevelWidgets())
  if(w->isVisible()&&w->windowType()==Qt::ToolTip&&w->inherits("QTipLabel"))return qobject_cast<QLabel*>(w);
 return nullptr;
}
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setStyle("Fusion");
 QTemporaryDir tmp;augment::Controller controller(tmp.filePath("rules.json"));
 QLabel owner("Hover");owner.resize(400,150);owner.show();wait();
 const auto pos=owner.mapToGlobal(QPoint(40,40));
 const QString longText=QString("A native tooltip with many readable words and no truncated content. ").repeated(12);
 owner.setToolTip(longText);
 QHelpEvent event(QEvent::ToolTip,QPoint(40,40),pos);
 QApplication::sendEvent(&owner,&event);wait();
 auto *w=tip();CHECK(w&&w->width()<=360&&w->text()==longText&&w->wordWrap());
 CHECK(w->height()>w->fontMetrics().height()*2&&w->height()>=w->heightForWidth(w->width()));
 CHECK(owner.toolTip()==longText);
 QToolTip::showText(pos,"Short hint",&owner);wait();w=tip();
 CHECK(w&&w->width()<160&&w->text()=="Short hint");
 QToolTip::showText(pos,longText+" Updated",&owner);wait();w=tip();
 CHECK(w&&w->width()<=360&&w->height()>w->fontMetrics().height()*2&&w->height()>=w->heightForWidth(w->width()));
 CHECK(controller.tooltipWidth()==360);
 CHECK(controller.setTooltipWidth(220)&&!controller.setTooltipWidth(159)&&!controller.setTooltipWidth(641));
 wait(400);
 QToolTip::showText(pos,longText+" Smaller",&owner);wait();w=tip();
 CHECK(w&&w->width()<=220&&w->height()>w->fontMetrics().height()*2&&w->height()>=w->heightForWidth(w->width()));
 const QString path="/home/user/"+QString("averylongfilename").repeated(18)+".json";
 QToolTip::showText(pos,path,&owner);wait();w=tip();
 CHECK(w&&w->width()<=220&&w->text()==path&&w->height()>w->fontMetrics().height()*2&&w->height()>=w->heightForWidth(w->width()));
 const QString rich="<b>Native formatting</b><p>"+longText+"</p>";
 QToolTip::showText(pos,rich,&owner);wait();w=tip();
 CHECK(w&&w->width()<=220&&w->text()==rich&&w->height()>w->fontMetrics().height()*2&&w->height()>=w->heightForWidth(w->width()));
 CHECK(controller.setTooltipWidth(0));wait(400);
 QToolTip::showText(pos,longText+" Native",&owner);wait();w=tip();
 CHECK(w&&w->width()>360&&w->maximumWidth()==QWIDGETSIZE_MAX);
 CHECK(controller.setTooltipWidth(280));wait(400);
 QToolTip::showText(pos,longText+" Enabled",&owner);wait();w=tip();CHECK(w&&w->width()<=280);
 controller.setRuntimeSafeMode(true);wait(400);
 QToolTip::showText(pos,longText+" Safe",&owner);wait();w=tip();CHECK(w&&w->width()>360);
 controller.setRuntimeSafeMode(false);wait(400);
 QToolTip::showText(pos,longText+" Resumed",&owner);wait();w=tip();CHECK(w&&w->width()<=280);
 CHECK(controller.tooltipWidth()==280);
 QFile config(tmp.filePath("rules.json"));CHECK(config.open(QIODevice::ReadOnly));
 const auto raw=config.readAll();config.close();
 CHECK(raw.contains("\"tooltipWidth\": 280"));
 return 0;
}
