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
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow=nullptr;
static QPointer<QStackedWidget> stack;
static QPointer<QToolButton> flip;
static QPointer<QWidget> original,alternate;
static void check(const char *label,bool pass){blog(pass?LOG_INFO:LOG_ERROR,"FLIPCHECK %s=%d",label,pass);}
class PaintCounter final:public QObject {
public:
 int paints=0;
 bool eventFilter(QObject *obj,QEvent *e)override{
   if(e->type()==QEvent::Paint&&stack){
     if(auto *w=qobject_cast<QWidget*>(obj);w&&
        (w==stack||stack->isAncestorOf(w)))++paints;
   }
   return false;
 }
};
static void switchPage(bool back,const char *label){
 PaintCounter counter;
 qApp->installEventFilter(&counter);
 QElapsedTimer t;t.start();flip->click();const auto elapsed=t.nsecsElapsed();
 qApp->removeEventFilter(&counter);
 blog(LOG_INFO,"FLIPMETRIC %s synchronous_paints=%d dispatch_us=%lld",label,counter.paints,(long long)(elapsed/1000));
 check(label,counter.paints==0&&stack->currentWidget()==(back?alternate:original));
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
   switchPage(true,"back_no_sync_paint");
   auto *surface=stack->findChild<QWidget*>("aftermarket-view-flip");
   check("overlay",surface&&surface->parentWidget()==alternate&&!surface->testAttribute(Qt::WA_NativeWindow)&&surface->testAttribute(Qt::WA_TransparentForMouseEvents)&&!original->graphicsEffect()&&!alternate->graphicsEffect());
   // Capture test evidence only; never performed by production transition code.
   if(surface)stack->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/flip-reveal.png");
   switchPage(false,"reverse_no_sync_paint");
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
