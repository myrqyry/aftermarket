#include "augment.h"
#include "gradient.h"
#include "audio-motion.h"
#include <QApplication>
#include <QSlider>
#include <QTemporaryDir>
#include <QFile>
#include <QElapsedTimer>
#include <QThread>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QImage>
#include <cmath>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(false)
class VolumeMeter final:public QWidget {
 Q_OBJECT
 Q_PROPERTY(QColor foregroundNominalColor READ nominal WRITE setNominal)
public:
 QColor color="#31748f";
 QColor nominal() const {return color;}
 void setNominal(QColor c){color=c;update();}
};
class VolumeControl final:public QWidget {Q_OBJECT
public: using QWidget::QWidget;
};
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setStyle("Fusion");
 REQUIRE(augment::audioLevelFromDb(-100)==0);
 REQUIRE(augment::audioLevelFromDb(-6)==1);
 REQUIRE(augment::audioLevelFromDb(-30)>0.4f);
 REQUIRE(augment::audioLevelFromDb(NAN)==0);
 const float attack=augment::smoothAudio(0,1,0.033);
 REQUIRE(attack>0&&attack<1);
 REQUIRE(augment::smoothAudio(1,0,0.033)>1-attack);
 QTemporaryDir tmp;auto path=tmp.filePath("rules.json");QFile file(path);
 REQUIRE(file.open(QIODevice::WriteOnly));file.write(R"({"enabled":true,"motion":true,"rules":[]})");file.close();
 augment::Controller controller(path);
 VolumeControl a,b;
 QSlider sa(Qt::Horizontal,&a),sb(Qt::Horizontal,&b);
 VolumeMeter ma,mb;ma.setParent(&a);mb.setParent(&b);
 for(auto *s:{&sa,&sb}){s->setRange(0,100);s->setValue(70);s->setGeometry(0,0,220,26);}
 for(auto *w:{&ma,&mb})w->setGeometry(0,30,220,24);
 a.resize(220,80);b.resize(220,80);a.show();b.show();ma.show();mb.show();
 float levelA=1,levelB=0;
 controller.setAudioProvider([&](const QWidget *w)->std::optional<float>{
  if(w->parentWidget()==&a)return levelA;
  if(w->parentWidget()==&b)return levelB;
  return {};
 });
 auto wait=[&](int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){app.processEvents();QThread::msleep(4);}app.processEvents();};
 QJsonObject spec{{"enabled",true},{"audioReactive",true},{"meters",true},{"period",1000},{"colors",QJsonArray{"#ff00ff","#0044ff"}}};
 REQUIRE(controller.setGradientSettings(spec));wait(310);
 REQUIRE(ma.nominal()!=QColor("#31748f")&&mb.nominal()==QColor("#31748f"));
 const auto a1=sa.grab().toImage(),b1=sb.grab().toImage();wait(155);
 REQUIRE(a1!=sa.grab().toImage());REQUIRE(b1==sb.grab().toImage());
 REQUIRE(sa.value()==70&&sb.value()==70);
 levelA=0;levelB=1;wait(1100);
 REQUIRE(ma.nominal()==QColor("#31748f")&&mb.nominal()!=QColor("#31748f"));
 // Preserve an externally changed theme color rather than restoring a stale one.
 mb.setNominal(QColor("#647a31"));wait(80);
 REQUIRE(controller.setMotionEnabled(false));wait(40);
 REQUIRE(mb.nominal()==QColor("#647a31"));
 REQUIRE(!controller.setGradientSettings(QJsonObject{{"enabled",true},{"meters","bad"}}));
 std::cout<<"per-source motion, theme base recovery, silence and native values passed\n";
}
#include "audio-effect-test.moc"
