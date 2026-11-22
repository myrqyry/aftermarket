#include "audio-patchbay.h"
#include <QApplication>
#include <QToolButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QImage>
#include <QFont>
#include <QCoreApplication>
#include <QAbstractItemModel>
#include <QMouseEvent>
#include <QPointer>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\\n";return 1;}}while(false)
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setStyle("Fusion");
 QVector<augment::RouteSource> saved={{"id-mic","Microphone",1u,0,false,true,true},{"id-desk","Desktop",3u,2,true,false,false}};
 int writes=0,monitors=0,connects=0;
 augment::AudioPatchbay ui(
  [&]{return saved;},
  [&](const QString &id,int track){
    for(auto &source:saved)if(source.id==id&&track>=1&&track<=6){
      source.tracks^=1u<<(track-1);++writes;return true;
    }
    return false;
  },
  [&](const QString &id,int mode){
    for(auto &source:saved)if(source.id==id&&mode>=0&&mode<=2){
      source.monitor=mode;++monitors;return true;
    }
    return false;
  },nullptr,
  [&](const QString &id,int track,bool enabled){
    for(auto &source:saved)if(source.id==id&&track>=1&&track<=6){
      const uint32_t bit=1u<<(track-1);
      source.tracks=enabled?(source.tracks|bit):(source.tracks&~bit);
      ++connects;++writes;return true;
    }
    return false;
  });
 ui.resize(560,510);ui.show();app.processEvents();
 REQUIRE(ui.routeCount()==2);
 // Typography: one restrained heading, body-weight labels, theme font intact.
 auto *heading=ui.findChild<QLabel*>("aftermarket-route-heading");
 auto *sourceLabel=ui.findChild<QLabel*>("aftermarket-route-source-label");
 auto *trackLabel=ui.findChild<QLabel*>("aftermarket-route-tracks-label");
 auto *monitorLabel=ui.findChild<QLabel*>("aftermarket-route-monitor-label");
 REQUIRE(heading&&heading->font().weight()==QFont::DemiBold);
 REQUIRE(sourceLabel&&sourceLabel->font().weight()==QFont::Normal);
 REQUIRE(trackLabel&&trackLabel->font().weight()==QFont::Normal);
 REQUIRE(monitorLabel&&monitorLabel->font().weight()==QFont::Normal);
 // Read-only diagnostics are sourced from each route record and must not
 // change mixer masks, monitoring modes or the native source mute setting.
 auto *diagnostics=ui.findChild<QLabel*>("aftermarket-route-diagnostics");
 REQUIRE(diagnostics);
 REQUIRE(diagnostics->text().contains("Track 1"));
 REQUIRE(diagnostics->text().contains("Monitor off"));
 REQUIRE(!diagnostics->text().contains("Muted"));
 REQUIRE(ui.selectSource("id-desk"));
 REQUIRE(diagnostics->text().contains("Tracks 1, 2"));
 REQUIRE(diagnostics->text().contains("Monitor and output"));
 REQUIRE(diagnostics->text().contains("Muted"));
 REQUIRE(diagnostics->text().contains("Disabled"));
 REQUIRE(diagnostics->text().contains("Audio inactive"));
 REQUIRE(writes==0&&monitors==0&&connects==0);
 REQUIRE(ui.selectSource("id-mic"));

 REQUIRE(heading->font().family()==ui.font().family());
 // The canvas must fit the dock without the prior 365px graph minimum.
 auto *canvas=ui.findChild<QWidget*>("aftermarket-route-canvas");
 REQUIRE(canvas&&canvas->minimumWidth()<=250&&canvas->minimumHeight()<=170);
 // A selected source should be an understated, theme-derived tint, not
 // an opaque bright highlight that overwhelms its label.
 const auto base=canvas->palette().color(QPalette::Button);
 const auto accent=canvas->palette().color(QPalette::Highlight);
 const auto distance=[](QColor a,QColor b){
   const int r=a.red()-b.red(),g=a.green()-b.green(),bl=a.blue()-b.blue();
   return r*r+g*g+bl*bl;
 };
 const auto selectedPixel=canvas->grab().toImage().pixelColor(113,35);
 REQUIRE(distance(selectedPixel,base)<distance(selectedPixel,accent));
 auto *showAll=ui.findChild<QCheckBox*>("aftermarket-show-all-wires");
 REQUIRE(showAll&&!showAll->isChecked());
 REQUIRE(!ui.showAllWires()&&ui.visibleCableCount()==1);
 const QImage focused=canvas->grab().toImage();
 showAll->click();app.processEvents();
 REQUIRE(ui.showAllWires()&&ui.visibleCableCount()==4); // Desktop also uses OBS monitoring
 const QImage all=canvas->grab().toImage();
 int cablePixelsChanged=0;
 for(int y=35;y<qMin(220,all.height());++y)
   for(int x=135;x<all.width()-55;++x)
     if(focused.pixel(x,y)!=all.pixel(x,y))++cablePixelsChanged;
 REQUIRE(cablePixelsChanged>0); // all-wires changes actual rendered routes
 showAll->click();app.processEvents();
 REQUIRE(!ui.showAllWires()&&ui.visibleCableCount()==1);
 // Drag from the first source to track 4, even with another source selected.
 REQUIRE(ui.selectSource("id-desk"));
 const auto gesture=[&](QPoint start,QPoint end){
   auto send=[&](QEvent::Type type,QPoint pt,Qt::MouseButton button,Qt::MouseButtons held){
     QMouseEvent event(type,QPointF(pt),QPointF(canvas->mapToGlobal(pt)),button,held,Qt::NoModifier);
     QCoreApplication::sendEvent(canvas,&event);
   };
   send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
   send(QEvent::MouseMove,end,Qt::NoButton,Qt::LeftButton);
   send(QEvent::MouseButtonRelease,end,Qt::LeftButton,Qt::NoButton);
   app.processEvents();
 };
 canvas->resize(qMax(canvas->width(),260),qMax(canvas->height(),240));
 // Hovering a source or track previews that signal path WITHOUT changing
 // selection, routing, or the persistent Show all wires setting.
 const auto hover=[&](QPoint point){
   QMouseEvent event(QEvent::MouseMove,QPointF(point),
                     QPointF(canvas->mapToGlobal(point)),
                     Qt::NoButton,Qt::NoButton,Qt::NoModifier);
   QCoreApplication::sendEvent(canvas,&event);
   app.processEvents();
 };
 REQUIRE(ui.selectSource("id-mic"));
 hover(QPoint(45,43+32)); // Desktop source: two existing wires
 REQUIRE(ui.selectedId()=="id-mic"&&!ui.showAllWires());
 REQUIRE(ui.visibleCableCount()==3); // Two output tracks plus native monitor path
 REQUIRE(canvas->toolTip().contains("Muted"));
 const auto hoveredDesk=canvas->grab().toImage();
 int hoverChanged=0;
 for(int y=35;y<qMin(220,hoveredDesk.height());++y)
   for(int x=135;x<hoveredDesk.width()-55;++x)
     if(focused.pixel(x,y)!=hoveredDesk.pixel(x,y))++hoverChanged;
 REQUIRE(hoverChanged>0);
 hover(QPoint(canvas->width()-23,43+32)); // Track 2: one incoming wire
 REQUIRE(ui.visibleCableCount()==1);
 REQUIRE(canvas->toolTip().contains("Track 2"));
 const auto hoveredTrack=canvas->grab().toImage();
 REQUIRE(hoveredTrack!=hoveredDesk);
 hover(QPoint(canvas->width()/2,canvas->height()-2));
 REQUIRE(ui.visibleCableCount()==1);
 hover(QPoint(45,43+32));
 REQUIRE(ui.visibleCableCount()==3);
 QEvent leave(QEvent::Leave);
 QCoreApplication::sendEvent(canvas,&leave);
 app.processEvents();
 REQUIRE(ui.visibleCableCount()==1&&!ui.showAllWires());
 REQUIRE(writes==0&&monitors==0&&connects==0);
 const QPoint source0(45,43);
 const QPoint destination4(canvas->width()-23,43+3*32);
 gesture(source0,destination4);
 REQUIRE(saved[0].tracks==(1u|(1u<<3)));
 REQUIRE(writes==1);
 REQUIRE(ui.selectedId()=="id-mic");
 // Dragging again to an already connected destination must not disconnect it.
 gesture(source0,destination4);
 REQUIRE(writes==1&&connects==1&&saved[0].tracks==(1u|(1u<<3)));
 // A drop outside targets must not mutate any source.
 gesture(source0,QPoint(canvas->width()/2,canvas->height()-2));
 REQUIRE(writes==1);
 // Polling an unchanged source list should not rebuild an open combo box.
 int removedRows=0;
 auto *sourceBox=ui.findChild<QComboBox*>("aftermarket-route-source");
 REQUIRE(sourceBox);
 QObject::connect(sourceBox->model(),&QAbstractItemModel::rowsRemoved,&ui,
                 [&](const QModelIndex&,int,int){++removedRows;});
 ui.refresh();REQUIRE(removedRows==0);
 REQUIRE(ui.selectSource("id-mic"));
 REQUIRE(ui.selectedId()=="id-mic");
 auto *track4=ui.findChild<QToolButton*>("aftermarket-track-4");
 REQUIRE(track4&&track4->isEnabled()&&track4->isChecked());
 REQUIRE(track4->font().weight()<=QFont::Medium);
 const auto trackImage=track4->grab().toImage();
 const auto trackBg=trackImage.pixelColor(5,trackImage.height()/2);
 REQUIRE(distance(trackBg,track4->palette().color(QPalette::Button)) <
         distance(trackBg,track4->palette().color(QPalette::Highlight)));
 track4->click();app.processEvents();
 REQUIRE(writes==2&&saved[0].tracks==1u&&!track4->isChecked());
 auto *track1=ui.findChild<QToolButton*>("aftermarket-track-1");
 REQUIRE(track1&&track1->isChecked());track1->click();app.processEvents();
 REQUIRE(writes==3&&saved[0].tracks==0u&&!track1->isChecked());
 auto *monitor=ui.findChild<QComboBox*>("aftermarket-monitor");
 REQUIRE(monitor&&monitor->currentIndex()==0);
 monitor->setCurrentIndex(1);app.processEvents();
 REQUIRE(monitors==1&&saved[0].monitor==1);
 saved[0].muted=true;
 saved[0].enabled=false;
 saved[0].audioActive=false;
 ui.refresh();
 REQUIRE(diagnostics->text().contains("Muted"));
 REQUIRE(diagnostics->text().contains("Disabled"));
 REQUIRE(diagnostics->text().contains("Audio inactive"));
 REQUIRE(writes==3&&monitors==1);
 saved[0].muted=false;
 saved[0].enabled=true;
 saved[0].audioActive=true;
 ui.refresh();
 REQUIRE(!diagnostics->text().contains("Muted"));
 saved[0].name="Renamed microphone";ui.refresh();
 REQUIRE(ui.selectedId()=="id-mic"&&ui.routeCount()==2);
 // Backend must remain authoritative when changed externally.
 saved[0].tracks=1u<<5;ui.refresh();
 REQUIRE(!track4->isChecked()&&ui.findChild<QToolButton*>("aftermarket-track-6")->isChecked());
 saved.removeFirst();ui.refresh();
 REQUIRE(ui.selectedId()=="id-desk"&&ui.routeCount()==1);
 REQUIRE(monitor->currentIndex()==2);
 ui.selectSource("id-desk");saved[0].id="id-changed";
 track4->click();app.processEvents(); // stale UUID fails safely
 REQUIRE(writes==3);
 saved.clear();ui.refresh();
 REQUIRE(ui.routeCount()==0&&ui.selectedId().isEmpty()&&!track4->isEnabled());
 REQUIRE(diagnostics->text().contains("No audio source selected"));
 // A dedicated output on the graph represents native OBS monitoring, not
 // a seventh output track or a newly generated audio bus.
 QVector<augment::RouteSource> monitorSaved={
     {"music","Music",1u,0,false,true,true},
     {"voice","Voice",2u,1,false,true,true}
 };
 int monitorChanges=0,trackChanges=0;
 augment::AudioPatchbay monitorUi(
     [&]{return monitorSaved;},
     [&](const QString &,int){++trackChanges;return false;},
     [&](const QString &id,int mode){
         for(auto &source:monitorSaved)if(source.id==id&&mode>=0&&mode<=2){
             source.monitor=mode;++monitorChanges;return true;
         }
         return false;
     });
 monitorUi.resize(560,540);monitorUi.show();app.processEvents();
 auto *monitorCanvas=monitorUi.findChild<QWidget*>("aftermarket-route-canvas");
 auto *monitorOutput=monitorUi.findChild<QToolButton*>("aftermarket-monitor-output");
 REQUIRE(monitorCanvas&&monitorOutput);
 REQUIRE(monitorOutput->isCheckable()&&!monitorOutput->isChecked());
 REQUIRE(monitorOutput->toolTip().contains("OBS"));
 const uint32_t originalTracks=monitorSaved[0].tracks;
 // The MON node must be visible even when the dock can only show its
 // first one or two numbered tracks (not hidden below the scroll area).
 const QPoint monitorPort(monitorCanvas->width()-23,14);
 const auto sendMonitorGesture=[&](QPoint begin,QPoint end){
     auto send=[&](QEvent::Type type,QPoint pt,Qt::MouseButton button,Qt::MouseButtons held){
         QMouseEvent event(type,QPointF(pt),QPointF(monitorCanvas->mapToGlobal(pt)),
                           button,held,Qt::NoModifier);
         QCoreApplication::sendEvent(monitorCanvas,&event);
     };
     send(QEvent::MouseButtonPress,begin,Qt::LeftButton,Qt::LeftButton);
     send(QEvent::MouseMove,end,Qt::NoButton,Qt::LeftButton);
     send(QEvent::MouseButtonRelease,end,Qt::LeftButton,Qt::NoButton);
     app.processEvents();
 };
 // Monitoring should appear as a visible wire and destination tooltip.
 REQUIRE(monitorUi.visibleCableCount()==1);
 REQUIRE(monitorUi.selectSource("voice"));
 REQUIRE(monitorUi.visibleCableCount()==2);
 {
     QMouseEvent event(QEvent::MouseMove,QPointF(monitorPort),
                       QPointF(monitorCanvas->mapToGlobal(monitorPort)),
                       Qt::NoButton,Qt::NoButton,Qt::NoModifier);
     QCoreApplication::sendEvent(monitorCanvas,&event);
 }
 REQUIRE(monitorCanvas->toolTip().contains("Monitor"));
 REQUIRE(monitorUi.selectSource("music"));
 const QImage beforeMonitor=monitorCanvas->grab().toImage();
 sendMonitorGesture(QPoint(45,43),monitorPort);
 REQUIRE(monitorSaved[0].monitor==2&&monitorChanges==1);
 const QImage afterMonitor=monitorCanvas->grab().toImage();
 int monitorCablePixels=0;
 for(int y=33;y<qMin(monitorPort.y()+20,afterMonitor.height());++y)
     for(int x=132;x<qMax(133,afterMonitor.width()-50);++x)
         if(beforeMonitor.pixel(x,y)!=afterMonitor.pixel(x,y))++monitorCablePixels;
 REQUIRE(monitorCablePixels>0); // Native monitor route is rendered as a cable.
 REQUIRE(monitorSaved[0].tracks==originalTracks&&trackChanges==0);
 REQUIRE(monitorOutput->isChecked());
 REQUIRE(monitorUi.visibleCableCount()==2); // Track 1 plus Monitor output
 sendMonitorGesture(QPoint(45,43),monitorPort);
 REQUIRE(monitorChanges==1); // Reconnecting never toggles monitored sources off.
 monitorOutput->click();app.processEvents();
 REQUIRE(monitorSaved[0].monitor==0&&monitorChanges==2&&!monitorOutput->isChecked());
 REQUIRE(monitorSaved[0].tracks==originalTracks);
 REQUIRE(monitorUi.setMonitoring(1));
 REQUIRE(monitorSaved[0].monitor==1&&monitorOutput->isChecked());
 REQUIRE(monitorUi.connectMonitor("music")&&monitorSaved[0].monitor==1);
 REQUIRE(monitorChanges==3); // Monitor Only is a valid existing connection.
 REQUIRE(!monitorUi.connectMonitor("missing-uuid"));
 REQUIRE(monitorChanges==3&&trackChanges==0);
 REQUIRE(monitorUi.selectSource("voice")&&monitorOutput->isChecked());
 monitorOutput->click();app.processEvents();
 REQUIRE(monitorSaved[1].monitor==0&&!monitorOutput->isChecked());
 REQUIRE(monitorSaved[1].tracks==2u);
 // The MON output node itself is clickable, not just the extra row button.
 REQUIRE(monitorUi.selectSource("music")&&monitorSaved[0].monitor==1);
 const auto clickOutput=[&]{
     QMouseEvent event(QEvent::MouseButtonPress,QPointF(monitorPort),
                       QPointF(monitorCanvas->mapToGlobal(monitorPort)),
                       Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
     QCoreApplication::sendEvent(monitorCanvas,&event);
     QMouseEvent released(QEvent::MouseButtonRelease,QPointF(monitorPort),
                          QPointF(monitorCanvas->mapToGlobal(monitorPort)),
                          Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
     QCoreApplication::sendEvent(monitorCanvas,&released);
     app.processEvents();
 };
 clickOutput();
 REQUIRE(monitorSaved[0].monitor==0&&!monitorOutput->isChecked());
 clickOutput();
 REQUIRE(monitorSaved[0].monitor==2&&monitorOutput->isChecked());
 REQUIRE(monitorSaved[0].tracks==originalTracks&&trackChanges==0);
 std::cout<<"audio patchbay routing and native monitor output verified\n";
 return 0;
}
