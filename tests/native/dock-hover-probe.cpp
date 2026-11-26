#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QToolButton>
#include <QMenu>
#include <QTimer>
#include <QCursor>
#include <QImage>
#include <QPointer>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow;
static QPointer<QDockWidget> dock;
static QPointer<QToolButton> button;
static QImage idle,hovered;
static QRect geometry;
static QString title;
static int stage=0;
static void check(const char *key,bool ok){blog(ok?LOG_INFO:LOG_ERROR,"HOVERCHECK %s=%d",key,ok);}
static QImage pixels(){return dock&&button?dock->grab(button->geometry()).toImage():QImage{};}
static void next(){
 if(!dock||!button){check("missing_dock_button",false);mainWindow->close();return;}
 const QByteArray prefix=dock->objectName().toUtf8()+"_";
 auto verify=[&](const char *key,bool ok){check((prefix+key).constData(),ok);};
 switch(stage++){
 case 0:
   button->clearFocus();
   QCursor::setPos(dock->mapToGlobal(QPoint(10,dock->widget()->geometry().top()+20)));
   break;
 case 1:
   idle=pixels();geometry=button->geometry();title=dock->windowTitle();
   QCursor::setPos(dock->mapToGlobal(QPoint(10,geometry.center().y())));
   break;
 case 2:
   hovered=pixels();
   verify("header_hover",!idle.isNull()&&hovered!=idle);
   verify("stable_title_geometry",button->geometry()==geometry&&dock->windowTitle()==title&&button->isVisible());
   QCursor::setPos(button->mapToGlobal(button->rect().center()));
   break;
 case 3:
   verify("button_hover",pixels()!=idle);
   QCursor::setPos(dock->mapToGlobal(QPoint(10,dock->widget()->geometry().top()+20)));
   break;
 case 4:
   verify("body_hides",pixels()==idle);
   button->menu()->popup(button->mapToGlobal(QPoint(0,button->height())));
   break;
 case 5:
   verify("open_menu_reveals",button->menu()->isVisible()&&pixels()!=idle);
   button->menu()->hide();
   QCursor::setPos(dock->mapToGlobal(QPoint(10,dock->widget()->geometry().top()+25)));
   break;
 case 6:
   verify("closed_menu_hides",pixels()==idle);
   button->setFocus(Qt::TabFocusReason);
   break;
 case 7:
   verify("keyboard_reveals",button->hasFocus()&&pixels()!=idle);
   button->clearFocus();
   if(dock->objectName()=="sourcesDock"){
     dock=mainWindow->findChild<QDockWidget*>("mixerDock");
     button=dock?dock->findChild<QToolButton*>("aftermarket-mixer-more",Qt::FindDirectChildrenOnly):nullptr;
     if(!button){check("mixer_specialized",false);mainWindow->close();return;}
     check("mixer_specialized",button->isVisible()&&button->menu());
     stage=0;
   }else{mainWindow->close();return;}
   break;
 }
 QTimer::singleShot(250,qApp,next);
}
static void start(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 dock=mainWindow->findChild<QDockWidget*>("sourcesDock");
 if(dock){dock->show();dock->raise();button=dock->findChild<QToolButton*>("aftermarket-dock-more",Qt::FindDirectChildrenOnly);}
 check("sources_native",dock&&button&&button->isVisible()&&button->menu());
 if(!dock||!button){mainWindow->close();return;}
 next();
}
static void event(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(1800,qApp,start);}
bool obs_module_load(){obs_frontend_add_event_callback(event,nullptr);return true;}
void obs_module_unload(){obs_frontend_remove_event_callback(event,nullptr);}
