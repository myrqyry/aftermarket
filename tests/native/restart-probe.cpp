#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-hotkey.h>
#include <QApplication>
#include <QAction>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QFile>
#include <QDir>
OBS_DECLARE_MODULE()
static QString root(){return qEnvironmentVariable("PRESET_ROOT");}
static void record(const QString &name,const QByteArray &value){
 QFile file(root()+"/"+name);if(file.open(QIODevice::WriteOnly|QIODevice::Truncate))file.write(value);
}
static void respond(QMessageBox::StandardButton choice){
 for(auto *w:QApplication::topLevelWidgets())
  if(auto *box=qobject_cast<QMessageBox*>(w);box&&box->isVisible()&&box->windowTitle()=="Restart OBS")
   if(auto *button=box->button(choice)){button->click();return;}
 record("failure","missing restart confirmation");
}
static void probe(){
 auto *main=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 if(QFile::exists(root()+"/first.pid")){
  record("second.pid",QByteArray::number(QCoreApplication::applicationPid()));
  record("second.args",QCoreApplication::arguments().join("\n").toUtf8());
  QTimer::singleShot(500,qApp,[main]{main->close();});return;
 }
 record("first.pid",QByteArray::number(QCoreApplication::applicationPid()));
 auto *action=main->findChild<QAction*>("aftermarket-tools-restart");
 obs_hotkey_id hotkey=OBS_INVALID_HOTKEY_ID;
 obs_enum_hotkeys([](void *p,obs_hotkey_id id,obs_hotkey_t *h){
  if(QByteArray(obs_hotkey_get_name(h))=="aftermarket.restart_obs"){*static_cast<obs_hotkey_id*>(p)=id;return false;}return true;
 },&hotkey);
 if(!action||!action->isEnabled()||hotkey==OBS_INVALID_HOTKEY_ID){record("failure","restart menu or hotkey missing");main->close();return;}
 record("registered","menu and hotkey");
 QTimer::singleShot(500,qApp,[]{respond(QMessageBox::Cancel);});
 action->trigger();
 QTimer::singleShot(1200,qApp,[main,hotkey]{
  if(!main->isVisible()||QFile::exists(root()+"/second.pid")){record("failure","cancel did not keep original OBS");return;}
  record("cancelled","original OBS still open");
  QTimer::singleShot(500,qApp,[]{respond(QMessageBox::Yes);});
  obs_hotkey_trigger_routed_callback(hotkey,true);
  obs_hotkey_trigger_routed_callback(hotkey,false);
 });
}
static void event(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(3500,qApp,probe);}
bool obs_module_load(){obs_frontend_add_event_callback(event,nullptr);return true;}
void obs_module_unload(){obs_frontend_remove_event_callback(event,nullptr);}
