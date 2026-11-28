#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QAction>
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QTimer>
#include <QPointer>
#include <QFile>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow;
static obs_source_t *source;
static obs_canvas_t *canvas;
static obs_scene_t *secondary;
static QString originalScene;
static QPointer<QDialog> popup;
static QPointer<QWidget> unscene;
static void check(const char *name,bool ok){blog(ok?LOG_INFO:LOG_ERROR,"SEARCHCHECK %s=%d",name,ok);}
static QString name(obs_source_t *s){return QString::fromUtf8(obs_source_get_name(s));}
static QString program(){auto *s=obs_frontend_get_current_scene();QString id=s?QString::fromUtf8(obs_source_get_uuid(s)):QString{};obs_source_release(s);return id;}
static bool rowContains(QListWidget *list,const QString &text){
 for(int i=0;list&&i<list->count();++i)if(list->item(i)->text().contains(text))return true;return false;
}
static void end(){
 if(popup)popup->hide();
 if(secondary){obs_canvas_scene_remove(secondary);obs_scene_release(secondary);}
 if(canvas){obs_canvas_remove(canvas);obs_canvas_release(canvas);}
 if(source)obs_source_release(source);
 QTimer::singleShot(300,qApp,[]{mainWindow->close();});
}
static void probe(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 originalScene=program();
 obs_frontend_source_list scenes{};obs_frontend_get_scenes(&scenes);
 source=obs_get_source_by_name("Color");
 if(!source||scenes.sources.num<2){check("fixture",false);obs_frontend_source_list_free(&scenes);end();return;}
 auto *inactive=scenes.sources.array[0];if(QString::fromUtf8(obs_source_get_uuid(inactive))==originalScene)inactive=scenes.sources.array[1];
 const auto inactiveName=name(inactive);
 auto *item=obs_scene_add(obs_scene_from_source(inactive),source);
 check("fixture",item!=nullptr);
 obs_frontend_source_list_free(&scenes);
 canvas=obs_canvas_create("Portrait fixture",nullptr,SCENE_REF);
 secondary=canvas?obs_canvas_scene_create(canvas,"Portrait search scene"):nullptr;
 if(secondary)obs_scene_add(secondary,source);
 auto *action=mainWindow->findChild<QAction*>("aftermarket-tools-find");
 if(!action){check("popup",false);end();return;}
 action->trigger();
 QTimer::singleShot(500,qApp,[inactiveName]{
  popup=mainWindow->findChild<QDialog*>("aftermarket-quick-find");
  unscene=mainWindow->findChild<QWidget*>("UnsceneContent");
  check("popup",popup&&popup->isVisible()&&unscene);
  if(!popup||!unscene){end();return;}
  auto *scope=popup->findChild<QComboBox*>("aftermarket-quick-find-scope");
  auto *type=popup->findChild<QComboBox*>("aftermarket-quick-find-type");
  auto *canvasFilter=popup->findChild<QComboBox*>("aftermarket-quick-find-canvas");
  auto *list=popup->findChild<QListWidget*>("aftermarket-quick-find-results");
  auto *input=popup->findChild<QLineEdit*>("aftermarket-quick-find-input");
  check("canvas",canvasFilter&&canvasFilter->findText("Portrait fixture")>0&&rowContains(list,"Portrait search scene"));
  canvasFilter->setCurrentIndex(canvasFilter->findText("Portrait fixture"));check("canvas_filter",rowContains(list,"Portrait search scene")&&rowContains(list,"Color"));
  canvasFilter->setCurrentIndex(0);
  scope->setCurrentIndex(2);
  type->setCurrentIndex(type->findData("crop_filter"));
  check("scope_filter",type->currentData().toString()=="crop_filter"&&list->count()==1&&list->item(0)->text().startsWith("Crop/Pad"));
  type->setCurrentIndex(0);
  scope->setCurrentIndex(1);type->setCurrentIndex(type->findData("color_source"));
  input->setText("Color");check("type_filter",list->count()>0&&rowContains(list,"Color"));
  type->setCurrentIndex(0);input->setText("Search probe renamed");
  obs_source_set_name(source,"Search probe renamed");
  QTimer::singleShot(1100,qApp,[inactiveName,list,input]{
   check("live_rename",list->count()==1&&rowContains(list,"Search probe renamed"));
   auto *count=popup->findChild<QLabel*>("aftermarket-quick-find-count");
   check("count",count&&count->text().startsWith("1 result"));
   popup->grab().save(qEnvironmentVariable("PRESET_ROOT")+"/quick-find.png");
   auto *used=popup->findChild<QPushButton*>("aftermarket-quick-find-used");
   if(!used||!used->isEnabled()){check("reveal",false);end();return;}
   QTimer::singleShot(0,qApp,[inactiveName]{
    auto *menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());
    bool revealed=false;
    if(menu)for(auto *branch:menu->actions()){
     if(!branch->text().contains(inactiveName)||!branch->menu())continue;
     for(auto *action:branch->menu()->actions())if(action->text().startsWith("Reveal in Unscene")){
      action->trigger();revealed=true;break;
     }
     if(revealed)break;
    }
    if(menu)menu->close();
    check("reveal",revealed&&unscene->isVisible()&&program()==originalScene&&!popup->isVisible());
   });
   used->click();
   QTimer::singleShot(500,qApp,[]{
    bool found=false;for(auto *label:unscene->findChildren<QLabel*>())if(label->isVisible()&&label->accessibleName()=="Search probe renamed")found=true;
    check("revealed_row",found&&program()==originalScene);
    auto *search=unscene->findChild<QLineEdit*>();
    for(auto *candidate:unscene->findChildren<QLineEdit*>())if(candidate->accessibleName().startsWith("Find scenes"))search=candidate;
    if(!search){check("hierarchy_search",false);end();return;}
    search->setText("search probe renamed");
    QTimer::singleShot(600,qApp,[]{
     auto *count=unscene->findChild<QLabel*>("unscene-search-count");
     check("hierarchy_search",count&&count->isVisible()&&!count->text().startsWith("0 match"));
     unscene->grab().save(qEnvironmentVariable("PRESET_ROOT")+"/unscene-search.png");
     auto *facet=unscene->findChild<QToolButton*>("unscene-search-facets");
     if(!facet){check("unscene_filter_scope",false);end();return;}
     for(auto *search:unscene->findChildren<QLineEdit*>())if(search->accessibleName().startsWith("Find scenes"))search->setText("Crop/Pad");
     QTimer::singleShot(0,qApp,[]{
      auto *menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());bool chosen=false;
      if(menu)for(auto *a:menu->actions())if(a->menu()&&a->text()=="Search scope")
       for(auto *scope:a->menu()->actions())if(scope->text()=="Sources with matching filters"){scope->trigger();chosen=true;break;}
      if(menu)menu->close();check("unscene_filter_scope",chosen);
     });
     facet->click();
     QTimer::singleShot(500,qApp,[]{
      auto *count=unscene->findChild<QLabel*>("unscene-search-count");
      check("unscene_filter_matches",count&&count->isVisible()&&!count->text().startsWith("0 match"));
      end();
     });
    });
   });
  });
 });
}
static void event(obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(3500,qApp,probe);}
bool obs_module_load(){obs_frontend_add_event_callback(event,nullptr);return true;}
void obs_module_unload(){obs_frontend_remove_event_callback(event,nullptr);}
