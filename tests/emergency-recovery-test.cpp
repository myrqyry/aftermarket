#include "emergency-recovery.h"
#include <QApplication>
#include <QMainWindow>
#include <QMenu>
#include <QFile>
#include <QTemporaryDir>
#include <QKeyEvent>
#include <QShortcut>
#include <QTimer>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(false)

int main(int argc,char **argv){
 QApplication app(argc,argv);app.setStyle("Fusion");
 QTemporaryDir dir;
 REQUIRE(dir.isValid());
 const QString flag=dir.filePath("safe-mode.flag");
 const QString rules=dir.filePath("rules.json");
 {QFile f(rules);REQUIRE(f.open(QIODevice::WriteOnly));REQUIRE(f.write("precious config unchanged")>0);}
 QMainWindow main;main.resize(540,430);main.show();
 QMenu popup("Popups",&main);
 popup.addAction("Normal native action");
 int safeCalls=0,resumeCalls=0;
 bool resumeWorks=true;
 {
  augment::EmergencyRecovery recovery(flag,&main,
    [&]{++safeCalls;},
    [&]{++resumeCalls;return resumeWorks;});
  REQUIRE(!recovery.active());
  // Plain, partial, and auto-repeat keys must not activate recovery.
  QKeyEvent normal(QEvent::KeyPress,Qt::Key_F12,Qt::NoModifier);
  QApplication::sendEvent(&main,&normal);
  QKeyEvent partial(QEvent::KeyPress,Qt::Key_F12,Qt::ControlModifier|Qt::AltModifier);
  QApplication::sendEvent(&main,&partial);
  QKeyEvent repeat(QEvent::KeyPress,Qt::Key_F12,Qt::ControlModifier|Qt::AltModifier|Qt::ShiftModifier,QString(),true);
  QApplication::sendEvent(&popup,&repeat);
  REQUIRE(!recovery.active()&&safeCalls==0);
  // This must also be handled when a Qt popup captures keyboard input.
  popup.popup(QPoint(40,40));app.processEvents();
  QKeyEvent panic(QEvent::KeyPress,Qt::Key_F12,Qt::ControlModifier|Qt::AltModifier|Qt::ShiftModifier);
  REQUIRE(QApplication::sendEvent(&popup,&panic));
  app.processEvents();
  REQUIRE(recovery.active()&&safeCalls==1);
  REQUIRE(QFile::exists(flag));
  REQUIRE(!recovery.engage()&&safeCalls==1);
 }
 // A separate instance must load safe mode from the independent marker.
 {
  augment::EmergencyRecovery recovery(flag,&main,
    [&]{++safeCalls;},
    [&]{++resumeCalls;return resumeWorks;});
  REQUIRE(recovery.active()); // independent of rules.json, survives restart
  REQUIRE(QFile::exists(flag));
  resumeWorks=false;
  REQUIRE(!recovery.resume()&&recovery.active()&&QFile::exists(flag));
  resumeWorks=true;
 }
 REQUIRE(QFile::exists(flag));
 {
  augment::EmergencyRecovery recovery(flag,&main,
    [&]{++safeCalls;},
    [&]{++resumeCalls;return resumeWorks;});
  REQUIRE(recovery.active());
  REQUIRE(recovery.resume());
  REQUIRE(!recovery.active()&&!QFile::exists(flag));
 }
 QFile cfg(rules);REQUIRE(cfg.open(QIODevice::ReadOnly));
 REQUIRE(cfg.readAll()=="precious config unchanged");

 // A directory where the marker file must be written is not writable as a file.
 augment::EmergencyRecovery blocked(dir.filePath("absent-parent/safe-mode.flag"),&main,[&]{++safeCalls;},[&]{return true;});
 REQUIRE(!blocked.engage());
 REQUIRE(!blocked.active());
 std::cout<<"emergency recovery persistence and keyboard verified\n";
 return 0;
}
