#include "dock-title-icons.h"
#include "dock-faces.h"
#include "panels.h"
#include "dock-header-button.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QToolButton>
#include <QMenu>
#include <QSettings>
#include <QTemporaryDir>
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
 QTemporaryDir tmp;
 REQUIRE(tmp.isValid());
 const QString settingsPath=tmp.filePath("dock-titles.ini");
 {
   augment::Panels panels;
   augment::DockTitleIcons icons(&main,&panels,settingsPath);
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
      auto *more=icons.moreButton(dock);
      REQUIRE(more&&more->parentWidget()==dock);
      REQUIRE(more->objectName()=="aftermarket-dock-more");
      REQUIRE(more->isVisible());
      REQUIRE(more->geometry().bottom()<dock->widget()->geometry().top());
      REQUIRE(more->menu()!=nullptr);
      more->menu()->popup(more->mapToGlobal(QPoint(0,more->height())));
      app.processEvents();
      REQUIRE(!more->menu()->actions().isEmpty());
      auto *first=more->menu()->actions().first();
      REQUIRE(first->text()=="Collapse");
      REQUIRE(first->isEnabled()==panels.canCollapse(dock));
      more->menu()->hide();app.processEvents();
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
   // Real menu action uses the existing Panels implementation, not a fake state.
   auto *scenes=docks[0].dock;
   auto *sceneMore=icons.moreButton(scenes);
   main.splitDockWidget(scenes,docks[1].dock,Qt::Vertical);app.processEvents();
   icons.refresh();app.processEvents();
   REQUIRE(panels.canCollapse(scenes));
   sceneMore->menu()->popup(sceneMore->mapToGlobal(QPoint(0,sceneMore->height())));app.processEvents();
   sceneMore->menu()->actions().first()->trigger();app.processEvents();
   REQUIRE(panels.collapsed(scenes)&&scenes->widget()->isHidden());
   sceneMore->menu()->hide();app.processEvents();
   sceneMore->menu()->popup(sceneMore->mapToGlobal(QPoint(0,sceneMore->height())));app.processEvents();
   REQUIRE(sceneMore->menu()->actions().first()->text()=="Expand");
   sceneMore->menu()->actions().first()->trigger();app.processEvents();
   REQUIRE(!panels.collapsed(scenes)&&scenes->widget()->isVisible());
   sceneMore->menu()->hide();app.processEvents();
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
   auto *toggle=mixer->findChild<QToolButton*>("aftermarket-audiobus-toggle");
   auto *glyph=icons.glyph(mixer);
   REQUIRE(toggle&&glyph&&toggle->isVisible());
   REQUIRE(toggle->mapTo(mixer,QPoint(0,0)).y()>=mixer->widget()->geometry().top());
   REQUIRE(!mixer->findChild<QToolButton*>("aftermarket-face-flip"));
   auto *mixerMore=icons.moreButton(mixer);
   REQUIRE(mixerMore&&mixerMore->isVisible());
   auto *specialized=new augment::DockHeaderButton(augment::DockHeaderGlyph::More,mixer);
   specialized->setObjectName("aftermarket-mixer-more");
   specialized->move(mixerMore->pos());specialized->show();
   icons.refresh();app.processEvents();
   REQUIRE(!mixerMore->isVisible()); // only one ellipsis in the native mixer
   specialized->hide();icons.refresh();app.processEvents();
   REQUIRE(mixerMore->isVisible());
   delete specialized;
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
   icons.refresh();app.processEvents(); // after Qt has assigned native dock geometry
   REQUIRE(icons.moreButton(custom)&&icons.moreButton(custom)->isVisible());
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
   REQUIRE(!icons.glyph(custom)->isVisible()); // long labels favor menu over icon
   REQUIRE(icons.moreButton(custom)->isVisible());
   REQUIRE(icons.originalTitle(custom)==QString(200,'W'));
   REQUIRE(custom->windowTitle().size()<200); // reversible visual elision
   REQUIRE(custom->toggleViewAction()->text()==QString(200,'W'));
   REQUIRE(custom->windowTitle().endsWith(QChar(0x2026))); // legacy end elision
   auto *more=icons.moreButton(custom);
   more->menu()->popup(more->mapToGlobal(QPoint(0,more->height())));
   app.processEvents();
   QMenu *titleMenu=nullptr;
   for(auto *action:more->menu()->actions())
       if(action->menu()&&action->text()=="Long titles")titleMenu=action->menu();
   REQUIRE(titleMenu&&titleMenu->actions().size()==2);
   auto *endElide=titleMenu->actions().at(0);
   auto *middleElide=titleMenu->actions().at(1);
   REQUIRE(endElide->isChecked()&&!middleElide->isChecked());
   middleElide->trigger();app.processEvents();
   REQUIRE(custom->windowTitle().contains(QChar(0x2026)));
   REQUIRE(custom->windowTitle().startsWith('W')&&custom->windowTitle().endsWith('W'));
   REQUIRE(icons.originalTitle(custom)==QString(200,'W'));
   REQUIRE(custom->toggleViewAction()->text()==QString(200,'W'));
   REQUIRE(QSettings(settingsPath,QSettings::IniFormat).value("title/ellipsis").toString()=="middle");
   more->menu()->hide();app.processEvents();
   more->menu()->popup(more->mapToGlobal(QPoint(0,more->height())));
   app.processEvents();
   titleMenu=nullptr;
   for(auto *action:more->menu()->actions())
       if(action->menu()&&action->text()=="Long titles")titleMenu=action->menu();
   REQUIRE(titleMenu&&titleMenu->actions().at(1)->isChecked());
   titleMenu->actions().at(0)->trigger();app.processEvents();
   REQUIRE(custom->windowTitle().endsWith(QChar(0x2026)));
   REQUIRE(QSettings(settingsPath,QSettings::IniFormat).value("title/ellipsis").toString()=="end");
   titleMenu->actions().at(1)->trigger();app.processEvents(); // persist selected middle mode
   more->menu()->hide();app.processEvents();
   custom->setWindowTitle("Plugin");
   icons.refresh();app.processEvents();
   REQUIRE(icons.glyph(custom)->isVisible());
   // Floating titlebar is owned by the window manager.
   custom->setFloating(true);icons.refresh();app.processEvents();
   REQUIRE(!icons.glyph(custom)->isVisible());
   REQUIRE(!icons.moreButton(custom)->isVisible());
   REQUIRE(!icons.titleIndented(custom));
   custom->setFloating(false);icons.refresh();app.processEvents();
   REQUIRE(icons.glyph(custom)->isVisible());
   // Custom titlebar is respected rather than painted over.
   auto *customBar=new QWidget(custom);
   custom->setTitleBarWidget(customBar);icons.refresh();app.processEvents();
   REQUIRE(!icons.glyph(custom)->isVisible());
   REQUIRE(!icons.moreButton(custom)->isVisible());
   REQUIRE(!icons.titleIndented(custom));
   custom->setTitleBarWidget(nullptr);
   delete customBar;
   icons.refresh();app.processEvents();
   REQUIRE(icons.glyph(custom)->isVisible());
   REQUIRE(faces.detach(mixer));
   REQUIRE(mixer->titleBarWidget()==docks[2].nativeTitle);
 }
 app.processEvents();
 REQUIRE(QSettings(settingsPath,QSettings::IniFormat).value("title/ellipsis").toString()=="middle");
 {
   augment::Panels panels;
   augment::DockTitleIcons restored(&main,&panels,settingsPath);
   REQUIRE(restored.ellipsisMode()==Qt::ElideMiddle);
   auto *transitions=docks[3].dock;
   transitions->setWindowTitle(QString(180,'Z'));
   restored.refresh();app.processEvents();
   REQUIRE(transitions->windowTitle().startsWith('Z'));
   REQUIRE(transitions->windowTitle().endsWith('Z'));
   REQUIRE(restored.originalTitle(transitions)==QString(180,'Z'));
   transitions->setWindowTitle(docks[3].title);
   restored.refresh();app.processEvents();
 }
 app.processEvents();
 for(const auto &entry:docks){
   REQUIRE(!entry.dock->findChild<QWidget*>("aftermarket-dock-title-icon",Qt::FindDirectChildrenOnly));
   REQUIRE(!entry.dock->findChild<QToolButton*>("aftermarket-dock-more",Qt::FindDirectChildrenOnly));
   REQUIRE(entry.dock->windowTitle()==entry.title);
   REQUIRE(entry.dock->toggleViewAction()->text()==entry.title);
 }
 std::cout<<"native OBS dock title icons verified\n";
 return 0;
}
