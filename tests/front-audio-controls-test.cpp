#include "front-audio-controls.h"
#include "audio-patchbay.h"
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPointer>
#include <QScrollArea>
#include <QToolButton>
#include <QSlider>
#include <QDial>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QFile>
#include <QLabel>
#include <QFrame>
#include <QSizePolicy>
#include <QEventLoop>
#include <QTimer>
#include <QMenu>
#include <QToolBar>
#include <QPushButton>
#include <QAction>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QImage>
#include <QPalette>
#include <QStylePainter>
#include <QStyleOptionButton>
#include <QAbstractButton>
#include <iostream>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
// Reproduce the actual OBS 32.2 VolumeName painter and reserved indicator
// geometry; the native child label continues to render independently.
class VolumeName final : public QAbstractButton {
 Q_OBJECT
public:
 explicit VolumeName(const QString &name,QWidget *parent):QAbstractButton(parent){
  setText(name);auto *box=new QHBoxLayout(this);
  box->setContentsMargins(0,0,style()->pixelMetric(QStyle::PM_MenuButtonIndicator,nullptr,this),0);
  box->addWidget(new QLabel(name,this));
 }
 QRect indicatorRect()const{
  QStyleOptionButton opt;opt.initFrom(this);
  const int padding=opt.rect.right()-style()->subElementRect(QStyle::SE_PushButtonContents,&opt,this).right();
  const int w=style()->pixelMetric(QStyle::PM_MenuButtonIndicator,&opt,this);
  return QRect(opt.rect.right()-w-padding/2,opt.rect.center().y()-w/2,w,w);
 }
protected:
 void paintEvent(QPaintEvent *)override{
  QStylePainter p(this);QStyleOptionButton opt;opt.initFrom(this);
  p.drawControl(QStyle::CE_PushButtonBevel,opt);
  QStyleOption arrow=opt;arrow.rect=indicatorRect();
  p.drawPrimitive(QStyle::PE_IndicatorArrowDown,arrow);
 }
};
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setStyle("Fusion");
 QTemporaryDir tmp;REQUIRE(tmp.isValid());
 auto file=tmp.filePath("front-settings.json");
 QWidget native;auto *layout=new QVBoxLayout(&native);
 auto *one=new QWidget(&native);one->setObjectName("VolumeControl");
 auto *oneLayout=new QVBoxLayout(one);
 auto *oneName=new QToolButton(one);oneName->setObjectName("VolumeName");
 oneName->setText("Live Latent: TTS");oneLayout->addWidget(oneName);
 auto *original=new QSlider(Qt::Horizontal,one);original->setValue(52);
 oneLayout->addWidget(original);
 layout->addWidget(one);
 auto *two=new QWidget(&native);two->setObjectName("VolumeControl");
 auto *twoLayout=new QVBoxLayout(two);
 auto *twoName=new QToolButton(two);twoName->setObjectName("VolumeName");
 twoName->setText("Media Source");twoLayout->addWidget(twoName);
 auto *secondOriginal=new QSlider(Qt::Horizontal,two);secondOriginal->setValue(65);
 twoLayout->addWidget(secondOriginal);
 layout->addWidget(two);
 auto *footer=new QToolBar(&native);layout->addWidget(footer);
 auto *hidden=new QPushButton("0 hidden",footer);footer->addWidget(hidden);
 auto *orientation=new QToolButton(footer);orientation->setToolTip("Swap mixer layout");
 footer->addWidget(orientation);
 auto *advanced=new QToolButton(footer);advanced->setText("Advanced Audio Properties");
 footer->addWidget(advanced);
 auto *options=new QPushButton("Options",footer);footer->addWidget(options);
 auto *nativeOptions=new QMenu(&native);
 nativeOptions->addAction("Show hidden sources");
 int hiddenClicks=0,orientationClicks=0,advancedClicks=0;
 QObject::connect(hidden,&QPushButton::clicked,[&]{++hiddenClicks;});
 QObject::connect(orientation,&QToolButton::clicked,[&]{++orientationClicks;});
 QObject::connect(advanced,&QToolButton::clicked,[&]{++advancedClicks;});
 QList<augment::AudioPropertyState> current={
     {"uuid-tts","Live Latent: TTS",.5,120,true},
     {"uuid-media","Media Source",.25,-80,true}
 };
 int pans=0,offsets=0,savedProjects=0;
 augment::FrontAudioControls ui(file,
     [&]{return current;},
     [&](const QString &id,double balance){
         for(auto &s:current)if(s.id==id){s.balance=balance;++pans;return true;}
         return false;
     },
     [&](const QString &id,int ms){
         for(auto &s:current)if(s.id==id){s.offsetMs=ms;++offsets;return true;}
         return false;
     },
     [&]{++savedProjects;});
 native.resize(410,420);native.show();app.processEvents();
 REQUIRE(ui.attach(&native));
 REQUIRE(ui.rowCount()==2);
 REQUIRE(footer->isVisible()); // Keep native controls until a usable title overflow exists
 auto *more=new QToolButton(&native);
 more->setObjectName("aftermarket-mixer-more");
 more->show();
 REQUIRE(ui.setMenuButton(more));
 app.processEvents();
 REQUIRE(!footer->isVisible()); // OBS's footer moves into the title overflow
 more->hide();app.processEvents();
 REQUIRE(footer->isVisible()); // Floating/narrow docks cannot lose native settings
 more->show();app.processEvents();
 REQUIRE(!footer->isVisible());
 QMenu nativeMenu;
 ui.populateSettingsMenu(&nativeMenu);
 const auto findEntry=[&](const QString &name)->QAction*{
     for(auto *a:nativeMenu.actions())if(a->text().contains(name,Qt::CaseInsensitive))return a;
     return nullptr;
 };
 REQUIRE(findEntry("hidden")&&findEntry("layout")&&findEntry("Advanced"));
 findEntry("hidden")->trigger();
 findEntry("layout")->trigger();
 findEntry("Advanced")->trigger();
 REQUIRE(hiddenClicks==1&&orientationClicks==1&&advancedClicks==1);
 REQUIRE(findEntry("Options")); // original OBS options remain reachable
 // Manual order mode is opt-in; dragging the name moves the original channel.
 auto *unlock=findEntry("Unlock channel ordering");
 REQUIRE(unlock&&unlock->isCheckable()&&!unlock->isChecked());
 unlock->trigger();app.processEvents();
 auto drag=[&](QWidget *handle,QWidget *target){
     const QPointF start=handle->mapToGlobal(handle->rect().center());
     const QPointF end=target->mapToGlobal(target->rect().center());
     QMouseEvent press(QEvent::MouseButtonPress,handle->mapFromGlobal(start.toPoint()),start,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
     QApplication::sendEvent(handle,&press);
     QMouseEvent move(QEvent::MouseMove,handle->mapFromGlobal(end.toPoint()),end,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
     QApplication::sendEvent(handle,&move);
     QMouseEvent release(QEvent::MouseButtonRelease,handle->mapFromGlobal(end.toPoint()),end,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
     QApplication::sendEvent(handle,&release);app.processEvents();
 };
 drag(twoName,one);
 REQUIRE(layout->itemAt(0)->widget()==two&&layout->itemAt(1)->widget()==one);
 REQUIRE(layout->itemAt(2)->widget()==footer);
 // AudioBus is another view of the same saved UUID order, not a second
 // order store. The first clickable node must follow the mixer position.
 QVector<augment::RouteSource> routes={
     {"uuid-tts","Live Latent: TTS",1,0},
     {"uuid-media","Media Source",2,0},
     {"uuid-extra","Hidden or inactive source",4,0}
 };
 int orderRoutingWrites=0;
 augment::AudioPatchbay orderBay([&]{return routes;},
     [&](const QString&,int){++orderRoutingWrites;return true;},{});
 orderBay.setSourceOrderProvider([&]{return ui.channelOrder();});
 orderBay.resize(350,340);orderBay.show();app.processEvents();
 auto *orderCanvas=orderBay.findChild<QWidget*>("aftermarket-route-canvas");
 REQUIRE(orderCanvas&&orderBay.routeCount()==3);
 auto pickFirst=[&]{
     QPoint local(39,43);
     QMouseEvent press(QEvent::MouseButtonPress,local,orderCanvas->mapToGlobal(local),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
     QApplication::sendEvent(orderCanvas,&press);
     QMouseEvent release(QEvent::MouseButtonRelease,local,orderCanvas->mapToGlobal(local),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
     QApplication::sendEvent(orderCanvas,&release);
 };
 pickFirst();REQUIRE(orderBay.selectedId()=="uuid-media");
 REQUIRE(orderRoutingWrites==0&&orderBay.visibleCableCount()==3);
 // Unlisted routes survive, selection follows UUID, not row number.
 REQUIRE(orderBay.selectSource("uuid-extra"));
 routes[1].name="Renamed media";orderBay.refresh();
 REQUIRE(orderBay.selectedId()=="uuid-extra");
 orderBay.hide();

 unlock->trigger(); // lock positions without resetting them
 drag(oneName,two);
 REQUIRE(layout->itemAt(0)->widget()==two);
 // Change native order while AudioBus is hidden, then flip back. No timer
 // tick or duplicate storage should be necessary.
 unlock->trigger();drag(oneName,two);unlock->trigger();
 REQUIRE(layout->itemAt(0)->widget()==one);
 orderBay.show();app.processEvents();
 REQUIRE(orderBay.selectedId()=="uuid-extra");
 pickFirst();REQUIRE(orderBay.selectedId()=="uuid-tts");
 orderBay.hide();
 QMenu orderReset;ui.populateSettingsMenu(&orderReset);
 QAction *resetShared=nullptr;
 for(auto *a:orderReset.actions())if(a->text()=="Reset channel order")resetShared=a;
 REQUIRE(resetShared&&resetShared->isEnabled());resetShared->trigger();
 REQUIRE(ui.channelOrder().isEmpty());
 orderBay.show();app.processEvents();pickFirst();
 REQUIRE(orderBay.selectedId()=="uuid-tts"&&orderRoutingWrites==0);
 orderBay.hide();
 unlock->trigger();drag(twoName,one);unlock->trigger();
 // OBS may rebuild/reinsert widgets in native order; refresh reapplies saved order.
 layout->removeWidget(one);layout->insertWidget(0,one);
 ui.refresh();REQUIRE(layout->itemAt(0)->widget()==two);

 auto *pan=one->findChild<QSlider*>("aftermarket-pan-slider");
 auto *offset=one->findChild<QSpinBox*>("aftermarket-offset-ms");
 auto *panDial=one->findChild<QDial*>("aftermarket-pan-dial");
 auto *offsetDial=one->findChild<QDial*>("aftermarket-offset-dial");
 REQUIRE(pan&&offset&&panDial&&offsetDial);
 REQUIRE(pan->isVisible()&&!panDial->isVisible());
 REQUIRE(offset->value()==120&&pan->value()==50);
 REQUIRE(original->value()==52);
 pan->setValue(78);
 REQUIRE(pans==1&&qAbs(current[0].balance-.78)<.001);
 offset->setValue(230);
 REQUIRE(offsets==1&&current[0].offsetMs==230);
 REQUIRE(original->value()==52);
 // External Advanced Audio Properties edits must update the controls without
 // writing back and without destroying the user's native slider.
 current[0].balance=.32;current[0].offsetMs=-250;
 ui.refresh();
 REQUIRE(pan->value()==32&&offset->value()==-250);
 REQUIRE(pans==1&&offsets==1);
 REQUIRE(ui.setKnobs(true));
 REQUIRE(ui.knobs()&&panDial->isVisible()&&!pan->isVisible());
 REQUIRE(offsetDial->isVisible()&&offset->isVisible()); // precise ms available
 // Small rotary controls must paint the same theme-accent ring language,
 // not the platform QDial face. Use an unmistakable palette accent.
 QPalette rotaryPalette=panDial->palette();
 rotaryPalette.setColor(QPalette::Highlight,QColor("#ff00ff"));
 rotaryPalette.setColor(QPalette::Link,QColor("#ff00ff"));
 rotaryPalette.setColor(QPalette::Mid,QColor("#303030"));
 rotaryPalette.setColor(QPalette::Button,QColor("#202020"));
 rotaryPalette.setColor(QPalette::ButtonText,QColor("#ffffff"));
 panDial->setPalette(rotaryPalette);offsetDial->setPalette(rotaryPalette);
 auto accentPixels=[](QDial *dial){
     const auto img=dial->grab().toImage().convertToFormat(QImage::Format_RGB32);
     int count=0;
     for(int y=0;y<img.height();++y)for(int x=0;x<img.width();++x){
         const auto c=img.pixelColor(x,y);
         if(c.red()>85&&c.blue()>85&&c.red()-c.green()>35&&c.blue()-c.green()>35)++count;
     }
     return count;
 };
 REQUIRE(accentPixels(panDial)>=5&&accentPixels(offsetDial)>=5);
 // The actual Fluffy Dark mixer has almost identical Mid/Button/Window
 // values. A readable rotary needs a tinted idle track, a bright pointer,
 // and enough diameter to resolve the two rings, not just accent pixels.
 QPalette dark=rotaryPalette;
 dark.setColor(QPalette::Window,QColor("#242424"));
 dark.setColor(QPalette::Mid,QColor("#242424"));
 dark.setColor(QPalette::Button,QColor("#222222"));
 dark.setColor(QPalette::Highlight,QColor("#f38fb1"));
 dark.setColor(QPalette::Link,QColor("#f8e6eb"));
 dark.setColor(QPalette::ButtonText,QColor("#eeeeee"));
 for(auto *dial:{panDial,offsetDial}){
     dial->setPalette(dark);
     const auto image=dial->grab().toImage().convertToFormat(QImage::Format_RGB32);
     REQUIRE(dial->width()>=30&&dial->height()>=30);
     int tinted=0,bright=0;
     const auto center=QPointF(image.width()/2.,image.height()/2.);
     for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){
         const auto color=image.pixelColor(x,y);
         const double radius=std::hypot(x+.5-center.x(),y+.5-center.y());
         if(radius>=image.width()*.36&&color.red()>75&&color.red()<155&&color.red()-color.green()>20)++tinted;
         if(radius<image.width()*.24&&color.red()>170&&color.green()>170&&color.blue()>170)++bright;
     }
     REQUIRE(tinted>=18&&bright>=3);
 }
 panDial->setPalette(rotaryPalette);offsetDial->setPalette(rotaryPalette);
 const auto oldFace=offsetDial->grab().toImage();
 rotaryPalette.setColor(QPalette::Highlight,QColor("#00ff00"));
 rotaryPalette.setColor(QPalette::Link,QColor("#00ff00"));
 offsetDial->setPalette(rotaryPalette);app.processEvents();
 REQUIRE(oldFace!=offsetDial->grab().toImage()&&accentPixels(offsetDial)==0);
 panDial->setPalette(QPalette());offsetDial->setPalette(QPalette());
 panDial->setValue(65);offsetDial->setValue(1000);
 REQUIRE(pans==2&&offsets==2);
 REQUIRE(qAbs(current[0].balance-.65)<.001&&current[0].offsetMs==1000);
 REQUIRE(offset->value()==1000&&pan->value()==65); // immediate sibling control sync
 // Match the main gain knob's no-jump, relative x-y drag. Pan's
 // range is bounded while sync's movement is unbounded until saturation.
 const auto dialDrag=[&](QDial *dial,QPoint delta,Qt::KeyboardModifiers mods=Qt::NoModifier){
     const QPoint p(5,8),global=dial->mapToGlobal(p);
     const int before=dial->value();
     QMouseEvent press(QEvent::MouseButtonPress,p,global,Qt::LeftButton,Qt::LeftButton,mods);
     QApplication::sendEvent(dial,&press);
     if(dial->value()!=before)return false;
     QMouseEvent move(QEvent::MouseMove,p+delta,global+delta,Qt::NoButton,Qt::LeftButton,mods);
     QApplication::sendEvent(dial,&move);
     QMouseEvent release(QEvent::MouseButtonRelease,p+delta,global+delta,Qt::LeftButton,Qt::NoButton,mods);
     QApplication::sendEvent(dial,&release);
     return !dial->isSliderDown();
 };
 REQUIRE(dialDrag(panDial,QPoint(15,-15))&&panDial->value()==85);
 REQUIRE(dialDrag(panDial,QPoint(-15,15))&&panDial->value()==65);
 REQUIRE(dialDrag(offsetDial,QPoint(12,-8))&&offsetDial->value()==1200);
 REQUIRE(dialDrag(offsetDial,QPoint(-12,8))&&offsetDial->value()==1000);
 REQUIRE(dialDrag(offsetDial,QPoint(12,-8),Qt::ShiftModifier)&&offsetDial->value()==1020);
 // A relative encoder must continue from an offset beyond the old ±2s range.
 current[0].offsetMs=12345;ui.refresh();
 REQUIRE(offsetDial->value()==12345);
 const auto turn=[&](int ticks,Qt::KeyboardModifiers mods=Qt::NoModifier){
     QPointF p=offsetDial->rect().center();
     QWheelEvent e(p,offsetDial->mapToGlobal(p.toPoint()),QPoint(),QPoint(0,ticks),Qt::NoButton,mods,Qt::NoScrollPhase,false);
     QApplication::sendEvent(offsetDial,&e);
 };
 turn(120);REQUIRE(current[0].offsetMs==12355&&offset->value()==12355);
 turn(-120,Qt::ShiftModifier);REQUIRE(current[0].offsetMs==12354);
 turn(60);REQUIRE(current[0].offsetMs==12354);
 turn(60);REQUIRE(current[0].offsetMs==12364);
 // Pressing an arbitrary angle never jumps the value; upward drag adds
 // relative increments and a fresh press continues the same offset.
 const QPoint start(3,6);const QPoint global=offsetDial->mapToGlobal(start);
 QMouseEvent press(QEvent::MouseButtonPress,start,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
 QApplication::sendEvent(offsetDial,&press);REQUIRE(current[0].offsetMs==12364);
 QMouseEvent move(QEvent::MouseMove,start-QPoint(0,45),global-QPoint(0,45),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
 QApplication::sendEvent(offsetDial,&move);REQUIRE(current[0].offsetMs==12814);
 QMouseEvent release(QEvent::MouseButtonRelease,start-QPoint(0,45),global-QPoint(0,45),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
 QApplication::sendEvent(offsetDial,&release);
 REQUIRE(!offsetDial->isSliderDown()&&current[0].offsetMs==12814);
 current[0].offsetMs=999995;ui.refresh();
 turn(120);REQUIRE(current[0].offsetMs==999999);
 turn(-120);REQUIRE(current[0].offsetMs==999989);
 current[0].offsetMs=-999995;ui.refresh();
 turn(-120);REQUIRE(current[0].offsetMs==-999999);
 turn(120);REQUIRE(current[0].offsetMs==-999989);
 QKeyEvent key(QEvent::KeyPress,Qt::Key_Up,Qt::ShiftModifier);
 QApplication::sendEvent(offsetDial,&key);REQUIRE(current[0].offsetMs==-999988);
 REQUIRE(savedProjects==0); // coalesce many dial/slider update events
 QEventLoop waitForSave;
 QTimer::singleShot(650,&waitForSave,&QEventLoop::quit);
 waitForSave.exec();
 REQUIRE(savedProjects==1); // OBS must persist native edits
 current[0].balance=.44;current[0].offsetMs=600;
 ui.refresh();app.processEvents();
 REQUIRE(savedProjects==1); // external OBS edits must not re-save themselves
 REQUIRE(QFile::exists(file));
 // Native source rename retains the UUID but the GUI text changes.
 oneName->setText("Renamed TTS");
 current[0].name="Renamed TTS";
 ui.refresh();
 REQUIRE(ui.rowCount()==2&&one->findChild<QSpinBox*>("aftermarket-offset-ms"));
 // If a native control is removed, its addon is removed and the other stays.
 oneName->setText("stale name");
 ui.refresh();
 REQUIRE(ui.rowCount()==1);
 REQUIRE(!one->findChild<QWidget*>("aftermarket-source-adjustments"));
 REQUIRE(secondOriginal->value()==65);
 REQUIRE(ui.setKnobs(false));
 REQUIRE(!ui.knobs());
 ui.detach();
 REQUIRE(footer->isVisible());
 REQUIRE(ui.rowCount()==0);
 REQUIRE(!two->findChild<QWidget*>("aftermarket-source-adjustments"));
 REQUIRE(one->layout()->count()==2&&two->layout()->count()==2);
 REQUIRE(layout->itemAt(0)->widget()==one&&layout->itemAt(1)->widget()==two);
 REQUIRE(original->value()==52&&secondOriginal->value()==65);
 // State survives a restart without requiring OBS's own rules or mixer state.
 augment::FrontAudioControls restored(file,[&]{return current;},{},{});
 REQUIRE(!restored.knobs());
 REQUIRE(restored.attach(&native));
 REQUIRE(restored.rowCount()==1);
 oneName->setText("Renamed TTS");restored.refresh();
 REQUIRE(layout->itemAt(0)->widget()==two&&layout->itemAt(1)->widget()==one);
 restored.detach();
 // Preserve the native button, child label, width and action. Only its
 // arrow primitive becomes three vertical dots; detach restores the arrow.
 QWidget menuMixer;auto *menuLayout=new QVBoxLayout(&menuMixer);
 auto *menuChannel=new QWidget(&menuMixer);menuChannel->setObjectName("VolumeControl");
 menuChannel->setMaximumWidth(110);auto *menuChannelLayout=new QVBoxLayout(menuChannel);
 menuChannelLayout->setContentsMargins(0,0,0,0);
 auto *volumeName=new VolumeName("Media Source",menuChannel);
 volumeName->setFixedHeight(24);menuChannelLayout->addWidget(volumeName);
 menuLayout->addWidget(menuChannel);menuMixer.resize(140,140);menuMixer.show();app.processEvents();
 auto *label=volumeName->findChild<QLabel*>();REQUIRE(label);
 int clicks=0;QObject::connect(volumeName,&QAbstractButton::clicked,[&]{++clicks;});
 auto palette=volumeName->palette();palette.setColor(QPalette::ButtonText,QColor("#ff00ff"));
 volumeName->setPalette(palette);
 const auto originalArrow=volumeName->grab().toImage().copy(volumeName->indicatorRect());
 augment::FrontAudioControls menuUi(tmp.filePath("menu-test.json"),[&]{return current;},{},{});
 REQUIRE(menuUi.attach(&menuMixer));app.processEvents();
 const auto glyph=volumeName->grab().toImage().copy(volumeName->indicatorRect());
 REQUIRE(glyph!=originalArrow);
 // Three disjoint painted runs down the vertical center, not a triangle.
 const auto ink=QColor("#ff00ff");
 int runs=0;bool previous=false;
 for(int y=0;y<glyph.height();++y){
  bool hit=false;for(int x=0;x<glyph.width();++x){const auto c=glyph.pixelColor(x,y);hit|=c.red()>180&&c.blue()>180&&c.green()<100;}
  if(hit&&!previous)++runs;previous=hit;
 }
 REQUIRE(runs==3);
 REQUIRE(volumeName->findChild<QLabel*>()==label&&volumeName->text()=="Media Source");
 volumeName->click();REQUIRE(clicks==1);
 menuUi.detach();app.processEvents();
 const auto restoredArrow=volumeName->grab().toImage().copy(volumeName->indicatorRect());
 REQUIRE(restoredArrow==originalArrow);
 // Real OBS has narrow side-by-side VolumeControl columns. Both
 // adjustment rows must have an actual hit target at 90px column width.
 QWidget narrow;
 auto *columns=new QHBoxLayout(&narrow);
 columns->setContentsMargins(0,0,0,0);
 for(const QString &name:{"Renamed TTS","Media Source"}){
     auto *volume=new QWidget(&narrow);
     volume->setObjectName("VolumeControl");
     auto *v=new QVBoxLayout(volume);
     auto *n=new QToolButton(volume);
     n->setObjectName("VolumeName");n->setText(name);v->addWidget(n);
     auto *fader=new QSlider(Qt::Vertical,volume);
     fader->setMinimumHeight(120);v->addWidget(fader,1);
     columns->addWidget(volume,1);
 }
 narrow.resize(190,330);
 narrow.show();app.processEvents();
 augment::FrontAudioControls narrowUi(tmp.filePath("narrow.json"),[&]{return current;},{},{});
 REQUIRE(narrowUi.attach(&narrow));
 app.processEvents();
 REQUIRE(narrowUi.rowCount()==2);
 auto *visiblePan=narrow.findChild<QSlider*>("aftermarket-pan-slider");
 auto *visibleOffset=narrow.findChild<QSpinBox*>("aftermarket-offset-ms");
 REQUIRE(visiblePan&&visibleOffset);
 auto *compactDial=narrow.findChild<QDial*>("aftermarket-pan-dial");
 REQUIRE(compactDial);
 auto *compactAddon=narrow.findChild<QWidget*>("aftermarket-source-adjustments");
 REQUIRE(compactAddon&&compactAddon->minimumHeight()<=34);
 REQUIRE(compactDial->isVisible()&&!visiblePan->isVisible()); // Narrow vertical channels use a real pan target
 REQUIRE(visibleOffset->isVisible());
 REQUIRE(qAbs(visiblePan->mapTo(compactAddon,QPoint(0,0)).y()-visibleOffset->mapTo(compactAddon,QPoint(0,0)).y())<=14);
 REQUIRE(visiblePan->minimumHeight()>=18);
 REQUIRE(visiblePan->height()>=14);
 REQUIRE(visibleOffset->height()>=20);
 REQUIRE(visiblePan->mapTo(&narrow,QPoint(visiblePan->width(),0)).x()<=narrow.width());
 // Side-by-side strips reorder without changing their original stretch.
 auto *left=columns->itemAt(0)->widget(),*right=columns->itemAt(1)->widget();
 QMenu narrowOrder;
 narrowUi.populateSettingsMenu(&narrowOrder);
 QAction *narrowUnlock=nullptr;
 for(auto *a:narrowOrder.actions())if(a->text()=="Unlock channel ordering")narrowUnlock=a;
 REQUIRE(narrowUnlock);
 narrowUnlock->trigger();
 drag(right->findChild<QToolButton*>("VolumeName"),left);
 REQUIRE(columns->itemAt(0)->widget()==right&&columns->itemAt(1)->widget()==left);
 REQUIRE(columns->stretch(0)==1&&columns->stretch(1)==1);
 QMenu resetMenu;narrowUi.populateSettingsMenu(&resetMenu);
 QAction *reset=nullptr;
 for(auto *a:resetMenu.actions())if(a->text()=="Reset channel order")reset=a;
 REQUIRE(reset&&reset->isEnabled());reset->trigger();
 REQUIRE(columns->itemAt(0)->widget()==left&&columns->itemAt(1)->widget()==right);

 // Overflow in the real dock title exposes precise per-channel settings.
 QMenu channelMenu;
 ui.populateSettingsMenu(&channelMenu);
 auto *channels=channelMenu.findChild<QMenu*>("aftermarket-channel-settings");
 REQUIRE(channels&&channels->actions().size()==2);
 auto *panEdit=channelMenu.findChild<QSpinBox*>("aftermarket-menu-pan");
 auto *offsetEdit=channelMenu.findChild<QSpinBox*>("aftermarket-menu-offset");
 REQUIRE(panEdit&&offsetEdit);
 REQUIRE(panEdit->value()==44&&offsetEdit->value()==600);
 panEdit->setValue(36);
 REQUIRE(qAbs(current[0].balance-.36)<.001);
 offsetEdit->setValue(150);
 REQUIRE(current[0].offsetMs==150);
 narrowUi.detach();
 // OBS's alternate mixer orientation can use a horizontal VolumeControl
 // layout. Keep native gain and our inline pan/sync on the same row.
 QWidget horizontal;
 auto *hRoot=new QVBoxLayout(&horizontal);
 auto *hVolume=new QWidget(&horizontal);hVolume->setObjectName("VolumeControl");
 auto *hRow=new QHBoxLayout(hVolume);
 auto *hName=new QToolButton(hVolume);
 hName->setObjectName("VolumeName");hName->setText("Media Source");
 hRow->addWidget(hName);
 auto *hGain=new QSlider(Qt::Horizontal,hVolume);
 hGain->setValue(63);hRow->addWidget(hGain);
 hRoot->addWidget(hVolume);
 horizontal.resize(530,130);horizontal.show();app.processEvents();
 augment::FrontAudioControls hUi(tmp.filePath("horizontal.json"),[&]{return current;},{},{});
 REQUIRE(hUi.attach(&horizontal));app.processEvents();
 auto *hAdd=hVolume->findChild<QWidget*>("aftermarket-source-adjustments");
 auto *hPan=hVolume->findChild<QSlider*>("aftermarket-pan-slider");
 auto *hSync=hVolume->findChild<QSpinBox*>("aftermarket-offset-ms");
 REQUIRE(hAdd&&hPan&&hSync);
 REQUIRE(hAdd->isVisible()&&hPan->isVisible()&&hSync->isVisible());
 REQUIRE(qAbs(hPan->mapTo(hAdd,QPoint(0,0)).y()-
              hSync->mapTo(hAdd,QPoint(0,0)).y())<=14);
 hUi.detach();app.processEvents();
 REQUIRE(hRow->count()==2&&hGain->value()==63);
 // Match the installed native horizontal rotary channel structure.
 QWidget rotary;auto *rotaryRoot=new QVBoxLayout(&rotary);
 auto *channel=new QWidget(&rotary);channel->setObjectName("VolumeControl");rotaryRoot->addWidget(channel);
 auto *channelBox=new QVBoxLayout(channel);
 auto *header=new QHBoxLayout;channelBox->addLayout(header,3);
 auto *channelName=new QToolButton(channel);channelName->setObjectName("VolumeName");channelName->setText("Media Source");header->addWidget(channelName);
 auto *statusRow=new QHBoxLayout;header->addLayout(statusRow);statusRow->addStretch();
 auto *status=new QLabel("Pinned",channel);statusRow->addWidget(status);
 auto *db=new QLabel("-8.7 dB",channel);db->setObjectName("volLabel");statusRow->addWidget(db);
 auto *bodyRow=new QHBoxLayout;channelBox->addLayout(bodyRow,6);
 auto *leftButtons=new QVBoxLayout;bodyRow->addLayout(leftButtons);
 auto *mute=new QPushButton("Mute",channel),*monitor=new QPushButton("Monitor",channel);
 mute->setToolTip("Mute");monitor->setToolTip("Enable Monitoring");leftButtons->addWidget(mute);leftButtons->addWidget(monitor);
 auto *frame=new QFrame(channel);frame->setObjectName("volMeterFrame");auto *frameBox=new QVBoxLayout(frame);
 auto *gain=new QSlider(Qt::Horizontal,frame);gain->setProperty("_obsUiAugmentKnob",true);gain->setMinimumHeight(68);gain->setValue(51);frameBox->addWidget(gain);bodyRow->addWidget(frame);
 rotary.resize(360,200);rotary.show();app.processEvents();
 augment::FrontAudioControls rotaryUi(tmp.filePath("rotary.json"),[&]{return current;},{},{});
 REQUIRE(rotaryUi.attach(&rotary));REQUIRE(rotaryUi.setKnobs(true));app.processEvents();rotaryUi.refresh();app.processEvents();
 auto *center=channel->findChild<QBoxLayout*>("aftermarket-rotary-center");
 auto *adjust=channel->findChild<QWidget*>("aftermarket-source-adjustments");
 auto *panRight=channel->findChild<QDial*>("aftermarket-pan-dial");
 auto *syncRight=channel->findChild<QDial*>("aftermarket-offset-dial");
 auto *precise=channel->findChild<QSpinBox*>("aftermarket-offset-ms");
 REQUIRE(center&&center->itemAt(0)->widget()==frame&&center->itemAt(1)->widget()==db);
 REQUIRE(bodyRow->indexOf(adjust)>=0&&panRight->isVisible()&&syncRight->isVisible()&&!precise->isVisible());
 REQUIRE(panRight->mapTo(channel,QPoint()).x()>gain->mapTo(channel,QPoint()).x());
 REQUIRE(panRight->mapTo(channel,QPoint()).y()<syncRight->mapTo(channel,QPoint()).y());
 REQUIRE(status->text()=="Pinned"&&status->maximumWidth()<=20&&header->stretch(0)==1);
 REQUIRE(gain->value()==51);
 REQUIRE(QMetaObject::invokeMethod(syncRight,"customContextMenuRequested",Qt::DirectConnection,Q_ARG(QPoint,QPoint(8,8))));
 auto *preciseMenu=syncRight->findChild<QMenu*>("aftermarket-precise-sync-menu");
 auto *preciseValue=preciseMenu?preciseMenu->findChild<QSpinBox*>("aftermarket-precise-sync-value"):nullptr;
 REQUIRE(preciseValue&&preciseValue->maximum()==999999&&preciseValue->minimum()==-999999);
 preciseValue->setValue(12345);REQUIRE(precise->value()==12345);
 preciseMenu->close();app.processEvents();
 status->setText("Active");rotaryUi.refresh();app.processEvents();
 REQUIRE(status->text()=="Active"&&status->maximumWidth()<=20);
 status->setText("Unknown status");rotaryUi.refresh();
 REQUIRE(status->maximumWidth()>20);
 status->setText("Active");rotaryUi.refresh();
 gain->setProperty("_obsUiAugmentKnob",false);rotaryUi.refresh();app.processEvents();
 REQUIRE(!channel->findChild<QBoxLayout*>("aftermarket-rotary-center"));
 REQUIRE(statusRow->indexOf(db)>=0&&bodyRow->indexOf(frame)>=0&&channelBox->indexOf(adjust)>=0);
 REQUIRE(status->maximumWidth()>20&&precise->isVisible());
 gain->setProperty("_obsUiAugmentKnob",true);rotaryUi.refresh();app.processEvents();
 REQUIRE(channel->findChild<QBoxLayout*>("aftermarket-rotary-center"));
 rotaryUi.detach();app.processEvents();
 REQUIRE(statusRow->indexOf(db)>=0&&bodyRow->indexOf(frame)>=0&&status->maximumWidth()>20&&gain->value()==51&&header->stretch(0)==0);
 // Native side-by-side channels have a 110px cap, a vertical gain
 // slider and separate centered status/name/dB/button layout rows.
 QWidget columnMixer;auto *columnsRoot=new QHBoxLayout(&columnMixer);
 auto *strip=new QWidget(&columnMixer);strip->setObjectName("VolumeControl");strip->setMaximumWidth(110);columnsRoot->addWidget(strip);
 auto *stripRoot=new QVBoxLayout(strip);stripRoot->setContentsMargins(0,0,0,0);
 auto *stripStatus=new QLabel("Pinned",strip);auto *sr=new QHBoxLayout;sr->addWidget(stripStatus);stripRoot->addLayout(sr);
 auto *stripName=new QToolButton(strip);stripName->setObjectName("VolumeName");stripName->setText("Media Source");
 auto *nr=new QHBoxLayout;nr->addWidget(stripName);stripRoot->addLayout(nr);
 auto *stripDb=new QLabel("-8.7 dB",strip);stripDb->setObjectName("volLabel");
 auto *dr=new QHBoxLayout;dr->addWidget(stripDb);dr->addStretch();stripRoot->addLayout(dr);
 auto *stripFrame=new QFrame(strip);stripFrame->setObjectName("volMeterFrame");auto *fl=new QHBoxLayout(stripFrame);
 auto *stripGain=new QSlider(Qt::Vertical,stripFrame);stripGain->setProperty("_obsUiAugmentKnob",true);stripGain->setProperty("_obsUiAugmentIntegratedMeter",true);
 stripGain->setFixedSize(68,76);stripGain->setValue(63);fl->addWidget(stripGain);stripRoot->addWidget(stripFrame,1);
 auto *br=new QHBoxLayout;auto *sm=new QPushButton(strip),*sn=new QPushButton(strip);br->setSpacing(2);sm->setFixedSize(18,22);sn->setFixedSize(18,22);br->addWidget(sm);br->addWidget(sn);stripRoot->addLayout(br);
 columnMixer.resize(360,500);columnMixer.show();app.processEvents();
 augment::FrontAudioControls columnUi(tmp.filePath("columnMixer.json"),[&]{return current;},{},{});
 REQUIRE(columnUi.attach(&columnMixer));REQUIRE(columnUi.setKnobs(true));columnUi.refresh();app.processEvents();
 auto *columnPan=strip->findChild<QDial*>("aftermarket-pan-dial");
 auto *columnSync=strip->findChild<QDial*>("aftermarket-offset-dial");
 auto *columnSpin=strip->findChild<QSpinBox*>("aftermarket-offset-ms");
 REQUIRE(columnPan&&columnSync&&columnSpin&&columnPan->isVisible()&&columnSync->isVisible()&&!columnSpin->isVisible());
 // Native buttons at left, gain at center, and vertically stacked
 // pan/sync at right; all hit targets fit the widened compact strip.
 const auto middle=stripGain->mapTo(strip,stripGain->rect().center());
 const auto ml=sm->mapTo(strip,sm->rect().center()),nl=sn->mapTo(strip,sn->rect().center());
 const auto pr=columnPan->mapTo(strip,columnPan->rect().center()),srp=columnSync->mapTo(strip,columnSync->rect().center());
 REQUIRE(ml.x()<middle.x()&&nl.x()<middle.x()&&pr.x()>middle.x()&&srp.x()>middle.x());
 REQUIRE(ml.x()==nl.x()&&pr.x()==srp.x()&&ml.y()<nl.y()&&pr.y()<srp.y());
 REQUIRE(qAbs(ml.y()-pr.y())<=6&&qAbs(nl.y()-srp.y())<=6);
 REQUIRE(sm->mapTo(strip,QPoint()).x()>=0&&columnPan->mapTo(strip,QPoint(columnPan->width(),0)).x()<=strip->width());
 REQUIRE(br->direction()==QBoxLayout::TopToBottom&&strip->width()>=118);

 REQUIRE(stripStatus->maximumWidth()==18);
 REQUIRE(stripDb->mapTo(strip,QPoint()).y()>stripGain->mapTo(strip,QPoint()).y()+stripGain->height()-1);
 REQUIRE(stripDb->mapTo(strip,QPoint()).y()-stripGain->mapTo(strip,QPoint()).y()-stripGain->height()<30);
 REQUIRE(stripGain->value()==63&&stripGain->parentWidget()==stripFrame);
 stripGain->setProperty("_obsUiAugmentKnob",false);stripGain->setProperty("_obsUiAugmentIntegratedMeter",false);columnUi.refresh();app.processEvents();
 REQUIRE(dr->indexOf(stripDb)==0&&stripRoot->stretch(stripRoot->indexOf(stripFrame))==1&&br->direction()==QBoxLayout::LeftToRight&&strip->maximumWidth()==110);
 REQUIRE(columnSync->isVisible()&&!columnSpin->isVisible());
 columnUi.setKnobs(false);app.processEvents();REQUIRE(columnSpin->isVisible());
 columnUi.detach();app.processEvents();
 REQUIRE(stripRoot->count()==5&&dr->indexOf(stripDb)==0&&stripStatus->maximumWidth()>18&&stripGain->value()==63&&br->spacing()==2);
 // Responsive grid keeps the real horizontal mixer container and channel
 // widgets, and restores the original native box on narrow/disabled views.
 QWidget gridMixer;
 auto *gridRoot=new QVBoxLayout(&gridMixer);
 auto *scroll=new QScrollArea(&gridMixer);scroll->setObjectName("hMixerScrollArea");scroll->setWidgetResizable(true);
 scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);gridRoot->addWidget(scroll);
 auto *host=new QWidget;host->setObjectName("hVolumeWidgets");
 auto *nativeBox=new QVBoxLayout(host);nativeBox->setObjectName("hVolControlLayout");
 nativeBox->setContentsMargins(0,0,0,0);nativeBox->setSpacing(0);
 scroll->setWidget(host);
 auto addChannel=[&](const QString &name){
     auto *w=new QWidget(host);w->setObjectName("VolControl");
     auto *v=new QVBoxLayout(w);auto *n=new QLabel(name,w);n->setObjectName("OBSSourceLabel");v->addWidget(n);
     auto *f=new QSlider(Qt::Horizontal,w);f->setValue(42);v->addWidget(f);
     nativeBox->addWidget(w);return w;
 };
 auto *g1=addChannel("Renamed TTS"),*g2=addChannel("Media Source");
 gridMixer.resize(850,400);gridMixer.show();app.processEvents();
 augment::FrontAudioControls gridUi(tmp.filePath("grid.json"),[&]{return current;},{},{});
 REQUIRE(gridUi.attach(&gridMixer));app.processEvents();
 auto *grid=host->findChild<QGridLayout*>("aftermarket-mixer-grid");
 REQUIRE(grid&&grid->itemAtPosition(0,0)->widget()==g1&&grid->itemAtPosition(0,1)->widget()==g2);
 REQUIRE(g1->parentWidget()==host&&g2->parentWidget()==host);
 REQUIRE(g1->geometry().top()==g2->geometry().top());
 REQUIRE(g1->findChild<QSlider*>()->value()==42);
 gridMixer.resize(360,400);app.processEvents();gridUi.refresh();app.processEvents();
 REQUIRE(!host->findChild<QGridLayout*>("aftermarket-mixer-grid"));
 REQUIRE(nativeBox->itemAt(0)->widget()==g1&&nativeBox->itemAt(1)->widget()==g2);
 gridMixer.resize(850,400);app.processEvents();gridUi.refresh();app.processEvents();
 REQUIRE(host->findChild<QGridLayout*>("aftermarket-mixer-grid"));
 // OBS re-adds its existing channel objects to the original box.
 nativeBox->addWidget(g1);nativeBox->addWidget(g2);
 auto *g3=addChannel("New source");
 current.append({"uuid-new","New source",.5,0,true});app.processEvents();gridUi.refresh();app.processEvents();
 grid=host->findChild<QGridLayout*>("aftermarket-mixer-grid");
 REQUIRE(grid&&grid->count()==3);
 REQUIRE(grid->itemAtPosition(0,0)->widget()==g1);
 REQUIRE(grid->itemAtPosition(0,1)->widget()==g2);
 REQUIRE(g3->parentWidget()==host);
 // Repeated audio-state polling should not rebuild a stable grid.
 QPointer<QGridLayout> stableGrid=grid;
 gridUi.refresh();gridUi.refresh();app.processEvents();
 REQUIRE(stableGrid&&stableGrid==host->findChild<QGridLayout*>("aftermarket-mixer-grid"));
 QMenu gridOrder;gridUi.populateSettingsMenu(&gridOrder);
 QAction *gridUnlock=nullptr;
 for(auto *a:gridOrder.actions())if(a->text()=="Unlock channel ordering")gridUnlock=a;
 REQUIRE(gridUnlock);gridUnlock->trigger();
 drag(g3->findChild<QLabel*>("OBSSourceLabel"),g1);
 grid=host->findChild<QGridLayout*>("aftermarket-mixer-grid");
 REQUIRE(grid&&grid->itemAtPosition(0,0)->widget()==g3);
 REQUIRE(gridUi.channelOrder().first()=="uuid-new");
 gridUnlock->trigger();
 QMenu gridReset;gridUi.populateSettingsMenu(&gridReset);
 QAction *resetGridOrder=nullptr;
 for(auto *a:gridReset.actions())if(a->text()=="Reset channel order")resetGridOrder=a;
 REQUIRE(resetGridOrder&&resetGridOrder->isEnabled());resetGridOrder->trigger();
 grid=host->findChild<QGridLayout*>("aftermarket-mixer-grid");
 REQUIRE(grid&&grid->itemAtPosition(0,0)->widget()==g1&&gridUi.channelOrder().isEmpty());
 QMenu gridMenu;gridUi.populateSettingsMenu(&gridMenu);
 QAction *autoGrid=nullptr;
 for(auto *a:gridMenu.actions())if(a->text()=="Auto mixer grid")autoGrid=a;
 REQUIRE(autoGrid&&autoGrid->isChecked());
 autoGrid->trigger();app.processEvents();
 REQUIRE(!host->findChild<QGridLayout*>("aftermarket-mixer-grid"));
 REQUIRE(nativeBox->itemAt(0)->widget()==g1&&nativeBox->itemAt(1)->widget()==g2);
 autoGrid->trigger();app.processEvents();
 REQUIRE(host->findChild<QGridLayout*>("aftermarket-mixer-grid"));
 // Exercise the resize debounce without forcing a state refresh.
 gridMixer.resize(360,400);
 QEventLoop gridSettle;QTimer::singleShot(150,&gridSettle,&QEventLoop::quit);gridSettle.exec();
 REQUIRE(!host->findChild<QGridLayout*>("aftermarket-mixer-grid"));
 gridMixer.resize(850,400);
 QTimer::singleShot(150,&gridSettle,&QEventLoop::quit);gridSettle.exec();
 REQUIRE(host->findChild<QGridLayout*>("aftermarket-mixer-grid"));
 autoGrid->trigger();gridUi.detach();app.processEvents();
 REQUIRE(nativeBox->count()==3&&nativeBox->itemAt(2)->widget()==g3);
 augment::FrontAudioControls gridRestored(tmp.filePath("grid.json"),[&]{return current;},{},{});
 REQUIRE(gridRestored.attach(&gridMixer));app.processEvents();
 REQUIRE(!host->findChild<QGridLayout*>("aftermarket-mixer-grid"));
 QMenu persistedGridMenu;gridRestored.populateSettingsMenu(&persistedGridMenu);
 QAction *persistedGrid=nullptr;
 for(auto *a:persistedGridMenu.actions())if(a->text()=="Auto mixer grid")persistedGrid=a;
 REQUIRE(persistedGrid&&!persistedGrid->isChecked());gridRestored.detach();
 current.removeLast();
 // A long source name must not determine vertical mixer channel width.
 // OBS 32.x uses VolControl + OBSSourceLabel (QLabel), while older
 // integrations can expose VolumeControl + VolumeName (QToolButton).
 QWidget nativeChannels;
 auto *nativeColumns=new QHBoxLayout(&nativeChannels);
 nativeColumns->setContentsMargins(0,0,0,0);
 auto makeNativeColumn=[&](const QString &name){
     auto *vol=new QFrame(&nativeChannels);
     vol->setObjectName("VolControl");
     vol->setMaximumWidth(110); // same OBS maximum, no fixed target width
     auto *vl=new QVBoxLayout(vol);
     vl->setContentsMargins(0,0,0,0);
     auto *label=new QLabel(name,vol);
     label->setObjectName("OBSSourceLabel");
     vl->addWidget(label);
     auto *meter=new QSlider(Qt::Vertical,vol);
     meter->setFixedSize(28,112);
     vl->addWidget(meter);
     nativeColumns->addWidget(vol,0,Qt::AlignLeft);
     return qMakePair(vol,label);
 };
 auto longChannel=makeNativeColumn("An Extremely Long Microphone Source Name That Should Never Stretch Its Channel");
 auto shortChannel=makeNativeColumn("Live Latent: TTS");
 nativeChannels.resize(320,230);
 nativeChannels.show();app.processEvents();
 const auto longOriginalPolicy=longChannel.second->sizePolicy();
 const auto shortOriginalPolicy=shortChannel.second->sizePolicy();
 const int originalLongMinimum=longChannel.second->minimumWidth();
 const int originalShortMinimum=shortChannel.second->minimumWidth();
 REQUIRE(longChannel.first->sizeHint().width()>shortChannel.first->sizeHint().width());
 augment::FrontAudioControls widths(tmp.filePath("channel-widths.json"),
     []{return QList<augment::AudioPropertyState>{};},{},{});
 REQUIRE(widths.attach(&nativeChannels));
 app.processEvents();
 REQUIRE(longChannel.second->sizePolicy().horizontalPolicy()==QSizePolicy::Ignored);
 REQUIRE(shortChannel.second->sizePolicy().horizontalPolicy()==QSizePolicy::Ignored);
 REQUIRE(longChannel.second->minimumWidth()==shortChannel.second->minimumWidth());
 // OBS itself caps these vertical channels at 110 px. Use their available
 // width, not an arbitrary 68px placeholder that pointlessly elides TTS.
 REQUIRE(longChannel.second->minimumWidth()==longChannel.first->maximumWidth());
 REQUIRE(shortChannel.second->minimumWidth()==shortChannel.first->maximumWidth());
 REQUIRE(longChannel.second->width()>=100&&shortChannel.second->width()>=100);
 if(shortChannel.second->fontMetrics().horizontalAdvance(shortChannel.second->text())
       <=shortChannel.first->maximumWidth())
     REQUIRE(shortChannel.second->fontMetrics().horizontalAdvance(shortChannel.second->text())
             <=shortChannel.second->contentsRect().width());
 REQUIRE(longChannel.first->sizeHint().width()==shortChannel.first->sizeHint().width());
 const QString longExact=longChannel.second->text();
 longChannel.second->setText(longExact+" [new descriptive suffix]");
 widths.refresh();app.processEvents();
 REQUIRE(longChannel.second->text()==longExact+" [new descriptive suffix]");
 REQUIRE(longChannel.first->sizeHint().width()==shortChannel.first->sizeHint().width());
 widths.detach();app.processEvents();
 REQUIRE(longChannel.second->sizePolicy()==longOriginalPolicy);
 REQUIRE(shortChannel.second->sizePolicy()==shortOriginalPolicy);
 REQUIRE(longChannel.second->minimumWidth()==originalLongMinimum);
 REQUIRE(shortChannel.second->minimumWidth()==originalShortMinimum);
 REQUIRE(longChannel.first->sizeHint().width()>shortChannel.first->sizeHint().width());
 // In a side-by-side layout native source button labels must also be
 // width-neutral, while the real source-name strings remain unmodified.
 QWidget legacyChannels;
 auto *legacyColumns=new QHBoxLayout(&legacyChannels);
 legacyColumns->setContentsMargins(0,0,0,0);
 QToolButton *wideButton=nullptr;
 for(const QString &name:{"A long source label that previously stretched mixer channels","Short"}){
     auto *vol=new QWidget(&legacyChannels);
     vol->setObjectName("VolumeControl");
     auto *vl=new QVBoxLayout(vol);
     vl->setContentsMargins(0,0,0,0);
     auto *label=new QToolButton(vol);
     label->setObjectName("VolumeName");
     label->setText(name);
     vl->addWidget(label);
     auto *fader=new QSlider(Qt::Vertical,vol);
     fader->setFixedSize(28,110);
     vl->addWidget(fader);
     legacyColumns->addWidget(vol,0,Qt::AlignLeft);
     if(!wideButton)wideButton=label;
 }
 legacyChannels.resize(320,230);legacyChannels.show();app.processEvents();
 augment::FrontAudioControls legacyWidth(tmp.filePath("legacy-channel-widths.json"),
    []{return QList<augment::AudioPropertyState>{};},{},{});
 REQUIRE(legacyWidth.attach(&legacyChannels));app.processEvents();
 REQUIRE(wideButton->sizePolicy().horizontalPolicy()==QSizePolicy::Ignored);
 REQUIRE(wideButton->text().startsWith("A long source label"));
 legacyWidth.detach();app.processEvents();
 REQUIRE(wideButton->sizePolicy().horizontalPolicy()!=QSizePolicy::Ignored);
 // Horizontal OBS mixer layout: source-name width must not override the
 // available gain/meter row width either. No fixed width on the entire
 // horizontal channel, so resizing the dock still works normally.
 QWidget horizontalNative;
 auto *horizontalRoot=new QVBoxLayout(&horizontalNative);
 horizontalRoot->setContentsMargins(0,0,0,0);
 auto makeHorizontal=[&](const QString &name){
     auto *vol=new QFrame(&horizontalNative);
     vol->setObjectName("VolControl");
     auto *layout=new QVBoxLayout(vol);
     layout->setContentsMargins(0,0,0,0);
     auto *textRow=new QHBoxLayout;
     textRow->setContentsMargins(0,0,0,0);
     auto *label=new QLabel(name,vol);
     label->setObjectName("OBSSourceLabel");
     textRow->addWidget(label);
     textRow->addWidget(new QLabel("-10 dB",vol));
     layout->addLayout(textRow);
     auto *fader=new QSlider(Qt::Horizontal,vol);
     fader->setFixedWidth(148);
     layout->addWidget(fader);
     horizontalRoot->addWidget(vol);
     return qMakePair(vol,label);
 };
 auto horizontalLong=makeHorizontal(
     "A long horizontally oriented OBS audio source name that must not expand its mixer row");
 auto horizontalShort=makeHorizontal("TTS");
 horizontalNative.resize(380,200);horizontalNative.show();app.processEvents();
 REQUIRE(horizontalLong.first->sizeHint().width()>horizontalShort.first->sizeHint().width());
 const auto horizontalOriginal=horizontalLong.second->sizePolicy();
 augment::FrontAudioControls hWidths(tmp.filePath("horizontal-native-widths.json"),
     []{return QList<augment::AudioPropertyState>{};},{},{});
 REQUIRE(hWidths.attach(&horizontalNative));app.processEvents();
 REQUIRE(horizontalLong.first->sizeHint().width()==horizontalShort.first->sizeHint().width());
 REQUIRE(horizontalLong.second->width()>=120); // existing meter row offers ample width
 REQUIRE(horizontalLong.second->text().contains("must not expand"));
 horizontalLong.second->setText("Updated very long horizontal source name");
 hWidths.refresh();app.processEvents();
 REQUIRE(horizontalLong.first->sizeHint().width()==horizontalShort.first->sizeHint().width());
 hWidths.detach();app.processEvents();
 REQUIRE(horizontalLong.second->sizePolicy()==horizontalOriginal);
 REQUIRE(horizontalLong.first->sizeHint().width()>horizontalShort.first->sizeHint().width());
 std::cout<<"native vertical and horizontal Audio Mixer channel widths are name-independent; reversible layout verified\n";
 return 0;
}

#include "front-audio-controls-test.moc"
