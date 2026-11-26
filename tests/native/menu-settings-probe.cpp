#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QToolButton>
#include <QAction>
#include <QDialog>
#include <QTimer>
OBS_DECLARE_MODULE()
static void check(const char *key,bool ok){blog(ok?LOG_INFO:LOG_ERROR,"GEARCHECK %s=%d",key,ok);}
static QMainWindow *mainWindow=nullptr;
static void probe(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 auto *bar=mainWindow->menuBar();mainWindow->resize(1200,800);
 QTimer::singleShot(500,qApp,[bar]{
  auto *gear=bar->findChild<QToolButton*>("aftermarket-menu-settings");
  auto *action=mainWindow->findChild<QAction*>("action_Settings");
  check("native_settings_identity",gear&&gear->isVisible()&&action&&gear->defaultAction()==action);
  const int right=gear?gear->mapTo(bar,QPoint(gear->width(),0)).x():-1;
  blog(LOG_INFO,"GEARDETAIL right=%d bar=%d",right,bar->width());
  check("far_right",right>=bar->width()-8&&right<=bar->width());
  bar->grab().save(qEnvironmentVariable("PRESET_ROOT")+"/menubar.png");
  if(!gear){mainWindow->close();return;}
  QTimer::singleShot(700,qApp,[]{
   QDialog *settings=nullptr;
   for(auto *w:QApplication::topLevelWidgets())if(w->inherits("OBSBasicSettings")&&w->isVisible())settings=qobject_cast<QDialog*>(w);
   check("opens_native_settings",settings!=nullptr);
   if(settings)settings->reject();
   QTimer::singleShot(250,qApp,[]{mainWindow->close();});
  });
  gear->click();
 });
}
static void event(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(1000,qApp,probe);}
bool obs_module_load(){obs_frontend_add_event_callback(event,nullptr);return true;}
void obs_module_unload(){obs_frontend_remove_event_callback(event,nullptr);}
