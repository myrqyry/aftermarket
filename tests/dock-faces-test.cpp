#include "dock-faces.h"
#include "dock-header-button.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QStyle>
#include <QStyleOptionDockWidget>
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
 a->setMinimumWidth(250);
 main.resize(920,720);main.show();a->raise();app.processEvents();
 {
  augment::DockFaces faces;
  auto *back=new QLabel("Audio routes");back->setWindowTitle("Routing");
  QStyleOptionDockWidget opt;
  opt.initFrom(a);
  opt.title=a->windowTitle();
  opt.closable=a->features()&QDockWidget::DockWidgetClosable;
  opt.floatable=a->features()&QDockWidget::DockWidgetFloatable;
  opt.movable=a->features()&QDockWidget::DockWidgetMovable;
  auto textRect=a->style()->subElementRect(QStyle::SE_DockWidgetTitleBarText,&opt,a);
  QWidget *originalTitleBar=a->titleBarWidget();
  auto *originalToggleView=a->toggleViewAction();
  const auto originalFeatures=a->features();
  REQUIRE(faces.attach(a,back));
  app.processEvents();
  REQUIRE(!a->findChild<QWidget*>("aftermarket-native-title-actions"));
  REQUIRE(a->titleBarWidget()==originalTitleBar);
  REQUIRE(a->toggleViewAction()==originalToggleView&&a->features()==originalFeatures);
  REQUIRE(!a->widget()->findChild<QWidget*>("aftermarket-dock-controls"));
  auto *unused=new QLabel("Other back");
  REQUIRE(!faces.attach(a,unused));delete unused;
  REQUIRE(faces.front(a)==native);
  REQUIRE(faces.back(a)==back);
  REQUIRE(!faces.backVisible(a));
  REQUIRE(a->widget()!=native&&slider->parentWidget()==native);
  REQUIRE(a->widget()->findChild<QLineEdit*>()==slider);
  auto *flip=a->findChild<QToolButton*>("aftermarket-face-flip");
  REQUIRE(flip&&flip->isVisible());
  REQUIRE(flip->parentWidget()==a);
  REQUIRE(flip->geometry().bottom()<a->widget()->geometry().top());
  REQUIRE(flip->isVisible());
  REQUIRE(flip->isCheckable()&&!flip->isChecked());
  REQUIRE(flip->property("aftermarket-header-button").toBool());
  REQUIRE(flip->toolTip().contains("routing",Qt::CaseInsensitive));
  REQUIRE(flip->accessibleName().contains("routing",Qt::CaseInsensitive));
  REQUIRE(flip->width()<=22&&flip->height()<=20);
  auto *previous=a->findChild<QToolButton*>("aftermarket-tab-previous");
  REQUIRE(previous&&previous->property("aftermarket-header-button").toBool());
  REQUIRE(previous->accessibleName().contains("Previous"));
  auto *knobs=new augment::DockHeaderButton(augment::DockHeaderGlyph::Knobs);
  knobs->setObjectName("aftermarket-front-knobs");
  knobs->setChecked(true);
  REQUIRE(knobs->property("aftermarket-header-button").toBool());
  REQUIRE(faces.addFrontAccessory(a,knobs));
  REQUIRE(knobs->parentWidget()==a&&knobs->isVisible());
  auto *orphan=new QLabel("unrelated");
  REQUIRE(!faces.addFrontAccessory(b,orphan));delete orphan;
  REQUIRE(knobs->geometry().left()>=textRect.left());
  REQUIRE(flip->geometry().right()<=textRect.right());
  REQUIRE(knobs->geometry().bottom()<a->widget()->geometry().top());
  flip->click();app.processEvents();
  REQUIRE(faces.backVisible(a)&&back->isVisible());
  REQUIRE(flip->isChecked());
  REQUIRE(flip->toolTip().contains("mixer",Qt::CaseInsensitive));
  const auto checkedPixels=flip->grab().toImage();
  QPalette alternatePalette=flip->palette();
  alternatePalette.setColor(QPalette::Highlight,QColor(205,115,40));
  flip->setPalette(alternatePalette);app.processEvents();
  REQUIRE(checkedPixels!=flip->grab().toImage()); // icon and selection follow palette
  REQUIRE(!knobs->isVisible());
  REQUIRE(!native->isVisible());
  flip->click();app.processEvents();
  REQUIRE(!faces.backVisible(a)&&native->isVisible());
  REQUIRE(!flip->isChecked());
  REQUIRE(flip->toolTip().contains("routing",Qt::CaseInsensitive));
  REQUIRE(knobs->isVisible());
  auto *next=a->findChild<QToolButton*>("aftermarket-tab-next");
  REQUIRE(next&&next->isVisible());
  next->click();app.processEvents();
  REQUIRE(scene->isVisible());
  a->raise();app.processEvents();
  // Longer native titles must not be covered by Aftermarket buttons.
  const QString nativeTitle=a->windowTitle();
  a->setWindowTitle("Audio Mixer - Extensively Long Native OBS Dock Title");
  app.processEvents();
  if(flip->isVisible()){
    REQUIRE(flip->geometry().left()>=
      a->fontMetrics().horizontalAdvance(a->windowTitle())+9);
  }
  a->setWindowTitle(nativeTitle);app.processEvents();
  REQUIRE(flip->isVisible());
  // The native window manager owns floating-dock title decoration.
  a->setFloating(true);app.processEvents();
  REQUIRE(a->isFloating()&&!flip->isVisible());
  REQUIRE(a->titleBarWidget()==originalTitleBar);
  a->setFloating(false);app.processEvents();
  a->raise();app.processEvents();
  REQUIRE(!a->isFloating()&&flip->isVisible());
  REQUIRE(a->titleBarWidget()==originalTitleBar);
  REQUIRE(faces.detach(a));app.processEvents();
  REQUIRE(a->widget()==native&&slider->text()=="gain");
  REQUIRE(!a->findChild<QToolButton*>("aftermarket-face-flip"));
  REQUIRE(!a->findChild<QWidget*>("aftermarket-native-title-actions"));
  REQUIRE(a->titleBarWidget()==originalTitleBar&&a->features()==originalFeatures);
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
