#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QPointer>
#include "inspector.h"
#include "panels.h"
#include "property-sections.h"
#include "row-stripes.h"
#include "menu-bar-compact.h"
#include "emergency-recovery.h"
#include "dock-faces.h"
#include "audio-patchbay.h"
#include "obs-routing.h"
#include <QDockWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QTimer>
#include <util/bmem.h>
#include <QApplication>
#include <QThread>
#include <QDir>
#include <QFileInfo>
#include "augment.h"
#include "obs-audio.h"
#include "obs-transform.h"
OBS_DECLARE_MODULE()
static augment::Controller *controller = nullptr;
static std::unique_ptr<augment::ObsAudio> audio;
static std::unique_ptr<augment::ObsTransformStyle> transforms;
static QPointer<augment::Inspector> inspector;
static QPointer<QAction> toolsAction, panelsAction, emergencyAction, resumeAction;
static std::unique_ptr<augment::Panels> panels;
static std::unique_ptr<augment::PropertySections> propertySections;
static std::unique_ptr<augment::RowStripes> rowStripes;
static std::unique_ptr<augment::MenuBarCompact> compactBar;
static std::unique_ptr<augment::DockFaces> dockFaces;
static std::unique_ptr<augment::EmergencyRecovery> emergency;
static bool uiActive=false,frontendReady=false;
static bool setHamburgerMenuBar(bool enabled) {
    if(!compactBar)return false;
    auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window()));
    if(!main)return !enabled;
    if(compactBar->enabled()==enabled)return true;
    return compactBar->setEnabled(main->menuBar(),enabled);
}
static bool setAudioMixerBackside(bool enabled) {
    if(!dockFaces)return false;
    auto *main=static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if(!main)return !enabled;
    auto *dock=main->findChild<QDockWidget*>("mixerDock");
    if(!dock)return !enabled;
    if(!enabled) {
        if(dockFaces->front(dock))dockFaces->detach(dock);
        return true;
    }
    if(dockFaces->front(dock))return true;
    auto *patchbay=new augment::AudioPatchbay(
        []{return augment::ObsRouting::list();},
        [](const QString &id,int track){return augment::ObsRouting::toggleTrack(id,track);},
        [](const QString &id,int mode){return augment::ObsRouting::setMonitoring(id,mode);},
        nullptr,
        [](const QString &id,int track,bool connected){
            return augment::ObsRouting::setTrack(id,track,connected);
        }
    );
    if(!dockFaces->attach(dock,patchbay)){delete patchbay;return false;}
    return true;
}
static void stopCustomUi(){
    // The recovery marker is written before this runs. Do not touch the OBS
    // scene collection, audio state, saved dock layout, or rules.json.
    if(controller)controller->setRuntimeSafeMode(true);
    delete inspector.data();
    if(panels)panels->restoreAll();
    dockFaces.reset(); // unwrap original mixer widget before panel teardown
    compactBar.reset(); // return original OBS QAction identities
    rowStripes.reset();
    propertySections.reset();
    transforms.reset();
    panels.reset(); // return original center preview (if docked)
    uiActive=false;
    if(toolsAction)toolsAction->setEnabled(false);
    if(panelsAction)panelsAction->setEnabled(false);
    if(emergencyAction)emergencyAction->setEnabled(false);
    if(resumeAction)resumeAction->setVisible(true);
}
static bool startCustomUi(){
    if(uiActive)return true;
    if(!controller)return false;
    propertySections=std::make_unique<augment::PropertySections>();
    propertySections->setEnabled(controller->collapsiblePropertyGroups());
    rowStripes=std::make_unique<augment::RowStripes>();
    rowStripes->configure(controller->listRowSettings());
    compactBar=std::make_unique<augment::MenuBarCompact>();
    dockFaces=std::make_unique<augment::DockFaces>();
    panels=std::make_unique<augment::Panels>();
    transforms=std::make_unique<augment::ObsTransformStyle>(
        static_cast<QWidget*>(obs_frontend_get_main_window()),
        []{return controller&&controller->modernTransformFrame();});
    controller->setRuntimeSafeMode(false);
    if(controller->hamburgerMenuBarEnabled()&&!setHamburgerMenuBar(true))
        blog(LOG_WARNING,"[ui-augment] hamburger menu bar unavailable in this layout");
    if(frontendReady&&controller->mixerBacksideEnabled()&&!setAudioMixerBackside(true))
        blog(LOG_WARNING,"[ui-augment] mixer backside unavailable in this layout");
    uiActive=true;
    if(toolsAction)toolsAction->setEnabled(true);
    if(panelsAction)panelsAction->setEnabled(true);
    if(emergencyAction)emergencyAction->setEnabled(true);
    if(resumeAction)resumeAction->setVisible(false);
    return true;
}
static void cleanupUi(){
    stopCustomUi();
    emergency.reset(); // remove key filter before deleting the tool actions
    delete resumeAction.data();
    delete emergencyAction.data();
    delete panelsAction.data();
    delete toolsAction.data();
}
static void frontendEvent(enum obs_frontend_event event, void *) {
    if(event==OBS_FRONTEND_EVENT_FINISHED_LOADING){
        frontendReady=true;
        if(uiActive&&controller&&controller->mixerBacksideEnabled())
            if(!setAudioMixerBackside(true))blog(LOG_WARNING,"[ui-augment] mixer backside unavailable in this layout");
    }
    if (event==OBS_FRONTEND_EVENT_EXIT) {
        cleanupUi();
        if(controller){controller->setMeterProvider({});controller->setAudioProvider({});}
        audio.reset();
    }
}
MODULE_EXPORT const char *obs_module_name(void) { return "OBS UI Augment"; }
MODULE_EXPORT const char *obs_module_description(void) { return "Runtime menu decorations drawn independently of OBS themes"; }
MODULE_EXPORT bool obs_module_load(void) {
    auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app || app->thread() != QThread::currentThread()) return false;
    char *raw = obs_module_config_path("rules.json");
    if (!raw) return false;
    const QString file = QString::fromUtf8(raw);
    bfree(raw);
    const QString folder=QFileInfo(file).absolutePath();
    QDir().mkpath(folder);
    auto *main=static_cast<QWidget*>(obs_frontend_get_main_window());
    emergency=std::make_unique<augment::EmergencyRecovery>(
        QDir(folder).filePath("safe-mode.flag"),main,
        []{
            // Do not delete dock/menu widgets inside an active QMenu signal
            // or a keyboard event filter; the marker is already on disk.
            QTimer::singleShot(0,[] {
                if(emergency&&emergency->active()){
                    stopCustomUi();
                    blog(LOG_WARNING,"[ui-augment] emergency safe mode enabled; native UI restored");
                }
            });
        },
        []{return startCustomUi();});
    controller = new augment::Controller(file, nullptr, [](const QString &message) {
        blog(LOG_INFO, "[ui-augment] %s", qPrintable(message));
    },emergency->active());
    audio=std::make_unique<augment::ObsAudio>();
    controller->setAudioProvider([](const QWidget *w){return audio?audio->level(w):std::optional<float>{};},
        []{if(audio)audio->clear();});
    controller->setMeterProvider([](const QWidget *w){return audio?audio->sample(w):std::optional<augment::MeterReading>{};});
    toolsAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction("UI Augment Inspector"));
    if(toolsAction)QObject::connect(toolsAction.data(),&QAction::triggered,controller,[]{
        if(!inspector)inspector=new augment::Inspector(*controller,
            static_cast<QWidget*>(obs_frontend_get_main_window()),panels.get(),propertySections.get(),setAudioMixerBackside,rowStripes.get(),compactBar.get());
        inspector->show();inspector->raise();inspector->activateWindow();
    });
    panelsAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction("UI Augment Panels"));
    if(panelsAction)QObject::connect(panelsAction.data(),&QAction::triggered,controller,[]{
        if(!panels)return;
        if(!inspector)inspector=new augment::Inspector(*controller,
            static_cast<QWidget*>(obs_frontend_get_main_window()),panels.get(),propertySections.get(),setAudioMixerBackside,rowStripes.get(),compactBar.get());
        inspector->openPanels();inspector->show();inspector->raise();inspector->activateWindow();
    });
    emergencyAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction(
        "Aftermarket: Emergency restore (Ctrl+Alt+Shift+F12)"));
    if(emergencyAction)QObject::connect(emergencyAction.data(),&QAction::triggered,controller,[]{
        if(emergency&&!emergency->engage()&&!emergency->active())
            blog(LOG_ERROR,"[ui-augment] emergency marker could not be saved; no UI state changed");
    });
    resumeAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction(
        "Aftermarket: Resume UI customizations"));
    if(resumeAction)QObject::connect(resumeAction.data(),&QAction::triggered,controller,[]{
        // A native Tools menu may still be closing; avoid reparenting it mid-signal.
        QTimer::singleShot(0,[]{
            if(emergency&&!emergency->resume())
                blog(LOG_ERROR,"[ui-augment] safe mode remains enabled; resume failed");
        });
    });
    if(emergency->active()){
        // Already suspended before any UI adapters were constructed.
        if(toolsAction)toolsAction->setEnabled(false);
        if(panelsAction)panelsAction->setEnabled(false);
        if(emergencyAction)emergencyAction->setEnabled(false);
        if(resumeAction)resumeAction->setVisible(true);
        blog(LOG_WARNING,"[ui-augment] safe-mode.flag detected; Aftermarket UI disabled");
    }else startCustomUi();
    obs_frontend_add_event_callback(frontendEvent,nullptr);
    blog(LOG_INFO, "[ui-augment] recovery shortcut registered; rules: %s", qPrintable(file));
    return true;
}
MODULE_EXPORT void obs_module_unload(void) {
    obs_frontend_remove_event_callback(frontendEvent,nullptr);
    cleanupUi();
    delete controller; controller = nullptr;
    audio.reset();
}
