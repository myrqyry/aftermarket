#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QAction>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QAbstractButton>
#include <QDialog>
#include <QMessageBox>
#include <QDockWidget>
#include <QTimer>
#include <QMouseEvent>
#include <QPointer>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow=nullptr;
static QPointer<QLineEdit> field,input;
static QPointer<QDialog> popup;
static QPointer<QDockWidget> dock;
static obs_source_t *source=nullptr,*second=nullptr;
static obs_sceneitem_t *item=nullptr,*other=nullptr;
static obs_hotkey_id hotkey=OBS_INVALID_HOTKEY_ID;
static int fired=0;
static void check(const char *name,bool pass){blog(pass?LOG_INFO:LOG_ERROR,"COMMANDCHECK %s=%d",name,pass);}
static void key(void*,obs_hotkey_id,obs_hotkey_t*,bool pressed){if(pressed)++fired;}
static bool open(const QString &query){
 if(!field||!field->isVisible())return false;
 const auto p=field->rect().center();
 QMouseEvent press(QEvent::MouseButtonPress,p,field->mapToGlobal(p),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
 QApplication::sendEvent(field,&press);
 popup=mainWindow->findChild<QDialog*>("aftermarket-command-popup");
 input=popup?popup->findChild<QLineEdit*>("aftermarket-command-input"):nullptr;
 if(!input||!popup->isVisible())return false;
 input->setText(query);
 auto *list=popup->findChild<QListWidget*>("aftermarket-command-results");
 return list&&list->count()==1;
}
static void enter(){if(input)QMetaObject::invokeMethod(input,"returnPressed",Qt::DirectConnection);}
static QString heading(){
 auto *label=dock?dock->findChild<QLabel*>("aftermarket-inspect-name"):nullptr;
 return label?label->text():QString{};
}
static void probe(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 auto *bar=mainWindow->menuBar();mainWindow->resize(1450,820);
 field=bar->findChild<QLineEdit*>("aftermarket-command-center");
 dock=mainWindow->findChild<QDockWidget*>("aftermarket-source-inspector-dock");
 auto *sceneSource=obs_frontend_get_current_scene();auto *scene=obs_scene_from_source(sceneSource);
 obs_data_t *settings=obs_data_create();obs_data_set_int(settings,"width",320);obs_data_set_int(settings,"height",180);
 source=obs_source_create("color_source_v3","CommandProbeUniqueAlpha",settings,nullptr);
 second=obs_source_create("color_source_v3","CommandProbeUniqueBeta",settings,nullptr);
 obs_data_release(settings);
 if(scene&&source&&second){item=obs_scene_add(scene,source);other=obs_scene_add(scene,second);if(item)obs_sceneitem_select(item,true);}
 obs_source_release(sceneSource);
 QTimer::singleShot(700,qApp,[]{
  check("field_and_dock",field&&field->isVisible()&&dock&&item&&other);
  if(!field||!dock||!item||!other){mainWindow->close();return;}
  const auto *bar=mainWindow->menuBar();bool gap=true;
  for(auto *a:bar->actions())if(a->isVisible())gap&=!bar->actionGeometry(a).intersects(field->geometry());
  check("no_menu_overlap",gap);
  check("source_query",open("@CommandProbeUniqueAlpha"));
  check("typing_no_effect",!dock->isVisible()&&fired==0);
  enter();
 });
 QTimer::singleShot(1300,qApp,[]{
  check("source_inspected",dock&&dock->isVisible()&&heading()=="CommandProbeUniqueAlpha");
  obs_sceneitem_select(item,false);obs_sceneitem_select(other,true);
 });
 QTimer::singleShot(2200,qApp,[]{
  check("pin_stable",heading()=="CommandProbeUniqueAlpha");
  auto *pin=dock?dock->findChild<QPushButton*>("aftermarket-inspect-pin"):nullptr;
  if(pin)pin->click();
 });
 QTimer::singleShot(3100,qApp,[]{
  check("follows_selection",heading()=="CommandProbeUniqueBeta");
  auto *transform=dock?dock->findChild<QLabel*>("aftermarket-inspect-transform"):nullptr;
  check("scene_item_context",transform&&transform->text().contains("Position"));
  check("hotkey_query",open("/CommandProbe unique callback"));enter();
 });
 QTimer::singleShot(3600,qApp,[]{
  auto *prompt=mainWindow->findChild<QMessageBox*>("aftermarket-command-confirm");
  check("hotkey_confirmation",prompt&&prompt->isVisible()&&fired==0);
  if(prompt)prompt->button(QMessageBox::No)->click();
 });
 QTimer::singleShot(4100,qApp,[]{
  check("decline_no_execution",fired==0);
  check("hotkey_query_again",open("/CommandProbe unique callback"));enter();
 });
 QTimer::singleShot(4700,qApp,[]{
  auto *prompt=mainWindow->findChild<QMessageBox*>("aftermarket-command-confirm");
  if(prompt)prompt->button(QMessageBox::Yes)->click();
 });
 QTimer::singleShot(5400,qApp,[]{
  check("hotkey_executes_once",fired==1);
  obs_source_remove(second);obs_source_release(second);second=nullptr;
 });
 QTimer::singleShot(6300,qApp,[]{
  check("removed_target_cleared",heading()!="CommandProbeUniqueBeta");
  if(mainWindow)mainWindow->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/command-center.png");
  if(source){obs_source_remove(source);obs_source_release(source);source=nullptr;}
  mainWindow->close();
 });
}
static void frontend(enum obs_frontend_event e,void*){
 if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(4000,qApp,probe);
}
bool obs_module_load(void){
 hotkey=obs_hotkey_register_frontend("commandprobe.unique","CommandProbe unique callback",key,nullptr);
 obs_frontend_add_event_callback(frontend,nullptr);return true;
}
void obs_module_unload(void){obs_frontend_remove_event_callback(frontend,nullptr);if(hotkey!=OBS_INVALID_HOTKEY_ID)obs_hotkey_unregister(hotkey);}
