#include "dock-faces.h"
#include "dock-header-button.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QStackedWidget>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QStyle>
#include <QStyleOptionDockWidget>
#include <QVariantAnimation>
#include <QEventLoop>
#include <QTimer>
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
  bool motionOn=false;
  augment::DockFaces faces(nullptr,[&]{return motionOn;});
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
  REQUIRE(!a->widget()->findChild<QSplitter*>("aftermarket-mixer-split"));
  auto *stack=a->widget()->findChild<QStackedWidget*>("aftermarket-dock-pages");
  REQUIRE(stack&&stack->count()==2);
  REQUIRE(stack->currentWidget()==native);
  auto *unused=new QLabel("Other back");
  REQUIRE(!faces.attach(a,unused));delete unused;
  REQUIRE(faces.front(a)==native);
  REQUIRE(faces.back(a)==back);
  REQUIRE(!faces.backVisible(a));
  REQUIRE(a->widget()!=native&&slider->parentWidget()==native);
  REQUIRE(a->widget()->findChild<QLineEdit*>()==slider);
  auto *flip=a->findChild<QToolButton*>("aftermarket-audiobus-toggle");
  REQUIRE(flip&&flip->isVisible());
  REQUIRE(flip->parentWidget()!=a);
  REQUIRE(flip->parentWidget()->parentWidget()==a->widget());
  REQUIRE(flip->mapTo(a->widget(),QPoint(0,0)).y()>=stack->geometry().bottom());
  REQUIRE(flip->isCheckable()&&!flip->isChecked());
  REQUIRE(flip->text()=="AudioBus");
  REQUIRE(!a->findChild<QToolButton*>("aftermarket-face-flip"));
  REQUIRE(flip->toolTip().contains("routing",Qt::CaseInsensitive));
  REQUIRE(flip->accessibleName().contains("routing",Qt::CaseInsensitive));
  REQUIRE(flip->height()<=26);
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
  REQUIRE(knobs->geometry().right()<=textRect.right());
  REQUIRE(knobs->geometry().bottom()<a->widget()->geometry().top());
  flip->click();app.processEvents();
  REQUIRE(faces.backVisible(a)&&back->isVisible());
  REQUIRE(stack->currentWidget()==back&&!native->isVisible());
  REQUIRE(flip->isChecked());
  REQUIRE(flip->text()=="Audio Mixer");
  REQUIRE(flip->toolTip().contains("mixer",Qt::CaseInsensitive));
  REQUIRE(flip->isEnabled()); // original action remains a real clickable control
  REQUIRE(knobs->isVisible());
  REQUIRE(!native->isVisible());
  flip->click();app.processEvents();
  REQUIRE(!faces.backVisible(a)&&native->isVisible());
  REQUIRE(!flip->isChecked());
  REQUIRE(flip->text()=="AudioBus");
  REQUIRE(flip->toolTip().contains("routing",Qt::CaseInsensitive));
  REQUIRE(knobs->isVisible());
  // With Motion on, a single click-through flip animates two paint-only
  // snapshots. The actual mixer/routing widgets retain identity, ownership
  // and native effects, and become interactive immediately underneath it.
  motionOn=true;
  flip->click();app.processEvents();
  REQUIRE(faces.backVisible(a)&&back->isVisible());
  auto *surface=stack->findChild<QWidget*>("aftermarket-view-flip");
  REQUIRE(surface&&surface->isVisible());
  REQUIRE(surface->testAttribute(Qt::WA_TransparentForMouseEvents));
  REQUIRE(surface->focusPolicy()==Qt::NoFocus);
  REQUIRE(surface->geometry()==stack->rect());
  REQUIRE(native->graphicsEffect()==nullptr&&back->graphicsEffect()==nullptr);
  auto *flipMotion=surface->findChild<QVariantAnimation*>("aftermarket-view-flip-motion");
  REQUIRE(flipMotion&&flipMotion->state()==QAbstractAnimation::Running);
  REQUIRE(flipMotion->duration()>=175&&flipMotion->duration()<=260);
  // At midway the old face must fold out of view; the new face folds in.
  // Compare real paint frames, not only the animation's running flag.
  auto first=surface->grab().toImage();
  QEventLoop foldLoop;
  QTimer::singleShot(95,&foldLoop,&QEventLoop::quit);
  foldLoop.exec();app.processEvents();
  REQUIRE(surface->grab().toImage()!=first);
  REQUIRE(stack->currentWidget()==back&&back->isVisible());
  flip->click();app.processEvents(); // rapid reversal cancels previous
  REQUIRE(!faces.backVisible(a)&&native->isVisible());
  REQUIRE(stack->findChildren<QWidget*>("aftermarket-view-flip").size()==1);
  QEventLoop finishLoop;
  QTimer::singleShot(280,&finishLoop,&QEventLoop::quit);
  finishLoop.exec();app.processEvents();
  REQUIRE(stack->findChildren<QWidget*>("aftermarket-view-flip").isEmpty());
  motionOn=false; // reduced-motion mode: immediate switch
  flip->click();app.processEvents();
  REQUIRE(faces.backVisible(a)&&back->isVisible());
  REQUIRE(!stack->findChild<QWidget*>("aftermarket-view-flip"));
  flip->click();app.processEvents();
  REQUIRE(!faces.backVisible(a)&&native->isVisible());
  auto *next=a->findChild<QToolButton*>("aftermarket-tab-next");
  REQUIRE(next&&next->isVisible());
  next->click();app.processEvents();
  REQUIRE(scene->isVisible());
  a->raise();app.processEvents();
  // Longer native titles must not be covered by Aftermarket buttons.
  const QString nativeTitle=a->windowTitle();
  a->setWindowTitle("Audio Mixer - Extensively Long Native OBS Dock Title");
  app.processEvents();
  if(knobs->isVisible()){
    REQUIRE(knobs->geometry().left()>=
      a->fontMetrics().horizontalAdvance(a->windowTitle())+9);
  }
  REQUIRE(flip->isVisible());
  a->setWindowTitle(nativeTitle);app.processEvents();
  REQUIRE(flip->isVisible());
  // The native window manager owns floating-dock title decoration.
  a->setFloating(true);app.processEvents();
  REQUIRE(a->isFloating()&&flip->isVisible());
  REQUIRE(!knobs->isVisible());
  REQUIRE(a->titleBarWidget()==originalTitleBar);
  a->setFloating(false);app.processEvents();
  a->raise();app.processEvents();
  REQUIRE(!a->isFloating()&&flip->isVisible());
  REQUIRE(knobs->isVisible());
  REQUIRE(a->titleBarWidget()==originalTitleBar);
  REQUIRE(faces.detach(a));app.processEvents();
  REQUIRE(a->widget()==native&&slider->text()=="gain");
  REQUIRE(!a->findChild<QToolButton*>("aftermarket-audiobus-toggle"));
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
