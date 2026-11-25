#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-hotkey.h>
#include <QAction>
#include <QPointer>
#include "inspector.h"
#include "panels.h"
#include "property-sections.h"
#include "row-stripes.h"
#include "menu-bar-compact.h"
#include "action-strip.h"
#include "quick-find.h"
#include "dock-layouts.h"
#include "cue-deck.h"
#include "native-action-bridge.h"
#include "dock-attach-retry.h"
#include "emergency-recovery.h"
#include "dock-faces.h"
#include "audio-patchbay.h"
#include "obs-routing.h"
#include "front-audio-controls.h"
#include "dock-header-button.h"
#include "dock-title-icons.h"
#include "obs-settings-page.h"
#include "source-picker.h"
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
#include "config-identity.h"
#include "obs-audio.h"
#include "obs-transform.h"
OBS_DECLARE_MODULE()
static augment::Controller *controller = nullptr;
static std::unique_ptr<augment::ObsAudio> audio;
static std::unique_ptr<augment::ObsTransformStyle> transforms;
static QPointer<augment::Inspector> inspector;
static QPointer<augment::QuickFind> quickFind;
static QPointer<QAction> toolsAction, panelsAction, stripAction, layoutsAction, findAction, cuesAction, emergencyAction, resumeAction;
static std::unique_ptr<augment::Panels> panels;
static std::unique_ptr<augment::PropertySections> propertySections;
static std::unique_ptr<augment::RowStripes> rowStripes;
static std::unique_ptr<augment::MenuBarCompact> compactBar;
static std::unique_ptr<augment::ActionStrip> actionStrip;
static std::unique_ptr<augment::DockLayouts> dockLayouts;
static std::unique_ptr<augment::CueDeck> cueDeck;
static std::unique_ptr<augment::NativeActionBridge> nativeActions;
static std::unique_ptr<augment::DockAttachRetry> mixerRetry;
static QString stripConfigPath;
static std::unique_ptr<augment::DockFaces> dockFaces;
static std::unique_ptr<augment::DockTitleIcons> dockTitleIcons;
static std::unique_ptr<augment::ObsSettingsPage> nativeSettings;
static std::unique_ptr<augment::SourcePicker> sourcePicker;
static std::unique_ptr<augment::FrontAudioControls> frontAudio;
static std::unique_ptr<augment::EmergencyRecovery> emergency;
static bool uiActive=false,frontendReady=false;
static obs_hotkey_id quickFindHotkey=OBS_INVALID_HOTKEY_ID;
static void showQuickFind(){
    if(!uiActive||!frontendReady)return;
    auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window()));
    if(!main)return;
    if(!quickFind){
        quickFind=new augment::QuickFind(main,
            []{return augment::collectObsQuickFind();},
            [](const augment::QuickFindEntry &hit,augment::QuickFind::Action action){
                return augment::performObsQuickFind(hit,action);
            });
    }
    quickFind->open();
}
// OBS owns the keybinding and user-defined shortcut. No default global
// shortcut is imposed; this callback never touches Qt widgets from OBS's
// hotkey thread.
static void quickFindHotkeyCallback(void *,obs_hotkey_id,obs_hotkey_t *,bool pressed){
    if(!pressed)return;
    if(auto *app=QCoreApplication::instance())
        QMetaObject::invokeMethod(app,[]{showQuickFind();},Qt::QueuedConnection);
}
static void quickFindHotkeyState(obs_data_t *data,bool saving,void *){
    if(quickFindHotkey==OBS_INVALID_HOTKEY_ID||!data)return;
    constexpr auto key="aftermarket.quick_find.hotkey";
    if(saving){
        auto *bindings=obs_hotkey_save(quickFindHotkey);
        if(bindings){obs_data_set_array(data,key,bindings);obs_data_array_release(bindings);}
    }else{
        auto *bindings=obs_data_get_array(data,key);
        if(bindings){obs_hotkey_load(quickFindHotkey,bindings);obs_data_array_release(bindings);}
    }
}
static void showCueDeck(){
    if(!uiActive||!frontendReady||!cueDeck)return;
    auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window()));
    if(!main)return;
    auto *dialog=cueDeck->createManager(main);
    dialog->show();dialog->raise();dialog->activateWindow();
}
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
        if(dockTitleIcons)dockTitleIcons->refresh();
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
    patchbay->setDeviceVisibilityFile(QDir(QFileInfo(stripConfigPath).absolutePath()).filePath("audio-device-visibility.json"));
    patchbay->setDeviceBackend(
        []{return augment::ObsRouting::deviceDestinations();},
        [](const QString &source,const QString &device,bool connected){
            const bool ok=augment::ObsRouting::setDeviceSend(source,device,connected);
            if(ok&&frontendReady)obs_frontend_save();
            return ok;
        });
    patchbay->setDeviceParameterWriter(
        [](const QString &source,const QString &device,augment::DeviceParameter parameter,double value){
            const bool ok=augment::ObsRouting::setDeviceParameter(source,device,parameter,value);
            if(ok&&frontendReady)obs_frontend_save();
            return ok;
        });
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
    // One order store owned by the native front; the routing view consumes
    // UUID order without owning or rewriting any channel configuration.
    patchbay->setSourceOrderProvider([]{
        return frontAudio?frontAudio->channelOrder():QStringList{};
    });
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
        if(panels){
            panels->addCollapseAction(menu,safeDock);
            menu->addSeparator();
        }
        if(dockTitleIcons){
            dockTitleIcons->appendDockWindowActions(menu,safeDock);
            menu->addSeparator();
        }
        // Routing-specific controls belong in the same native dock overflow,
        // but only while the AudioBus face is visible. No duplicate header
        // row, no separate floating toolbar and no source/track mutations.
        if(dockFaces&&dockFaces->backVisible(safeDock)){
            auto *routing=dockFaces->back(safeDock);
            if(routing&&routing->objectName()=="aftermarket-audio-patchbay"){
                static_cast<augment::AudioPatchbay*>(routing)->populateRoutingMenu(menu);
                menu->addSeparator();
            }
        }
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
        if(dockTitleIcons)dockTitleIcons->refresh();
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
    delete quickFind.data();
    if(cueDeck){cueDeck->cancel();cueDeck->closeManager();}
    nativeSettings.reset(); // remove only plugin page from any open native Settings
    sourcePicker.reset(); // existing native source dialogs finish normally
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
    if(layoutsAction)layoutsAction->setEnabled(false);
    if(findAction)findAction->setEnabled(false);
    if(cuesAction)cuesAction->setEnabled(false);
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
        [](const QString &binding){return frontendReady&&nativeActions&&nativeActions->triggerHotkey(binding);},
        nullptr,
        []{
            QList<augment::LayoutChoice> result;
            if(frontendReady&&dockLayouts)
                for(const auto &layout:dockLayouts->snapshots())
                    result.append({layout.id,layout.name});
            return result;
        },
        [](const QString &id){
            return frontendReady&&dockLayouts&&dockLayouts->restore(id);
        },
        []{showQuickFind();},
        []{
            QList<augment::CueChoiceStrip> choices;
            if(frontendReady&&cueDeck)
                for(const auto &cue:cueDeck->cues())
                    choices.append({cue.id,cue.name});
            return choices;
        },
        [](const QString &id){return frontendReady&&cueDeck&&cueDeck->run(id);});
    // Global Motion already controls decorative animation. Use the same
    // current setting for native dock height and AudioBus page transitions.
    // Motion off preserves all original immediate UI behavior.
    auto uiMotion=[]{return controller&&controller->motionEnabled();};
    panels=std::make_unique<augment::Panels>(nullptr,uiMotion);
    if(auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window())))
        dockTitleIcons=std::make_unique<augment::DockTitleIcons>(
            main,panels.get(),QDir(QFileInfo(stripConfigPath).absolutePath())
                               .filePath("dock-title-settings.ini"));
    dockFaces=std::make_unique<augment::DockFaces>(nullptr,uiMotion);
    if(auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window())))
        sourcePicker=std::make_unique<augment::SourcePicker>(
            main,QDir(QFileInfo(stripConfigPath).absolutePath())
                .filePath("source-picker.ini"));
    mixerRetry=std::make_unique<augment::DockAttachRetry>([]{return setAudioMixerBackside(true);});
    transforms=std::make_unique<augment::ObsTransformStyle>(
        static_cast<QWidget*>(obs_frontend_get_main_window()),
        []{return controller&&controller->modernTransformFrame();});
    controller->setRuntimeSafeMode(false);
    if(controller->hamburgerMenuBarEnabled()&&!setHamburgerMenuBar(true))
        blog(LOG_WARNING,"[aftermarket] hamburger menu bar unavailable in this layout");
    if(actionStrip->desiredEnabled()){
        auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window()));
        if(main&&!actionStrip->setEnabled(main->menuBar(),true))
            blog(LOG_WARNING,"[aftermarket] action strip unavailable in this layout");
    }
    if(frontendReady&&controller->mixerBacksideEnabled())mixerRetry->start();
    if(dockTitleIcons){
        augment::ObsSettingsPage::Services services;
        services.setMixer=[](bool enabled){return setAudioMixerBackside(enabled);};
        services.setHamburger=[](bool enabled){return setHamburgerMenuBar(enabled);};
        services.setActionStrip=[](bool enabled){
            auto *main=qobject_cast<QMainWindow*>(
                static_cast<QWidget*>(obs_frontend_get_main_window()));
            if(!main||!actionStrip)return false;
            return actionStrip->setEnabled(main->menuBar(),enabled);
        };
        services.actionStripEnabled=[]{
            return actionStrip&&actionStrip->desiredEnabled();
        };
        services.editActionStrip=[](QWidget *owner){
            if(!actionStrip)return;
            auto *main=qobject_cast<QMainWindow*>(
                static_cast<QWidget*>(obs_frontend_get_main_window()));
            if(main)actionStrip->showEditor(owner,main->menuBar());
        };
        services.setPropertySections=[](bool enabled){
            if(propertySections)propertySections->setEnabled(enabled);
        };
        services.setRowStripes=[](const QJsonObject &settings){
            if(rowStripes)rowStripes->configure(settings);
        };
        services.compactSourcePickerEnabled=[]{
            return sourcePicker&&sourcePicker->enabled();
        };
        services.setCompactSourcePicker=[](bool enabled){
            return sourcePicker&&sourcePicker->setEnabled(enabled);
        };
        nativeSettings=std::make_unique<augment::ObsSettingsPage>(
            *controller,*dockTitleIcons,std::move(services));
    }
    uiActive=true;
    if(toolsAction)toolsAction->setEnabled(true);
    if(panelsAction)panelsAction->setEnabled(true);
    if(stripAction)stripAction->setEnabled(true);
    if(layoutsAction)layoutsAction->setEnabled(frontendReady);
    if(findAction)findAction->setEnabled(frontendReady);
    if(cuesAction)cuesAction->setEnabled(frontendReady);
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
    delete layoutsAction.data();
    delete findAction.data();
    delete cuesAction.data();
    cueDeck.reset();
    dockLayouts.reset();
    delete toolsAction.data();
}
static void frontendEvent(enum obs_frontend_event event, void *) {
    if(nativeActions&&event!=OBS_FRONTEND_EVENT_EXIT)nativeActions->onFrontendEvent(event);
    if(event==OBS_FRONTEND_EVENT_FINISHED_LOADING){
        frontendReady=true;
        if(uiActive&&layoutsAction)layoutsAction->setEnabled(true);
        if(uiActive&&findAction)findAction->setEnabled(true);
        if(uiActive&&cuesAction)cuesAction->setEnabled(true);
        if(actionStrip)actionStrip->refresh();
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
MODULE_EXPORT const char *obs_module_name(void) { return "Aftermarket"; }
MODULE_EXPORT const char *obs_module_description(void) { return "Native OBS interface customization, workspace controls and audio routing"; }
MODULE_EXPORT bool obs_module_load(void) {
    auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app || app->thread() != QThread::currentThread()) return false;
    char *raw = obs_module_config_path("rules.json");
    if (!raw) return false;
    const QString file = QString::fromUtf8(raw);
    bfree(raw);
    const QString folder=QFileInfo(file).absolutePath();
    // Refuse a side-by-side upgrade rather than load duplicate UI adapters.
    const auto profile=QFileInfo(folder).dir().absoluteFilePath("..");
    if(obs_get_module("obs-ui-augment")||
       QFileInfo::exists(QDir(profile).filePath("plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so"))){
        blog(LOG_ERROR,"[aftermarket] remove the old obs-ui-augment installation before upgrading");
        return false;
    }
    QString migrationError;
    if(!augment::prepareConfigIdentity(folder,&migrationError)){
        blog(LOG_ERROR,"[aftermarket] %s",qPrintable(migrationError));
        return false;
    }
    stripConfigPath=QDir(folder).filePath("action-strip.json");
    auto *mainWindow=qobject_cast<QMainWindow*>(
        static_cast<QWidget*>(obs_frontend_get_main_window()));
    dockLayouts=std::make_unique<augment::DockLayouts>(
        QDir(folder).filePath("dock-layouts.json"),mainWindow);
    nativeActions=std::make_unique<augment::NativeActionBridge>();
    cueDeck=std::make_unique<augment::CueDeck>(
        QDir(folder).filePath("cue-deck.json"),
        []{
            QList<augment::CueChoice> list;
            if(frontendReady&&dockLayouts)
                for(const auto &layout:dockLayouts->snapshots())
                    list.append({layout.id,layout.name,"Dock layouts"});
            return list;
        },
        [](const QString &id){return frontendReady&&dockLayouts&&dockLayouts->restore(id);},
        []{
            QList<augment::CueChoice> list;
            if(frontendReady&&nativeActions)
                for(const auto &key:nativeActions->enumerateHotkeys())
                    list.append({key.binding,key.label,key.group});
            return list;
        },
        [](const QString &id){return frontendReady&&nativeActions&&nativeActions->triggerHotkey(id);});
    quickFindHotkey=obs_hotkey_register_frontend("aftermarket.quick_find",
        "Aftermarket: Quick Find sources and filters",quickFindHotkeyCallback,nullptr);
    obs_frontend_add_save_callback(quickFindHotkeyState,nullptr);
    auto *main=static_cast<QWidget*>(obs_frontend_get_main_window());
    emergency=std::make_unique<augment::EmergencyRecovery>(
        QDir(folder).filePath("safe-mode.flag"),main,
        []{
            // Do not delete dock/menu widgets inside an active QMenu signal
            // or a keyboard event filter; the marker is already on disk.
            QTimer::singleShot(0,[] {
                if(emergency&&emergency->active()){
                    stopCustomUi();
                    blog(LOG_WARNING,"[aftermarket] emergency safe mode enabled; native UI restored");
                }
            });
        },
        []{return startCustomUi();});
    controller = new augment::Controller(file, nullptr, [](const QString &message) {
        blog(LOG_INFO, "[aftermarket] %s", qPrintable(message));
    },emergency->active());
    audio=std::make_unique<augment::ObsAudio>();
    controller->setAudioProvider([](const QWidget *w){return audio?audio->level(w):std::optional<float>{};},
        []{if(audio)audio->clear();});
    controller->setMeterProvider([](const QWidget *w){return audio?audio->sample(w):std::optional<augment::MeterReading>{};});
    toolsAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction("Aftermarket: Inspector"));
    if(toolsAction)QObject::connect(toolsAction.data(),&QAction::triggered,controller,[]{
        if(!inspector)inspector=new augment::Inspector(*controller,
            static_cast<QWidget*>(obs_frontend_get_main_window()),panels.get(),propertySections.get(),setAudioMixerBackside,rowStripes.get(),compactBar.get(),actionStrip.get());
        inspector->show();inspector->raise();inspector->activateWindow();
    });
    panelsAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction("Aftermarket: Panels"));
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
    layoutsAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction(
        "Aftermarket: Dock Layouts…"));
    blog(LOG_INFO,"[aftermarket] AFTERMARKET_LAYOUT_TOOLS_20261123 registered=%d",layoutsAction?1:0);
    if(layoutsAction)QObject::connect(layoutsAction.data(),&QAction::triggered,controller,[]{
        auto *main=qobject_cast<QMainWindow*>(
            static_cast<QWidget*>(obs_frontend_get_main_window()));
        if(!main||!frontendReady)return;
        // Wait until the native Tools popup closes before opening a modal UI.
        QPointer<QMainWindow> safeMain=main;
        QTimer::singleShot(0,[safeMain]{
            if(safeMain&&dockLayouts&&uiActive)dockLayouts->showManager(safeMain);
            if(actionStrip)actionStrip->refresh();
        });
    });
    findAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction(
        "Aftermarket: Find Sources…"));
    blog(LOG_INFO,"[aftermarket] AFTERMARKET_QUICK_FIND_20261123 registered=%d",findAction?1:0);
    if(findAction)QObject::connect(findAction.data(),&QAction::triggered,controller,[]{
        // The tools menu can still be closing when this signal arrives.
        QTimer::singleShot(0,[]{showQuickFind();});
    });
    cuesAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction(
        "Aftermarket: Cue Deck…"));
    blog(LOG_INFO,"[aftermarket] AFTERMARKET_CUE_DECK_20261123 registered=%d",cuesAction?1:0);
    if(cuesAction)QObject::connect(cuesAction.data(),&QAction::triggered,controller,[]{
        QTimer::singleShot(0,[]{showCueDeck();});
    });
    emergencyAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction(
        "Aftermarket: Emergency restore (Ctrl+Alt+Shift+F12)"));
    if(emergencyAction)QObject::connect(emergencyAction.data(),&QAction::triggered,controller,[]{
        if(emergency&&!emergency->engage()&&!emergency->active())
            blog(LOG_ERROR,"[aftermarket] emergency marker could not be saved; no UI state changed");
    });
    resumeAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction(
        "Aftermarket: Resume UI customizations"));
    if(resumeAction)QObject::connect(resumeAction.data(),&QAction::triggered,controller,[]{
        // A native Tools menu may still be closing; avoid reparenting it mid-signal.
        QTimer::singleShot(0,[]{
            if(emergency&&!emergency->resume())
                blog(LOG_ERROR,"[aftermarket] safe mode remains enabled; resume failed");
        });
    });
    if(emergency->active()){
        // Already suspended before any UI adapters were constructed.
        if(toolsAction)toolsAction->setEnabled(false);
        if(panelsAction)panelsAction->setEnabled(false);
        if(stripAction)stripAction->setEnabled(false);
        if(layoutsAction)layoutsAction->setEnabled(false);
        if(findAction)findAction->setEnabled(false);
        if(cuesAction)cuesAction->setEnabled(false);
        if(emergencyAction)emergencyAction->setEnabled(false);
        if(resumeAction)resumeAction->setVisible(true);
        blog(LOG_WARNING,"[aftermarket] safe-mode.flag detected; Aftermarket UI disabled");
    }else startCustomUi();
    obs_frontend_add_event_callback(frontendEvent,nullptr);
    blog(LOG_INFO, "[aftermarket] recovery shortcut registered; rules: %s", qPrintable(file));
    return true;
}
MODULE_EXPORT void obs_module_unload(void) {
    obs_frontend_remove_event_callback(frontendEvent,nullptr);
    obs_frontend_remove_save_callback(quickFindHotkeyState,nullptr);
    if(quickFindHotkey!=OBS_INVALID_HOTKEY_ID){
        obs_hotkey_unregister(quickFindHotkey);
        quickFindHotkey=OBS_INVALID_HOTKEY_ID;
    }
    cleanupUi();
    delete controller; controller = nullptr;
    audio.reset();
    nativeActions.reset();
}
