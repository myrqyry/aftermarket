#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QStatusBar>
#include <QToolButton>
#include <QTimer>
#include <QDialog>
#include <QAction>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QListWidget>
OBS_DECLARE_MODULE()
static void check(const char *key,bool ok){blog(ok?LOG_INFO:LOG_ERROR,"HEALTHCHECK %s=%d",key,ok);}
static void probe(){
 auto *main=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 auto *button=main->statusBar()->findChild<QToolButton*>("aftermarket-health-stats");
 check("attached",button!=nullptr);
 check("sampled",button&&button->toolTip().contains("OBS CPU:")&&button->toolTip().contains("Render frame budget:"));
 if(button)blog(LOG_INFO,"HEALTHDETAIL %s",qPrintable(button->toolTip().replace('\n'," | ")));
 // The host may actually be busy; never assume that an idle output
 // means healthy rendering or system usage. Deterministic unit tests
 // exercise the healthy and recovery cases independently.
 const bool normal=button&&button->toolTip().startsWith("Performance normal");
 check("visibility_matches_sample",button&&(normal?button->isHidden():button->isVisible()&&!button->text().isEmpty()));
 if(button&&button->isVisible())main->statusBar()->grab().save(qEnvironmentVariable("PRESET_ROOT")+"/alert.png");
 auto *settings=main->findChild<QAction*>("action_Settings");
 if(!settings){check("settings",false);main->close();return;}
 QTimer::singleShot(700,qApp,[main,button]{
  QDialog *dialog=nullptr;
  for(auto *w:QApplication::topLevelWidgets())if(w->inherits("OBSBasicSettings")&&w->isVisible())dialog=qobject_cast<QDialog*>(w);
  auto *toggle=dialog?dialog->findChild<QCheckBox*>("aftermarket-setting-health-stats"):nullptr;
  check("settings",toggle&&toggle->isChecked());
  if(toggle){
   toggle->setChecked(false);
   auto*buttons=dialog->findChild<QDialogButtonBox*>();
   if(buttons&&buttons->button(QDialogButtonBox::Apply))buttons->button(QDialogButtonBox::Apply)->click();
   check("disable",button&&button->isHidden()&&!toggle->isChecked());
  }else check("disable",false);
  if(dialog)dialog->reject();
  QTimer::singleShot(250,qApp,[main]{main->close();});
 });
 settings->trigger();
}
static void event(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(10000,qApp,probe);}
bool obs_module_load(){obs_frontend_add_event_callback(event,nullptr);return true;}
void obs_module_unload(){obs_frontend_remove_event_callback(event,nullptr);}
