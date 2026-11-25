#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QToolTip>
#include <QHelpEvent>
#include <QTimer>
#include <QSaveFile>
#include <QScreen>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow;
static QDockWidget *dock;
static QPoint point;
static const QString longText=QString("Native source settings can contain very long tooltip descriptions. ").repeated(10);
static void check(const char *name,bool ok){blog(ok?LOG_INFO:LOG_ERROR,"TIPCHECK %s=%d",name,ok);}
static QLabel *tip(){
 for(auto *w:QApplication::topLevelWidgets())if(w->isVisible()&&w->windowType()==Qt::ToolTip&&w->inherits("QTipLabel"))
   return qobject_cast<QLabel*>(w);
 return nullptr;
}
static bool settings(int width){
 QSaveFile file(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/home/.config/obs-studio/plugin_config/aftermarket/rules.json");
 const QByteArray bytes=QString("{\"enabled\":true,\"tooltipWidth\":%1,\"rules\":[]}").arg(width).toUtf8();
 return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size()&&file.commit();
}
static bool wrapped(int width,const QString &text){
 auto *w=tip();return w&&w->width()<=width&&w->text()==text&&w->wordWrap()&&w->height()>=w->heightForWidth(w->width());
}
static void probe(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 dock=mainWindow->findChild<QDockWidget*>("sourcesDock");
 check("native_dock",dock!=nullptr);
 if(!dock){mainWindow->close();return;}
 dock->show();dock->raise();
 point=dock->mapToGlobal(QPoint(20,20));dock->setToolTip(longText);
 QHelpEvent e(QEvent::ToolTip,QPoint(20,20),point);QApplication::sendEvent(dock,&e);
 QTimer::singleShot(200,qApp,[]{
  check("native_wrapped",wrapped(360,longText)&&dock->toolTip()==longText);
  auto *w=tip();if(w){check("screen_bounds",w->screen()->availableGeometry().contains(w->geometry()));w->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/tooltip.png");}
  else check("screen_bounds",false);
  QToolTip::showText(point,"Short hint",dock);
 });
 QTimer::singleShot(400,qApp,[]{
  auto *w=tip();check("short_natural",w&&w->width()<160&&w->text()=="Short hint");
  check("set_width",settings(220));
 });
 QTimer::singleShot(1000,qApp,[]{QToolTip::showText(point,longText+" Revised",dock);});
 QTimer::singleShot(1200,qApp,[]{
  check("live_width",wrapped(220,longText+" Revised"));
  QToolTip::showText(point,"<b>Rich title</b><p>"+longText+"</p>",dock);
 });
 QTimer::singleShot(1400,qApp,[]{
  check("rich_text",wrapped(220,"<b>Rich title</b><p>"+longText+"</p>"));
  check("disable_write",settings(0));
 });
 QTimer::singleShot(2100,qApp,[]{QToolTip::showText(point,longText+" Native",dock);});
 QTimer::singleShot(2300,qApp,[]{
  auto *w=tip();check("native_restored",w&&w->width()>360&&w->maximumWidth()==QWIDGETSIZE_MAX);
  QToolTip::hideText();mainWindow->close();
 });
}
static void frontend(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(4000,qApp,probe);}
bool obs_module_load(void){obs_frontend_add_event_callback(frontend,nullptr);return true;}
void obs_module_unload(void){obs_frontend_remove_event_callback(frontend,nullptr);}
