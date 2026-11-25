#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QAction>
#include <QTimer>
#include <QPointer>
#include <graphics/vec2.h>
#include <cmath>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow;
static QPointer<QDockWidget> dock;
static obs_source_t *source,*other,*f1,*f2;
static obs_sceneitem_t *a,*b;
static void check(const char *name,bool pass){blog(pass?LOG_INFO:LOG_ERROR,"EDITCHECK %s=%d",name,pass);}
static QPushButton *button(const char *name){return dock?dock->findChild<QPushButton*>(name):nullptr;}
static QComboBox *presets(){return dock?dock->findChild<QComboBox*>("aftermarket-inspect-transform-presets"):nullptr;}
static QListWidget *list(){return dock?dock->findChild<QListWidget*>("aftermarket-inspect-filter-list"):nullptr;}
static bool click(const char *name){auto *w=button(name);if(!w||!w->isEnabled())return false;w->click();return true;}
static bool history(bool undo){
 auto *action=mainWindow->findChild<QAction*>(undo?"actionMainUndo":"actionMainRedo");
 if(!action||!action->isEnabled())return false;
 blog(LOG_INFO,"EDITDETAIL history %s",qPrintable(action->text()));action->trigger();return true;
}
static bool pos(obs_sceneitem_t *item,float x,float y){
 vec2 v;obs_sceneitem_get_pos(item,&v);return std::abs(v.x-x)<.1&&std::abs(v.y-y)<.1;
}
static void probe(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 dock=mainWindow->findChild<QDockWidget*>("aftermarket-source-inspector-dock");
 auto *sceneSource=obs_frontend_get_current_scene();auto *scene=obs_scene_from_source(sceneSource);
 obs_data_t *settings=obs_data_create();obs_data_set_int(settings,"width",320);obs_data_set_int(settings,"height",180);
 source=obs_source_create("color_source_v3","Inspector Edit Alpha",settings,nullptr);
 other=obs_source_create("color_source_v3","Inspector Edit Beta",settings,nullptr);
 obs_data_release(settings);
 if(scene&&source&&other){a=obs_scene_add(scene,source);b=obs_scene_add(scene,other);}
 obs_source_release(sceneSource);
 if(!dock||!a||!b){check("setup",false);mainWindow->close();return;}
 f1=obs_source_create_private("color_filter_v2","Warm",nullptr);
 f2=obs_source_create_private("color_filter_v2","Cool",nullptr);
 if(!f1||!f2){check("setup",false);mainWindow->close();return;}
 obs_source_filter_add(other,f1);obs_source_filter_add(other,f2);
 vec2 p{123,45};obs_sceneitem_set_pos(a,&p);p={400,80};obs_sceneitem_set_pos(b,&p);
 obs_sceneitem_select(b,false);obs_sceneitem_select(a,true);dock->show();dock->raise();
 check("setup",true);
 QTimer::singleShot(900,qApp,[]{
  check("preset_controls",presets()&&presets()->isEnabled());
  if(presets())presets()->setEditText("Probe transform");
  check("save",click("aftermarket-inspect-transform-save"));
  obs_sceneitem_select(a,false);obs_sceneitem_select(b,true);
 });
 QTimer::singleShot(1900,qApp,[]{
  check("load",click("aftermarket-inspect-transform-load")&&pos(b,123,45)&&pos(a,123,45));
  check("transform_undo",history(true)&&pos(b,400,80));
  check("transform_redo",history(false)&&pos(b,123,45));
  // An external change outside the undo stack must not be overwritten.
  vec2 external{777,222};obs_sceneitem_set_pos(b,&external);
  check("stale_transform_undo_refused",history(true)&&pos(b,777,222));
  external={555,333};obs_sceneitem_set_pos(b,&external);
  check("stale_transform_redo_refused",history(false)&&pos(b,555,333));
  obs_sceneitem_set_locked(b,true);
 });
 QTimer::singleShot(2900,qApp,[]{
  check("locked_rejected",presets()&&!presets()->isEnabled());
  obs_sceneitem_set_locked(b,false);
  auto *l=list();check("filter_list",l&&l->count()==2);
  if(l&&l->count()==2){
   auto *row=l->item(0);row->setCheckState(Qt::Unchecked);
   auto *f=obs_source_get_filter_by_name(other,row->text().toUtf8().constData());
   check("toggle",f&&!obs_source_enabled(f));
   check("toggle_undo",history(true)&&f&&obs_source_enabled(f));
   check("toggle_redo",history(false)&&f&&!obs_source_enabled(f));
   if(f)obs_source_release(f);
  }
 });
 QTimer::singleShot(3900,qApp,[]{
  auto *l=list();if(l&&l->count()==2){
   l->setCurrentRow(1);
   auto *f=obs_source_get_filter_by_name(other,l->currentItem()->text().toUtf8().constData());
   const int index=f?obs_source_filter_get_index(other,f):-1;
   check("reorder",f&&click("aftermarket-inspect-filter-up")&&obs_source_filter_get_index(other,f)!=index);
   check("reorder_undo",history(true)&&f&&obs_source_filter_get_index(other,f)==index);
   check("reorder_redo",history(false)&&f&&obs_source_filter_get_index(other,f)!=index);
   if(f)obs_source_release(f);
  }
  if(dock)dock->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/inspector.png");
  obs_source_remove(source);obs_source_remove(other);obs_source_release(source);obs_source_release(other);obs_source_release(f1);obs_source_release(f2);
  mainWindow->close();
 });
}
static void frontend(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(4000,qApp,probe);}
bool obs_module_load(void){obs_frontend_add_event_callback(frontend,nullptr);return true;}
void obs_module_unload(void){obs_frontend_remove_event_callback(frontend,nullptr);}
