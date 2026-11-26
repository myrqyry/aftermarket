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
#include <QDial>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QSpinBox>
#include <QMenu>
#include <QAbstractButton>
#include <cmath>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow=nullptr;
static QPointer<QDockWidget> dock;
static QPointer<QBoxLayout> box;
static QList<obs_source_t*> sources;
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
static QBoxLayout *centerLayout(QLayout *layout){
 if(!layout)return nullptr;
 if(layout->objectName()=="aftermarket-rotary-center")return qobject_cast<QBoxLayout*>(layout);
 for(int i=0;i<layout->count();++i)if(auto *found=centerLayout(layout->itemAt(i)->layout()))return found;
 return nullptr;
}
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
  auto all=channels();check("channels",all.size()==5);
  bool layout=!all.isEmpty(),status=layout,position=layout,values=layout,exact=layout,encoder=false;
  for(auto *w:all){
   auto *center=centerLayout(w->layout());
   auto *db=w->findChild<QLabel*>("volLabel");
   auto *frame=w->findChild<QWidget*>("volMeterFrame");
   auto *pan=w->findChild<QDial*>("aftermarket-pan-dial");
   auto *offset=w->findChild<QDial*>("aftermarket-offset-dial");
   auto *spin=w->findChild<QSpinBox*>("aftermarket-offset-ms");
   layout&=center&&db&&frame&&center->indexOf(frame)>=0&&db->isVisible();
   if(!db||!frame||!pan||!offset||!spin){position=false;exact=false;continue;}
   QList<QAbstractButton*> buttons;
   for(auto *b:w->findChildren<QAbstractButton*>(QString(),Qt::FindDirectChildrenOnly))
    if(b->toolTip()=="Mute"||b->toolTip()=="Enable Monitoring")buttons.append(b);
   const auto dbPos=db->mapTo(w,db->rect().center());
   const auto panPos=pan->mapTo(w,pan->rect().center()),syncPos=offset->mapTo(w,offset->rect().center());
   position&=pan->isVisible()&&offset->isVisible()&&!spin->isVisible()&&
      dbPos.y()>frame->geometry().bottom()&&panPos.x()>dbPos.x()&&panPos.y()<syncPos.y()&&buttons.size()==2;
   auto *gain=frame->findChild<QSlider*>();
   if(gain&&buttons.size()==2){
     const auto middle=gain->mapTo(w,gain->rect().center());
     const int radius=(qMin(68,qMin(gain->width(),gain->height()))-6)/2;
     auto top=[w](QWidget *control,int inset=0){return control->mapTo(w,QPoint(0,inset)).y();};
     auto bottom=[w](QWidget *control,int inset=0){return control->mapTo(w,QPoint(0,control->height()-inset)).y();};
     position&=buttons[0]->mapTo(w,buttons[0]->rect().center()).x()<middle.x()&&middle.x()<panPos.x()&&
      std::abs(top(buttons[0])-(middle.y()-radius))<=5&&
      std::abs(bottom(buttons[1])-(middle.y()+radius))<=5&&
      std::abs(top(pan,2)-(middle.y()-radius))<=5&&
      std::abs(bottom(offset,2)-(middle.y()+radius))<=5;
     blog(LOG_INFO,"GRIDDETAIL vertical gain=%d,%d radius=%d buttons=%d..%d dials=%d..%d",middle.x(),middle.y(),radius,top(buttons[0]),bottom(buttons[1]),top(pan,2),bottom(offset,2));
   }else position=false;
   bool icon=false;
   for(auto *label:w->findChildren<QLabel*>(QString(),Qt::FindDirectChildrenOnly))
      if(label->text()=="Active"&&label->maximumWidth()==18&&label->toolTip()=="Active")icon=true;
   status&=icon;
   values&=db->text().contains("dB")&&db->text().contains("-7.5");
   if(w==all.first()){
    QMetaObject::invokeMethod(offset,"customContextMenuRequested",Qt::DirectConnection,Q_ARG(QPoint,QPoint(8,8)));
    auto *menu=offset->findChild<QMenu*>("aftermarket-precise-sync-menu");
    auto *edit=menu?menu->findChild<QSpinBox*>("aftermarket-precise-sync-value"):nullptr;
    auto *source=obs_get_source_by_name(w->objectName().toUtf8().constData());
    exact=edit&&source;
    if(edit&&source){edit->setValue(12345);exact&=obs_source_get_sync_offset(source)==12345000000LL;}
    if(menu)menu->close();
    if(source){
      const QPoint p=offset->rect().center(),global=offset->mapToGlobal(p);
      QWheelEvent wheel(p,global,QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
      QApplication::sendEvent(offset,&wheel);
      encoder=obs_source_get_sync_offset(source)==12355000000LL;
      QMouseEvent press(QEvent::MouseButtonPress,p,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
      QApplication::sendEvent(offset,&press);
      encoder&=obs_source_get_sync_offset(source)==12355000000LL;
      QMouseEvent move(QEvent::MouseMove,p-QPoint(0,42),global-QPoint(0,42),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
      QApplication::sendEvent(offset,&move);
      QMouseEvent release(QEvent::MouseButtonRelease,p-QPoint(0,42),global-QPoint(0,42),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
      QApplication::sendEvent(offset,&release);
      encoder&=obs_source_get_sync_offset(source)==12775000000LL;
      spin->setValue(0);
      obs_source_release(source);
    }
   }
  }
  check("db_below_gain",layout);check("side_controls_aligned",position);check("native_active_icon",status);
  check("native_db_values",values);check("exact_sync_popup",exact);check("relative_encoder_beyond_2s",encoder);
  dock->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/rotary-grid.png");
  dock->resize(360,580);
 });
 QTimer::singleShot(3100,qApp,[]{
  bool narrow=!grid()&&channels().size()==5;
  for(auto *w:channels())narrow&=centerLayout(w->layout())!=nullptr;
  check("narrow_rotary",narrow);
  dock->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/rotary-list.png");
  bool unchanged=true;for(auto *s:sources)unchanged&=std::abs(obs_source_get_volume(s)-.42f)<.001f&&obs_source_get_sync_offset(s)==0;
  check("gain_and_offset_preserved",unchanged);
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
