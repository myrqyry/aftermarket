#include "augment.h"
#include <QApplication>
#include <QSlider>
#include <QAbstractButton>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QHBoxLayout>
#include <QElapsedTimer>
#include <QThread>
#include <QImage>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(false)
class VolumeControl:public QWidget {
 Q_OBJECT
public: using QWidget::QWidget;
};
class VolumeMeter:public QWidget {
 Q_OBJECT
 Q_PROPERTY(QColor foregroundNominalColor READ nominal)
 Q_PROPERTY(QColor foregroundWarningColor READ warning)
 Q_PROPERTY(QColor foregroundErrorColor READ error)
public:
 using QWidget::QWidget;
 QColor nominal()const{return QColor("#00ff00");}
 QColor warning()const{return QColor("#ffff00");}
 QColor error()const{return QColor("#ff0000");}
};
int main(int argc,char **argv) {
 QApplication app(argc,argv);app.setStyle("Fusion");
 QTemporaryDir tmp;auto path=tmp.filePath("rules.json");QFile f(path);REQUIRE(f.open(QIODevice::WriteOnly));f.write(R"({"enabled":true,"rules":[]})");f.close();
 augment::Controller c(path);
 VolumeControl strip;
 auto *frame=new QWidget(&strip);frame->setObjectName("volMeterFrame");
 auto *box=new QHBoxLayout(frame);
 auto *s=new QSlider(Qt::Vertical,frame);s->setValue(40);
 auto *m=new VolumeMeter(frame);m->setMinimumWidth(22);
 box->addWidget(s);box->addWidget(m);
 auto *outer=new QHBoxLayout(&strip);outer->addWidget(frame);
 strip.resize(180,250);strip.show();app.processEvents();
 auto initialMax=s->maximumSize(),initialMin=s->minimumSize();
 std::optional<augment::MeterReading> reading=augment::MeterReading{-30,false};
 c.setMeterProvider([&](const QWidget*){return reading;});
 auto wait=[&](int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){app.processEvents();QThread::msleep(4);}app.processEvents();};
 REQUIRE(c.setGradientSettings({{"knobs",true}}));wait(100);
 REQUIRE(m->isHidden()&&s->property("_obsUiAugmentIntegratedMeter").toBool());
 REQUIRE(s->maximumHeight()<=80&&s->value()==40);
 auto low=s->grab().toImage();
 reading=augment::MeterReading{-3,false};wait(120);
 auto high=s->grab().toImage();
 REQUIRE(low!=high&&s->value()==40);
 int red=0;for(int y=0;y<high.height();++y)for(int x=0;x<high.width();++x){auto p=high.pixelColor(x,y);if(p.red()>180&&p.green()<70&&p.blue()<70)++red;}
 REQUIRE(red>5);
 REQUIRE(!c.motionEnabled()); // actual metering does not depend on decorative motion
 reading.reset();wait(100);
 REQUIRE(!m->isHidden()&&!s->property("_obsUiAugmentIntegratedMeter").toBool());
 reading=augment::MeterReading{-12,false};wait(100);REQUIRE(m->isHidden());
 REQUIRE(c.setGradientSettings({{"knobs",true},{"knobMeters",false}}));wait(100);REQUIRE(!m->isHidden());
 REQUIRE(c.setGradientSettings({{"knobs",false}}));wait(60);
 REQUIRE(s->maximumSize()==initialMax&&s->minimumSize()==initialMin&&s->value()==40);
 std::cout<<"Integrated meter, real level independent of volume and motion, warning color, fallback, restore passed\n";
}
#include "knob-meter-test.moc"
