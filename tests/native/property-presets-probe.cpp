#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QToolButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QMainWindow>
#include <QPushButton>
#include <QListWidget>
#include <QTimer>
#include <QFileInfo>
#include <QMenu>
#include <QAction>
#include <QPointer>
#include <QFile>
#include <QLayout>
OBS_DECLARE_MODULE()
static obs_source_t *source=nullptr,*other=nullptr,*filter1=nullptr,*filter2=nullptr;
static QPointer<QDialog> dlg;
static void check(const char *name,bool pass){blog(pass?LOG_INFO:LOG_ERROR,"PRESETPROBE %s=%d",name,pass);}
static QDialog *dialog(const char *cls){
 for(auto *w:QApplication::topLevelWidgets())if(w->isVisible()&&w->inherits(cls))return qobject_cast<QDialog*>(w);
 return nullptr;
}
static double get(obs_source_t *s,const char *key){
 auto *d=obs_source_get_settings(s);double v=obs_data_get_double(d,key);obs_data_release(d);return v;
}
static void set(obs_source_t *s,const char *key,double v){
 auto *d=obs_data_create();obs_data_set_double(d,key,v);obs_source_update(s,d);obs_data_release(d);obs_source_update_properties(s);
}
static QComboBox *combo(){return dlg?dlg->findChild<QComboBox*>("aftermarket-preset-select"):nullptr;}
static QToolButton *button(const char *name){return dlg?dlg->findChild<QToolButton*>(name):nullptr;}
static bool save(const char *name){
 auto *b=button("aftermarket-preset-save");if(!b||!b->isEnabled())return false;b->click();
 auto *prompt=dlg->findChild<QInputDialog*>();if(!prompt)return false;
 prompt->setTextValue(name);prompt->accept();return true;
}
static bool load(const char *name){
 auto *c=combo();auto *b=button("aftermarket-preset-load");if(!c||!b)return false;
 int index=c->findData(QString::fromUtf8(name));if(index<0)return false;c->setCurrentIndex(index);
 if(!b->isEnabled())return false;b->click();return true;
}
static QAction *action(const QString &name){
 auto *more=button("aftermarket-preset-more");
 if(more&&more->menu())for(auto *a:more->menu()->actions())if(a->text()==name)return a;
 return nullptr;
}
static bool manage(const QString &name,const QString &newName={}){
 auto *a=action(name);if(!a||!a->isEnabled())return false;a->trigger();
 if(!newName.isEmpty()){
  for(auto *prompt:dlg->findChildren<QInputDialog*>())if(prompt->isVisible()){prompt->setTextValue(newName);prompt->accept();return true;}
 }else{
  for(auto *prompt:dlg->findChildren<QMessageBox*>())if(prompt->isVisible()){
   if(auto *yes=prompt->button(QMessageBox::Yes)){yes->click();return true;}
  }
 }
 return false;
}
static void nativeButton(QDialogButtonBox::StandardButton which){
 if(!dlg)return;
 auto *box=dlg->findChild<QDialogButtonBox*>("buttonBox");if(box&&box->button(which))box->button(which)->click();
}
static void closeObs(){
 if(filter1)obs_source_release(filter1);
 if(filter2)obs_source_release(filter2);
 if(source)obs_source_release(source);
 if(other)obs_source_release(other);
 static_cast<QMainWindow*>(obs_frontend_get_main_window())->close();
}
static void probe(){
 auto *s=obs_data_create();obs_data_set_int(s,"width",320);obs_data_set_int(s,"height",180);obs_data_set_int(s,"color",0xff123456);
 source=obs_source_create("color_source_v3","Preset color A",s,nullptr);obs_data_release(s);s=obs_data_create();obs_data_set_int(s,"width",640);obs_data_set_int(s,"height",180);other=obs_source_create("color_source_v3","Preset color B",s,nullptr);obs_data_release(s);
 if(!source||!other){check("sources",false);closeObs();return;}
 auto *scene=obs_frontend_get_current_scene();obs_scene_add(obs_scene_from_source(scene),source);obs_scene_add(obs_scene_from_source(scene),other);obs_source_release(scene);
 obs_frontend_open_source_properties(source);
 QTimer::singleShot(500,qApp,[]{
  dlg=dialog("OBSBasicProperties");auto *c=combo();
    check("source_footer",c&&c->isVisible());
  check("source_save",save("Small"));
  check("source_saved",combo()&&combo()->findData("Small")>=0);
  nativeButton(QDialogButtonBox::Ok);
  obs_frontend_open_source_properties(other);
 });
 QTimer::singleShot(1100,qApp,[]{
  dlg=dialog("OBSBasicProperties");
  check("cross_source_load",load("Small")&&get(other,"width")==320);
  check("source_unchanged",get(source,"width")==320);
  nativeButton(QDialogButtonBox::Cancel);
 });
 QTimer::singleShot(1700,qApp,[]{
  check("cancel_restores",get(other,"width")==640);
  obs_frontend_open_source_properties(other);
 });
 QTimer::singleShot(2300,qApp,[]{
  dlg=dialog("OBSBasicProperties");
  check("source_load_commit",load("Small"));nativeButton(QDialogButtonBox::Ok);
 });
 QTimer::singleShot(2900,qApp,[]{
  check("ok_keeps",get(other,"width")==320);
  filter1=obs_source_create_private("color_filter_v2","Color first",nullptr);
  filter2=obs_source_create_private("color_filter_v2","Color second",nullptr);
  obs_source_filter_add(source,filter1);obs_source_filter_add(source,filter2);
  set(filter1,"gamma",.25);set(filter2,"gamma",.75);
  obs_frontend_open_source_filters(source);
 });
 QTimer::singleShot(3600,qApp,[]{
  dlg=dialog("OBSBasicFilters");auto *list=dlg?dlg->findChild<QListWidget*>("effectFilters"):nullptr;
  if(list)list->setCurrentRow(0);
 });
 QTimer::singleShot(4000,qApp,[]{
  auto *c=combo();check("filter_footer",c&&c->isVisible());
  check("type_isolation",c&&c->findData("Small")<0);
  check("filter_save",save("Color preset"));
  auto *list=dlg?dlg->findChild<QListWidget*>("effectFilters"):nullptr;if(list)list->setCurrentRow(1);
 });
 QTimer::singleShot(4500,qApp,[]{
  check("filter_load",load("Color preset")&&get(filter2,"gamma")==.25&&get(filter1,"gamma")==.25);
  auto *more=button("aftermarket-preset-more");
  bool undone=false;
  if(more&&more->menu())for(auto *a:more->menu()->actions())if(a->text()=="Undo last preset load"&&a->isEnabled()){a->trigger();undone=true;break;}
  check("filter_undo",undone&&get(filter2,"gamma")==.75);
  if(dlg)dlg->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/filters.png");
  nativeButton(QDialogButtonBox::Close);
  obs_frontend_open_source_properties(source);
 });
 QTimer::singleShot(5100,qApp,[]{
  dlg=dialog("OBSBasicProperties");
  check("source_type_isolation",combo()&&combo()->findData("Color preset")<0&&combo()->findData("Small")>=0);
  if(dlg)dlg->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/properties.png");
  if(combo())combo()->setCurrentIndex(combo()->findData("Small"));
  check("rename",manage("Rename…","Renamed")&&combo()&&combo()->findData("Small")<0&&combo()->findData("Renamed")>=0);
  set(source,"width",480);
  check("update",manage("Update selected preset…"));
  set(source,"width",320);
  check("updated_load",load("Renamed")&&get(source,"width")==480);
  check("delete",manage("Delete…")&&combo()&&combo()->findData("Renamed")<0);
  nativeButton(QDialogButtonBox::Ok);
  closeObs();
 });
}
static void frontend(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(1500,qApp,probe);}
bool obs_module_load(void){obs_frontend_add_event_callback(frontend,nullptr);return true;}
void obs_module_unload(void){obs_frontend_remove_event_callback(frontend,nullptr);}
