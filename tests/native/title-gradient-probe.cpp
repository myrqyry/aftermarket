#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QTimer>
#include <QPointer>
#include <QImage>
#include <QSaveFile>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow=nullptr;
static QPointer<QDockWidget> dock;
static QPointer<QWidget> body;
static QImage before;
static QString title;
static QString pattern(){auto p=qEnvironmentVariable("GRADIENT_PATTERN");return p.isEmpty()?QStringLiteral("shimmer"):p;}
static int scale(){int s=qEnvironmentVariableIntValue("GRADIENT_SCALE");return s>=50&&s<=400?s:100;}
static bool full(){return qEnvironmentVariableIntValue("FULL_HEADER")==1;}
static void check(const char *name,bool pass){blog(pass?LOG_INFO:LOG_ERROR,"GRADIENTCHECK %s=%d",name,pass);}
static QWidget *accent(){return dock?dock->findChild<QWidget*>("aftermarket-title-gradient",Qt::FindDirectChildrenOnly):nullptr;}
static bool settings(bool motion,bool titlebars){
 QSaveFile f(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/home/.config/obs-studio/plugin_config/aftermarket/rules.json");
 const auto json=QString("{\"enabled\":true,\"motion\":%1,\"rules\":[],\"gradient\":{\"enabled\":true,\"sliders\":false,\"progress\":false,\"titlebars\":%2,\"titlebarFill\":%3,\"pattern\":\"%4\",\"period\":1200,\"scale\":%5,\"colors\":[\"#31748f\",\"#c4a7e7\"]}}").arg(motion?"true":"false",titlebars&&!full()?"true":"false",titlebars&&full()?"true":"false",pattern()).arg(scale()).toUtf8();
 return f.open(QIODevice::WriteOnly)&&f.write(json)==json.size()&&f.commit();
}
static void probe(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 dock=mainWindow->findChild<QDockWidget*>("sourcesDock");
 if(!dock){check("native_dock",false);mainWindow->close();return;}
 dock->show();dock->raise();body=dock->widget();title=dock->windowTitle();
 check("native_dock",body&&dock->isVisible());
 QTimer::singleShot(300,qApp,[]{
  auto *s=accent();
  check("accent",s&&s->isVisible()&&(full()?s->height()>10:s->height()==2)&&s->geometry().bottom()<body->geometry().top()&&s->testAttribute(Qt::WA_TransparentForMouseEvents));
  if(s)before=s->grab().toImage();
 });
 QTimer::singleShot(510,qApp,[]{
  auto *s=accent();
  check("animated_pixels",s&&!before.isNull()&&before!=s->grab().toImage());
  check("native_identity",dock&&dock->widget()==body&&dock->windowTitle()==title&&!dock->titleBarWidget());
  if(dock)dock->grab().save(QString::fromUtf8(qgetenv("PRESET_ROOT"))+"/title-gradient.png");
  check("disable_motion_write",settings(false,true));
 });
 QTimer::singleShot(1000,qApp,[]{auto *s=accent();before=s?s->grab().toImage():QImage{};});
 QTimer::singleShot(1250,qApp,[]{
  auto *s=accent();
  check("reduced_motion",s&&!before.isNull()&&before==s->grab().toImage());
  check("disable_accent_write",settings(false,false));
 });
 QTimer::singleShot(1750,qApp,[]{
  check("removed",!accent()&&dock&&dock->widget()==body);
  mainWindow->close();
 });
}
static void frontend(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(4500,qApp,probe);}
bool obs_module_load(void){obs_frontend_add_event_callback(frontend,nullptr);return true;}
void obs_module_unload(void){obs_frontend_remove_event_callback(frontend,nullptr);}
