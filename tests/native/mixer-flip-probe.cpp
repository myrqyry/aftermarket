#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QStackedWidget>
#include <QToolButton>
#include <QTimer>
#include <QPointer>
#include <QElapsedTimer>
#include <QEvent>
#include <QImage>
#include <QStyle>
#include <QVariantAnimation>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow=nullptr;
static QPointer<QStackedWidget> stack;
static QPointer<QToolButton> flip;
static QPointer<QWidget> original,alternate;
static void check(const char *label,bool pass){blog(pass?LOG_INFO:LOG_ERROR,"FLIPCHECK %s=%d",label,pass);}
class PaintCounter final:public QObject {
public:
 int paints=0,frontPaints=0,backPaints=0;
 bool eventFilter(QObject *obj,QEvent *e)override{
   if(e->type()==QEvent::Paint&&stack){
     if(auto *w=qobject_cast<QWidget*>(obj);w&&
        (w==stack||stack->isAncestorOf(w)))++paints;
     if(obj==original)++frontPaints;
     if(obj==alternate)++backPaints;
   }
   return false;
 }
};
static void switchPage(bool back,const char *label){
 PaintCounter counter;
 qApp->installEventFilter(&counter);
 QElapsedTimer t;t.start();flip->click();const auto elapsed=t.nsecsElapsed();
 qApp->removeEventFilter(&counter);
 blog(LOG_INFO,"FLIPMETRIC %s synchronous_paints=%d front=%d back=%d dispatch_us=%lld",label,counter.paints,counter.frontPaints,counter.backPaints,(long long)(elapsed/1000));
 check(label,counter.frontPaints<=1&&counter.backPaints<=1&&stack->currentWidget()==(back?alternate:original));
}
static void probe(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 auto *dock=mainWindow->findChild<QDockWidget*>("mixerDock");
 stack=dock?dock->findChild<QStackedWidget*>("aftermarket-dock-pages"):nullptr;
 flip=dock?dock->findChild<QToolButton*>("aftermarket-audiobus-toggle"):nullptr;
 check("attached",stack&&flip&&stack->count()==2);
 if(!stack||!flip){mainWindow->close();return;}
 original=stack->widget(0);alternate=stack->widget(1);
 dock->setFloating(false);dock->show();dock->raise();mainWindow->resizeDocks({dock},{370},Qt::Horizontal);
 QTimer::singleShot(500,qApp,[]{
   if(!stack||!flip){mainWindow->close();return;}
   blog(LOG_INFO,"FLIPDETAIL visible=%d size=%dx%d dpr=%.2f style_animate=%d style=%s",stack->isVisible(),stack->width(),stack->height(),stack->devicePixelRatioF(),stack->style()->styleHint(QStyle::SH_Widget_Animate,nullptr,stack),stack->style()->metaObject()->className());
   for(auto *page:{original.data(),alternate.data()})if(page){
     QList<QWidget*> list{page};list.append(page->findChildren<QWidget*>());
     for(auto *w:list)if(w->testAttribute(Qt::WA_NativeWindow)||w->testAttribute(Qt::WA_PaintOnScreen)||w->inherits("QOpenGLWidget")||w->inherits("OBSQTDisplay"))
       blog(LOG_INFO,"FLIPDETAIL surface class=%s name=%s native=%d paintOnScreen=%d",w->metaObject()->className(),qPrintable(w->objectName()),w->testAttribute(Qt::WA_NativeWindow),w->testAttribute(Qt::WA_PaintOnScreen));
   }
   switchPage(true,"back_bounded_capture");
   auto *surface=stack->findChild<QWidget*>("aftermarket-view-flip");
   check("overlay",surface&&surface->parentWidget()==alternate&&!surface->testAttribute(Qt::WA_NativeWindow)&&surface->testAttribute(Qt::WA_TransparentForMouseEvents)&&!original->graphicsEffect()&&!alternate->graphicsEffect());
   // Inspect the painted frames on the real native mixer, at exact times.
   auto *anim=surface?surface->findChild<QVariantAnimation*>("aftermarket-view-flip-motion"):nullptr;
   bool geometry=false;
   if(anim){
     anim->pause();
     QList<QImage> frames;
     for(int ms:{0,70,105,140,209}){
       anim->setCurrentTime(ms);
       auto frame=surface->grab().toImage();
       frame.save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+QString("/flip-%1.png").arg(ms));
       frames.append(frame);
     }
     const auto mid=frames[2];
     const auto background=mid.pixelColor(0,0);
     int nonBackground=0;
     for(int y=0;y<mid.height();++y)for(int x=0;x<mid.width();++x)
       if(mid.pixelColor(x,y)!=background)++nonBackground;
     geometry=nonBackground==0&&frames[0]!=frames[1]&&frames[1]!=mid&&frames[3]!=mid&&frames[0]!=frames[4]&&frames[3]!=frames[4];
   }
   check("two_face_geometry",geometry);
   switchPage(false,"reverse_bounded_capture");
   check("one_overlay",stack->findChildren<QWidget*>("aftermarket-view-flip").size()==1);
 });
 QTimer::singleShot(1000,qApp,[]{
   check("settled",stack&&stack->currentWidget()==original&&!stack->findChild<QWidget*>("aftermarket-view-flip"));
   check("identity",stack&&stack->widget(0)==original&&stack->widget(1)==alternate);
   mainWindow->close();
 });
}
static void frontend(enum obs_frontend_event e,void*){
 if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(4500,qApp,probe);
}
bool obs_module_load(void){obs_frontend_add_event_callback(frontend,nullptr);return true;}
void obs_module_unload(void){obs_frontend_remove_event_callback(frontend,nullptr);}
