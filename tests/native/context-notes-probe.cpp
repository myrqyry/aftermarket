#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QToolButton>
#include <QPlainTextEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QToolTip>
#include <QTimer>
#include <QPointer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow;
static QPointer<QDockWidget> dock;
static obs_source_t *source,*second,*filter;
static obs_sceneitem_t *a,*b;
static QString sourceId,sceneId,filterId;
static int checks=0;
static void check(const char *name,bool ok){++checks;blog(ok?LOG_INFO:LOG_ERROR,"NOTECHECK %s=%d",name,ok);}
static QDialog *editor(){
 for(auto *d:mainWindow->findChildren<QDialog*>("aftermarket-note-editor"))if(d->isVisible())return d;
 return nullptr;
}
static bool save(const QString &text){
 auto *d=editor();if(!d)return false;
 auto *input=d->findChild<QPlainTextEdit*>("aftermarket-note-text");
 auto *buttons=d->findChild<QDialogButtonBox*>();
 if(!input||!buttons)return false;
 input->setPlainText(text);buttons->button(QDialogButtonBox::Save)->click();return !d->isVisible();
}
static QListWidget *query(const QString &text){
 auto *field=mainWindow->findChild<QLineEdit*>("aftermarket-command-center");
 if(!field||!field->isVisible())return nullptr;
 const auto p=field->rect().center();
 QMouseEvent press(QEvent::MouseButtonPress,p,field->mapToGlobal(p),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
 QApplication::sendEvent(field,&press);
 auto *popup=mainWindow->findChild<QDialog*>("aftermarket-command-popup");
 auto *input=popup?popup->findChild<QLineEdit*>("aftermarket-command-input"):nullptr;
 if(!input)return nullptr;input->setText(text);
 return popup->findChild<QListWidget*>("aftermarket-command-results");
}
static void context(QWidget *w,const QPoint &p){
 QContextMenuEvent e(QContextMenuEvent::Mouse,p,w->mapToGlobal(p));
 QApplication::sendEvent(w,&e);
}
static void chooseMenu(QWidget *w,const QPoint &p){
 // Native context menu is synchronous; select its first action on the next turn.
 QTimer::singleShot(50,qApp,[]{if(auto *menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());menu&&!menu->actions().isEmpty()){menu->actions().first()->trigger();menu->close();}});
 context(w,p);
}
static void badgeClick(QListWidget *list){
 auto *row=list->item(0);const auto r=list->visualItemRect(row);
 const QPoint p(r.right()-10,r.center().y());
 QMouseEvent press(QEvent::MouseButtonPress,p,list->viewport()->mapToGlobal(p),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
 QMouseEvent release(QEvent::MouseButtonRelease,p,list->viewport()->mapToGlobal(p),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
 QApplication::sendEvent(list->viewport(),&press);QApplication::sendEvent(list->viewport(),&release);
}
static void probe(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());mainWindow->resize(1450,820);
 dock=mainWindow->findChild<QDockWidget*>("aftermarket-source-inspector-dock");
 auto *sceneSource=obs_frontend_get_current_scene();sceneId=QString::fromUtf8(obs_source_get_uuid(sceneSource));
 auto *scene=obs_scene_from_source(sceneSource);
 source=obs_source_create("color_source_v3","NotesProbe Alpha",nullptr,nullptr);
 second=obs_source_create("color_source_v3","NotesProbe Beta",nullptr,nullptr);
 filter=obs_source_create_private("color_filter_v2","NotesProbe Filter",nullptr);
 if(scene&&source&&second&&filter){a=obs_scene_add(scene,source);b=obs_scene_add(scene,second);obs_source_filter_add(source,filter);sourceId=QString::fromUtf8(obs_source_get_uuid(source));filterId=QString::fromUtf8(obs_source_get_uuid(filter));obs_sceneitem_select(b,false);obs_sceneitem_select(a,true);}
 obs_source_release(sceneSource);
 check("setup",dock&&a&&b&&filter);
 if(!dock||!a||!b||!filter){mainWindow->close();return;}
 dock->show();dock->raise();
 QTimer::singleShot(700,qApp,[]{
  auto *button=dock->findChild<QToolButton*>("aftermarket-source-note");
  check("empty_has_no_badge",button&&button->isHidden());
  auto *list=query("@NotesProbe Alpha");check("source_query",list&&list->count()>=1&&list->item(0)->text().contains("NotesProbe Alpha"));
  if(list&&list->count())context(list->viewport(),list->visualItemRect(list->item(0)).center());
 });
 QTimer::singleShot(1150,qApp,[]{
  check("command_note_editor",editor()!=nullptr);
  check("save_source",save("Private calibration reminder <b>literal</b>"));
 });
 QTimer::singleShot(1600,qApp,[]{
  auto *button=dock->findChild<QToolButton*>("aftermarket-source-note");
  check("source_badge",button&&button->isVisible()&&button->toolTip().contains("&lt;b&gt;literal&lt;/b&gt;"));
  if(button){const QPoint p=button->rect().center();QHelpEvent hover(QEvent::ToolTip,p,button->mapToGlobal(p));QApplication::sendEvent(button,&hover);}
 });
 QTimer::singleShot(1850,qApp,[]{
  check("native_hover",QToolTip::isVisible()&&QToolTip::text().contains("calibration reminder"));
  if(dock)dock->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/note-badge.png");
  for(auto *w:QApplication::topLevelWidgets())if(w->isVisible()&&w->windowType()==Qt::ToolTip)w->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/note-tooltip.png");
  QToolTip::hideText();
  auto *list=query("@calibration reminder");check("search_note_text",list&&list->count()==1&&list->item(0)->text().contains("NotesProbe Alpha"));
  if(list&&list->count())badgeClick(list);
 });
 QTimer::singleShot(2300,qApp,[]{
  auto *d=editor();auto *input=d?d->findChild<QPlainTextEdit*>():nullptr;
  check("badge_click_edit",input&&input->toPlainText()=="Private calibration reminder <b>literal</b>");
  if(d)d->reject();
  auto *label=dock->findChild<QLabel*>("aftermarket-inspect-transform");if(label)chooseMenu(label,label->rect().center());
  check("item_note_editor",editor()!=nullptr);check("save_item",save("Placement reminder"));
 });
 QTimer::singleShot(3000,qApp,[]{
  auto *badge=dock->findChild<QToolButton*>("aftermarket-item-note");
  check("item_badge",badge&&badge->isVisible()&&badge->toolTip().contains("Placement reminder"));
  auto *list=dock->findChild<QListWidget*>("aftermarket-inspect-filter-list");
  if(list&&list->count())chooseMenu(list->viewport(),list->visualItemRect(list->item(0)).center());
  check("filter_note_editor",editor()!=nullptr);check("save_filter",save("Noise adjustment reminder"));
 });
 QTimer::singleShot(3650,qApp,[]{
  auto *list=dock->findChild<QListWidget*>("aftermarket-inspect-filter-list");
  check("filter_preserves_check",list&&list->count()==1&&list->item(0)->checkState()==Qt::Checked&&obs_source_enabled(filter));
  if(list&&list->count())badgeClick(list);
  check("filter_badge_click",editor()!=nullptr);
  if(auto *d=editor())d->reject();
  obs_source_set_name(source,"NotesProbe Renamed");
  obs_sceneitem_select(a,false);obs_sceneitem_select(b,true);
 });
 QTimer::singleShot(4600,qApp,[]{
  auto *badge=dock->findChild<QToolButton*>("aftermarket-source-note");
  auto *item=dock->findChild<QToolButton*>("aftermarket-item-note");
  check("other_target_no_notes",badge&&badge->isHidden()&&item&&item->isHidden());
  auto *list=query("@calibration reminder");check("rename_search",list&&list->count()==1&&list->item(0)->text().contains("NotesProbe Renamed"));
  if(list&&list->count())badgeClick(list);
 });
 QTimer::singleShot(5200,qApp,[]{
  check("delete_note",save(""));
  QFile file(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/home/.config/obs-studio/plugin_config/aftermarket/context-notes.json");
  bool read=file.open(QIODevice::ReadOnly);auto data=QJsonDocument::fromJson(file.readAll()).object().value("notes").toObject();
  check("persistent_separate_scopes",read&&data.size()==2);
  auto *list=query("@calibration reminder");check("deleted_not_searchable",list&&list->count()==0);
  if(auto *popup=mainWindow->findChild<QDialog*>("aftermarket-command-popup"))popup->hide();
  auto *scene=obs_frontend_get_current_scene();if(scene){obs_source_set_name(scene,"NotesProbe Scene");obs_source_release(scene);}
  auto *scenes=query("@NotesProbe Scene");check("scene_query",scenes&&scenes->count()>=1&&scenes->item(0)->text().contains("NotesProbe Scene"));
  if(scenes&&scenes->count())context(scenes->viewport(),scenes->visualItemRect(scenes->item(0)).center());
 });
 QTimer::singleShot(5750,qApp,[]{
  check("save_scene",save("Scene-only cue reminder"));
  auto *list=query("@Scene-only cue reminder");check("scene_note_search",list&&list->count()==1&&list->item(0)->text().contains("NotesProbe Scene"));
  if(auto *popup=mainWindow->findChild<QDialog*>("aftermarket-command-popup"))popup->hide();
  obs_source_remove(source);obs_source_remove(second);obs_source_release(source);obs_source_release(second);obs_source_release(filter);
  mainWindow->close();
 });
}
static void frontend(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(4000,qApp,probe);}
bool obs_module_load(void){obs_frontend_add_event_callback(frontend,nullptr);return true;}
void obs_module_unload(void){obs_frontend_remove_event_callback(frontend,nullptr);}
