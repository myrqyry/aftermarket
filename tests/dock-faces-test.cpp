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
#include <QPaintEvent>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\\n";return 1;}}while(false)

// QWidget::grab renders synchronously, even when the event loop is idle.
class PaintProbe final : public QLabel {
public:
 using QLabel::QLabel;
 int paints=0;
protected:
 void paintEvent(QPaintEvent *e) override {++paints;QLabel::paintEvent(e);}
};
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
  auto *back=new PaintProbe("Audio routes");back->setWindowTitle("Routing");
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
  // Solid, distinguishable faces make the *visual* contract testable.
  auto frontPalette=native->palette();frontPalette.setColor(QPalette::Window,QColor("#c83232"));
  native->setPalette(frontPalette);native->setAutoFillBackground(true);
  auto backPalette=back->palette();backPalette.setColor(QPalette::Window,QColor("#3278c8"));
  back->setPalette(backPalette);back->setAutoFillBackground(true);
  app.processEvents();
  const int paintsBefore=back->paints;
  flip->click();
  REQUIRE(back->paints<=paintsBefore+1); // at most one bounded destination capture
  app.processEvents();
  REQUIRE(faces.backVisible(a)&&back->isVisible());
  auto *surface=stack->findChild<QWidget*>("aftermarket-view-flip");
  REQUIRE(surface&&surface->isVisible());
  REQUIRE(surface->testAttribute(Qt::WA_TransparentForMouseEvents));
  REQUIRE(surface->focusPolicy()==Qt::NoFocus);
  REQUIRE(surface->parentWidget()==back&&surface->geometry()==back->rect());
  REQUIRE(native->graphicsEffect()==nullptr&&back->graphicsEffect()==nullptr);
  auto *flipMotion=surface->findChild<QVariantAnimation*>("aftermarket-view-flip-motion");
  REQUIRE(flipMotion&&flipMotion->state()==QAbstractAnimation::Running);
  REQUIRE(flipMotion->duration()>=175&&flipMotion->duration()<=260);
  // Sample the real painted overlay at exact times. The outgoing face must
  // be visible first, contract around a perspective edge, vanish at the
  // midpoint, and the incoming face must expand from the opposite edge.
  flipMotion->pause();
  auto sample=[&](int ms){flipMotion->setCurrentTime(ms);return surface->grab().toImage();};
  const auto first=sample(0);
  const QColor red("#c83232"),blue("#3278c8");
  auto count=[&](const QImage &img,const QColor &color){
    int n=0;for(int y=0;y<img.height();++y)for(int x=0;x<img.width();++x)
      if(img.pixelColor(x,y)==color)++n;
    return n;
  };
  REQUIRE(count(first,red)>first.width()*first.height()/2);
  const int beforeSample=back->paints;
  const auto turning=sample(70);
  const auto midpoint=sample(105);
  const auto incoming=sample(140);
  const auto almost=sample(209);
  REQUIRE(count(turning,red)>0&&count(turning,red)<count(first,red));
  REQUIRE(count(turning,blue)==0);
  REQUIRE(count(midpoint,red)==0&&count(midpoint,blue)==0);
  REQUIRE(count(incoming,blue)>0&&count(incoming,red)==0);
  REQUIRE(count(almost,blue)>count(incoming,blue));
  // The shrinking face must be skewed, not an untransformed live reveal.
  auto bounds=[&](const QImage &img,const QColor &color,int x){
    int top=img.height(),bottom=-1;
    for(int y=0;y<img.height();++y)if(img.pixelColor(x,y)==color){top=qMin(top,y);bottom=qMax(bottom,y);}
    return bottom-top+1;
  };
  const int l=turning.width()*2/5,r=turning.width()*3/5;
  REQUIRE(bounds(turning,red,l)>bounds(turning,red,r));
  REQUIRE(bounds(incoming,blue,l)<bounds(incoming,blue,r));
  REQUIRE(back->paints==beforeSample); // frames never recapture the live widgets
  REQUIRE(stack->currentWidget()==back&&back->isVisible());
  flip->click();app.processEvents(); // rapid reversal cancels previous
  REQUIRE(!faces.backVisible(a)&&native->isVisible());
  REQUIRE(stack->findChildren<QWidget*>("aftermarket-view-flip").size()==1);
  QEventLoop finishLoop;
  QTimer::singleShot(280,&finishLoop,&QEventLoop::quit);
  finishLoop.exec();app.processEvents();
  REQUIRE(stack->findChildren<QWidget*>("aftermarket-view-flip").isEmpty());
  // In-flight transitions disappear when motion is disabled.
  REQUIRE(faces.showBack(a,true));app.processEvents();
  motionOn=false;
  QEventLoop cancelLoop;QTimer::singleShot(80,&cancelLoop,&QEventLoop::quit);cancelLoop.exec();
  REQUIRE(!stack->findChild<QWidget*>("aftermarket-view-flip"));
  REQUIRE(faces.backVisible(a));
  REQUIRE(faces.showBack(a,false));motionOn=true;
  // Large paint surfaces skip decorative transitions.
  // Skip decoration, not the real page change, when over the budget.
  main.resize(2400,1600);app.processEvents();
  const QSize previousSize=stack->size();
  stack->resize(1400,1000);
  REQUIRE(faces.showBack(a,true));
  REQUIRE(faces.backVisible(a)&&!stack->findChild<QWidget*>("aftermarket-view-flip"));
  stack->resize(1400,1000);
  REQUIRE(faces.showBack(a,false));
  REQUIRE(!stack->findChild<QWidget*>("aftermarket-view-flip"));
  stack->resize(previousSize);main.resize(920,720);app.processEvents();
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
 // Native page roots are fine: the decoration lives inside that root,
 // not as a sibling trying to cover a separate native child window.
 {
  augment::DockFaces faces(nullptr,[]{return true;});
  auto *nativePage=new QWidget;
  nativePage->setAttribute(Qt::WA_NativeWindow);
  REQUIRE(faces.attach(a,nativePage));a->raise();app.processEvents();
  REQUIRE(faces.showBack(a,true));
  auto *reveal=nativePage->findChild<QWidget*>("aftermarket-view-flip");
  REQUIRE(reveal&&reveal->parentWidget()==nativePage&&!reveal->testAttribute(Qt::WA_NativeWindow));
  REQUIRE(faces.showBack(a,false));
 }
 // Nested native/GPU surfaces still bypass decoration on that destination.
 {
  augment::DockFaces faces(nullptr,[]{return true;});
  auto *page=new QWidget;
  auto *nativeChild=new QWidget(page);nativeChild->setAttribute(Qt::WA_NativeWindow);
  REQUIRE(faces.attach(a,page));a->raise();app.processEvents();
  REQUIRE(faces.showBack(a,true));
  REQUIRE(faces.backVisible(a)&&!page->findChild<QWidget*>("aftermarket-view-flip"));
  REQUIRE(faces.showBack(a,false));
 }
 // Refuse non-owned/no-native widget dock.
 auto *empty=new QDockWidget("Empty",&main);
 main.addDockWidget(Qt::RightDockWidgetArea,empty);
 {augment::DockFaces faces; auto *bad=new QLabel("bad");REQUIRE(!faces.attach(empty,bad));delete bad;}
 std::cout<<"dock faces verified\n";
 return 0;
}
