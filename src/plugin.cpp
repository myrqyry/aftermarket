#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QPointer>
#include "inspector.h"
#include "panels.h"
#include "property-sections.h"
#include "row-stripes.h"
#include "menu-bar-compact.h"
#include "action-strip.h"
#include "native-action-bridge.h"
#include "dock-attach-retry.h"
#include "emergency-recovery.h"
#include "dock-faces.h"
#include "audio-patchbay.h"
#include "obs-routing.h"
#include "front-audio-controls.h"
#include "dock-header-button.h"
#include "dock-title-icons.h"
#include "native-audio-settings.h"
#include "obs-signal-levels.h"
#include <QToolButton>
#include <QMenu>
#include <QAction>
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
static QPointer<QAction> toolsAction, panelsAction, stripAction, emergencyAction, resumeAction;
static std::unique_ptr<augment::Panels> panels;
static std::unique_ptr<augment::PropertySections> propertySections;
static std::unique_ptr<augment::RowStripes> rowStripes;
static std::unique_ptr<augment::MenuBarCompact> compactBar;
static std::unique_ptr<augment::ActionStrip> actionStrip;
static std::unique_ptr<augment::NativeActionBridge> nativeActions;
static std::unique_ptr<augment::DockAttachRetry> mixerRetry;
static QString stripConfigPath;
static std::unique_ptr<augment::DockFaces> dockFaces;
static std::unique_ptr<augment::DockTitleIcons> dockTitleIcons;
static std::unique_ptr<augment::FrontAudioControls> frontAudio;
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
        frontAudio.reset(); // remove only our controls before restoring OBS front
        if(dockFaces->front(dock))dockFaces->detach(dock);
        return true;
    }
    if(dockFaces->front(dock))return true;
    auto levels=std::make_shared<augment::ObsSignalLevels>();
    auto *patchbay=new augment::AudioPatchbay(
        []{return augment::ObsRouting::list();},
        [](const QString &id,int track){return augment::ObsRouting::toggleTrack(id,track);},
        [](const QString &id,int mode){return augment::ObsRouting::setMonitoring(id,mode);},
        nullptr,
        [](const QString &id,int track,bool connected){
            return augment::ObsRouting::setTrack(id,track,connected);
        },
        [levels](const QVector<augment::RouteSource> &routes){
            return levels->sample(routes);
        }
    );
    if(!dockFaces->attach(dock,patchbay)){delete patchbay;return false;}
    const QString settingsFile=QDir(QFileInfo(stripConfigPath).absolutePath())
        .filePath("front-audio-controls.json");
    frontAudio=std::make_unique<augment::FrontAudioControls>(
        settingsFile,
        []{return augment::NativeAudioSettings::list();},
        [](const QString &id,double pan){return augment::NativeAudioSettings::setPan(id,pan);},
        [](const QString &id,int ms){return augment::NativeAudioSettings::setOffsetMs(id,ms);},
        []{if(frontendReady)obs_frontend_save();}
    );
    frontAudio->attach(dockFaces->front(dock));
    // A single native-title overflow holds the presentation toggle and all
    // advanced per-channel pan/sync editors. No second content header.
    auto *more=new augment::DockHeaderButton(augment::DockHeaderGlyph::More);
    more->setObjectName("aftermarket-mixer-more");
    more->setToolTip("Audio Mixer options and channel settings");
    more->setAccessibleName("Audio Mixer more options");
    auto *menu=new QMenu(more);
    menu->setObjectName("aftermarket-mixer-more-menu");
    more->setMenu(menu);
    more->setPopupMode(QToolButton::InstantPopup);
    QPointer<QDockWidget> safeDock=dock;
    QObject::connect(menu,&QMenu::aboutToShow,more,[menu,safeDock]{
        menu->clear();
        if(!safeDock||!dockFaces||!frontAudio)return;
        auto *knobs=menu->addAction("Pan and sync as knobs");
        knobs->setCheckable(true);
        knobs->setChecked(frontAudio->knobs());
        QObject::connect(knobs,&QAction::triggered,menu,[](bool enabled){
            if(frontAudio)frontAudio->setKnobs(enabled);
        });
        menu->addSeparator();
        frontAudio->populateSettingsMenu(menu);
    });
    if(dockFaces->addFrontAccessory(dock,more)){
        // In floating/narrow docks the native footer returns when the title
        // overflow hides, so no OBS button becomes unreachable.
        frontAudio->setMenuButton(more);
    }else delete more;
    // The small AudioBus button at the bottom switches between the two pages.
    // Do not stack the entire routing canvas under OBS's native mixer.
    return true;
}
static void stopCustomUi(){
    // The recovery marker is written before this runs. Do not touch the OBS
    // scene collection, audio state, saved dock layout, or rules.json.
    if(controller)controller->setRuntimeSafeMode(true);
    delete inspector.data();
    if(panels)panels->restoreAll();
    mixerRetry.reset();
    actionStrip.reset(); // preserve desired state, restore OBS-owned menu corner
    frontAudio.reset(); // remove pan/offset controls before original mixer is restored
    dockFaces.reset(); // unwrap original mixer widget before panel teardown
    dockTitleIcons.reset(); // remove only plugin-painted native title icons
    compactBar.reset(); // return original OBS QAction identities
    rowStripes.reset();
    propertySections.reset();
    transforms.reset();
    panels.reset(); // return original center preview (if docked)
    uiActive=false;
    if(toolsAction)toolsAction->setEnabled(false);
    if(panelsAction)panelsAction->setEnabled(false);
    if(stripAction)stripAction->setEnabled(false);
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
    actionStrip=std::make_unique<augment::ActionStrip>(stripConfigPath,
        []{return frontendReady&&nativeActions?nativeActions->snapshot():augment::StatusSnapshot{};},
        []{return frontendReady&&nativeActions?nativeActions->enumerateHotkeys():QList<augment::HotkeyChoice>{};},
        [](const QString &binding){return frontendReady&&nativeActions&&nativeActions->triggerHotkey(binding);});
    if(auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window())))
        dockTitleIcons=std::make_unique<augment::DockTitleIcons>(main);
    dockFaces=std::make_unique<augment::DockFaces>();
    mixerRetry=std::make_unique<augment::DockAttachRetry>([]{return setAudioMixerBackside(true);});
    panels=std::make_unique<augment::Panels>();
    transforms=std::make_unique<augment::ObsTransformStyle>(
        static_cast<QWidget*>(obs_frontend_get_main_window()),
        []{return controller&&controller->modernTransformFrame();});
    controller->setRuntimeSafeMode(false);
    if(controller->hamburgerMenuBarEnabled()&&!setHamburgerMenuBar(true))
        blog(LOG_WARNING,"[ui-augment] hamburger menu bar unavailable in this layout");
    if(actionStrip->desiredEnabled()){
        auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window()));
        if(main&&!actionStrip->setEnabled(main->menuBar(),true))
            blog(LOG_WARNING,"[ui-augment] action strip unavailable in this layout");
    }
    if(frontendReady&&controller->mixerBacksideEnabled())mixerRetry->start();
    uiActive=true;
    if(toolsAction)toolsAction->setEnabled(true);
    if(panelsAction)panelsAction->setEnabled(true);
    if(stripAction)stripAction->setEnabled(true);
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
    delete stripAction.data();
    delete toolsAction.data();
}
static void frontendEvent(enum obs_frontend_event event, void *) {
    if(nativeActions&&event!=OBS_FRONTEND_EVENT_EXIT)nativeActions->onFrontendEvent(event);
    if(event==OBS_FRONTEND_EVENT_FINISHED_LOADING){
        frontendReady=true;
        if(uiActive&&controller&&controller->mixerBacksideEnabled()&&mixerRetry)
            mixerRetry->start();
    }
    if (event==OBS_FRONTEND_EVENT_EXIT) {
        cleanupUi();
        if(controller){controller->setMeterProvider({});controller->setAudioProvider({});}
        audio.reset();
        nativeActions.reset();
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
    stripConfigPath=QDir(folder).filePath("action-strip.json");
    nativeActions=std::make_unique<augment::NativeActionBridge>();
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
            static_cast<QWidget*>(obs_frontend_get_main_window()),panels.get(),propertySections.get(),setAudioMixerBackside,rowStripes.get(),compactBar.get(),actionStrip.get());
        inspector->show();inspector->raise();inspector->activateWindow();
    });
    panelsAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction("UI Augment Panels"));
    if(panelsAction)QObject::connect(panelsAction.data(),&QAction::triggered,controller,[]{
        if(!panels)return;
        if(!inspector)inspector=new augment::Inspector(*controller,
            static_cast<QWidget*>(obs_frontend_get_main_window()),panels.get(),propertySections.get(),setAudioMixerBackside,rowStripes.get(),compactBar.get(),actionStrip.get());
        inspector->openPanels();inspector->show();inspector->raise();inspector->activateWindow();
    });
    stripAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction("Aftermarket: Action Strip…"));
    if(stripAction)QObject::connect(stripAction.data(),&QAction::triggered,controller,[]{
        if(!actionStrip)return;
        auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window()));
        if(main)QTimer::singleShot(0,[main]{
            if(actionStrip)actionStrip->showEditor(main,main->menuBar());
        });
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
        if(stripAction)stripAction->setEnabled(false);
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
    nativeActions.reset();
}
