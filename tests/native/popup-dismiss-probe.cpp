#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QDialog>
#include <QAction>
#include <QTimer>
#include <QPointer>
#include <QTabBar>
#include <QComboBox>
#include <QListWidget>
#include <QLineEdit>
#include <QMenu>
#include <QDockWidget>
#include <QAbstractButton>
#include <QToolButton>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
OBS_DECLARE_MODULE()
static QMainWindow *mainWindow=nullptr;
static QAction *add=nullptr,*find=nullptr;
static QPointer<QDialog> picker;
static void check(const char *name,bool pass){blog(pass?LOG_INFO:LOG_ERROR,"POPUPPROBE %s=%d",name,pass);}
static QDialog *dialog(const QString &name){
 for(auto *w:QApplication::topLevelWidgets())if(w->isVisible()&&w->objectName()==name)return qobject_cast<QDialog*>(w);
 return nullptr;
}
static void mouse(const QPoint &point){
 auto *d=XOpenDisplay(nullptr);if(!d)return;
 XTestFakeMotionEvent(d,-1,point.x(),point.y(),CurrentTime);
 XTestFakeButtonEvent(d,1,True,CurrentTime);XTestFakeButtonEvent(d,1,False,CurrentTime);
 XFlush(d);XCloseDisplay(d);
}
static void click(QWidget *w){if(w)mouse(w->mapToGlobal(w->rect().center()));}
static void outside(QDialog *w){
 if(!w)return;
 const QRect rect(w->mapToGlobal(QPoint(0,0)),w->size());
 const QRect owner(mainWindow->mapToGlobal(QPoint(0,0)),mainWindow->size());
 for(auto point:{owner.topLeft()+QPoint(30,80),owner.topRight()+QPoint(-30,80),owner.center()}){
  if(!rect.contains(point)){mouse(point);return;}
 }
}
static int sourceCount(){
 int count=0;obs_enum_sources([](void *p,obs_source_t*){++*static_cast<int*>(p);return true;},&count);return count;
}
static int initialCount=0;
static void openAdd(){
 for(auto *b:mainWindow->findChildren<QToolButton*>())if(b->isVisible()&&b->defaultAction()==add){click(b);return;}
 add->trigger();
}
static void probe(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 add=mainWindow->findChild<QAction*>("actionAddSource");
 for(auto *a:mainWindow->findChildren<QAction*>())if(a->text().contains("Find Sources",Qt::CaseInsensitive))find=a;
 check("actions",add&&find);
 if(!add||!find){mainWindow->close();return;}
 initialCount=sourceCount();
 QTimer::singleShot(0,qApp,[]{openAdd();});
 QTimer::singleShot(500,qApp,[]{
  picker=dialog("OBSBasicSourceSelect");
  check("add_popup",picker&&picker->windowType()==Qt::Popup);
  if(picker){auto *tabs=picker->findChild<QTabBar*>("aftermarket-source-picker-tabs");if(tabs)mouse(tabs->mapToGlobal(tabs->tabRect(1).center()));}
 });
 QTimer::singleShot(850,qApp,[]{
  check("inside_click_keeps_add",picker&&picker->isVisible());
  if(picker)click(picker->findChild<QComboBox*>("aftermarket-existing-type"));
 });
 QTimer::singleShot(1200,qApp,[]{
  check("nested_combo_keeps_add",picker&&picker->isVisible()&&QApplication::activePopupWidget()!=picker);
  if(picker){auto *combo=picker->findChild<QComboBox*>("aftermarket-existing-type");if(combo)combo->hidePopup();}
 });
 QTimer::singleShot(1500,qApp,[]{if(picker)outside(picker);});
 QTimer::singleShot(1900,qApp,[]{
  check("outside_dismisses_add",!dialog("OBSBasicSourceSelect")&&sourceCount()==initialCount);
  if(auto *w=dialog("OBSBasicSourceSelect"))w->reject();
  QTimer::singleShot(0,qApp,[]{find->trigger();});
 });
 QTimer::singleShot(2350,qApp,[]{
  auto *w=dialog("aftermarket-quick-find");check("find_popup",w&&w->windowType()==Qt::Popup);
  if(w)click(w->findChild<QLineEdit*>("aftermarket-quick-find-input"));
 });
 QTimer::singleShot(2700,qApp,[]{
  auto *w=dialog("aftermarket-quick-find");check("inside_click_keeps_find",w);
  if(w)outside(w);
 });
 QTimer::singleShot(3050,qApp,[]{
  check("outside_dismisses_find",!dialog("aftermarket-quick-find"));
  find->trigger();
 });
 QTimer::singleShot(3400,qApp,[]{
  check("find_reopens",dialog("aftermarket-quick-find"));
  if(auto *w=dialog("aftermarket-quick-find"))w->reject();
  QTimer::singleShot(0,qApp,[]{openAdd();});
 });
 QTimer::singleShot(3800,qApp,[]{
  auto *w=dialog("OBSBasicSourceSelect");
  check("add_reopens",w);
  if(w){auto *list=w->findChild<QListWidget*>("sourceTypeList");
   if(list)for(int i=0;i<list->count();++i){
    auto *item=list->item(i);
    if(item->data(Qt::UserRole+1).toString().startsWith("color_source")&&!item->isHidden()){
      list->scrollToItem(item);mouse(list->viewport()->mapToGlobal(list->visualItemRect(item).center()));break;
    }
   }
  }
 });
 QTimer::singleShot(4400,qApp,[]{
  // OBS may open a native naming dialog, or create immediately and show Properties.
  bool handled=false;
  for(auto *w:QApplication::topLevelWidgets()){
   auto *d=qobject_cast<QDialog*>(w);if(!d||!d->isVisible()||d->objectName()=="OBSBasicSourceSelect")continue;
   if(auto *edit=d->findChild<QLineEdit*>()){
    if(d->inherits("OBSBasicProperties"))continue;
    edit->setText("Popup created color");d->accept();handled=true;break;
   }
  }
  blog(LOG_INFO,"POPUPPROBE_DETAIL naming handled=%d count=%d",handled,sourceCount());
 });
 QTimer::singleShot(5300,qApp,[]{
  check("native_create",sourceCount()>initialCount);
  for(auto *w:QApplication::topLevelWidgets())if(auto *d=qobject_cast<QDialog*>(w))if(d->isVisible())d->reject();
  mainWindow->close();
 });
}
static void frontend(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(3500,qApp,probe);}
bool obs_module_load(void){obs_frontend_add_event_callback(frontend,nullptr);return true;}
void obs_module_unload(void){obs_frontend_remove_event_callback(frontend,nullptr);}
