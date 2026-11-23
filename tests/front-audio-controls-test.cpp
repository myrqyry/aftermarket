#include "front-audio-controls.h"
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QToolButton>
#include <QSlider>
#include <QDial>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QFile>
#include <QLabel>
#include <QEventLoop>
#include <QTimer>
#include <QMenu>
#include <QToolBar>
#include <QPushButton>
#include <QAction>
#include <iostream>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
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
 panDial->setValue(65);offsetDial->setValue(1000);
 REQUIRE(pans==2&&offsets==2);
 REQUIRE(qAbs(current[0].balance-.65)<.001&&current[0].offsetMs==1000);
 REQUIRE(offset->value()==1000&&pan->value()==65); // immediate sibling control sync
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
 REQUIRE(original->value()==52&&secondOriginal->value()==65);
 // State survives a restart without requiring OBS's own rules or mixer state.
 augment::FrontAudioControls restored(file,[&]{return current;},{},{});
 REQUIRE(!restored.knobs());
 REQUIRE(restored.attach(&native));
 REQUIRE(restored.rowCount()==1);
 restored.detach();
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
 std::cout<<"native Audio Mixer front pan/offset, knob mode and reversible lifecycle verified\n";
 return 0;
}
