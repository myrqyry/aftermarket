#include "augment.h"
#include <QApplication>
#include <QAbstractButton>
#include <QSlider>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QKeyEvent>
#include <QSignalBlocker>
#include <QImage>
#include <QVBoxLayout>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(false)
class VolumeControl:public QWidget {Q_OBJECT
public:using QWidget::QWidget;};
class VolumeName:public QAbstractButton {Q_OBJECT
public:using QAbstractButton::QAbstractButton; void paintEvent(QPaintEvent*) override {}};
int main(int argc,char **argv) {
 QApplication app(argc,argv);app.setStyle("Fusion");
 QTemporaryDir tmp;auto path=tmp.filePath("rules.json");QFile f(path);REQUIRE(f.open(QIODevice::WriteOnly));f.write(R"({"enabled":true,"rules":[]})");f.close();
 augment::Controller c(path);
 VolumeControl a,b;VolumeName nameA(&a),nameB(&b);nameA.setText("Music");nameB.setText("Mic");
 QWidget frame(&a);frame.setObjectName("volMeterFrame");frame.setGeometry(0,0,100,180);
 QSlider s(Qt::Vertical,&frame),other(Qt::Vertical,&b);s.setRange(0,100);s.setValue(40);s.setGeometry(0,0,20,160);other.setGeometry(0,0,20,160);
 a.resize(100,180);b.resize(100,180);a.show();b.show();app.processEvents();
 auto min=s.minimumSize();auto original=s.grab().toImage();
 REQUIRE(c.setGradientSettings({{"knobs",true},{"knobSources",QJsonArray{"Music"}}}));app.processEvents();
 REQUIRE(s.property("_obsUiAugmentKnob").toBool());REQUIRE(!other.property("_obsUiAugmentKnob").toBool());
 REQUIRE(s.minimumWidth()>=40 && s.value()==40);
 auto point=QPointF(s.rect().center());
 auto send=[&](QEvent::Type type,QPointF p,Qt::MouseButton button,Qt::MouseButtons buttons){
  QMouseEvent e(type,p,s.mapToGlobal(p.toPoint()),button,buttons,Qt::NoModifier);
  QApplication::sendEvent(&s,&e);
 };
 int pressed=0,released=0;QObject::connect(&s,&QSlider::sliderPressed,[&]{++pressed;});QObject::connect(&s,&QSlider::sliderReleased,[&]{++released;});
 send(QEvent::MouseButtonPress,point,Qt::LeftButton,Qt::LeftButton);REQUIRE(s.value()==40&&s.isSliderDown());
 send(QEvent::MouseMove,point-QPointF(0,40),Qt::NoButton,Qt::LeftButton);REQUIRE(s.value()>40);
 send(QEvent::MouseButtonRelease,point-QPointF(0,40),Qt::LeftButton,Qt::NoButton);REQUIRE(!s.isSliderDown()&&pressed==1&&released==1);
 auto knob=s.grab().toImage();s.setValue(15);app.processEvents();REQUIRE(knob!=s.grab().toImage());
 auto before=s.grab().toImage();
 {QSignalBlocker block(&s);s.setValue(80);}
 app.processEvents();REQUIRE(before!=s.grab().toImage());
 QKeyEvent key(QEvent::KeyPress,Qt::Key_Right,Qt::NoModifier);
 QApplication::sendEvent(&s,&key);REQUIRE(s.value()>80);
 // Reset goes through the original slider value signal, without starting
 // another drag. A release after double-click must not change the value.
 int edits=0;QObject::connect(&s,&QSlider::valueChanged,[&]{++edits;});
 s.setValue(35);edits=0;
 send(QEvent::MouseButtonDblClick,point,Qt::LeftButton,Qt::LeftButton);
 REQUIRE(s.value()==s.maximum()&&!s.isSliderDown()&&edits==1);
 send(QEvent::MouseButtonRelease,point,Qt::LeftButton,Qt::NoButton);
 REQUIRE(s.value()==s.maximum()&&edits==1);
 s.setValue(35);send(QEvent::MouseButtonPress,point,Qt::MiddleButton,Qt::MiddleButton);
 REQUIRE(s.value()==35&&s.isSliderDown());
 send(QEvent::MouseMove,point-QPointF(0,15),Qt::NoButton,Qt::MiddleButton);
 REQUIRE(s.value()>35);
 send(QEvent::MouseButtonRelease,point,Qt::MiddleButton,Qt::NoButton);
 REQUIRE(!s.isSliderDown());
 // A native mixer context menu retains its actions and adds reset.
 a.setContextMenuPolicy(Qt::CustomContextMenu);
 QMenu nativeMenu(&a);auto *nativeAction=nativeMenu.addAction("Native action");
 QObject::connect(&a,&QWidget::customContextMenuRequested,[&](QPoint p){nativeMenu.popup(a.mapToGlobal(p));});
 s.setValue(30);
 QContextMenuEvent context(QContextMenuEvent::Mouse,point.toPoint(),s.mapToGlobal(point.toPoint()));
 QApplication::sendEvent(&s,&context);
 auto *resetAction=nativeMenu.findChild<QAction*>("aftermarket-reset-gain");
 REQUIRE(resetAction&&nativeMenu.actions().contains(nativeAction));
 resetAction->trigger();nativeMenu.hide();REQUIRE(s.value()==s.maximum());
 s.setDisabled(true);app.processEvents();REQUIRE(s.property("_obsUiAugmentKnob").toBool());
 s.setEnabled(true);s.setValue(40);
 REQUIRE(c.setMotionEnabled(false));
 const auto plainKnob=s.grab().toImage();
 for(const auto &pattern:QStringList{"flow","orbit","nebula","vortex","ripple"}){
    REQUIRE(c.setGradientSettings({{"knobs",true},{"enabled",true},{"pattern",pattern},{"knobSources",QJsonArray{"Music"}},{"colors",QJsonArray{"#ff00ff","#00ffff","#ff8800"}}}));
    app.processEvents();
    REQUIRE(s.grab().toImage()!=plainKnob&&s.value()==40);
    auto before=s.grab().toImage();auto scaled=c.gradientSettings();scaled["scale"]=300;
    REQUIRE(c.setGradientSettings(scaled));app.processEvents();
    REQUIRE(s.grab().toImage()!=before&&s.value()==40);
 }
 REQUIRE(!c.setGradientSettings({{"knobs",true},{"knobSources",QJsonArray{1}}}));
 REQUIRE(c.setGradientSettings({{"knobs",false}}));app.processEvents();
 REQUIRE(!s.property("_obsUiAugmentKnob").toBool()&&s.minimumSize()==min&&s.value()==40);
 std::cout<<"selected knob, native drag/value signals, external update, restore passed\n";
}
#include "knob-test.moc"
