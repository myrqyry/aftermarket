#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QListView>
#include <QListWidget>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QHelpEvent>
#include <QToolTip>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow;
static obs_source_t *source;
static void check(const char *name,bool ok){blog(ok?LOG_INFO:LOG_ERROR,"NATIVENOTE %s=%d",name,ok);}
static QDialog *editor(){for(auto *d:mainWindow->findChildren<QDialog*>("aftermarket-note-editor"))if(d->isVisible())return d;return nullptr;}
static bool save(QString text){auto *d=editor();if(!d)return false;d->findChild<QPlainTextEdit*>()->setPlainText(text);d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();return !d->isVisible();}
static bool menu(QListView *v,int row){
 if(!v||row<0)return false;
 bool found=false;
 const auto p=v->visualRect(v->model()->index(row,0)).center();
 QTimer::singleShot(120,qApp,[&]{
  if(auto *m=qobject_cast<QMenu*>(QApplication::activePopupWidget())){
   for(auto *a:m->actions())if(a->objectName()=="aftermarket-native-note-action"){found=true;a->trigger();break;}
   m->close();
  }
 });
 QContextMenuEvent e(QContextMenuEvent::Mouse,p,v->viewport()->mapToGlobal(p));QApplication::sendEvent(v->viewport(),&e);
 return found;
}
static QToolButton *badge(const char *name){for(auto *b:mainWindow->findChildren<QToolButton*>(name))if(b->isVisible())return b;return nullptr;}
static void probe(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());mainWindow->resize(1450,820);
 auto *scene=obs_frontend_get_current_scene();source=obs_source_create("color_source_v3","NativeNote Probe",nullptr,nullptr);
 obs_scene_add(obs_scene_from_source(scene),source);obs_source_release(scene);
 QTimer::singleShot(700,qApp,[]{
  auto *v=mainWindow->findChild<QListView*>("sources");
  check("source_menu",menu(v,0));
 });
 QTimer::singleShot(1300,qApp,[]{check("source_save",save("Native source reminder"));});
 QTimer::singleShot(1900,qApp,[]{
  auto *b=badge("aftermarket-native-source-note");check("source_badge",b&&b->toolTip().contains("Native source reminder"));
  if(b){auto p=b->rect().center();QHelpEvent e(QEvent::ToolTip,p,b->mapToGlobal(p));QApplication::sendEvent(b,&e);}
 });
 QTimer::singleShot(2200,qApp,[]{
  check("source_hover",QToolTip::isVisible()&&QToolTip::text().contains("Native source reminder"));QToolTip::hideText();
  if(auto *b=badge("aftermarket-native-source-note"))b->click();
  check("badge_editor",editor()!=nullptr);if(auto *d=editor())d->reject();
  auto *v=mainWindow->findChild<QListWidget*>("scenes");check("scene_menu",menu(v,0));
 });
 QTimer::singleShot(2850,qApp,[]{check("scene_save",save("Native scene reminder"));});
 QTimer::singleShot(3450,qApp,[]{
  auto *b=badge("aftermarket-native-scene-note");check("scene_badge",b&&b->toolTip().contains("Native scene reminder"));
  mainWindow->findChild<QListView*>("sources")->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/source-notes.png");
  mainWindow->findChild<QListWidget*>("scenes")->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/scene-notes.png");
  obs_source_set_name(source,"NativeNote Renamed");
 });
 QTimer::singleShot(4100,qApp,[]{
  auto *b=badge("aftermarket-native-source-note");check("rename_keeps_badge",b&&b->toolTip().contains("Native source reminder"));
  if(b)b->click();check("delete_note",save(""));
 });
 QTimer::singleShot(4700,qApp,[]{
  check("removed_badge",!badge("aftermarket-native-source-note"));
  obs_source_remove(source);obs_source_release(source);mainWindow->close();
 });
}
static void frontend(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(4000,qApp,probe);}
bool obs_module_load(void){obs_frontend_add_event_callback(frontend,nullptr);return true;}
void obs_module_unload(void){obs_frontend_remove_event_callback(frontend,nullptr);}
