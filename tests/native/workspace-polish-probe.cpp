#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QDialog>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QTabWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QSpinBox>
#include <QTimer>
#include <QPointer>
#include <QCursor>
#include <QImage>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow;
static QPointer<QDialog> workspace;
static QPointer<QTabWidget> tabs;
static int tab=0,pass=0;
static bool fit=true,context=true,footer=true;
static void check(const char *key,bool ok){blog(ok?LOG_INFO:LOG_ERROR,"WORKCHECK %s=%d",key,ok);}
static void finish(){if(workspace)workspace->close();mainWindow->close();}
static void next(){
 if(!workspace||!tabs){check("workspace_missing",false);finish();return;}
 auto *page=tabs->currentWidget();
 auto *toolbar=workspace->findChild<QWidget*>("augment-workspace-toolbar");
 auto *options=workspace->findChild<QWidget*>("augment-workspace-menu-options");
 auto *apply=workspace->findChild<QPushButton*>("augment-gradient-apply");
 context&=toolbar&&options&&toolbar->isVisible()==(tab<=2)&&options->isVisible()==(tab==0);
 footer&=apply&&apply->isVisible()==(tab==1||tab==2);
 int horizontal=0,vertical=0;
 for(auto *area:page->findChildren<QScrollArea*>())if(area->isVisible()){
    horizontal=qMax(horizontal,area->horizontalScrollBar()->maximum());
    vertical=qMax(vertical,area->verticalScrollBar()->maximum());
 }
 if(auto *area=qobject_cast<QScrollArea*>(page)){
    horizontal=qMax(horizontal,area->horizontalScrollBar()->maximum());
    vertical=qMax(vertical,area->verticalScrollBar()->maximum());
 }
 fit&=horizontal==0;
 blog(LOG_INFO,"WORKDETAIL %s %dx%d h=%d v=%d",qPrintable(tabs->tabText(tab)),workspace->width(),workspace->height(),horizontal,vertical);
 if(pass==0)workspace->grab().save(qEnvironmentVariable("PRESET_ROOT")+"/"+QString::number(tab)+".png");
 if(++tab<tabs->count()){tabs->setCurrentIndex(tab);QTimer::singleShot(180,qApp,next);return;}
 if(pass++==0){tab=0;workspace->resize(640,480);tabs->setCurrentIndex(tab);QTimer::singleShot(200,qApp,next);return;}
 check("all_tabs_fit_width",fit);
 check("contextual_toolbar",context);
 check("contextual_apply",footer);
 tabs->setCurrentIndex(2);
 auto *colors=workspace->findChild<QLineEdit*>("augment-gradient-colors");
 auto *preview=workspace->findChild<QWidget*>("augment-gradient-preview");
 auto *useTheme=workspace->findChild<QPushButton*>("augment-gradient-use-theme");
 if(!colors||!preview||!useTheme){check("preview_missing",false);finish();return;}
 const auto initial=preview->grab().toImage();
 colors->setText("#ff0000, #0000ff");QApplication::processEvents();
 check("draft_preview",preview->property("validPalette").toBool()&&preview->grab().toImage()!=initial&&apply->isEnabled());
 useTheme->click();QApplication::processEvents();
 check("theme_reset",colors->text().isEmpty()&&preview->property("validPalette").toBool()&&!apply->isEnabled());
 tabs->setCurrentIndex(0);
 auto *pick=workspace->findChild<QPushButton*>("augment-pick");
 auto delay=workspace->findChildren<QSpinBox*>();
 for(auto *spin:delay)if(spin->suffix()==" s"&&spin->minimum()==2)spin->setValue(2);
 QMenu *menu=nullptr;
 for(auto *action:mainWindow->menuBar()->actions())if(action->menu()&&action->text().contains("File",Qt::CaseInsensitive)){menu=action->menu();break;}
 if(!menu||!pick){check("native_menu_capture",false);finish();return;}
 pick->click();
 QTimer::singleShot(250,qApp,[menu]{
    menu->popup(mainWindow->mapToGlobal(QPoint(20,35)));
    QTimer::singleShot(200,qApp,[menu]{
       for(auto *action:menu->actions())if(action->isVisible()&&!action->isSeparator()){
          QCursor::setPos(menu->mapToGlobal(menu->actionGeometry(action).center()));break;
       }
    });
 });
 QTimer::singleShot(2700,qApp,[menu]{
    menu->hide();
    auto *list=workspace->findChild<QListWidget*>("augment-actions");
    check("native_menu_capture",workspace->isVisible()&&list&&list->count()>0);
    workspace->resize(820,590);
    QTimer::singleShot(200,qApp,[]{
       workspace->grab().save(qEnvironmentVariable("PRESET_ROOT")+"/menu-captured.png");
       finish();
    });
 });
}
static void start(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 auto *action=mainWindow->findChild<QAction*>("aftermarket-tools-workspace");
 if(!action){check("workspace_action",false);finish();return;}
 action->trigger();
 QTimer::singleShot(500,qApp,[]{
    workspace=mainWindow->findChild<QDialog*>("aftermarket-inspector");
    tabs=workspace?workspace->findChild<QTabWidget*>("augment-workspace"):nullptr;
    check("workspace_tabs",workspace&&tabs&&tabs->count()==7);
    if(!workspace||!tabs){finish();return;}
    workspace->resize(820,590);tabs->setCurrentIndex(0);
    QTimer::singleShot(200,qApp,next);
 });
}
static void ev(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(1500,qApp,start);}
bool obs_module_load(){obs_frontend_add_event_callback(ev,nullptr);return true;}
void obs_module_unload(){obs_frontend_remove_event_callback(ev,nullptr);}
