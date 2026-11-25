#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QMouseEvent>
#include <QTimer>
#include <QElapsedTimer>
#include <QPointer>
#include <QSet>
#include <algorithm>

OBS_DECLARE_MODULE()
static QMainWindow *mainWindow=nullptr;
static QPointer<QDockWidget> dock;
static int originalMin,originalMax,originalWidth,originalHeight;
class Sample:public QObject{
public:
 QElapsedTimer clock;
 int dockResizes=0,previewResizes=0,heartbeats=0,last=0,maxGap=0;
 QSet<int> heights;
 QPointer<QWidget> preview;
 QTimer heartbeat;
 Sample(QWidget *p):preview(p){
   heartbeat.setInterval(8);heartbeat.setTimerType(Qt::PreciseTimer);
   connect(&heartbeat,&QTimer::timeout,this,[this]{
     int now=clock.elapsed();maxGap=std::max(maxGap,now-last);last=now;++heartbeats;
     if(dock)heights.insert(dock->maximumHeight());
   });
 }
 bool eventFilter(QObject *o,QEvent *e)override{
   if(e->type()==QEvent::Resize){if(o==dock)++dockResizes;else if(o==preview)++previewResizes;}
   return false;
 }
 void start(){
   clock.start();last=0;maxGap=heartbeats=dockResizes=previewResizes=0;heights.clear();
   if(dock)dock->installEventFilter(this);
   if(preview)preview->installEventFilter(this);
   heartbeat.start();
 }
 void finish(const char *phase){
   heartbeat.stop();if(dock)dock->removeEventFilter(this);if(preview)preview->removeEventFilter(this);
   blog(LOG_INFO,"PANELMETRIC %s dock_resize=%d preview_resize=%d sampled_max_heights=%d heartbeat=%d max_gap_ms=%d",phase,dockResizes,previewResizes,int(heights.size()),heartbeats,maxGap);
 }
};
static Sample *sample=nullptr;
static void check(const char *name,bool ok){blog(ok?LOG_INFO:LOG_ERROR,"PANELCHECK %s=%d",name,ok);}
static void toggle(){
 if(!dock)return;
 const QPointF p(12,6);
 QMouseEvent event(QEvent::MouseButtonRelease,p,dock->mapToGlobal(p.toPoint()),Qt::MiddleButton,Qt::NoButton,Qt::NoModifier);
 QApplication::sendEvent(dock,&event);
}
static void probe(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 dock=mainWindow->findChild<QDockWidget*>("sourcesDock");
 check("native_dock",dock&&dock->isVisible()&&dock->widget()&&dock->widget()->isVisible());
 if(!dock){mainWindow->close();return;}
 // Use a dock that shares the live preview's main-window layout.
 originalMin=dock->minimumHeight();originalMax=dock->maximumHeight();originalWidth=dock->minimumWidth();originalHeight=dock->height();
 sample=new Sample(mainWindow->findChild<QWidget*>("preview"));
 sample->start();toggle();
 QTimer::singleShot(350,qApp,[]{
   sample->finish("collapse");
   check("collapsed",dock&&dock->widget()->isHidden()&&dock->height()<originalHeight);
   sample->start();toggle();
 });
 QTimer::singleShot(700,qApp,[]{
   sample->finish("expand");
   check("expanded",dock&&dock->widget()->isVisible()&&dock->minimumHeight()==originalMin&&dock->maximumHeight()==originalMax&&dock->minimumWidth()==originalWidth);
   delete sample;sample=nullptr;
   mainWindow->close();
 });
}
static void frontend(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(3500,qApp,probe);}
bool obs_module_load(void){obs_frontend_add_event_callback(frontend,nullptr);return true;}
void obs_module_unload(void){obs_frontend_remove_event_callback(frontend,nullptr);}
