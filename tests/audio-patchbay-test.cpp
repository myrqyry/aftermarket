#include "audio-patchbay.h"
#include <QApplication>
#include <QMenu>
#include <QComboBox>
#include <QAction>
#include <QCheckBox>
#include <QToolButton>
#include <QLabel>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QPainter>
#include <QImage>
#include <QCoreApplication>
#include <QStyle>
#include <QPalette>
#include <QScrollArea>
#include <QScrollBar>
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
 sources[0].sourceType="live_latent_tts";
 sources[1].sourceType="browser_source";
 sources[2].sourceType="ffmpeg_source";
 int trackWrites=0,monitorWrites=0,routeQueries=0;
 QHash<QString,augment::SignalReading> levels;
 augment::AudioPatchbay bay(
   [&]{++routeQueries;return sources;},
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
 auto *menu=bay.findChild<QMenu*>("aftermarket-monitor-mode-menu");
 REQUIRE(canvas&&menu);
 // AudioBus has only a native-styled section heading; the two actions live
 // in the existing dock-title overflow, never in another row of controls.
 REQUIRE(!bay.findChild<QCheckBox*>("aftermarket-show-all-wires"));
 REQUIRE(!bay.findChild<QToolButton*>("aftermarket-route-refresh"));
 REQUIRE(!bay.findChild<QToolButton*>());
 QMenu routing(&bay);
 routing.setObjectName("aftermarket-test-dock-overflow");
 bay.populateRoutingMenu(&routing);
 REQUIRE(routing.actions().size()==2);
 auto *all=routing.actions()[0];
 auto *reload=routing.actions()[1];
 REQUIRE(all->objectName()=="aftermarket-show-all-wires");
 REQUIRE(all->text()=="Show all wires"&&all->isCheckable()&&all->isChecked());
 REQUIRE(reload->objectName()=="aftermarket-refresh-audio-routes");
 REQUIRE(reload->text()=="Refresh audio routes"&&!reload->isCheckable());
 REQUIRE(!reload->icon().isNull()); // Qt theme / QStyle, not a Unicode refresh glyph
 REQUIRE(routing.styleSheet().isEmpty());
 // This must follow OBS's actual QPalette, not the desktop icon theme
 // (which may render a hardcoded blue refresh glyph in a dark OBS theme).
 QMenu pale(&bay),dark(&bay);
 auto palePalette=routing.palette();
 palePalette.setColor(QPalette::Text,QColor(24,30,38));
 pale.setPalette(palePalette);
 auto darkPalette=routing.palette();
 darkPalette.setColor(QPalette::Text,QColor(238,226,217));
 dark.setPalette(darkPalette);
 bay.populateRoutingMenu(&pale);
 bay.populateRoutingMenu(&dark);
 const QImage paleIcon=pale.actions()[1]->icon().pixmap(18,18).toImage();
 const QImage darkIcon=dark.actions()[1]->icon().pixmap(18,18).toImage();
 REQUIRE(!paleIcon.isNull()&&!darkIcon.isNull());
 REQUIRE(paleIcon!=darkIcon);
 REQUIRE(all->icon().isNull()); // native QMenu checkmark, not hand-painted
 auto *heading=bay.findChild<QLabel*>("aftermarket-route-heading");
 REQUIRE(heading&&heading->styleSheet().isEmpty());
 REQUIRE(bay.styleSheet().isEmpty());
 REQUIRE(menu->actions().size()==3);
 REQUIRE(!bay.findChild<QComboBox*>("aftermarket-monitor"));
 REQUIRE(!bay.findChild<QLabel*>("aftermarket-selected-source"));
 REQUIRE(augment::AudioPatchbay::iconForType("browser_source")==augment::AudioPatchbay::SourceIcon::Browser);
 REQUIRE(augment::AudioPatchbay::iconForType("ffmpeg_source")==augment::AudioPatchbay::SourceIcon::Media);
 REQUIRE(augment::AudioPatchbay::iconForType("pulse_input_capture")==augment::AudioPatchbay::SourceIcon::Microphone);
 REQUIRE(augment::AudioPatchbay::iconForType("pulse_output_capture")==augment::AudioPatchbay::SourceIcon::Speaker);
 REQUIRE(augment::AudioPatchbay::iconForType("live_latent_tts")==augment::AudioPatchbay::SourceIcon::Waveform);
 REQUIRE(augment::AudioPatchbay::iconForType("unknown_plugin")==augment::AudioPatchbay::SourceIcon::Waveform);
 REQUIRE(!bay.findChild<QWidget*>("aftermarket-route-source"));
 REQUIRE(!bay.findChild<QWidget*>("aftermarket-route-tracks-label"));
 REQUIRE(!bay.findChild<QWidget*>("aftermarket-route-diagnostics"));
 REQUIRE(!bay.findChild<QWidget*>("aftermarket-route-status"));
 REQUIRE(!bay.findChild<QWidget*>("aftermarket-track-1"));
 REQUIRE(!bay.findChild<QWidget*>("aftermarket-monitor-output"));
 REQUIRE(canvas->minimumHeight()>=238);
 REQUIRE(bay.routeCount()==3);
 REQUIRE(bay.selectedId()=="voice");
 REQUIRE(menu->actions()[2]->isChecked());
 // All connections are visible immediately, including unselected sources.
 REQUIRE(bay.visibleCableCount()==10);
 REQUIRE(bay.showAllWires());
 // Hovering an output must not collapse all-wires to that single target.
 const QPoint firstOutput(canvas->width()-26,43);
 sendMouse(canvas,QEvent::MouseMove,firstOutput,Qt::NoButton,Qt::NoButton);
 REQUIRE(bay.visibleCableCount()==10);
 sendMouse(canvas,QEvent::MouseMove,QPoint(canvas->width()/2,24),Qt::NoButton,Qt::NoButton);
 REQUIRE(bay.visibleCableCount()==10);
 // Selection must not hide unrelated connections when all-wires is on.
 const auto selectedStart=canvas->grab().toImage();
 const QColor initialSelectedBorder=selectedStart.pixelColor(5,43);
 const QColor initialBrowserBorder=selectedStart.pixelColor(5,75);
 REQUIRE(bay.selectSource("browser"));
 app.processEvents();
 REQUIRE(bay.visibleCableCount()==10);
 const auto selectedBrowser=canvas->grab().toImage();
 REQUIRE(selectedBrowser.pixelColor(5,43)!=initialSelectedBorder);
 REQUIRE(selectedBrowser.pixelColor(5,75)!=initialBrowserBorder);
 REQUIRE(bay.selectSource("voice"));
 app.processEvents();
 REQUIRE(canvas->grab().toImage().pixelColor(5,43)==initialSelectedBorder);
 // Selected outline is theme-aware and different from the signal border.
 auto initialPalette=canvas->palette();
 auto selectedPalette=initialPalette;
 selectedPalette.setColor(QPalette::Link,QColor(32,200,140));
 canvas->setPalette(selectedPalette);app.processEvents();
 REQUIRE(canvas->grab().toImage().pixelColor(5,43)!=initialSelectedBorder);
 REQUIRE(canvas->grab().toImage().pixelColor(5,75)==initialBrowserBorder);
 canvas->setPalette(initialPalette);app.processEvents();
 // Explicitly turning wires off restores focused-only display.
 all->trigger();app.processEvents();
 REQUIRE(!all->isChecked()&&!bay.showAllWires());
 REQUIRE(bay.visibleCableCount()==7);
 all->trigger();app.processEvents();
 REQUIRE(all->isChecked()&&bay.showAllWires());
 REQUIRE(bay.visibleCableCount()==10);
 // The title menu is rebuilt at every opening; the checked state must
 // come from the graph, rather than from a transient QAction.
 bay.setShowAllWires(true);
 QMenu rebuilt(&bay);bay.populateRoutingMenu(&rebuilt);
 REQUIRE(rebuilt.actions()[0]->isChecked());
 bay.setShowAllWires(false);
 QMenu fresh(&bay);bay.populateRoutingMenu(&fresh);
 REQUIRE(!fresh.actions()[0]->isChecked());
 const auto queriesBefore=routeQueries;
 reload->trigger();app.processEvents();
 REQUIRE(routeQueries==queriesBefore+1);
 // Rebuilding from an old menu must never flip the current wire state.
 REQUIRE(!bay.showAllWires());
 const QPoint sourceBrowser(39,43+32);
 sendMouse(canvas,QEvent::MouseButtonPress,sourceBrowser,Qt::LeftButton,Qt::LeftButton);
 sendMouse(canvas,QEvent::MouseButtonRelease,sourceBrowser,Qt::LeftButton,Qt::NoButton);
 app.processEvents();
 REQUIRE(bay.selectedId()=="browser"&&menu->actions()[0]->isChecked());
 REQUIRE(bay.visibleCableCount()==1);
 REQUIRE(menu->actions()[0]->isChecked());
 // Keep Monitor Only accessible with a right-click menu on the headphones,
 // without recreating a redundant dropdown below the graph.
 REQUIRE(bay.selectSource("voice"));
 REQUIRE(menu->actions()[2]->isChecked());
 menu->actions()[1]->trigger();
 REQUIRE(monitorWrites==1&&sources[0].monitor==1);
 const auto head=QPoint(canvas->width()-26,16);
 // OBS sends QContextMenuEvent separately from mouse presses; it must
 // be accepted here or the native Audio Mixer context menu steals it.
 QContextMenuEvent context(QContextMenuEvent::Mouse,head,canvas->mapToGlobal(head));
 QCoreApplication::sendEvent(canvas,&context);
 REQUIRE(context.isAccepted()&&menu->isVisible());menu->hide();
 REQUIRE(menu->actions()[1]->isChecked());
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
 const QPoint track6(canvas->width()-26,43+5*32);
 const QPoint track6Corner(canvas->width()-39,43+5*32-13);
 sendMouse(canvas,QEvent::MouseButtonPress,track6Corner,Qt::LeftButton,Qt::LeftButton);
 sendMouse(canvas,QEvent::MouseButtonRelease,track6Corner,Qt::LeftButton,Qt::NoButton);
 REQUIRE(trackWrites==0); // ellipse hit testing rejects transparent corners
 sendMouse(canvas,QEvent::MouseButtonPress,track6,Qt::LeftButton,Qt::LeftButton);
 sendMouse(canvas,QEvent::MouseButtonRelease,track6,Qt::LeftButton,Qt::NoButton);
 REQUIRE((sources[0].tracks&(1u<<5))==0&&trackWrites==1);
 // Circle background doesn't fill the rectangular corner of its 28px hit box.
 const auto shape=canvas->grab().toImage();
 const int left=canvas->width()-41;
 const int top=43+4*32-14; // selected Track 5 has an accent outline
 REQUIRE(shape.pixelColor(left+2,top+2)==shape.pixelColor(left-5,top+2));
 REQUIRE(shape.pixelColor(left+14,top)!=shape.pixelColor(left-5,top));
 // Different registered OBS source types must actually change the glyph.
 REQUIRE(bay.selectSource("browser"));
 const auto browserIcon=canvas->grab().toImage();
 sources[1].sourceType="ffmpeg_source";bay.refresh();app.processEvents();
 const auto mediaIcon=canvas->grab().toImage();
 int iconPixels=0;
 for(int y=70;y<81;++y)for(int x=10;x<23;++x)
     if(browserIcon.pixel(x,y)!=mediaIcon.pixel(x,y))++iconPixels;
 REQUIRE(iconPixels>3);
 sources[1].sourceType="browser_source";bay.refresh();
 REQUIRE(bay.selectSource("voice"));
 // Activity is conveyed by the SOURCE/OUTPUT outlines themselves.
 // Neither the former right-side dot nor the interior dot may change.
 // The signal must still come from real level readings, not OBS enabled.
 auto before=canvas->grab().toImage();
 levels.insert("voice",{true,true});
 levels.insert("browser",{true,false});
 levels.insert("music",{true,true});
 bay.refreshLevels();app.processEvents();
 auto after=canvas->grab().toImage();
 REQUIRE(before!=after);
 // One outline per node: selected uses selection color, unselected
 // active uses signal color. A selected+active source must NOT gain an
 // inner/second border or change its selected outline when signal starts.
 const QRect selectedNode(5,30,124,26);
 REQUIRE(after.copy(selectedNode)==before.copy(selectedNode));
 const auto selectedColor=after.pixelColor(5,43);
 const auto activeColor=after.pixelColor(5,107);
 REQUIRE(selectedColor!=activeColor);
 // Different *hues*, not two slightly different blues in Colorway.
 const int selectedHue=selectedColor.hsvHue();
 const int activeHue=activeColor.hsvHue();
 REQUIRE(selectedHue>=0&&activeHue>=0);
 const int hueDifference=qAbs(selectedHue-activeHue);
 REQUIRE(qMin(hueDifference,360-hueDifference)>=65);
 int sourceOutlinePixels=0,outputOutlinePixels=0,oldSourceDotPixels=0,
     oldOutputDotPixels=0,leftIconPixels=0;
 for(int y=38;y<=48;++y){
     // Unselected Music has real signal: it gets the activity-color
     // OUTLINE; selected Voice keeps its selection color.
     for(int x=5;x<=9;++x)
         if(before.pixel(x,y+64)!=after.pixel(x,y+64))++sourceOutlinePixels;
     for(int x=canvas->width()-41;x<=canvas->width()-37;++x)
         if(before.pixel(x,y)!=after.pixel(x,y))++outputOutlinePixels;
     for(int x=115;x<=124;++x)
         if(before.pixel(x,y)!=after.pixel(x,y))++oldSourceDotPixels;
     for(int x=canvas->width()-12;x<=canvas->width()-4;++x)
         if(before.pixel(x,y)!=after.pixel(x,y))++oldOutputDotPixels;
     for(int x=10;x<=20;++x)
         if(before.pixel(x,y)!=after.pixel(x,y))++leftIconPixels;
 }
 REQUIRE(sourceOutlinePixels>0);
 REQUIRE(outputOutlinePixels>0);
 REQUIRE(oldSourceDotPixels==0);
 REQUIRE(oldOutputDotPixels==0);
 REQUIRE(leftIconPixels==0);
 // Active outlines must use the live theme highlight, not a fixed RGB.
 const auto originalPalette=canvas->palette();
 const auto originalOutline=canvas->grab().toImage();
 auto alternatePalette=originalPalette;
 alternatePalette.setColor(QPalette::Highlight,QColor(214,110,38));
 canvas->setPalette(alternatePalette);app.processEvents();
 const auto recoloredOutline=canvas->grab().toImage();
 int themedPixels=0;
 for(int y=38;y<=48;++y)for(int x=5;x<=9;++x)
     if(originalOutline.pixel(x,y)!=recoloredOutline.pixel(x,y))++themedPixels;
 REQUIRE(themedPixels>0);
 canvas->setPalette(originalPalette);app.processEvents();
 // The canvas supports both light and dark themes from Qt palette roles;
 // changing the OBS theme must repaint text, node fills and active outlines.
 auto day=originalPalette;
 day.setColor(QPalette::Text,QColor(25,29,36));
 day.setColor(QPalette::Button,QColor(240,242,247));
 day.setColor(QPalette::Mid,QColor(112,116,124));
 day.setColor(QPalette::Highlight,QColor(50,99,176));
 canvas->setPalette(day);app.processEvents();
 const auto dayImage=canvas->grab().toImage();
 auto night=originalPalette;
 night.setColor(QPalette::Text,QColor(228,229,235));
 night.setColor(QPalette::Button,QColor(37,39,49));
 night.setColor(QPalette::Mid,QColor(105,108,126));
 night.setColor(QPalette::Highlight,QColor(180,124,208));
 canvas->setPalette(night);app.processEvents();
 const auto nightImage=canvas->grab().toImage();
 int themePixels=0,themeBorderPixels=0;
 for(int y=37;y<=49;++y){
     for(int x=6;x<100;++x)
         if(dayImage.pixel(x,y)!=nightImage.pixel(x,y))++themePixels;
     for(int x=5;x<=9;++x)
         if(dayImage.pixel(x,y)!=nightImage.pixel(x,y))++themeBorderPixels;
 }
 REQUIRE(themePixels>45&&themeBorderPixels>0);
 canvas->setPalette(originalPalette);app.processEvents();
 REQUIRE(bay.sourceSignal("voice"));
 REQUIRE(bay.sourceActivity("voice")==augment::AudioPatchbay::ActivityState::Active);
 // OBS audio-inactive and stale/unknown meters are NOT measured silence.
 auto inactiveSource=sources[0];
 inactiveSource.audioActive=false;
 REQUIRE(augment::AudioPatchbay::activityForSource(inactiveSource)==
         augment::AudioPatchbay::ActivityState::Unknown);
 inactiveSource.audioActive=true;
 inactiveSource.signalKnown=false;
 REQUIRE(augment::AudioPatchbay::activityForSource(inactiveSource)==
         augment::AudioPatchbay::ActivityState::Unknown);
 REQUIRE(bay.sourceActivity("browser")==augment::AudioPatchbay::ActivityState::Silent);
 REQUIRE(bay.outputActivity(1)==augment::AudioPatchbay::ActivityState::Active);
 REQUIRE(bay.outputActivity(6)==augment::AudioPatchbay::ActivityState::Unknown);
 REQUIRE(!bay.sourceSignal("browser"));
 REQUIRE(bay.outputSignal(1));
 REQUIRE(!bay.outputSignal(6));
 REQUIRE(bay.outputSignal(7)); // monitoring signal
 const auto activeBorder=canvas->grab().toImage();
 sources[0].muted=true;
 bay.refresh();
 bay.refreshLevels();
 app.processEvents();
 const auto mutedBorder=canvas->grab().toImage();
 int mutedSourceOutline=0,mutedOutputOutline=0,mutedFormerDots=0;
 for(int y=38;y<=48;++y){
     // Muted Voice stays selected: its single selection outline is stable.
     for(int x=5;x<=9;++x)
         if(activeBorder.pixel(x,y)!=mutedBorder.pixel(x,y))++mutedSourceOutline;
     for(int x=canvas->width()-41;x<=canvas->width()-37;++x)
         if(activeBorder.pixel(x,y)!=mutedBorder.pixel(x,y))++mutedOutputOutline;
     for(int x=115;x<=124;++x)
         if(activeBorder.pixel(x,y)!=mutedBorder.pixel(x,y))++mutedFormerDots;
     for(int x=canvas->width()-12;x<=canvas->width()-4;++x)
         if(activeBorder.pixel(x,y)!=mutedBorder.pixel(x,y))++mutedFormerDots;
 }
 REQUIRE(mutedSourceOutline==0&&mutedOutputOutline>0);
 REQUIRE(mutedFormerDots==0);
 REQUIRE(!bay.outputSignal(1)); // browser is silent, voice is muted
 REQUIRE(bay.outputSignal(7)); // Music is independently monitored and producing signal
 REQUIRE(!bay.sourceSignal("voice"));
 REQUIRE(bay.sourceActivity("voice")==augment::AudioPatchbay::ActivityState::Muted);
 REQUIRE(bay.outputActivity(1)==augment::AudioPatchbay::ActivityState::Silent);
 // A source flagged active by OBS is not automatically an audible signal.
 levels.clear();bay.refreshLevels();
 REQUIRE(!bay.sourceSignal("music"));
 REQUIRE(bay.outputActivity(1)==augment::AudioPatchbay::ActivityState::Unknown);
 // Muted + unmeasured is unknown, never a false all-muted claim.
 REQUIRE(bay.sourceActivity("music")==augment::AudioPatchbay::ActivityState::Unknown);
 // Hot-reload and source UUID changes don't redirect routing.
 sources.removeFirst();bay.refresh();
 REQUIRE(bay.selectedId()=="browser");
 REQUIRE(!bay.selectSource("voice"));
 REQUIRE(!bay.connectRoute("voice",2));
 REQUIRE(trackWrites==1);
 // Independent device destination nodes use stable device IDs, not track bits.
 int sends=0;QString sentSource,sentDevice;bool enabled=false;
 bay.setDeviceBackend([]{return QVector<augment::DeviceDestination>{{"sink-A","Test speakers"}};},
     [&](const QString &id,const QString &device,bool connect){
         ++sends;sentSource=id;sentDevice=device;enabled=connect;
         for(auto &src:sources)if(src.id==id){
             src.deviceSends.clear();if(connect)src.deviceSends.append({device,true,true,75});return true;
         }return false;
     });
 bay.refresh();app.processEvents();
 REQUIRE(bay.deviceDestinationCount()==1);
 REQUIRE(bay.selectSource("browser"));
 const int cablesBeforeDevice=bay.visibleCableCount();
 REQUIRE(bay.connectDevice("browser","sink-A"));
 REQUIRE(sends==1&&sentSource=="browser"&&sentDevice=="sink-A"&&enabled);
 REQUIRE(bay.visibleCableCount()==cablesBeforeDevice+1);
 REQUIRE(bay.connectDevice("browser","sink-A")&&sends==1);
 int parameterWrites=0;
 bay.setDeviceParameterWriter([&](const QString &id,const QString &device,augment::DeviceParameter param,double value){
     if(id!="browser"||device!="sink-A"||param!=augment::DeviceParameter::Gain)return false;
     ++parameterWrites;for(auto &src:sources)if(src.id==id)src.deviceSends[0].volume=value;
     return true;
 });
 QMenu sendMenu;
 REQUIRE(bay.populateDeviceSendMenu(&sendMenu,"browser","sink-A"));
 auto *sendGain=sendMenu.findChild<QSpinBox*>("aftermarket-send-gain");
 REQUIRE(sendGain&&sendGain->value()==75);
 sendGain->setValue(25);
 REQUIRE(parameterWrites==1&&sources[0].deviceSends[0].volume==25);
 REQUIRE(trackWrites==1);
 QTemporaryDir visibilityDir;
 REQUIRE(visibilityDir.isValid());
 bay.setDeviceVisibilityFile(visibilityDir.filePath("devices.json"));
 QMenu deviceMenu;bay.populateRoutingMenu(&deviceMenu);
 auto *visibility=deviceMenu.findChild<QMenu*>("aftermarket-device-visibility");
 REQUIRE(visibility&&visibility->actions().size()==1);
 visibility->actions()[0]->trigger();
 REQUIRE(bay.deviceDestinationCount()==0&&sends==1);
 REQUIRE(sources[0].deviceSends.size()==1&&sources[0].deviceSends[0].enabled);
 REQUIRE(bay.visibleCableCount()==cablesBeforeDevice);
 augment::AudioPatchbay restored([&]{return sources;},{},{});
 restored.setDeviceVisibilityFile(visibilityDir.filePath("devices.json"));
 restored.setDeviceBackend([]{return QVector<augment::DeviceDestination>{{"sink-A","Renamed speakers"}};},{});
 REQUIRE(restored.deviceDestinationCount()==0);
 visibility->actions()[0]->trigger();
 REQUIRE(bay.deviceDestinationCount()==1&&bay.visibleCableCount()==cablesBeforeDevice+1);
 bay.setDeviceParameterWriter([&](const QString &id,const QString &device,augment::DeviceParameter param,double value){
     if(id!="browser"||device!="sink-A"||param!=augment::DeviceParameter::Muted)return false;
     sources[0].deviceSends[0].muted=value!=0;return true;
 });
 QMenu muteMenu;REQUIRE(bay.populateDeviceSendMenu(&muteMenu,"browser","sink-A"));
 auto *mute=muteMenu.findChild<QAction*>("aftermarket-send-muted");
 REQUIRE(mute&&mute->isCheckable()&&!mute->isChecked());
 mute->trigger();
 REQUIRE(sources[0].deviceSends[0].muted&&sources[0].deviceSends[0].volume==25);
 REQUIRE(bay.visibleCableCount()==cablesBeforeDevice+1);
 mute->trigger();
 REQUIRE(!sources[0].deviceSends[0].muted&&sends==1);

 REQUIRE(bay.toggleDevice("sink-A")&&sends==2&&!enabled);
 REQUIRE(bay.visibleCableCount()==cablesBeforeDevice);
 REQUIRE(trackWrites==1);
 sources.clear();bay.refresh();
 REQUIRE(bay.routeCount()==0&&bay.selectedId().isEmpty());
 REQUIRE(!menu->isEnabled());
 // OBS33 has a binary monitor toggle. No unsupported mode or hidden
 // change to output tracks may leak through the UI.
 QVector<augment::RouteSource> modern={{"modern","OBS33 source",0x15,0}};
 int modernWrites=0;
 augment::AudioPatchbay modernBay([&]{return modern;},{},
   [&](const QString &id,int mode){
     if(id!="modern")return false;
     ++modernWrites;modern[0].monitor=mode;return true;
   },nullptr,{},{},false);
 modernBay.resize(350,340);modernBay.show();app.processEvents();
 REQUIRE(modernBay.selectSource("modern"));
 auto *modernMenu=modernBay.findChild<QMenu*>("aftermarket-monitor-mode-menu");
 REQUIRE(modernMenu&&modernMenu->actions().size()==2);
 REQUIRE(modernMenu->actions()[0]->data().toInt()==0);
 REQUIRE(modernMenu->actions()[1]->data().toInt()==2);
 REQUIRE(!modernBay.setMonitoring(1)&&modernWrites==0);
 modernMenu->actions()[1]->trigger();
 REQUIRE(modern[0].monitor==2&&modern[0].tracks==0x15&&modernWrites==1);
 REQUIRE(modernBay.toggleMonitorOutput());
 REQUIRE(modern[0].monitor==0&&modern[0].tracks==0x15&&modernWrites==2);
 // A short/narrow dock must scroll rather than flatten the track nodes.
 augment::AudioPatchbay shortBay([]{return QVector<augment::RouteSource>{};},{},{});
 shortBay.resize(200,125);shortBay.show();app.processEvents();
 auto *shortCanvas=shortBay.findChild<QWidget*>("aftermarket-route-canvas");
 auto *shortScroll=shortBay.findChild<QScrollArea*>();
 REQUIRE(shortCanvas&&shortScroll&&shortScroll->verticalScrollBar()->maximum()>0);
 REQUIRE(shortCanvas->height()>=238&&shortCanvas->width()>=240);
 auto contrast=shortCanvas->palette();contrast.setColor(QPalette::Button,QColor("#345678"));shortCanvas->setPalette(contrast);app.processEvents();
 const auto round=shortCanvas->grab().toImage();
 const int cx=shortCanvas->width()-27,cy=43+5*30;
 // Test actual painted edges, not just the intended geometry.
 auto bg=round.pixelColor(cx-20,cy);
 int spanX=0,spanY=0;
 for(int i=-15;i<=15;++i){
  if(round.pixelColor(cx+i,cy)!=bg)++spanX;
  if(round.pixelColor(cx,cy+i)!=bg)++spanY;
 }
 REQUIRE(spanX>=27&&spanY>=27&&qAbs(spanX-spanY)<=1);
 std::cout<<"compact AudioHub graph, headphones route, live dots and native editing verified\n";
 return 0;
}
