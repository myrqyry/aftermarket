#include "dock-title-icons.h"
#include "dock-faces.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QToolButton>
#include <QStyleOptionDockWidget>
#include <QStyle>
#include <QFontMetrics>
#include <QEventLoop>
#include <QTimer>
#include <QMouseEvent>
#include <QImage>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)

int main(int argc,char **argv){
 QApplication app(argc,argv);app.setStyle("Fusion");
 using Kind=augment::DockTitleIcons::Kind;
 REQUIRE(augment::DockTitleIcons::classify("scenesDock","Escenas")==Kind::Scenes);
 REQUIRE(augment::DockTitleIcons::classify("sourcesDock","Sources")==Kind::Sources);
 REQUIRE(augment::DockTitleIcons::classify("mixerDock","Audio Mixer")==Kind::Audio);
 REQUIRE(augment::DockTitleIcons::classify("transitionsDock","Scene Transitions")==Kind::Transitions);
 REQUIRE(augment::DockTitleIcons::classify("controlsDock","Controls")==Kind::Controls);
 REQUIRE(augment::DockTitleIcons::classify("aftermarket-preview-dock","Preview")==Kind::Preview);
 REQUIRE(augment::DockTitleIcons::classify("thirdPartyDock","Twitch Chat")==Kind::Chat);
 REQUIRE(augment::DockTitleIcons::classify("thirdPartyDock","Browser Widgets")==Kind::Browser);
 REQUIRE(augment::DockTitleIcons::classify("notRecognized","Custom Suite")==Kind::Generic);
 // Native dock title bars can leave a sliver of space beside long labels.
 // Use a smaller glyph only when it fits; never collide with Qt controls.
 REQUIRE(augment::DockTitleIcons::glyphSizeForRoom(21)==14);
 REQUIRE(augment::DockTitleIcons::glyphSizeForRoom(14)==10);
 REQUIRE(augment::DockTitleIcons::glyphSizeForRoom(10)==0);
 REQUIRE(augment::DockTitleIcons::titleTextInset()==20);
 QMainWindow main;main.setCentralWidget(new QLabel("Preview",&main));
 struct Dock {QDockWidget *dock;QString title;QWidget *nativeTitle;};
 QList<Dock> docks;
 const struct {const char *id;const char *title; Qt::DockWidgetArea area;} specs[]={
    {"scenesDock","Scenes",Qt::LeftDockWidgetArea},
    {"sourcesDock","Sources",Qt::LeftDockWidgetArea},
    {"mixerDock","Audio Mixer",Qt::RightDockWidgetArea},
    {"transitionsDock","Scene Transitions",Qt::RightDockWidgetArea},
    {"controlsDock","Controls",Qt::RightDockWidgetArea},
 };
 for(const auto &spec:specs){
    auto *dock=new QDockWidget(spec.title,&main);
    dock->setObjectName(spec.id);
    dock->setMinimumWidth(230);
    dock->setWidget(new QLabel(spec.title,dock));
    main.addDockWidget(spec.area,dock);
    docks.append({dock,spec.title,dock->titleBarWidget()});
 }
 main.resize(1350,720);main.show();app.processEvents();
 {
   augment::DockTitleIcons icons(&main);
   icons.refresh();app.processEvents();
   REQUIRE(icons.decoratedCount()==5);
   QList<QImage> pictures;
   for(const auto &entry:docks){
      auto *dock=entry.dock;
      auto *glyph=icons.glyph(dock);
      REQUIRE(glyph&&glyph->parentWidget()==dock);
      REQUIRE(glyph->objectName()=="aftermarket-dock-title-icon");
      REQUIRE(glyph->testAttribute(Qt::WA_TransparentForMouseEvents));
      REQUIRE(glyph->isVisible());
      REQUIRE(glyph->width()>=12&&glyph->height()>=12);
      REQUIRE(icons.originalTitle(dock)==entry.title);
      REQUIRE(dock->windowTitle().trimmed()==entry.title);
      REQUIRE(dock->toggleViewAction()->text()==entry.title);
      REQUIRE(dock->titleBarWidget()==entry.nativeTitle);
      REQUIRE(glyph->geometry().bottom()<dock->widget()->geometry().top());
      QStyleOptionDockWidget option;option.initFrom(dock);
      option.title=dock->windowTitle();
      option.closable=dock->features()&QDockWidget::DockWidgetClosable;
      option.floatable=dock->features()&QDockWidget::DockWidgetFloatable;
      option.movable=dock->features()&QDockWidget::DockWidgetMovable;
      auto region=dock->style()->subElementRect(QStyle::SE_DockWidgetTitleBarText,&option,dock);
      const int start=region.left();
      const int nativeTextWidth=QFontMetrics(dock->font()).horizontalAdvance(dock->windowTitle());
      // Icon is BEFORE (left of) native text; reserve actual painted space.
      REQUIRE(glyph->x()>=start);
      REQUIRE(glyph->geometry().right()<=start+augment::DockTitleIcons::titleTextInset()-2);
      REQUIRE(glyph->geometry().right()<start+nativeTextWidth);
      REQUIRE(icons.titleIndented(dock));
      REQUIRE(dock->toggleViewAction()->text()==entry.title);
      pictures.append(glyph->grab().toImage());
   }
   REQUIRE(pictures[0]!=pictures[1]);
   REQUIRE(pictures[1]!=pictures[2]);
   for(const auto &entry:docks){
      const auto shown=entry.dock->windowTitle();
      for(int i=0;i<8;++i){icons.refresh();app.processEvents();}
      REQUIRE(entry.dock->windowTitle()==shown); // never accumulate padding
      REQUIRE(entry.dock->toggleViewAction()->text()==entry.title);
   }
   // Existing controls must reserve room for the icon rather than covering it.
   auto *mixer=docks[2].dock;
   augment::DockFaces faces;
   REQUIRE(faces.attach(mixer,new QLabel("AudioHub")));
   icons.refresh();app.processEvents();
   auto *flip=mixer->findChild<QToolButton*>("aftermarket-face-flip");
   auto *glyph=icons.glyph(mixer);
   REQUIRE(flip&&glyph);
   REQUIRE(!flip->isVisible()||flip->geometry().left()>
      glyph->x()+augment::DockTitleIcons::titleTextInset()+
      QFontMetrics(mixer->font()).horizontalAdvance(mixer->windowTitle()));
   // Theme-derived glyph color, not a hardcoded SVG stroke.
   auto before=glyph->grab().toImage();
   auto palette=glyph->palette();
   palette.setColor(QPalette::WindowText,QColor(211,101,25));
   palette.setColor(QPalette::Text,QColor(211,101,25));
   glyph->setPalette(palette);
   app.processEvents();
   REQUIRE(before!=glyph->grab().toImage());
   // Child added AFTER start is picked up without asking the user to restart.
   auto *custom=new QDockWidget("Unknown Plugin",&main);
   custom->setObjectName("newPluginDock");
   custom->setMinimumWidth(210);
   custom->setWidget(new QLabel("Third-party widget",custom));
   main.addDockWidget(Qt::BottomDockWidgetArea,custom);
   // Watch the dock tree: newly added plugin docks are discovered automatically.
   app.processEvents();
   REQUIRE(icons.glyph(custom));
   REQUIRE(icons.decoratedCount()==6);
   const auto genericGlyph=icons.glyph(custom)->grab().toImage();
   custom->setWindowTitle("Twitch Chat");
   app.processEvents();
   REQUIRE(icons.glyph(custom)->isVisible());
   REQUIRE(genericGlyph!=icons.glyph(custom)->grab().toImage());
   REQUIRE(icons.originalTitle(custom)=="Twitch Chat");
   REQUIRE(custom->toggleViewAction()->text()=="Twitch Chat");
   // Long titles never obscure native text or native float/close buttons.
   custom->setWindowTitle(QString(200,'W'));
   icons.refresh();app.processEvents();
   REQUIRE(!icons.glyph(custom)->isVisible());
   REQUIRE(!icons.titleIndented(custom));
   custom->setWindowTitle("Plugin");
   icons.refresh();app.processEvents();
   REQUIRE(icons.glyph(custom)->isVisible());
   // Floating titlebar is owned by the window manager.
   custom->setFloating(true);icons.refresh();app.processEvents();
   REQUIRE(!icons.glyph(custom)->isVisible());
   REQUIRE(!icons.titleIndented(custom));
   custom->setFloating(false);icons.refresh();app.processEvents();
   REQUIRE(icons.glyph(custom)->isVisible());
   // Custom titlebar is respected rather than painted over.
   auto *customBar=new QWidget(custom);
   custom->setTitleBarWidget(customBar);icons.refresh();app.processEvents();
   REQUIRE(!icons.glyph(custom)->isVisible());
   REQUIRE(!icons.titleIndented(custom));
   custom->setTitleBarWidget(nullptr);
   delete customBar;
   icons.refresh();app.processEvents();
   REQUIRE(icons.glyph(custom)->isVisible());
   REQUIRE(faces.detach(mixer));
   REQUIRE(mixer->titleBarWidget()==docks[2].nativeTitle);
 }
 app.processEvents();
 for(const auto &entry:docks){
   REQUIRE(!entry.dock->findChild<QWidget*>("aftermarket-dock-title-icon",Qt::FindDirectChildrenOnly));
   REQUIRE(entry.dock->windowTitle()==entry.title);
   REQUIRE(entry.dock->toggleViewAction()->text()==entry.title);
 }
 std::cout<<"native OBS dock title icons verified\n";
 return 0;
}
