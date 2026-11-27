#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QToolButton>
#include <QTimer>
#include <QStyleOptionDockWidget>
#include <QStyle>
#include <QFontMetrics>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow;
static QDockWidget *dock;
static const QString title=QStringLiteral("Live Latent Lab — Source Monitoring");
static void check(const char *name,bool ok){blog(ok?LOG_INFO:LOG_ERROR,"ELISIONCHECK %s=%d",name,ok);}
static int room(){
 QStyleOptionDockWidget opt;opt.initFrom(dock);opt.title=title;
 opt.closable=dock->features()&QDockWidget::DockWidgetClosable;
 opt.floatable=dock->features()&QDockWidget::DockWidgetFloatable;
 opt.movable=dock->features()&QDockWidget::DockWidgetMovable;
 return dock->style()->subElementRect(QStyle::SE_DockWidgetTitleBarText,&opt,dock).width();
}
static QToolButton *more(){return dock->findChild<QToolButton*>("aftermarket-dock-more",Qt::FindDirectChildrenOnly);}
static void capture(const char *name){
 dock->grab(QRect(0,0,dock->width(),dock->widget()->geometry().top())).save(qEnvironmentVariable("PRESET_ROOT")+"/"+name+".png");
 blog(LOG_INFO,"ELISIONDETAIL width=%d room=%d title=%s",dock->width(),room(),qPrintable(dock->windowTitle()));
}
static void probe(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 dock=new QDockWidget(title,mainWindow);dock->setObjectName("nativeElisionProbeDock");
 dock->setWidget(new QLabel("Content",dock));dock->setGeometry(10,80,140,160);dock->show();dock->raise();
 QTimer::singleShot(400,qApp,[]{
  check("narrow",more()&&more()->isVisible()&&dock->windowTitle().contains(QChar(0x2026)));
  capture("narrow");
  dock->resize(650,160);
 });
 QTimer::singleShot(750,qApp,[]{
  check("widen",dock->windowTitle().trimmed()==title&&dock->toggleViewAction()->text()==title);
  capture("wide");
  if(more()){
   const int need=QFontMetrics(dock->font()).horizontalAdvance(title)+more()->width()+3;
   dock->resize(dock->width()+need-room(),160);
  }
 });
 QTimer::singleShot(1050,qApp,[]{
  auto *icon=dock->findChild<QWidget*>("aftermarket-dock-title-icon",Qt::FindDirectChildrenOnly);
  check("exact_fit",more()&&more()->isVisible()&&icon&&!icon->isVisible()&&dock->windowTitle().trimmed()==title);
  capture("fit");
  dock->resize(140,160);
 });
 QTimer::singleShot(1400,qApp,[]{
  check("shrink",dock->windowTitle().contains(QChar(0x2026))&&dock->toggleViewAction()->text()==title);
  dock->deleteLater();QTimer::singleShot(100,qApp,[]{mainWindow->close();});
 });
}
static void frontend(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(4000,qApp,probe);}
bool obs_module_load(){obs_frontend_add_event_callback(frontend,nullptr);return true;}
void obs_module_unload(){obs_frontend_remove_event_callback(frontend,nullptr);}
