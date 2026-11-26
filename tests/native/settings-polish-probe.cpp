#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QDialog>
#include <QDialogButtonBox>
#include <QAction>
#include <QListWidget>
#include <QTabWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QGroupBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QTimer>
#include <QPointer>
#include <QImage>
#include <QSettings>
#include <QFile>
#include <QDir>
OBS_DECLARE_MODULE()
static void check(const char *key,bool ok){blog(ok?LOG_INFO:LOG_ERROR,"SETTINGSCHECK %s=%d",key,ok);}
static QMainWindow *mainWindow;
static QPointer<QDialog> dialog;
static QPointer<QTabWidget> tabs;
static QString root;
static int tab=0,widthPass=0;
static bool fitted=true,groups=true,controls=true;
static void next(){
 if(!dialog||!tabs)return;
 auto *area=qobject_cast<QScrollArea*>(tabs->currentWidget());
 const auto size=area?area->viewport()->size():QSize();
 bool fits=area&&area->horizontalScrollBar()->maximum()==0;
 bool grouped=area&&!area->findChildren<QGroupBox*>().isEmpty();
 fitted&=fits;groups&=grouped;
 if(area)for(auto *b:area->findChildren<QCheckBox*>()){
    const auto r=QRect(b->mapTo(area->widget(),QPoint()),b->size());
    controls&=area->widget()->rect().contains(r)&&b->width()>=b->minimumSizeHint().width();
 }
 blog(LOG_INFO,"SETTINGSDETAIL %s dialog=%dx%d viewport=%dx%d horizontal=%d vertical=%d",qPrintable(tabs->tabText(tab)),dialog->width(),dialog->height(),size.width(),size.height(),area?area->horizontalScrollBar()->maximum():-1,area?area->verticalScrollBar()->maximum():-1);
 if(widthPass==0)dialog->grab().save(root+"/"+tabs->tabText(tab).toLower().replace(' ','-')+".png");
 if(++tab<tabs->count()){tabs->setCurrentIndex(tab);QTimer::singleShot(150,qApp,next);return;}
 if(widthPass++==0){tab=0;dialog->resize(780,620);tabs->setCurrentIndex(tab);QTimer::singleShot(200,qApp,next);return;}
 check("no_horizontal_overflow",fitted);
 check("native_groups_all_tabs",groups);
 check("controls_fit",controls);
 tabs->setCurrentIndex(3);
 auto *colors=dialog->findChild<QLineEdit*>("aftermarket-setting-gradient-colors");
 auto *preview=dialog->findChild<QWidget*>("aftermarket-settings-palette-preview");
 auto *theme=dialog->findChild<QPushButton*>("aftermarket-settings-use-theme");
 auto *apply=dialog->findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Apply);
 if(!colors||!preview||!theme){check("preview",false);dialog->reject();mainWindow->close();return;}
 const auto before=preview->grab().toImage();
 colors->setText("#ff0000, #0000ff");QApplication::processEvents();
 check("live_draft_preview",preview->property("validPalette").toBool()&&preview->grab().toImage()!=before&&apply->isEnabled());
 theme->click();QApplication::processEvents();
 check("restore_theme",colors->text().isEmpty()&&preview->property("validPalette").toBool());
 colors->setText("#ff0000, #00ff00");QApplication::processEvents();
 apply->click();
 QTimer::singleShot(150,qApp,[]{
    QFile file(QDir::homePath()+"/.config/obs-studio/plugin_config/aftermarket/rules.json");
    file.open(QIODevice::ReadOnly);
    auto data=file.readAll();
    check("native_apply_persists",data.contains("#ff0000")&&data.contains("#00ff00"));
    dialog->reject();
    QTimer::singleShot(200,qApp,[]{mainWindow->close();});
 });
}
static void start(){
 mainWindow=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 root=qEnvironmentVariable("PRESET_ROOT");
 auto *action=mainWindow->findChild<QAction*>("action_Settings");
 if(!action){check("native_action",false);mainWindow->close();return;}
 QTimer::singleShot(800,qApp,[]{
    for(auto *w:QApplication::topLevelWidgets())if(w->inherits("OBSBasicSettings")&&w->isVisible())dialog=qobject_cast<QDialog*>(w);
    auto *nav=dialog?dialog->findChild<QListWidget*>("listWidget"):nullptr;
    tabs=dialog?dialog->findChild<QTabWidget*>("aftermarket-settings-tabs"):nullptr;
    auto items=nav?nav->findItems("Aftermarket",Qt::MatchExactly):QList<QListWidgetItem*>{};
    check("native_aftermarket_page",dialog&&tabs&&tabs->count()==6&&items.size()==1);
    if(!dialog||!tabs||items.size()!=1){if(dialog)dialog->reject();mainWindow->close();return;}
    nav->setCurrentItem(items.first());dialog->resize(1040,720);tabs->setCurrentIndex(0);
    QTimer::singleShot(200,qApp,next);
 });
 action->trigger();
}
static void event(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(1100,qApp,start);}
bool obs_module_load(){obs_frontend_add_event_callback(event,nullptr);return true;}
void obs_module_unload(){obs_frontend_remove_event_callback(event,nullptr);}
