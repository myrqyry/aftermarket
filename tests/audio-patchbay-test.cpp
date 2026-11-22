#include "audio-patchbay.h"
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QToolButton>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QImage>
#include <QCoreApplication>
#include <iostream>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)

static void sendMouse(QWidget *w,QEvent::Type type,QPoint point,
                      Qt::MouseButton button,Qt::MouseButtons held){
 QMouseEvent ev(type,QPointF(point),QPointF(w->mapToGlobal(point)),button,held,Qt::NoModifier);
 QCoreApplication::sendEvent(w,&ev);
}
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setStyle("Fusion");
 QVector<augment::RouteSource> sources={
    {"voice","Live Latent: TTS",0x3f,2,false,true,true},
    {"browser","Browser",0x1,0,false,true,true},
    {"music","Media Source",0x4,1,false,true,true}
 };
 int trackWrites=0,monitorWrites=0;
 QHash<QString,augment::SignalReading> levels;
 augment::AudioPatchbay bay(
   [&]{return sources;},
   [&](const QString &id,int track){
      for(auto &s:sources)if(s.id==id){s.tracks^=1u<<(track-1);++trackWrites;return true;}
      return false;
   },
   [&](const QString &id,int mode){
      for(auto &s:sources)if(s.id==id){s.monitor=mode;++monitorWrites;return true;}
      return false;
   },
   nullptr,
   [&](const QString &id,int track,bool enable){
      for(auto &s:sources)if(s.id==id){auto bit=1u<<(track-1);s.tracks=enable?s.tracks|bit:s.tracks&~bit;++trackWrites;return true;}
      return false;
   },
   [&](const QVector<augment::RouteSource>&){return levels;}
 );
 bay.resize(350,340);bay.show();app.processEvents();
 auto *canvas=bay.findChild<QWidget*>("aftermarket-route-canvas");
 auto *mode=bay.findChild<QComboBox*>("aftermarket-monitor");
 auto *focused=bay.findChild<QLabel*>("aftermarket-selected-source");
 auto *all=bay.findChild<QCheckBox*>("aftermarket-show-all-wires");
 REQUIRE(canvas&&mode&&focused&&all);
 REQUIRE(!bay.findChild<QWidget*>("aftermarket-route-source"));
 REQUIRE(!bay.findChild<QWidget*>("aftermarket-route-tracks-label"));
 REQUIRE(!bay.findChild<QWidget*>("aftermarket-route-diagnostics"));
 REQUIRE(!bay.findChild<QWidget*>("aftermarket-route-status"));
 REQUIRE(!bay.findChild<QWidget*>("aftermarket-track-1"));
 REQUIRE(!bay.findChild<QWidget*>("aftermarket-monitor-output"));
 REQUIRE(canvas->minimumHeight()<=170);
 REQUIRE(bay.routeCount()==3);
 REQUIRE(bay.selectedId()=="voice");
 REQUIRE(focused->text()=="Monitor"&&focused->toolTip().contains("Live Latent"));
 REQUIRE(bay.visibleCableCount()==7); // Six tracks + headphone monitor
 REQUIRE(!bay.showAllWires());
 all->click();app.processEvents();
 REQUIRE(bay.visibleCableCount()==10); // + Browser/Media connections
 all->click();app.processEvents();
 REQUIRE(!bay.showAllWires());
 const QPoint sourceBrowser(39,43+32);
 sendMouse(canvas,QEvent::MouseButtonPress,sourceBrowser,Qt::LeftButton,Qt::LeftButton);
 sendMouse(canvas,QEvent::MouseButtonRelease,sourceBrowser,Qt::LeftButton,Qt::NoButton);
 app.processEvents();
 REQUIRE(bay.selectedId()=="browser"&&focused->toolTip().contains("Browser"));
 REQUIRE(bay.visibleCableCount()==1);
 REQUIRE(mode->currentIndex()==0);
 // The source list is the selector, and monitored mode is a selected-source property.
 REQUIRE(bay.selectSource("voice"));
 REQUIRE(mode->currentIndex()==2);
 mode->setCurrentIndex(1);
 REQUIRE(monitorWrites==1&&sources[0].monitor==1);
 const auto head=QPoint(canvas->width()-23,14);
 const auto image=canvas->grab().toImage();
 // Headphones are a visual drawing in the graph, not the text "MON".
 REQUIRE(!image.isNull());
 sendMouse(canvas,QEvent::MouseMove,head,Qt::NoButton,Qt::NoButton);
 REQUIRE(canvas->toolTip().contains("Monitor"));
 REQUIRE(canvas->toolTip().contains("not Track 7"));
 // Click to disconnect; drag from a source to connect (without touching tracks).
 sendMouse(canvas,QEvent::MouseButtonPress,head,Qt::LeftButton,Qt::LeftButton);
 sendMouse(canvas,QEvent::MouseButtonRelease,head,Qt::LeftButton,Qt::NoButton);
 REQUIRE(sources[0].monitor==0&&monitorWrites==2);
 const uint32_t original= sources[0].tracks;
 const auto drag=[&](QPoint start,QPoint end){
    sendMouse(canvas,QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
    sendMouse(canvas,QEvent::MouseMove,end,Qt::NoButton,Qt::LeftButton);
    sendMouse(canvas,QEvent::MouseButtonRelease,end,Qt::LeftButton,Qt::NoButton);
    app.processEvents();
 };
 drag(QPoint(42,43),head);
 REQUIRE(sources[0].monitor==2&&sources[0].tracks==original);
 drag(QPoint(42,43),head);
 REQUIRE(sources[0].monitor==2&&monitorWrites==3);
 // Native numbered track clicks keep working; no duplicate toggles needed.
 const QPoint track6(canvas->width()-23,43+5*32);
 sendMouse(canvas,QEvent::MouseButtonPress,track6,Qt::LeftButton,Qt::LeftButton);
 sendMouse(canvas,QEvent::MouseButtonRelease,track6,Qt::LeftButton,Qt::NoButton);
 REQUIRE((sources[0].tracks&(1u<<5))==0&&trackWrites==1);
 // Signal dots come from actual level readings, not 'source enabled'.
 auto before=canvas->grab().toImage();
 levels.insert("voice",{true,true});
 levels.insert("browser",{true,false});
 levels.insert("music",{true,true});
 bay.refreshLevels();app.processEvents();
 auto after=canvas->grab().toImage();
 REQUIRE(before!=after);
 REQUIRE(bay.sourceSignal("voice"));
 REQUIRE(!bay.sourceSignal("browser"));
 REQUIRE(bay.outputSignal(1));
 REQUIRE(!bay.outputSignal(6));
 REQUIRE(bay.outputSignal(7)); // monitoring signal
 sources[0].muted=true;
 bay.refresh();
 bay.refreshLevels();
 REQUIRE(!bay.outputSignal(1)); // browser is silent, voice is muted
 REQUIRE(bay.outputSignal(7)); // Music is independently monitored and producing signal
 REQUIRE(!bay.sourceSignal("voice"));
 // A source flagged active by OBS is not automatically an audible signal.
 levels.clear();bay.refreshLevels();
 REQUIRE(!bay.sourceSignal("music"));
 // Hot-reload and source UUID changes don't redirect routing.
 sources.removeFirst();bay.refresh();
 REQUIRE(bay.selectedId()=="browser");
 REQUIRE(!bay.selectSource("voice"));
 REQUIRE(!bay.connectRoute("voice",2));
 REQUIRE(trackWrites==1);
 sources.clear();bay.refresh();
 REQUIRE(bay.routeCount()==0&&bay.selectedId().isEmpty());
 REQUIRE(mode->currentIndex()==-1&&!mode->isEnabled());
 std::cout<<"compact AudioHub graph, headphones route, live dots and native editing verified\n";
 return 0;
}
