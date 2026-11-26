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
#include <QContextMenuEvent>
#include <QSpinBox>
#include <QMenu>
#include <QAbstractButton>
#include <QStyleOptionButton>
#include <QStyle>
#include <QImage>
#include <QEventLoop>
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
 auto *host=dock?dock->findChild<QWidget*>("vVolumeWidgets"):nullptr;
 box=host?qobject_cast<QBoxLayout*>(host->layout()):nullptr;
 check("native_mixer",dock&&box&&box->parentWidget()->objectName()=="vVolumeWidgets");
 if(!dock||!box){mainWindow->close();return;}
 auto *ss=obs_frontend_get_current_scene();scene=obs_scene_from_source(ss);
 if(!scene){obs_source_release(ss);mainWindow->close();return;}
 scene=obs_scene_get_ref(scene);obs_source_release(ss);
 for(int i=1;i<=5;++i)add(i);
 dock->setFloating(true);dock->resize(1050,580);dock->show();
 if(!box->parentWidget()->isVisible()){
  auto *bar=dock->findChild<QToolBar*>();
  if(bar)for(auto *b:bar->findChildren<QToolButton*>(QString(),Qt::FindDirectChildrenOnly)){
    if(b->objectName()=="qt_toolbar_ext_button"||!b->toolTip().contains("Vertical Layout",Qt::CaseInsensitive))continue;
    blog(LOG_INFO,"GRIDDETAIL layout button %s %s",qPrintable(b->text()),qPrintable(b->toolTip()));b->click();break;
  }
 }
 QTimer::singleShot(1700,qApp,[]{
  auto all=channels();check("channels",all.size()==5);
  bool layout=!all.isEmpty(),status=layout,position=layout,values=layout,exact=layout,encoder=false,resetKnobs=false,resetMenus=false;
  for(auto *w:all){
   auto *root=qobject_cast<QBoxLayout*>(w->layout());
   auto *db=w->findChild<QLabel*>("volLabel");
   auto *frame=w->findChild<QWidget*>("volMeterFrame");
   auto *pan=w->findChild<QDial*>("aftermarket-pan-dial");
   auto *offset=w->findChild<QDial*>("aftermarket-offset-dial");
   auto *spin=w->findChild<QSpinBox*>("aftermarket-offset-ms");
   auto *gain=frame?frame->findChild<QSlider*>():nullptr;
   layout&=root&&db&&frame&&gain&&db->isVisible()&&gain->property("_obsUiAugmentIntegratedMeter").toBool();
   if(!db||!frame||!pan||!offset||!spin){position=false;exact=false;continue;}
   QList<QAbstractButton*> buttons;
   for(auto *b:w->findChildren<QAbstractButton*>(QString(),Qt::FindDirectChildrenOnly))
    if(b->toolTip()=="Mute"||b->toolTip()=="Enable Monitoring")buttons.append(b);
   const auto dbPos=db->mapTo(w,db->rect().center());
   const auto panPos=pan->mapTo(w,pan->rect().center()),syncPos=offset->mapTo(w,offset->rect().center());
   position&=gain&&pan->isVisible()&&offset->isVisible()&&!spin->isVisible()&&
       dbPos.y()>frame->geometry().bottom()&&dbPos.y()-gain->mapTo(w,QPoint()).y()-gain->height()<30&&panPos.y()==syncPos.y()&&panPos.x()<syncPos.x()&&buttons.size()==2;
   if(gain&&buttons.size()==2){
      const auto first=buttons[0]->mapTo(w,buttons[0]->rect().center()),second=buttons[1]->mapTo(w,buttons[1]->rect().center());
      const auto gainPos=gain->mapTo(w,gain->rect().center());
      const int radius=(qMin(68,qMin(gain->width(),gain->height()))-6)/2;
      auto left=[w](QWidget *control,int inset=0){return control->mapTo(w,QPoint(inset,0)).x();};
      auto right=[w](QWidget *control,int inset=0){return control->mapTo(w,QPoint(control->width()-inset,0)).x();};
      position&=first.y()==second.y()&&first.y()<gainPos.y()&&gainPos.y()<panPos.y()&&
        std::abs(left(buttons[0])-(gainPos.x()-radius))<=5&&
        std::abs(right(buttons[1])-(gainPos.x()+radius))<=5&&
        std::abs(left(pan,2)-(gainPos.x()-radius))<=5&&
        std::abs(right(offset,2)-(gainPos.x()+radius))<=5;
      blog(LOG_INFO,"GRIDDETAIL width=%d gain=%d,%d radius=%d buttons=%d..%d dials=%d..%d",w->width(),gainPos.x(),gainPos.y(),radius,left(buttons[0]),right(buttons[1]),left(pan,2),right(offset,2));
   }
   bool icon=false;
   for(auto *label:w->findChildren<QLabel*>(QString(),Qt::FindDirectChildrenOnly))
      if(label->text()=="Active"&&label->isHidden())icon=true;
   QAbstractButton *title=nullptr;
   for(auto *b:w->findChildren<QAbstractButton*>(QString(),Qt::FindDirectChildrenOnly))
       if(b->inherits("VolumeName"))title=b;
   status&=icon&&title&&title->property("aftermarket-channel-state").toString()=="Active";
   if(title){
       const int right=w->width()-title->mapTo(w,QPoint(title->width(),0)).x();
       blog(LOG_INFO,"GRIDDETAIL header width=%d channel=%d right-gap=%d height=%d",title->width(),w->width(),right,title->height());
       status&=right<=8;
   }
   values&=db->text().contains("dB")&&db->text().contains("-7.5");
   status&=title&&title->property("aftermarket-two-line-title").toBool()&&title->height()>=2*title->fontMetrics().lineSpacing();
   if(w==all.first()){
    QAbstractButton *nameButton=nullptr;
    for(auto *b:w->findChildren<QAbstractButton*>())if(b->inherits("VolumeName"))nameButton=b;
    bool dots=false,nativeMenuOpened=false;
    if(nameButton){
      QStyleOptionButton opt;opt.initFrom(nameButton);
      const int padding=opt.rect.right()-nameButton->style()->subElementRect(QStyle::SE_PushButtonContents,&opt,nameButton).right();
      const int width=nameButton->style()->pixelMetric(QStyle::PM_MenuButtonIndicator,&opt,nameButton);
      const QRect area(opt.rect.right()-width-padding/2,qMax(0,(nameButton->fontMetrics().lineSpacing()-width)/2),width,width);
      const auto img=nameButton->grab().toImage().copy(area);
      // Compare against palette foreground; each dot yields a distinct
      // connected vertical run through the glyph's center.
      const auto fg=nameButton->palette().color(QPalette::ButtonText);
      int runs=0;bool before=false;
      for(int y=0;y<img.height();++y){
        bool hit=false;
        for(int x=0;x<img.width();++x){
          const auto c=img.pixelColor(x,y);
          hit|=std::abs(c.red()-fg.red())+std::abs(c.green()-fg.green())+std::abs(c.blue()-fg.blue())<120;
        }
        if(hit&&!before)++runs;before=hit;
      }
      dots=runs==3;
      nameButton->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/channel-menu-dots.png");
      QTimer::singleShot(50,qApp,[&nativeMenuOpened]{
        for(auto *widget:QApplication::topLevelWidgets())
          if(auto *menu=qobject_cast<QMenu*>(widget);menu&&menu->isVisible()&&!menu->actions().isEmpty()){
            nativeMenuOpened=true;menu->close();
          }
      });
      nameButton->click();
      QEventLoop settle;QTimer::singleShot(100,&settle,&QEventLoop::quit);settle.exec();
    }
    check("channel_menu_three_dots",dots);
    check("channel_menu_native_action",nativeMenuOpened);
   }
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
      auto reset=[](QWidget *knob){
        const QPoint p=knob->rect().center(),global=knob->mapToGlobal(p);
        QMouseEvent dbl(QEvent::MouseButtonDblClick,p,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(knob,&dbl);
        QMouseEvent release(QEvent::MouseButtonRelease,p,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(knob,&release);
      };
      pan->setValue(75);
      reset(gain);reset(pan);reset(offset);
      resetKnobs=std::abs(obs_source_get_volume(source)-1.f)<.001f&&
        std::abs(obs_source_get_balance_value(source)-.5f)<.001f&&
        obs_source_get_sync_offset(source)==0&&db->text().contains("0.0 dB")&&
        !gain->isSliderDown()&&!pan->isSliderDown()&&!offset->isSliderDown();
      // Exercise the actual context menu invocation and action, rather
      // than only checking the existence of a synthetic QAction.
      auto resetFromMenu=[](QWidget *knob,const char *id,bool requireNative){
        const QPoint p=knob->rect().center();
        QContextMenuEvent context(QContextMenuEvent::Mouse,p,knob->mapToGlobal(p));
        QApplication::sendEvent(knob,&context);
        auto *menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if(!menu)return false;
        auto *action=menu->findChild<QAction*>(id);
        const bool ok=action&&action->isEnabled()&&(!requireNative||menu->actions().size()>2);
        if(ok)action->trigger();
        menu->close();
        return ok;
      };
      gain->setValue(gain->maximum()/2);pan->setValue(75);offset->setValue(1200);
      resetMenus=resetFromMenu(gain,"aftermarket-reset-gain",true)&&
        resetFromMenu(pan,"aftermarket-reset-pan",false)&&
        resetFromMenu(offset,"aftermarket-reset-sync",false)&&
        std::abs(obs_source_get_volume(source)-1.f)<.001f&&
        std::abs(obs_source_get_balance_value(source)-.5f)<.001f&&obs_source_get_sync_offset(source)==0;
      obs_source_set_volume(source,.42f);
      spin->setValue(0);
      obs_source_release(source);
    }
   }
  }
  check("db_below_gain",layout);check("side_controls_aligned",position);check("native_state_styled_header",status);
  check("native_db_values",values);check("exact_sync_popup",exact);check("relative_encoder_beyond_2s",encoder);check("native_knob_defaults",resetKnobs);check("native_knob_reset_menus",resetMenus);
  dock->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/rotary-columns.png");
  dock->resize(360,580);
 });
 QTimer::singleShot(3100,qApp,[]{
  bool narrow=channels().size()==5;
  for(auto *w:channels()){
    auto *offset=w->findChild<QDial*>("aftermarket-offset-dial");
    narrow&=offset&&offset->isVisible()&&w->width()<=110;
  }
  check("narrow_column_encoder",narrow);
  auto *g=grid();
  check("column_grid_when_room",g&&g->count()==5&&g->rowCount()>=2&&g->columnCount()>=2);
  if(g)check("column_grid_order",g->itemAtPosition(0,0)->widget()->objectName()=="Grid audio 1"&&g->itemAtPosition(1,0)->widget()->y()>g->itemAtPosition(0,0)->widget()->y());
  else check("column_grid_order",false);
  dock->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/rotary-columns-narrow.png");
  dock->resize(360,260);
 });
 QTimer::singleShot(4600,qApp,[]{
  check("short_dock_native_row",!grid()&&channels().size()==5);
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
