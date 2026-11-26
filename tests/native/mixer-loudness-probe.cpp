#include <functional>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QMenu>
#include <QToolButton>
#include <QTimer>
#include <QPointer>
#include <QVariant>
#include <array>
#include <cmath>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow;
static QPointer<QWidget> footer;
static obs_source_t *source=nullptr;
static QTimer *tone=nullptr;
static uint64_t sample=0,start=0;
static void check(const char *name,bool ok){blog(ok?LOG_INFO:LOG_ERROR,"LOUDCHECK %s=%d",name,ok);}
static void finish(){
 if(tone){tone->stop();delete tone;tone=nullptr;}
 if(source){obs_source_remove(source);obs_source_release(source);source=nullptr;}
 mainWindow->close();
}
static const char *name(void*){return "Loudness probe tone";}
static void *create(obs_data_t*,obs_source_t *s){return s;}
static void destroy(void*){}
static void send(){
 std::array<float,480> wave;
 for(auto &v:wave){v=.1f*std::sin(2*3.141592653589793*1000*sample/48000);++sample;}
 struct obs_source_audio a={};
 a.data[0]=reinterpret_cast<const uint8_t*>(wave.data());a.data[1]=a.data[0];
 a.frames=wave.size();a.speakers=SPEAKERS_STEREO;a.format=AUDIO_FORMAT_FLOAT_PLANAR;a.samples_per_sec=48000;
 a.timestamp=start+(sample-wave.size())*1000000000ULL/48000;
 obs_source_output_audio(source,&a);
}
static QAction *findAction(QMenu *m,const QString &id){
 for(auto *a:m->actions()){if(a->objectName()==id)return a;if(a->menu())if(auto *sub=findAction(a->menu(),id))return sub;}
 return nullptr;
}
static void openMenu(std::function<void(QMenu*)> callback){
 auto *dock=mainWindow->findChild<QDockWidget*>("mixerDock");
 auto *more=dock?dock->findChild<QToolButton*>("aftermarket-mixer-more"):nullptr;
 if(!more||!more->menu()){check("menu",false);finish();return;}
 auto *m=more->menu();m->popup(more->mapToGlobal(QPoint(0,more->height())));
 QTimer::singleShot(80,qApp,[m,callback]{m->hide();callback(m);});
}
static void probe(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 auto *dock=mainWindow->findChild<QDockWidget*>("mixerDock");
 footer=dock?dock->findChild<QWidget*>("aftermarket-mixer-loudness"):nullptr;
 check("footer",footer&&footer->isVisible()&&footer->parentWidget()->objectName()=="aftermarket-audiobus-controls");
 if(!footer){finish();return;}
 source=obs_source_create("am_loudness_probe","Loudness tone",nullptr,nullptr);
 auto *scene=obs_frontend_get_current_scene();
 if(!source||!scene){check("source",false);if(scene)obs_source_release(scene);finish();return;}
 obs_source_set_audio_mixers(source,1);
 obs_source_set_monitoring_type(source,OBS_MONITORING_TYPE_NONE);
 obs_scene_add(obs_scene_from_source(scene),source);
 obs_source_release(scene);
 start=os_gettime_ns();sample=0;tone=new QTimer(qApp);tone->setTimerType(Qt::PreciseTimer);
 QObject::connect(tone,&QTimer::timeout,qApp,send);tone->start(10);
 QTimer::singleShot(6500,qApp,[]{
  const auto i=footer->property("integrated").toDouble(),p=footer->property("truePeak").toDouble();
  blog(LOG_INFO,"LOUDDETAIL track1 I=%f TP=%f frames=%llu",i,p,(unsigned long long)footer->property("frames").toULongLong());
  check("tone_on_track1",std::abs(i+20)<1&&std::abs(p+20)<1);
  openMenu([](QMenu *m){
   auto *a=findAction(m,"aftermarket-loudness-track-2");check("track_selector",a);if(!a){finish();return;}a->trigger();
   QTimer::singleShot(3500,qApp,[]{
    check("track2_silence",footer->property("track").toInt()==2&&footer->property("frames").toULongLong()>100000&&!std::isfinite(footer->property("integrated").toDouble()));
    openMenu([](QMenu *m){
     auto *a=findAction(m,"aftermarket-loudness-track-1");if(!a){check("back_to_track1",false);finish();return;}a->trigger();
     QTimer::singleShot(2800,qApp,[]{
      check("back_to_track1",std::abs(footer->property("integrated").toDouble()+20)<1);
      openMenu([](QMenu *m){
       auto *reset=findAction(m,"aftermarket-loudness-reset");
       if(reset)reset->trigger();
       check("reset_history",reset&&footer->property("frames").toULongLong()==0);
       auto *enabled=findAction(m,"aftermarket-loudness-enabled");
       if(enabled)enabled->trigger();
       QTimer::singleShot(500,qApp,[enabled]{
        check("disable",enabled&&!footer->isVisible()&&footer->property("frames").toULongLong()==0);
        finish();
       });
      });
     });
    });
   });
  });
 });
}
static void ev(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(1800,qApp,probe);}
bool obs_module_load(){obs_source_info info={};info.id="am_loudness_probe";info.type=OBS_SOURCE_TYPE_INPUT;info.output_flags=OBS_SOURCE_AUDIO;info.get_name=name;info.create=create;info.destroy=destroy;obs_register_source(&info);obs_frontend_add_event_callback(ev,nullptr);return true;}
void obs_module_unload(){obs_frontend_remove_event_callback(ev,nullptr);}
