#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QGridLayout>
#include <QBoxLayout>
#include <QScrollArea>
#include <QTimer>
#include <QLabel>
#include <QPointer>
#include <QSet>
#include <QSlider>
#include <QToolButton>
#include <QToolBar>
#include <cmath>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow=nullptr;
static QPointer<QDockWidget> dock;
static QPointer<QBoxLayout> box;
static QList<obs_source_t*> sources;
static QList<QPointer<QWidget>> nativeChannels;
static obs_scene_t *scene=nullptr;
static void check(const char *name,bool ok){blog(ok?LOG_INFO:LOG_ERROR,"GRIDCHECK %s=%d",name,ok);}
static const char *name(void*){return "Grid probe audio";}
static void *create(obs_data_t*,obs_source_t *s){return s;}
static void destroy(void*){}
static void add(int n){
 auto label=QString("Grid audio %1").arg(n).toUtf8();
 auto *s=obs_source_create("am_grid_probe_audio",label.constData(),nullptr,nullptr);
 if(!s)return;
 sources.append(s);obs_source_set_volume(s,.42f);obs_scene_add(scene,s);
}
static QGridLayout *grid(){return dock?dock->findChild<QGridLayout*>("aftermarket-mixer-grid"):nullptr;}
static bool volume(QWidget *w){return w&&(w->inherits("VolControl")||w->objectName()=="VolControl"||w->inherits("VolumeControl")||w->objectName()=="VolumeControl");}
static QList<QWidget*> channels(){
 QList<QWidget*> out;if(!box||!box->parentWidget())return out;
 for(auto *w:box->parentWidget()->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly))
    if(volume(w))out.append(w);
 return out;
}
static void probe(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 dock=mainWindow->findChild<QDockWidget*>("mixerDock");
 auto *host=dock?dock->findChild<QWidget*>("hVolumeWidgets"):nullptr;
 box=host?qobject_cast<QBoxLayout*>(host->layout()):nullptr;
 check("native_mixer",dock&&box&&box->parentWidget()->objectName()=="hVolumeWidgets");
 if(!dock||!box){mainWindow->close();return;}
 auto *ss=obs_frontend_get_current_scene();scene=obs_scene_from_source(ss);
 if(!scene){obs_source_release(ss);mainWindow->close();return;}
 scene=obs_scene_get_ref(scene);obs_source_release(ss);
 for(int i=1;i<=5;++i)add(i);
 dock->setFloating(true);dock->resize(1050,580);dock->show();
 if(!box->parentWidget()->isVisible()){
  auto *bar=dock->findChild<QToolBar*>();
  if(bar)for(auto *b:bar->findChildren<QToolButton*>(QString(),Qt::FindDirectChildrenOnly)){
    if(b->objectName()=="qt_toolbar_ext_button"||!b->toolTip().contains("Horizontal Layout",Qt::CaseInsensitive))continue;
    blog(LOG_INFO,"GRIDDETAIL layout button %s %s",qPrintable(b->text()),qPrintable(b->toolTip()));b->click();break;
  }
 }
 QTimer::singleShot(1700,qApp,[]{
  auto *g=grid();const auto all=channels();
  auto *v=dock->findChild<QWidget*>("vVolumeWidgets");
  blog(LOG_INFO,"GRIDDETAIL host visible=%d vertical visible=%d count=%d attached=%d",box->parentWidget()->isVisible(),v&&v->isVisible(),v&&v->layout()?v->layout()->count():-1,bool(dock->findChild<QWidget*>("aftermarket-dock-faces")));
  for(auto *s:sources)blog(LOG_INFO,"GRIDDETAIL active=%d audio=%d",obs_source_active(s),obs_source_audio_active(s));
  blog(LOG_INFO,"GRIDDETAIL wide %d channels=%d grid=%d cols=%d",dock->width(),int(all.size()),g?g->count():0,g?g->columnCount():0);
  check("wide_grid",g&&g->count()>=5&&g->columnCount()>=2&&g->rowCount()>=2);
  bool identity=all.size()>=5;for(auto *w:all){nativeChannels.append(w);identity&=w->parentWidget()==box->parentWidget()&&w->findChild<QWidget*>("aftermarket-source-adjustments");}
  check("native_channel_controls",identity);
  if(g){auto *a=g->itemAtPosition(0,0);auto *b=g->itemAtPosition(0,1);check("same_row",a&&b&&a->widget()->y()==b->widget()->y()&&a->widget()->x()<b->widget()->x());}
  else check("same_row",false);
  dock->resize(360,580);
 });
 QTimer::singleShot(2700,qApp,[]{
  check("narrow_native_list",!grid()&&box->count()>=5);
  bool same=true;for(const auto &w:nativeChannels)same&=w&&box->indexOf(w)>=0;
  check("same_widgets_after_resize",same);
  dock->resize(1050,580);
 });
 QTimer::singleShot(3800,qApp,[]{
  check("reflow_wide",grid()&&grid()->count()>=5);
  add(6);
 });
 QTimer::singleShot(5400,qApp,[]{
  check("native_add",grid()&&grid()->count()>=6);
  auto *s=sources.takeLast();obs_source_remove(s);obs_source_release(s);
 });
 QTimer::singleShot(6900,qApp,[]{
  check("native_remove",grid()&&grid()->count()==channels().size()&&grid()->count()>=5);
  bool unchanged=true;for(auto *s:sources)unchanged&=std::abs(obs_source_get_volume(s)-.42f)<.001f;
  check("gain_unchanged",unchanged);
  if(dock)dock->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/grid.png");
  for(auto *s:sources){obs_source_remove(s);obs_source_release(s);}sources.clear();
  obs_scene_release(scene);scene=nullptr;mainWindow->close();
 });
}
static void frontend(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(3500,qApp,probe);}
bool obs_module_load(void){
 obs_source_info info={};info.id="am_grid_probe_audio";info.type=OBS_SOURCE_TYPE_INPUT;info.output_flags=OBS_SOURCE_AUDIO;
 info.get_name=name;info.create=create;info.destroy=destroy;
 obs_register_source(&info);obs_frontend_add_event_callback(frontend,nullptr);return true;
}
void obs_module_unload(void){obs_frontend_remove_event_callback(frontend,nullptr);}
