#include "augment.h"
#include "motion.h"
#include "dock-header-button.h"
#include <QApplication>
#include <QMenu>
#include <QTemporaryDir>
#include <QFile>
#include <QElapsedTimer>
#include <QTimer>
#include <QEventLoop>
#include <QThread>
#include <QImage>
#include <QJsonObject>
#include <iostream>
#define REQUIRE(x) do { if (!(x)) { std::cerr << "FAILED line " << __LINE__ << ": " << #x << '\n'; return 1; } } while(false)
int main(int argc,char **argv) {
 QApplication app(argc,argv);
 app.setStyle("Fusion");
 augment::MotionTrack track;
 track.reset(0);
 track.retarget(1, 0, 200);
 const auto mid=track.value(70);
 REQUIRE(mid>0 && mid<1);
 track.retarget(0,70,200);
 REQUIRE(qAbs(track.value(70)-mid)<0.0001);
 REQUIRE(track.value(140)<mid && track.value(300)==0 && !track.running(300));
 augment::MotionFrameBudget budget;
 int updates=0;
 for(int t=1;t<=210;++t)if(budget.advance(t,210))++updates;
 REQUIRE(updates==6);
 augment::MotionFrameBudget skipped;
 REQUIRE(skipped.advance(100,210));
 REQUIRE(!skipped.advance(101,210));
 REQUIRE(skipped.advance(210,210));
 REQUIRE(!skipped.advance(210,210));
 bool allowed=false;
 augment::DockHeaderButton header(augment::DockHeaderGlyph::Knobs);
 header.setMotionProvider([&]{return allowed;});
 header.show();app.processEvents();
 auto *feedback=header.findChild<QTimer*>("aftermarket-feedback-clock");
 REQUIRE(feedback&&!feedback->isActive());
 allowed=true;header.setChecked(true);
 REQUIRE(feedback->isActive());
 QEventLoop feedbackLoop;QTimer::singleShot(200,&feedbackLoop,&QEventLoop::quit);feedbackLoop.exec();
 REQUIRE(!feedback->isActive());
 header.setChecked(false);REQUIRE(feedback->isActive());
 header.hide();REQUIRE(!feedback->isActive());
 header.show();app.processEvents();allowed=false;
 header.setChecked(false);REQUIRE(!feedback->isActive());
 QTemporaryDir temp;
 auto path=temp.filePath("rules.json");
 QFile f(path);REQUIRE(f.open(QIODevice::WriteOnly));
 f.write(R"({"enabled":true,"motion":true,"rules":[{"text":"Filters","icon":"sliders","color":"#ff00ff","animation":{"preset":"pop","duration":400}}]})");f.close();
 augment::Controller controller(path);
 QMenu menu;auto *a=menu.addAction("Filters");bool triggered=false;
 QObject::connect(a,&QAction::triggered,[&]{triggered=true;});
 auto wait=[&](int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){app.processEvents();QThread::msleep(4);}app.processEvents();};
 menu.popup(QPoint(40,40));app.processEvents();
 auto *overlay=menu.findChild<QWidget*>("aftermarket-overlay");
 REQUIRE(overlay && overlay->isVisible());
 auto *clock=overlay->findChild<QTimer*>("augment-motion-clock");
 REQUIRE(clock && clock->isActive());
 auto early=menu.grab().toImage();
 wait(470);
 auto settled=menu.grab().toImage();
 REQUIRE(early!=settled && !clock->isActive());
 menu.setActiveAction(a);wait(150);
 REQUIRE(clock->isActive());
 auto hovered=menu.grab().toImage();
 wait(450);
 REQUIRE(!clock->isActive() && hovered!=settled);
 menu.setActiveAction(nullptr);wait(50);
 menu.hide(); REQUIRE(!clock->isActive() && a->icon().isNull());
 REQUIRE(controller.setMotionEnabled(false));
 REQUIRE(!controller.cachedMotionEnabled());
 menu.popup(QPoint(40,40));app.processEvents();
 REQUIRE(!clock->isActive());
 a->trigger();REQUIRE(triggered);
 menu.hide();
 REQUIRE(controller.setMotionEnabled(true));
 REQUIRE(controller.cachedMotionEnabled());
 augment::MotionSpec spec;
 for(const auto *preset:{"fade","lift"}) {
     REQUIRE(augment::MotionSpec::parse(QJsonObject{{"preset",preset},{"duration",180}},spec));
     REQUIRE(spec.preset==preset&&spec.duration==180);
     REQUIRE(controller.saveRule(QJsonObject{{"text","Filters"},{"icon","sliders"},{"color","#ff00ff"},{"animation",QJsonObject{{"preset",preset},{"duration",180}}}}));
     menu.popup(QPoint(40,40));app.processEvents();
     REQUIRE(clock->isActive());
     const auto beginning=menu.grab().toImage();
     wait(250);REQUIRE(!clock->isActive()&&beginning!=menu.grab().toImage());
     menu.hide();
 }
 menu.popup(QPoint(40,40));app.processEvents();
 REQUIRE(clock->isActive());
 REQUIRE(controller.setMotionEnabled(false));REQUIRE(!clock->isActive());menu.hide();
 REQUIRE(controller.setMotionEnabled(true));
 auto broken=QJsonObject{{"text","Filters"},{"icon","sliders"},{"animation",QJsonObject{{"preset","spin"},{"duration",-1}}}};
 REQUIRE(!controller.saveRule(broken));
 std::cout<<"motion retarget, rendered frames, idle clock, reduced motion, native action passed\n";
}
