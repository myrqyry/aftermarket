#include "transform-visual.h"
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QWidget>
#include <iostream>
#define REQUIRE(x) do { if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\n";return 1;} } while(false)
int main(int argc,char **argv){
 QApplication app(argc,argv);
 QWidget parent;
 parent.resize(400,300);parent.show();app.processEvents();
 augment::TransformVisual visual(&parent);
 REQUIRE(visual.testAttribute(Qt::WA_TransparentForMouseEvents));
 REQUIRE(visual.testAttribute(Qt::WA_NoSystemBackground));
 visual.setGeometry(parent.rect());
 auto quad=augment::previewQuad({100,50,200,100}, {400,300}, {400,300});
 REQUIRE(quad.size()==4);
 REQUIRE(augment::matchesFittedPreview( QSize(1280,720), QSize(636,88), 9));
 REQUIRE(!augment::matchesFittedPreview(QSize(1280,720),QSize(636,88),12));
 REQUIRE(!augment::matchesFittedPreview(QSize(1280,720),QSize(636,88),6));
 REQUIRE(!augment::matchesFittedPreview(QSize(1280,720),QSize(636,88),-1));
 REQUIRE(!augment::matchesFittedPreview(QSize(0,720),QSize(636,88),9));
 REQUIRE(qAbs(quad.at(0).x()-100)<0.1);
 REQUIRE(qAbs(quad.at(2).x()-300)<0.1);
 visual.show();app.processEvents();
 QImage baseline=visual.grab().toImage();
 visual.setShapes({quad});app.processEvents();
 QImage shot=visual.grab().toImage();
 REQUIRE(shot.pixelColor(100,50)!=baseline.pixelColor(100,50));
 REQUIRE(shot.pixelColor(200,100)==baseline.pixelColor(200,100));
 int nonSquare=0;
 for(int y=42;y<=58;++y)for(int x=92;x<=108;++x){
   if(!shot.rect().contains(x,y))continue;
   if(shot.pixelColor(x,y).alpha()>100 && (x-100)*(x-100)+(y-50)*(y-50)<80)++nonSquare;
 }
 REQUIRE(nonSquare>35);
 visual.setShapes({});app.processEvents();
 REQUIRE(visual.grab().toImage()==baseline);
 visual.hide();visual.useTransientSurface();
 REQUIRE(visual.windowFlags().testFlag(Qt::WindowTransparentForInput));
 REQUIRE(visual.windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus));
 REQUIRE(visual.testAttribute(Qt::WA_TransparentForMouseEvents));
 std::cout<<"translucent, mouse transparent, themed transform adornments and geometry verified\n";
}
