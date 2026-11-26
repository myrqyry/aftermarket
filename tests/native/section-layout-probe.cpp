#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QSpinBox>
#include <QToolButton>
#include <QComboBox>
#include <QMenu>
#include <QTimer>
#include <QPointer>
#include <QImage>
OBS_DECLARE_MODULE()
static obs_source_t *source=nullptr;
static QPointer<QDialog> dlg;
static QPointer<QGroupBox> group;
static QPointer<QToolButton> toggle;
static QList<QSpinBox*> fields;
static void check(const char *key,bool ok){blog(ok?LOG_INFO:LOG_ERROR,"SECTIONCHECK %s=%d",key,ok);}
static const char *name(void*){return "Section layout probe";}
static void *create(obs_data_t*,obs_source_t *s){return s;}
static void destroy(void*){}
static uint32_t width(void*){return 320;}
static uint32_t height(void*){return 180;}
static obs_properties_t *properties(void*){
 auto *p=obs_properties_create();auto *g=obs_properties_create();
 for(int i=0;i<6;++i){auto k=QByteArray("control")+QByteArray::number(i);auto label=QByteArray("Camera ")+QByteArray::number(i);obs_properties_add_int_slider(g,k.constData(),label.constData(),0,100,1);}
 auto *hidden=obs_properties_add_int(g,"hidden","Hidden",0,100,1);obs_property_set_visible(hidden,false);
 obs_properties_add_group(p,"camera","Camera Controls",OBS_GROUP_NORMAL,g);
 return p;
}
static void wide(){
 dlg->resize(860,720);
 QTimer::singleShot(500,qApp,[]{
  bool arranged=fields.size()==6&&fields[0]->y()==fields[1]->y()&&fields[0]->x()<fields[1]->x();
  check("wide_section_columns",arranged);
  auto *more=dlg->findChild<QToolButton*>("aftermarket-preset-more");
  auto *combo=dlg->findChild<QComboBox*>("aftermarket-preset-select");
  bool dots=false;
  if(more){
    auto img=more->grab().toImage();auto fg=more->palette().color(QPalette::ButtonText);
    int runs=0;bool before=false;
    for(int y=0;y<img.height();++y){bool hit=false;for(int x=qMax(0,img.width()/2-2);x<qMin(img.width(),img.width()/2+3);++x){auto p=img.pixelColor(x,y);hit|=qAbs(p.red()-fg.red())+qAbs(p.green()-fg.green())+qAbs(p.blue()-fg.blue())<100;}
     if(hit&&!before)++runs;before=hit;
    }
    dots=runs==3&&more->text().isEmpty()&&more->menu();
  }
  check("preset_single_ellipsis",dots&&combo&&combo->isVisible());
  if(more&&more->menu()){
    QTimer::singleShot(80,qApp,[more]{if(more->menu())more->menu()->close();});
    bool opened=false;QObject::connect(more->menu(),&QMenu::aboutToShow,more,[&opened]{opened=true;},Qt::SingleShotConnection);
    more->click();check("preset_menu_opens",opened);
  }else check("preset_menu_opens",false);
  dlg->grab().save(qEnvironmentVariable("PRESET_ROOT")+"/wide.png");
  if(toggle)toggle->click();
  QTimer::singleShot(200,qApp,[]{
   bool hidden=fields.size()==6;for(auto *w:fields)hidden&=!w->isVisible();
   check("collapse_reflowed_group",hidden);
   dlg->resize(480,720);
   if(toggle)toggle->click();
   QTimer::singleShot(500,qApp,[]{
    bool narrow=fields.size()==6&&fields[0]->y()<fields[1]->y();for(auto *w:fields)narrow&=w->isVisible();
    check("narrow_expand_restores",narrow);
    dlg->grab().save(qEnvironmentVariable("PRESET_ROOT")+"/narrow.png");
    dlg->close();obs_source_release(source);static_cast<QMainWindow*>(obs_frontend_get_main_window())->close();
   });
  });
 });
}
static void start(){
 source=obs_source_create("am_section_probe","Camera fixture",nullptr,nullptr);
 if(!source){check("source",false);static_cast<QMainWindow*>(obs_frontend_get_main_window())->close();return;}
 auto *scene=obs_frontend_get_current_scene();if(scene){obs_scene_add(obs_scene_from_source(scene),source);obs_source_release(scene);}
 obs_frontend_open_source_properties(source);
 QTimer::singleShot(750,qApp,[]{
  for(auto *w:QApplication::topLevelWidgets())if(w->inherits("OBSBasicProperties")&&w->isVisible())dlg=qobject_cast<QDialog*>(w);
  if(dlg)for(auto *g:dlg->findChildren<QGroupBox*>())if(g->title()=="Camera Controls")group=g;
  if(group){toggle=group->findChild<QToolButton*>("aftermarket-group-toggle");for(auto *spin:group->findChildren<QSpinBox*>())if(!spin->isHidden())fields.append(spin);}
  check("native_group",group&&toggle&&fields.size()==6);
  if(!group||!toggle||fields.size()!=6){if(dlg)dlg->close();obs_source_release(source);static_cast<QMainWindow*>(obs_frontend_get_main_window())->close();return;}
  wide();
 });
}
static void event(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(900,qApp,start);}
bool obs_module_load(){obs_source_info info={};info.id="am_section_probe";info.type=OBS_SOURCE_TYPE_INPUT;info.output_flags=OBS_SOURCE_VIDEO;info.get_name=name;info.create=create;info.destroy=destroy;info.get_properties=properties;info.get_width=width;info.get_height=height;obs_register_source(&info);obs_frontend_add_event_callback(event,nullptr);return true;}
void obs_module_unload(){obs_frontend_remove_event_callback(event,nullptr);}
