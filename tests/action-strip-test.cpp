#include "action-strip.h"
#include "menu-settings-corner.h"
#include "menu-bar-compact.h"
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QLabel>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QToolButton>
#include <QDockWidget>
#include <QAction>
#include <QMimeData>
#include <QDropEvent>
#include <iostream>
#define REQUIRE(c) do{if(!(c)){std::cerr<<"FAILED "<<__LINE__<<" "<<#c<<"\n";return 1;}}while(false)
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setStyle("Fusion");
 QTemporaryDir temp;REQUIRE(temp.isValid());
 const QString path=temp.filePath("action-strip.json");
 QMainWindow win;win.resize(880,510);
 auto *file=win.menuBar()->addMenu("&File");
 auto *corner=new QLabel("Original corner",win.menuBar());
 win.menuBar()->setCornerWidget(corner);
 auto *mixer=new QDockWidget("Audio Mixer",&win);
 mixer->setObjectName("mixerDock");
 mixer->setWidget(new QLabel("original audio"));
 win.addDockWidget(Qt::BottomDockWidgetArea,mixer);
 win.show();app.processEvents();
 int hotkeyCalls=0,queries=0,hotkeyQueries=0;
 augment::StatusSnapshot stats;stats.recording=true;stats.streaming=false;stats.fps=59.94;stats.dropped=4;
 stats.cpu=12.6;stats.recordSeconds=3724;stats.streamSeconds=-1;stats.lagged=2;
 augment::ActionStrip strip(path,[&]{++queries;return stats;},
     [&]{++hotkeyQueries;return QList<augment::HotkeyChoice>{{"hotkey:v1:source-a:mute","Mute mic","Sources"}};},
     [&](const QString &binding){if(binding=="hotkey:v1:source-a:mute"){++hotkeyCalls;return true;}return false;});
 REQUIRE(!strip.enabled());
 REQUIRE(strip.items().isEmpty());
 REQUIRE(strip.addItem({"rec","status","recording",""}));
 REQUIRE(strip.addItem({"fps","status","fps",""}));
 REQUIRE(strip.addItem({"mute","hotkey","hotkey:v1:source-a:mute","Mute mic"}));
 REQUIRE(strip.addItem({"dock","dock","mixerDock",""}));
 REQUIRE(strip.addItem({"space","spacer","",""}));
 REQUIRE(!strip.addItem({"fps","status","fps",""})); // duplicate IDs rejected
 REQUIRE(!strip.addItem({"bad","status","bogus",""}));
 REQUIRE(strip.items().size()==5);
 REQUIRE(strip.setEnabled(win.menuBar(),true));
 app.processEvents();
 REQUIRE(strip.enabled());
 REQUIRE(win.menuBar()->cornerWidget()!=corner);
 REQUIRE(strip.widget()!=nullptr);
 REQUIRE(file->menuAction()==win.menuBar()->actions().at(0));
 REQUIRE(strip.widget()->findChild<QToolButton*>("aftermarket-strip-mute"));
 auto *mixerButton=strip.widget()->findChild<QToolButton*>("aftermarket-strip-dock");
 REQUIRE(mixerButton&&mixerButton->isCheckable());
 strip.refresh();
 // Empty custom labels must use the native dock name, never its technical
 // objectName (the user's Controls button currently says controlsDock).
 REQUIRE(mixerButton->text()=="Audio Mixer");
 REQUIRE(mixerButton->toolTip()=="Hide Audio Mixer");
 REQUIRE(mixerButton->accessibleName()=="Hide Audio Mixer");
 REQUIRE(mixerButton->isChecked());
 REQUIRE(strip.trigger("dock"));
 REQUIRE(!mixerButton->isChecked());
 REQUIRE(mixerButton->text()=="Audio Mixer");
 REQUIRE(mixerButton->toolTip()=="Show Audio Mixer");
 REQUIRE(mixerButton->accessibleName()=="Show Audio Mixer");
 REQUIRE(strip.trigger("dock"));
 REQUIRE(mixerButton->isChecked());
 // OBS/Aftermarket may shorten the visible dock title for a narrow title
 // bar. The action-strip label must use the full native Docks-menu text.
 mixer->setWindowTitle("  Aud…");
 mixer->toggleViewAction()->setText("Audio Mixer");
 strip.refresh();
 REQUIRE(mixerButton->text()=="Audio Mixer");
 // Native rename changes the default label without rewriting the saved
 // dock target. A user-defined label always takes precedence.
 mixer->setWindowTitle("  Mus…");
 mixer->toggleViewAction()->setText("Music Mixer");
 strip.refresh();
 REQUIRE(mixerButton->text()=="Music Mixer");
 REQUIRE(mixerButton->toolTip()=="Hide Music Mixer");
 REQUIRE(strip.replaceItem("dock",{"dock","dock","mixerDock","Custom mixer"}));
 mixerButton=strip.widget()->findChild<QToolButton*>("aftermarket-strip-dock");
 REQUIRE(mixerButton&&mixerButton->text()=="Custom mixer");
 REQUIRE(mixerButton->toolTip()=="Hide Custom mixer");
 REQUIRE(strip.replaceItem("dock",{"dock","dock","mixerDock",""}));
 mixerButton=strip.widget()->findChild<QToolButton*>("aftermarket-strip-dock");
 REQUIRE(mixerButton&&mixerButton->text()=="Music Mixer");
 REQUIRE(strip.items().at(3).label.isEmpty());
 REQUIRE(strip.itemText("rec").contains("REC"));
 REQUIRE(strip.itemText("fps").contains("59.9"));
 REQUIRE(queries>0);
 const int enumerations=hotkeyQueries;
 strip.refresh();strip.refresh();strip.refresh();
 REQUIRE(hotkeyQueries==enumerations); // cached, not enumerated at status rate
 // Drag/drop goes through the live Qt accessory, not just model manipulation.
 QMimeData payload;
 payload.setData("application/x-aftermarket-action-strip","rec");
 auto *lastButton=strip.widget()->findChild<QToolButton*>("aftermarket-strip-dock");
 REQUIRE(lastButton);
 const QPoint dropPosition=lastButton->mapTo(strip.widget(),QPoint(lastButton->width()+3,lastButton->height()/2));
 QDragEnterEvent enter(dropPosition,Qt::MoveAction,&payload,Qt::LeftButton,Qt::NoModifier);
 QApplication::sendEvent(strip.widget(),&enter);
 QDropEvent drop(QPointF(dropPosition),Qt::MoveAction,&payload,Qt::LeftButton,Qt::NoModifier);
 QApplication::sendEvent(strip.widget(),&drop);
 app.processEvents();
 if(strip.items().at(3).id!="rec"){
   std::cerr<<"drag enter "<<enter.isAccepted()<<" drop "<<drop.isAccepted()
            <<" x "<<dropPosition.x()<<" width "<<strip.widget()->width()<<" order:";
   for(const auto &entry:strip.items())std::cerr<<" "<<entry.id.toStdString();
   std::cerr<<"\n";
 }
 REQUIRE(strip.items().at(3).id=="rec");
 REQUIRE(strip.trigger("mute")&&hotkeyCalls==1);
 REQUIRE(!strip.trigger("fps"));
 // A missing target is shown but cannot accidentally execute another action.
 REQUIRE(strip.replaceItem("mute",{"mute","hotkey","hotkey:v1:missing","Missing"}));
 REQUIRE(!strip.trigger("mute")&&hotkeyCalls==1);
 REQUIRE(strip.moveItem("mute",0));
 REQUIRE(strip.items().first().id=="mute");
 REQUIRE(strip.removeItem("space"));
 REQUIRE(strip.setEnabled(win.menuBar(),false));
 app.processEvents();
 REQUIRE(win.menuBar()->cornerWidget()==corner);
 REQUIRE(corner->parentWidget()==win.menuBar());
 REQUIRE(win.menuBar()->actions().first()==file->menuAction());
 REQUIRE(strip.save());
 QFile oldFile(path);REQUIRE(oldFile.open(QIODevice::ReadOnly));
 const QByteArray before=oldFile.readAll();oldFile.close();
 REQUIRE(!before.isEmpty());
 {
     augment::ActionStrip restored(path);
     REQUIRE(restored.load());
     REQUIRE(!restored.enabled());
     REQUIRE(restored.items()==strip.items());
     REQUIRE(!restored.setEnabled(win.menuBar(),false));
 }
 REQUIRE(!strip.addItem({"evil","dock","","No target"}));
 REQUIRE(!strip.replaceItem("dock",{"dock","status","bogus",""}));
 // Corrupt files are rejected without dropping the current model.
 {QFile f(path);REQUIRE(f.open(QIODevice::WriteOnly));f.write("{invalid");}
 REQUIRE(!strip.load()&&strip.items().size()==4);
 REQUIRE(!strip.save()==false);
 REQUIRE(strip.setEnabled(win.menuBar(),true));
 REQUIRE(strip.removeItem("dock"));
 REQUIRE(strip.setEnabled(win.menuBar(),false));
 REQUIRE(win.menuBar()->cornerWidget()==corner);
 // A plugin teardown (including emergency mode) preserves desired visibility.
 const QString safePath=temp.filePath("safe-strip.json");
 QMainWindow spare;spare.menuBar()->addMenu("File");
 {
   augment::ActionStrip session(safePath);
   REQUIRE(session.addItem({"state","status","recording",""}));
   REQUIRE(session.setEnabled(spare.menuBar(),true));
 }
 REQUIRE(spare.menuBar()->cornerWidget()==nullptr);
 {
   augment::ActionStrip restored(safePath);
   REQUIRE(restored.desiredEnabled()&&!restored.enabled());
   REQUIRE(restored.setEnabled(spare.menuBar(),true));
   REQUIRE(restored.setEnabled(spare.menuBar(),false));
 }
 augment::ActionStrip explicitlyDisabled(safePath);
 REQUIRE(!explicitlyDisabled.desiredEnabled());
 // Reattach with a fresh adapter after emergency-mode cleanup.
 const QString resumePath=temp.filePath("resume-strip.json");
 QMainWindow resumeWindow;resumeWindow.menuBar()->addMenu("File");
 resumeWindow.show();app.processEvents();
 {
   augment::MenuBarCompact compact;
   REQUIRE(compact.setEnabled(resumeWindow.menuBar(),true));
   augment::ActionStrip session(resumePath);
   REQUIRE(session.addItem({"rec","status","recording",""}));
   REQUIRE(session.setEnabled(resumeWindow.menuBar(),true));
   app.processEvents();
   REQUIRE(session.widget()->isVisible());
 }
 {
   augment::MenuBarCompact compact;
   REQUIRE(compact.setEnabled(resumeWindow.menuBar(),true));
   augment::ActionStrip resumed(resumePath);
   REQUIRE(resumed.desiredEnabled());
   REQUIRE(resumed.setEnabled(resumeWindow.menuBar(),true));
   app.processEvents();
   REQUIRE(resumed.widget()->isVisible());
   REQUIRE(resumeWindow.menuBar()->cornerWidget()->isVisible());
 }
 // Shared OBS metric formats, including honest unknown elapsed times.
 QMainWindow statusMain;statusMain.menuBar()->addMenu("File");
 statusMain.show();app.processEvents();
 augment::ActionStrip metrics(temp.filePath("metrics.json"),[&]{return stats;});
 REQUIRE(metrics.addItem({"cpu","status","cpu",""}));
 REQUIRE(metrics.addItem({"clock","status","record-time",""}));
 REQUIRE(metrics.addItem({"live-clock","status","stream-time",""}));
 REQUIRE(metrics.addItem({"lagged","status","lagged",""}));
 REQUIRE(metrics.setEnabled(statusMain.menuBar(),true));
 metrics.refresh();
 REQUIRE(metrics.itemText("cpu").contains("12.6"));
 REQUIRE(metrics.itemText("clock").contains("01:02:04"));
 REQUIRE(metrics.itemText("live-clock").contains("--:--"));
 REQUIRE(metrics.itemText("lagged").contains("2"));
 REQUIRE(metrics.setEnabled(statusMain.menuBar(),false));
 // A newly added button must not be clipped by QMenuBar's old corner size.
 // In genuinely narrow windows it belongs in a real, actionable overflow menu.
 QMainWindow narrow;
 narrow.menuBar()->addMenu("File");
 narrow.menuBar()->addMenu("Edit");
 narrow.menuBar()->addMenu("Tools");
 narrow.resize(420,360);narrow.show();app.processEvents();
 auto *controlsDock=new QDockWidget("Controls",&narrow);
 controlsDock->setObjectName("controlsDock");
 controlsDock->setWidget(new QLabel("Native Controls"));
 narrow.addDockWidget(Qt::RightDockWidgetArea,controlsDock);
 app.processEvents();
 augment::ActionStrip constrained(temp.filePath("overflow.json"),
     [&]{return stats;},
     []{return QList<augment::HotkeyChoice>{{"hotkey:v1:source-a:mute","Mute mic","Sources"}};},
     [&](const QString &binding){if(binding=="hotkey:v1:source-a:mute"){++hotkeyCalls;return true;}return false;});
 for(const QString &metric:{"recording","streaming","fps","dropped","stream-bitrate","record-bitrate"})
     REQUIRE(constrained.addItem({"status-"+metric,"status",metric,{}}));
 REQUIRE(constrained.setEnabled(narrow.menuBar(),true));
 REQUIRE(constrained.addItem({"control","hotkey","hotkey:v1:source-a:mute","Mute mic"}));
 REQUIRE(constrained.addItem({"dock-controls","dock","controlsDock",""}));
 app.processEvents();
 auto *overflow=constrained.widget()->findChild<QToolButton*>("aftermarket-strip-overflow");
 REQUIRE(overflow&&overflow->isVisible()&&overflow->menu()&&!overflow->menu()->actions().isEmpty());
 REQUIRE(constrained.widget()->parentWidget()->geometry().right()<=narrow.menuBar()->width());
 QAction *overflowHotkey=nullptr;
 for(auto *candidate:overflow->menu()->actions())
     if(candidate->text()=="Mute mic")overflowHotkey=candidate;
 REQUIRE(overflowHotkey&&overflowHotkey->isEnabled());
 // Overflow has the same readable native name and checkable Show/Hide
 // semantics as the corresponding visible dock button.
 QAction *overflowDock=nullptr;
 for(auto *candidate:overflow->menu()->actions())
     if(candidate->data().toString()=="dock-controls")overflowDock=candidate;
 REQUIRE(overflowDock&&overflowDock->text()=="Controls");
 REQUIRE(overflowDock->isEnabled()&&overflowDock->isCheckable()&&overflowDock->isChecked());
 REQUIRE(overflowDock->toolTip()=="Hide Controls");
 overflowDock->trigger();app.processEvents();
 REQUIRE(!controlsDock->isVisible());
 REQUIRE(overflowDock->text()=="Controls"&&!overflowDock->isChecked());
 REQUIRE(overflowDock->toolTip()=="Show Controls");
 overflowDock->trigger();app.processEvents();
 REQUIRE(controlsDock->isVisible()&&overflowDock->isChecked());
 const int beforeOverflowHotkey=hotkeyCalls;
 overflowHotkey->trigger();
 REQUIRE(hotkeyCalls==beforeOverflowHotkey+1);

 narrow.resize(1120,360);app.processEvents();
 REQUIRE(!overflow->isVisible());
 auto *control=constrained.widget()->findChild<QToolButton*>("aftermarket-strip-control");
 REQUIRE(control&&control->isVisible());
 const QPoint farEdge=control->mapTo(narrow.menuBar(),QPoint(control->width(),0));
 REQUIRE(farEdge.x()<=narrow.menuBar()->width());
 // The Settings shortcut stays outside the editable strip and
 // survives both strip toggles and compact menu transitions.
 QMainWindow gearWindow;auto *bar=gearWindow.menuBar();
 auto *nativeFile=bar->addMenu("File");
 auto *settings=nativeFile->addAction("Settings");settings->setObjectName("action_Settings");
 auto *nativeCorner=new QLabel("Native",bar);bar->setCornerWidget(nativeCorner);
 gearWindow.resize(650,320);gearWindow.show();app.processEvents();
 int opens=0;QObject::connect(settings,&QAction::triggered,[&]{++opens;});
 {
  augment::MenuSettingsCorner gear(bar,settings);
  app.processEvents();
  auto *button=bar->findChild<QToolButton*>("aftermarket-menu-settings");
  REQUIRE(button&&button->isVisible()&&button->defaultAction()==settings);
  REQUIRE(button->mapTo(bar,QPoint(button->width(),0)).x()>=bar->width()-5);
  button->click();REQUIRE(opens==1);
  settings->setEnabled(false);REQUIRE(!button->isEnabled());button->click();REQUIRE(opens==1);
  settings->setEnabled(true);
  augment::ActionStrip accessories(temp.filePath("gear-strip.json"));
  for(int i=0;i<6;++i)REQUIRE(accessories.addItem({QString::number(i),"status","recording",""}));
  REQUIRE(accessories.setEnabled(bar,true));app.processEvents();
  REQUIRE(button->isVisible()&&button->mapTo(bar,QPoint(button->width(),0)).x()>=bar->width()-5);
  REQUIRE(accessories.widget()->mapTo(bar,QPoint(accessories.widget()->width(),0)).x()<=button->mapTo(bar,QPoint()).x());
  gearWindow.resize(390,320);app.processEvents();
  REQUIRE(button->isVisible()&&button->mapTo(bar,QPoint(button->width(),0)).x()<=bar->width());
  augment::MenuBarCompact compact;REQUIRE(compact.setEnabled(bar,true));app.processEvents();
  button->click();REQUIRE(opens==2);
  REQUIRE(accessories.setEnabled(bar,false));app.processEvents();
  REQUIRE(button->isVisible()&&button->mapTo(bar,QPoint(button->width(),0)).x()>=bar->width()-5);
 }
 REQUIRE(bar->cornerWidget()==nativeCorner&&nativeCorner->parentWidget()==bar);
 REQUIRE(!bar->findChild<QToolButton*>("aftermarket-menu-settings"));

 std::cout<<"action strip persistence, routing, recovery and status verified\n";
 return 0;
}
