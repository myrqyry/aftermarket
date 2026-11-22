#include "dock-faces.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\\n";return 1;}}while(false)

int main(int argc,char **argv) {
 QApplication app(argc,argv);app.setStyle("Fusion");
 QMainWindow main;main.setCentralWidget(new QLabel("preview",&main));
 auto *a=new QDockWidget("Audio Mixer",&main);a->setObjectName("mixerDock");
 auto *native=new QWidget(a);auto *nl=new QVBoxLayout(native);
 auto *slider=new QLineEdit("gain",native);nl->addWidget(slider);
 a->setWidget(native);main.addDockWidget(Qt::LeftDockWidgetArea,a);
 auto *b=new QDockWidget("Scenes",&main);b->setObjectName("scenesDock");
 auto *scene=new QLabel("native scene widget",b);b->setWidget(scene);
 main.tabifyDockWidget(a,b);
 main.resize(920,720);main.show();a->raise();app.processEvents();
 {
  augment::DockFaces faces;
  auto *back=new QLabel("Audio routes");back->setWindowTitle("Routing");
  REQUIRE(faces.attach(a,back));
  auto *unused=new QLabel("Other back");
  REQUIRE(!faces.attach(a,unused));delete unused;
  REQUIRE(faces.front(a)==native);
  REQUIRE(faces.back(a)==back);
  REQUIRE(!faces.backVisible(a));
  REQUIRE(a->widget()!=native&&slider->parentWidget()==native);
  REQUIRE(a->widget()->findChild<QLineEdit*>()==slider);
  auto *flip=a->widget()->findChild<QToolButton*>("aftermarket-face-flip");
  REQUIRE(flip&&flip->isVisible());
  auto *faceLabel=a->widget()->findChild<QLabel*>("aftermarket-dock-label");
  REQUIRE(faceLabel&&faceLabel->text()=="Front");
  auto *knobs=new QToolButton();
  knobs->setText("Knobs");
  REQUIRE(faces.addFrontAccessory(a,knobs));
  REQUIRE(knobs->parentWidget()&&knobs->isVisible());
  auto *orphan=new QLabel("unrelated");
  REQUIRE(!faces.addFrontAccessory(b,orphan));delete orphan;
  REQUIRE(faceLabel->font().weight()==QFont::Medium);
  REQUIRE(faceLabel->font().family()==a->font().family());
  flip->click();app.processEvents();
  REQUIRE(faceLabel->text()=="Routing");
  REQUIRE(faces.backVisible(a)&&back->isVisible());
  REQUIRE(!knobs->isVisible());
  REQUIRE(!native->isVisible());
  flip->click();app.processEvents();
  REQUIRE(!faces.backVisible(a)&&native->isVisible());
  REQUIRE(knobs->isVisible());
  auto *next=a->widget()->findChild<QToolButton*>("aftermarket-tab-next");
  REQUIRE(next&&next->isVisible());
  next->click();app.processEvents();
  REQUIRE(scene->isVisible());
  a->raise();app.processEvents();
  REQUIRE(faces.detach(a));app.processEvents();
  REQUIRE(a->widget()==native&&slider->text()=="gain");
  REQUIRE(!a->findChild<QToolButton*>("aftermarket-face-flip"));
  REQUIRE(faces.attach(a,new QLabel("Other view")));
  faces.showBack(a);app.processEvents();
  // A queued layout update must not touch header controls after detach.
  main.removeDockWidget(a);
  main.addDockWidget(Qt::LeftDockWidgetArea,a);
  REQUIRE(faces.detach(a));app.processEvents();
  REQUIRE(a->widget()==native);
  REQUIRE(faces.attach(a,new QLabel("Other view again")));
  faces.showBack(a);app.processEvents();
 } // destructor must restore native widget even while back active.
 app.processEvents();
 REQUIRE(a->widget()==native&&slider->parentWidget()==native);
 REQUIRE(slider->text()=="gain");
 // Native dock can be closed while the host exists without destroying its content.
 {
  augment::DockFaces faces;
  REQUIRE(faces.attach(a,new QLabel("Back")));
  a->close();app.processEvents();
  REQUIRE(faces.detach(a));
  REQUIRE(a->widget()==native);
  a->show();app.processEvents();
 }
 // Refuse non-owned/no-native widget dock.
 auto *empty=new QDockWidget("Empty",&main);
 main.addDockWidget(Qt::RightDockWidgetArea,empty);
 {augment::DockFaces faces; auto *bad=new QLabel("bad");REQUIRE(!faces.attach(empty,bad));delete bad;}
 std::cout<<"dock faces verified\n";
 return 0;
}
