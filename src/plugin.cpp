#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-hotkey.h>
#include <QAction>
#include <QPointer>
#include "inspector.h"
#include "panels.h"
#include "property-sections.h"
#include "property-columns.h"
#include "property-preset-footer.h"
#include "tools-menu.h"
#include "row-stripes.h"
#include "menu-bar-compact.h"
#include "action-strip.h"
#include "menu-settings-corner.h"
#include "quick-find.h"
#include "command-center.h"
#include "source-inspector-dock.h"
#include "obs-note-targets.h"
#include "native-notes.h"
#include "source-dock-pin.h"
#include "source-performance.h"
#include "dock-layouts.h"
#include "workspace-checkpoints.h"
#include "cue-deck.h"
#include "native-action-bridge.h"
#include "dock-attach-retry.h"
#include "emergency-recovery.h"
#include "dock-faces.h"
#include "audio-patchbay.h"
#include "obs-routing.h"
#include "obs-mixer-loudness.h"
#include "front-audio-controls.h"
#include "dock-header-button.h"
#include "dock-title-icons.h"
#include "obs-settings-page.h"
#include "source-picker.h"
#include "health-stats.h"
#include "restart-request.h"
#include <QProcess>
#include <QScopedValueRollback>
#include "health-resources.h"
#include "native-plugin-icons.h"
#include <obs.hpp>
#include <QVariant>
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
#include <QFile>
#include <QEventLoop>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QRegularExpression>
#include <util/config-file.h>
#include <util/platform.h>
#include "augment.h"
#include "config-identity.h"
#include "obs-audio.h"
#include "obs-transform.h"
Q_DECLARE_METATYPE(OBSSource);
OBS_DECLARE_MODULE()
static augment::Controller *controller = nullptr;
static std::unique_ptr<augment::ObsAudio> audio;
static std::unique_ptr<augment::ObsTransformStyle> transforms;
static QPointer<augment::Inspector> inspector;
static QPointer<augment::QuickFind> quickFind;
static std::unique_ptr<augment::CommandCenter> commandCenter;
static QPointer<augment::SourceInspectorDock> sourceInspector;
static std::shared_ptr<augment::ContextNotes> contextNotes;
static std::unique_ptr<augment::NativeNotes> nativeNotes;
static std::unique_ptr<augment::ToolsMenu> toolsMenu;
static std::unique_ptr<augment::Panels> panels;
static std::unique_ptr<augment::PropertySections> propertySections;
static std::unique_ptr<augment::PropertyColumns> propertyColumns;
static std::unique_ptr<augment::PropertyPresetFooter> propertyPresetFooter;
static std::unique_ptr<augment::RowStripes> rowStripes;
static std::unique_ptr<augment::MenuBarCompact> compactBar;
static std::unique_ptr<augment::ActionStrip> actionStrip;
static std::unique_ptr<augment::MenuSettingsCorner> menuSettings;
static std::unique_ptr<augment::DockLayouts> dockLayouts;
static std::unique_ptr<augment::CueDeck> cueDeck;
static std::unique_ptr<augment::NativeActionBridge> nativeActions;
static std::unique_ptr<augment::DockAttachRetry> mixerRetry;
static QString stripConfigPath;
static std::unique_ptr<augment::DockFaces> dockFaces;
static std::unique_ptr<augment::DockTitleIcons> dockTitleIcons;
static std::unique_ptr<augment::ObsSettingsPage> nativeSettings;
static std::unique_ptr<augment::SourcePicker> sourcePicker;
static std::unique_ptr<augment::HealthStats> healthStats;
static std::unique_ptr<augment::HealthResources> healthResources;
static std::unique_ptr<os_cpu_usage_info_t,decltype(&os_cpu_usage_info_destroy)> healthCpu(nullptr,os_cpu_usage_info_destroy);
static std::unique_ptr<augment::NativePluginIcons> nativePluginIcons;
static std::unique_ptr<augment::FrontAudioControls> frontAudio;
static std::unique_ptr<augment::EmergencyRecovery> emergency;
static bool uiActive=false,frontendReady=false;
static void setHealthResources(bool enabled){
    if(enabled){
        if(!healthResources){healthResources=std::make_unique<augment::HealthResources>();healthResources->start();}
        if(!healthCpu)healthCpu.reset(os_cpu_usage_info_start());
    }else{
        healthResources.reset();healthCpu.reset();
    }
}
static augment::HealthSample sampleHealth(){
    augment::HealthSample s;
    if(!frontendReady||!uiActive){s.valid=false;return s;}
    auto *video=obs_get_video();
    if(!video){s.valid=false;return s;}
    s.rendered=obs_get_total_frames();s.renderMissed=obs_get_lagged_frames();
    s.encoded=video_output_get_total_frames(video);s.encodeSkipped=video_output_get_skipped_frames(video);
    if(healthCpu)s.obsCpu=os_cpu_usage_info_query(healthCpu.get());
    obs_video_info info{};
    if(obs_get_video_info(&info)&&info.fps_den&&info.fps_num)
        s.renderBudget=double(obs_get_average_frame_time_ns())*double(info.fps_num)/double(info.fps_den)/1e7;
    if(auto *output=obs_frontend_get_streaming_output()){
        s.stream=obs_output_active(output)||obs_output_reconnecting(output);
        s.reconnecting=obs_output_reconnecting(output);
        s.streamId=reinterpret_cast<uintptr_t>(output);
        if(s.stream){
            s.networkDropped=uint64_t(qMax(0,obs_output_get_frames_dropped(output)));
            s.congestion=double(obs_output_get_congestion(output));
        }
        obs_output_release(output);
    }
    if(healthResources){
        const auto resources=healthResources->latest();s.systemCpu=resources.cpu;s.gpu=resources.gpu;s.gpuLabel=resources.gpuLabel;
    }
    return s;
}
static obs_hotkey_id quickFindHotkey=OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id restartHotkey=OBS_INVALID_HOTKEY_ID;
static augment::RestartRequest restartRequest;
static QString restartHelper,restartExecutable,restartCwd;
static QStringList restartArgs;
static unsigned restartingOutputs=0;
static bool restartPrompt=false;
// OBS frontend_save posts SaveProjectDeferred. Its callback receives module
// data, not the whole scene JSON, so wait for that save to finish before
// reading the native scene file. Never derive a filename from the UI label.
static augment::CheckpointScene captureCheckpointScene(const QString &folder){
    if(!frontendReady||!uiActive)return {};
    auto identity=[](){
        auto *name=obs_frontend_get_current_scene_collection();
        const QString collection=name?QString::fromUtf8(name):QString{};
        if(name)bfree(name);
        auto *config=obs_frontend_get_user_config();
        const char *file=config?config_get_string(config,"Basic","SceneCollectionFile"):nullptr;
        return qMakePair(collection,file?QString::fromUtf8(file):QString{});
    };
    const auto before=identity();
    if(before.first.isEmpty()||before.second.isEmpty()||before.second=="."||before.second==".."||
       before.second.contains('/')||before.second.contains('\\'))return {};
    const auto filename=before.second.endsWith(".json",Qt::CaseInsensitive)?before.second:before.second+".json";
    const auto path=QDir(folder).absoluteFilePath("../../basic/scenes/"+filename);
    const QFileInfo oldFile(path);
    const auto oldModified=oldFile.lastModified();
    struct SaveWait {QEventLoop loop;bool observed=false;};
    SaveWait wait;
    auto callback=[](obs_data_t *,bool saving,void *opaque){
        auto *state=static_cast<SaveWait*>(opaque);
        if(saving){
            state->observed=true;
            QTimer::singleShot(0,&state->loop,&QEventLoop::quit);
        }
    };
    obs_frontend_add_save_callback(callback,&wait);
    QTimer::singleShot(2000,&wait.loop,&QEventLoop::quit);
    obs_frontend_save();
    wait.loop.exec(QEventLoop::ExcludeUserInputEvents);
    obs_frontend_remove_save_callback(callback,&wait);
    if(!wait.observed||!frontendReady||!uiActive||identity()!=before)return {};
    const QFileInfo saved(path);
    // Fail closed if the callback happened but OBS did not publish a new
    // file. Native save writes a temporary file then atomically replaces it.
    if(!saved.isFile()||saved.isSymLink()||saved.size()>32*1024*1024||
       !saved.lastModified().isValid()||saved.lastModified()==oldModified)return {};
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))return {};
    const auto bytes=file.read(32*1024*1024+1);
    if(file.error()!=QFile::NoError)return {};
    return {before.first,bytes};
}

static bool collectionManagerActionAvailable(const QString &name){
    if(!frontendReady||!uiActive||!nativeActions||!obs_get_module("scene-collection-manager"))return false;
    const auto binding=augment::NativeActionBridge::binding("frontend","",name);
    const auto choices=nativeActions->enumerateHotkeys();
    return std::any_of(choices.begin(),choices.end(),[&](const auto &item){return item.binding==binding;});
}
static augment::CompanionBackupResult backupCheckpointCompanion(const QString &folder,
                                                               const augment::WorkspaceCheckpoint &saved){
    using Result=augment::CompanionBackupResult;
    if(!collectionManagerActionAvailable("backup_scene_collection"))
        return Result{false,{},"Scene Collection Manager backup action is unavailable."};
    auto *name=obs_frontend_get_current_scene_collection();
    const QString current=name?QString::fromUtf8(name):QString{};
    if(name)bfree(name);
    if(current!=saved.collection)return Result{false,{},"Active scene collection changed; companion backup skipped."};
    // The installed manager derives its source filename from the collection
    // name, rather than OBS's actual file identifier. Fail closed on collisions
    // and names whose platform-dependent sanitization we cannot verify.
    if(!QRegularExpression("^[ -~]+$").match(current).hasMatch())
        return Result{false,{},"Collection filename cannot be safely resolved by this manager integration."};
    QString safe=current;
    safe.replace(QRegularExpression("\\s"),"_");
    safe.remove(QRegularExpression("[^A-Za-z0-9_]"));
    auto *config=obs_frontend_get_user_config();
    const auto *rawFile=config?config_get_string(config,"Basic","SceneCollectionFile"):nullptr;
    QString filename=rawFile?QString::fromUtf8(rawFile):QString{};
    if(filename.endsWith(".json",Qt::CaseInsensitive))filename.chop(5);
    if(safe.isEmpty()||safe!=filename)return Result{false,{},"Manager filename differs from OBS; companion backup skipped."};
    const auto *custom=config_get_string(config,"SceneCollectionManager","BackupDir");
    const QString root=custom&&*custom?QString::fromUtf8(custom):QDir(folder).absoluteFilePath("../../basic/scenes");
    if(!QDir::isAbsolutePath(root))return Result{false,{},"Manager backup folder is not an absolute path."};
    const auto directory=QDir(root).filePath(safe);
    const QDir dir(directory);
    // Manager writes timestamp filenames with one-second resolution. Avoid
    // dispatch near an existing timestamp, including a possible second tick.
    const auto now=QDateTime::currentDateTime();
    for(int seconds=-1;seconds<=2;++seconds){
        const auto candidate=now.addSecs(seconds).toString("yyyyMMdd_HHmmss")+".json";
        const QFileInfo info(dir.filePath(candidate));
        if(info.exists()||info.isSymLink())return Result{false,{},"A recent manager backup already exists; companion skipped to avoid overwriting it."};
    }
    return augment::WorkspaceCheckpoints::verifyCompanionBackup(directory,saved.scene,[]{
        return nativeActions&&nativeActions->triggerHotkey(
            augment::NativeActionBridge::binding("frontend","","backup_scene_collection"));
    });
}

static QAction *sourceDockManagerAction(){
    if(!frontendReady||!uiActive)return nullptr;
    auto *module=obs_get_module("source-dock");
    auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window()));
    auto *tools=main?main->findChild<QMenu*>("menuTools"):nullptr;
    const char *label=nullptr;
    if(!module||!tools||!obs_module_get_locale_string(module,"SourceDock",&label)||!label||!*label)return nullptr;
    QAction *match=nullptr;
    for(auto *action:tools->actions())if(action->text()==QString::fromUtf8(label)){
        if(match)return nullptr;
        match=action;
    }
    return match&&match->isEnabled()?match:nullptr;
}
using NativeSourceDockPin=bool (*)(const char *,const char *);
static NativeSourceDockPin nativeSourceDockPin(){
    if(!frontendReady||!uiActive)return nullptr;
    auto *module=obs_get_module("source-dock");
    auto *library=module?obs_get_module_lib(module):nullptr;
    return library?reinterpret_cast<NativeSourceDockPin>(os_dlsym(library,"source_dock_pin_v1")):nullptr;
}
static bool pinSourceDock(const augment::QuickFindEntry &hit){
    if(hit.kind==augment::QuickFindEntry::Filter||hit.id.isEmpty())return false;
    const auto nativePin=nativeSourceDockPin();
    auto *action=sourceDockManagerAction();
    auto *module=obs_get_module("source-dock");
    auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window()));
    const char *addLabel=nullptr;
    if(!module||!main||(!nativePin&&(!action||!obs_module_get_locale_string(module,"Add",&addLabel)||!addLabel)))return false;
    auto *source=obs_get_source_by_uuid(hit.id.toUtf8().constData());
    if(!source)return false;
    if(obs_source_removed(source)||obs_source_get_type(source)==OBS_SOURCE_TYPE_FILTER){
        obs_source_release(source);return false;
    }
    const QString name=QString::fromUtf8(obs_source_get_name(source));
    const auto flags=obs_source_get_output_flags(source);
    const bool scene=obs_source_get_type(source)==OBS_SOURCE_TYPE_SCENE;
    augment::SourceDockPinOptions opts{bool(flags&OBS_SOURCE_VIDEO),bool(flags&OBS_SOURCE_AUDIO),
        bool(flags&OBS_SOURCE_CONTROLLABLE_MEDIA),scene};
    // Source Dock uses the chosen title as its native dock ID. Never replace
    // another dock, even when its label resembles the requested source.
    QSet<QString> used;
    for(auto *w:QApplication::allWidgets())if(auto *dock=qobject_cast<QDockWidget*>(w))used.insert(dock->objectName());
    QString title=name+" · pinned";
    for(int suffix=2;used.contains(title)&&suffix<=100;++suffix)title=name+" · pinned "+QString::number(suffix);
    if(name.isEmpty()||used.contains(title)){obs_source_release(source);return false;}
    if(nativePin){
        const bool created=nativePin(hit.id.toUtf8().constData(),title.toUtf8().constData());
        obs_source_release(source);
        auto *dock=created?main->findChild<QDockWidget*>(title):nullptr;
        if(dock&&dock->widget()&&dock->widget()->inherits("SourceDock")){
            if(dockTitleIcons)dockTitleIcons->refresh();
            dock->show();dock->raise();
            return true;
        }
        QMessageBox::information(main,"Pin to workspace","Source Dock declined the creation request.");
        return false;
    }
    QSet<QDialog*> prior;
    for(auto *dialog:main->findChildren<QDialog*>())prior.insert(dialog);
    action->trigger();
    QList<QDialog*> fresh;
    for(auto *dialog:main->findChildren<QDialog*>())
        if(!prior.contains(dialog)&&dialog->inherits("SourceDockSettingsDialog"))fresh.append(dialog);
    QPointer<QDialog> form=fresh.size()==1?fresh.first():nullptr;
    auto *add=form?augment::prepareSourceDockPin(form,name,title,QString::fromUtf8(addLabel),opts):nullptr;
    bool ok=false;
    if(add&&!obs_source_removed(source)&&QString::fromUtf8(obs_source_get_name(source))==name){
        add->click();
        auto *dock=main->findChild<QDockWidget*>(title);
        ok=dock&&dock->widget()&&dock->widget()->inherits("SourceDock");
        if(ok){
            if(form)form->close();
            obs_frontend_save();
            if(dockTitleIcons)dockTitleIcons->refresh();
            dock->show();dock->raise();
        }
    }
    obs_source_release(source);
    if(!ok)QMessageBox::information(main,"Pin to workspace",
        "The native Source Dock creation form could not be verified or the dock was not created. Use the Source Dock manager directly.");
    return ok;
}
static bool configureSourcePerformance(const augment::QuickFindEntry &hit){
    if(!frontendReady||!uiActive||hit.kind!=augment::QuickFindEntry::Source)return false;
    auto state=augment::SourcePerformance::inspect(hit.id);
    auto *parent=static_cast<QWidget*>(obs_frontend_get_main_window());
    if(!state.eligible)return false;
    if(state.blocked){
        QMessageBox::information(parent,"Off-air frame limit",
            "This source already has an external or duplicate Frame Interleave filter. Manage it in native Filters.");
        return false;
    }
    bool accepted=false;
    const int ms=QInputDialog::getInt(parent,"Off-air frame limit",
        "Minimum interval in milliseconds while OBS considers this source inactive.\n"
        "0 removes Aftermarket's filter. 100 ≈ 10 fps; 500 ≈ 2 fps.\n"
        "All previews of this source share the limit. Active sources bypass it.",
        state.managed&&state.enabled?state.intervalMs:0,0,1000,1,&accepted);
    if(!accepted)return false;
    if(!augment::SourcePerformance::setInterval(hit.id,ms)){
        QMessageBox::warning(parent,"Off-air frame limit","The source or its filter changed. No new limit was confirmed.");
        return false;
    }
    obs_frontend_save();
    return true;
}
static QList<augment::QuickFindEntry> notesCatalog(){
    auto result=augment::collectObsQuickFind();
    if(contextNotes){
        contextNotes->reload();
        for(auto &entry:result)entry.note=contextNotes->get(augment::entryNoteKey(entry));
    }
    return result;
}
static bool openEntryNote(const augment::QuickFindEntry &entry){
    if(!uiActive||!frontendReady||!contextNotes||!augment::noteEntryExists(entry))return false;
    auto *main=static_cast<QWidget*>(obs_frontend_get_main_window());
    const auto collection=augment::noteCollection();
    augment::editContextNote(main,contextNotes,augment::entryNoteKey(entry),entry.name,
        [collection,entry]{return uiActive&&frontendReady&&collection==augment::noteCollection()&&augment::noteEntryExists(entry);},
        []{if(sourceInspector)sourceInspector->refresh();if(nativeNotes)nativeNotes->schedule();});
    return true;
}
static void showQuickFind(){
    if(!uiActive||!frontendReady)return;
    auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window()));
    if(!main)return;
    if(!quickFind){
        quickFind=new augment::QuickFind(main,
            []{return notesCatalog();},
            [](const augment::QuickFindEntry &hit,augment::QuickFind::Action action){
                if(action==augment::QuickFind::PinSource)return pinSourceDock(hit);
                if(action==augment::QuickFind::Performance)return configureSourcePerformance(hit);
                if(action==augment::QuickFind::Note)return openEntryNote(hit);
                return augment::performObsQuickFind(hit,action);
            },nativeSourceDockPin()!=nullptr||sourceDockManagerAction()!=nullptr,contextNotes!=nullptr);
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
static void restartHotkeyState(obs_data_t *data,bool saving,void *){
    if(restartHotkey==OBS_INVALID_HOTKEY_ID||!data)return;
    constexpr auto key="aftermarket.restart_obs.hotkey";
    if(saving){
        auto *bindings=obs_hotkey_save(restartHotkey);
        if(bindings){obs_data_set_array(data,key,bindings);obs_data_array_release(bindings);}
    }else{
        auto *bindings=obs_data_get_array(data,key);
        if(bindings){obs_hotkey_load(restartHotkey,bindings);obs_data_array_release(bindings);}
    }
}
static bool outputsBusy(){
    return restartingOutputs||obs_frontend_streaming_active()||obs_frontend_recording_active()||
        obs_frontend_replay_buffer_active()||obs_frontend_virtualcam_active();
}
static void requestRestart(){
    if(!frontendReady||!uiActive||restartPrompt||QApplication::activeModalWidget())return;
    auto *main=static_cast<QWidget*>(obs_frontend_get_main_window());
    if(!main)return;
    QScopedValueRollback<bool> guard(restartPrompt,true);
    if(outputsBusy()){
        QMessageBox::information(main,"Restart OBS","Stop streaming, recording, replay buffer and virtual camera before restarting OBS.");
        return;
    }
    // Sandboxed/temporary-mount launches require the original package
    // launcher and a host-side handoff, not the inner OBS executable.
    if(QFileInfo::exists("/.flatpak-info")||qEnvironmentVariableIsSet("APPIMAGE")||qEnvironmentVariableIsSet("SNAP")){
        QMessageBox::information(main,"Restart OBS","Automatic restart is not available for this packaged OBS launch. Close OBS and reopen it using its launcher.");
        return;
    }
    auto helper=QFileInfo(QString::fromUtf8(obs_get_module_binary_path(obs_current_module()))).absoluteDir().filePath("aftermarket-restart-helper"
#ifdef Q_OS_WIN
        ".exe"
#endif
    );
    const auto executable=QCoreApplication::applicationFilePath();
    if(!QFileInfo(helper).isExecutable()||!QFileInfo(executable).isExecutable()){
        QMessageBox::warning(main,"Restart OBS","The restart helper or OBS executable is unavailable. Reinstall the complete Aftermarket package.");
        return;
    }
    if(QMessageBox::question(main,"Restart OBS","Close and reopen OBS? Your current workspace will be saved by OBS. Outputs will not start automatically.",
        QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    if(outputsBusy()||!frontendReady||!uiActive)return;
    restartHelper=helper;restartExecutable=executable;restartCwd=QDir::currentPath();
    restartArgs=augment::restartArguments(QCoreApplication::arguments().mid(1));
    restartRequest.request([main]{return main->close();});
}
static void restartHotkeyCallback(void *,obs_hotkey_id,obs_hotkey_t *,bool pressed){
    if(pressed)if(auto *app=QCoreApplication::instance())
        QMetaObject::invokeMethod(app,[]{requestRestart();},Qt::QueuedConnection);
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
        },
        augment::ObsRouting::monitorOnlyAvailable()
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
    auto *loudness=new augment::ObsMixerLoudness(QDir(QFileInfo(stripConfigPath).absolutePath()).filePath("mixer-loudness.ini"));
    if(!dockFaces->addFooterAccessory(dock,loudness))delete loudness;

    // One order store owned by the native front; the routing view consumes
    // UUID order without owning or rewriting any channel configuration.
    patchbay->setSourceOrderProvider([]{
        return frontAudio?frontAudio->channelOrder():QStringList{};
    });
    // A single native-title overflow holds the presentation toggle and all
    // advanced per-channel pan/sync editors. No second content header.
    auto *more=new augment::DockHeaderButton(augment::DockHeaderGlyph::More);
    more->setMotionProvider([]{return controller&&controller->cachedMotionEnabled();});
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
        if(auto *widget=safeDock->findChild<QWidget*>("aftermarket-mixer-loudness")){
            auto *sub=menu->addMenu("Output loudness");
            static_cast<augment::ObsMixerLoudness*>(widget)->populateMenu(sub);
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
static bool showSourceInspector(const augment::QuickFindEntry *entry=nullptr){
    if(!frontendReady||!uiActive||!sourceInspector)return false;
    auto *dock=qobject_cast<QDockWidget*>(sourceInspector->parentWidget());
    if(!dock)return false;
    if(entry&&!sourceInspector->inspect(*entry))return false;
    dock->show();dock->raise();sourceInspector->refresh();return true;
}
static void stopCustomUi(){
    // The recovery marker is written before this runs. Do not touch the OBS
    // scene collection, audio state, saved dock layout, or rules.json.
    if(controller)controller->setRuntimeSafeMode(true);
    delete inspector.data();
    commandCenter.reset();
    if(auto *main=static_cast<QWidget*>(obs_frontend_get_main_window()))
        for(auto *editor:main->findChildren<QDialog*>("aftermarket-note-editor"))delete editor;
    nativeNotes.reset();
    contextNotes.reset();
    if(sourceInspector){obs_frontend_remove_dock("aftermarket-source-inspector-dock");sourceInspector.clear();}
    delete quickFind.data();
    if(cueDeck){cueDeck->cancel();cueDeck->closeManager();}
    nativeSettings.reset(); // remove only plugin page from any open native Settings
    healthStats.reset();setHealthResources(false);
    sourcePicker.reset(); // existing native source dialogs finish normally
    if(panels)panels->restoreAll();
    mixerRetry.reset();
    actionStrip.reset(); // preserve desired state, restore Settings corner
    menuSettings.reset(); // restore OBS-owned menu corner
    frontAudio.reset(); // remove pan/offset controls before original mixer is restored
    dockFaces.reset(); // unwrap original mixer widget before panel teardown
    dockTitleIcons.reset(); // remove only plugin-painted native title icons
    compactBar.reset(); // return original OBS QAction identities
    nativePluginIcons.reset(); // restore native source pixmaps and remove filter glyphs
    rowStripes.reset();
    propertyPresetFooter.reset();
    propertyColumns.reset();
    propertySections.reset();
    transforms.reset();
    panels.reset(); // return original center preview (if docked)
    uiActive=false;
    if(toolsMenu)toolsMenu->setState(false,frontendReady);
}
// Workspace editors/checkpoints may be stack-owned modal dialogs. Unwind
// their nested event loops before destroying the workspace in recovery.
static void restoreEmergencyUi(){
    if(!emergency||!emergency->active())return;
    if(inspector){
        auto *modal=qobject_cast<QDialog*>(QApplication::activeModalWidget());
        // QWidget::isAncestorOf stops at window boundaries. Modal dialogs
        // are windows but are still QObject-owned by the workspace.
        bool owned=false;
        for(QObject *p=modal;p;p=p->parent())if(p==inspector.data()){owned=true;break;}
        if(modal&&owned){
            modal->reject();
            QTimer::singleShot(0,[]{restoreEmergencyUi();});
            return;
        }
    }
    stopCustomUi();
    blog(LOG_WARNING,"[aftermarket] emergency safe mode enabled; native UI restored");
}
static bool startCustomUi(){
    if(uiActive)return true;
    if(!controller)return false;
    propertySections=std::make_unique<augment::PropertySections>();
    propertySections->setEnabled(controller->collapsiblePropertyGroups());
    propertyColumns=std::make_unique<augment::PropertyColumns>();
    propertyColumns->setMode(controller->propertyColumns());
    propertyPresetFooter=std::make_unique<augment::PropertyPresetFooter>(QFileInfo(stripConfigPath).dir().filePath("property-presets.json"));
    nativePluginIcons=std::make_unique<augment::NativePluginIcons>(
        [](const QString &name)->QString{
            const auto bytes=name.toUtf8();
            OBSSourceAutoRelease source=obs_get_source_by_name(bytes.constData());
            if(!source||obs_source_removed(source)||obs_source_get_type(source)!=OBS_SOURCE_TYPE_INPUT)return {};
            return QString::fromUtf8(obs_source_get_unversioned_id(source));
        },
        [](const QVariant &value)->QString{
            if(value.metaType()!=QMetaType::fromType<OBSSource>())return {};
            OBSSource filter=value.value<OBSSource>();
            if(!filter||obs_source_removed(filter)||obs_source_get_type(filter)!=OBS_SOURCE_TYPE_FILTER)return {};
            return QString::fromUtf8(obs_source_get_unversioned_id(filter));
        });
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
    auto uiMotion=[]{return controller&&controller->cachedMotionEnabled();};
    actionStrip->setMotionProvider(uiMotion);
    if(auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window())))
        if(auto *settings=main->findChild<QAction*>("action_Settings"))
            menuSettings=std::make_unique<augment::MenuSettingsCorner>(main->menuBar(),settings,uiMotion);

    panels=std::make_unique<augment::Panels>(nullptr,uiMotion);
    if(auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window())))
        dockTitleIcons=std::make_unique<augment::DockTitleIcons>(
            main,panels.get(),QDir(QFileInfo(stripConfigPath).absolutePath())
                               .filePath("dock-title-settings.ini"));
    if(dockTitleIcons){
        dockTitleIcons->setSourceDockSettingsAction([]{return sourceDockManagerAction();});
        dockTitleIcons->setSourceDockPreviewSave([]{if(frontendReady&&uiActive)obs_frontend_save();});
    }
    dockFaces=std::make_unique<augment::DockFaces>(nullptr,uiMotion);
    if(auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window())))
        sourcePicker=std::make_unique<augment::SourcePicker>(
            main,QDir(QFileInfo(stripConfigPath).absolutePath())
                .filePath("source-picker.ini"));
    if(auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window()))){
        healthStats=std::make_unique<augment::HealthStats>(main,QDir(QFileInfo(stripConfigPath).absolutePath()).filePath("health-stats.ini"),sampleHealth);
        setHealthResources(healthStats->enabled());
    }
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
        services.setPropertyColumns=[](int mode){if(propertyColumns)propertyColumns->setMode(mode);};
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
        services.healthPlacement=[]{return healthStats?int(healthStats->placement()):0;};
        services.setHealthPlacement=[](int value){return healthStats&&healthStats->setPlacement(static_cast<augment::HealthPlacement>(value));};
        services.healthStatsEnabled=[]{return healthStats&&healthStats->enabled();};
        services.setHealthStats=[](bool enabled){
            if(!healthStats||!healthStats->setEnabled(enabled))return false;
            setHealthResources(enabled);return true;
        };
        nativeSettings=std::make_unique<augment::ObsSettingsPage>(
            *controller,*dockTitleIcons,std::move(services));
    }
    uiActive=true;
    contextNotes=std::make_shared<augment::ContextNotes>(QFileInfo(stripConfigPath).dir().filePath("context-notes.json"));
    if(auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window()))){
        nativeNotes=std::make_unique<augment::NativeNotes>(main,contextNotes,openEntryNote);
        auto *dock=new augment::SourceInspectorDock(main,QFileInfo(stripConfigPath).dir().filePath("property-presets.json"),contextNotes);
        if(obs_frontend_add_dock_by_id("aftermarket-source-inspector-dock","Source Inspector",dock))sourceInspector=dock;
        else delete dock;
    }
    if(auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window())))
        commandCenter=std::make_unique<augment::CommandCenter>(main,
            []{return frontendReady&&uiActive?notesCatalog():QList<augment::QuickFindEntry>{};},
            [](const auto &hit){
                if(!frontendReady||!uiActive)return false;
                if(hit.kind==augment::QuickFindEntry::Source&&sourceInspector)return showSourceInspector(&hit);
                return augment::performObsQuickFind(hit,augment::QuickFind::Default);
            },
            []{
                QList<augment::CommandChoice> result;
                if(!frontendReady||!uiActive)return result;
                result.append({"workspace","Open Workspace","Aftermarket",false});
                result.append({"find","Find Sources","Aftermarket",false});
                if(sourceInspector)result.append({"inspect","Source Inspector","Aftermarket",false});
                if(nativeActions)for(const auto &key:nativeActions->enumerateHotkeys())
                    result.append({key.binding,key.label,key.group,true});
                return result;
            },
            [](const QString &id){
                if(!frontendReady||!uiActive)return false;
                using Command=augment::ToolsMenu::Command;
                if(id=="inspect")return showSourceInspector();
                if(id=="workspace"||id=="find"){
                    auto *action=toolsMenu?toolsMenu->action(id=="workspace"?Command::Workspace:Command::Find):nullptr;
                    if(!action||!action->isEnabled())return false;
                    action->trigger();return true;
                }
                return nativeActions&&nativeActions->triggerHotkey(id);
            },openEntryNote);
    if(toolsMenu)toolsMenu->setState(true,frontendReady);
    return true;
}
static void cleanupUi(){
    stopCustomUi();
    emergency.reset(); // remove key filter before deleting the tool actions
    toolsMenu.reset();
    cueDeck.reset();
    dockLayouts.reset();
}
static void frontendEvent(enum obs_frontend_event event, void *) {
    switch(event){
      case OBS_FRONTEND_EVENT_STREAMING_STARTING:case OBS_FRONTEND_EVENT_STREAMING_STOPPING:restartingOutputs|=1;break;
      case OBS_FRONTEND_EVENT_STREAMING_STARTED:case OBS_FRONTEND_EVENT_STREAMING_STOPPED:restartingOutputs&=~1u;break;
      case OBS_FRONTEND_EVENT_RECORDING_STARTING:case OBS_FRONTEND_EVENT_RECORDING_STOPPING:restartingOutputs|=2;break;
      case OBS_FRONTEND_EVENT_RECORDING_STARTED:case OBS_FRONTEND_EVENT_RECORDING_STOPPED:restartingOutputs&=~2u;break;
      case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING:case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING:restartingOutputs|=4;break;
      case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:restartingOutputs&=~4u;break;
      default:break;
    }
    if(nativeActions&&event!=OBS_FRONTEND_EVENT_EXIT)nativeActions->onFrontendEvent(event);
    if(nativeNotes&&event!=OBS_FRONTEND_EVENT_EXIT)nativeNotes->schedule();
    if(sourceInspector){
        if(event==OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING||event==OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP)
            sourceInspector->clearSelection();
        else if(event==OBS_FRONTEND_EVENT_SCENE_CHANGED||event==OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED||
                event==OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED||event==OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED||
                event==OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED)
            sourceInspector->refresh();
    }
    if(event==OBS_FRONTEND_EVENT_FINISHED_LOADING){
        frontendReady=true;
        if(toolsMenu)toolsMenu->setState(uiActive,frontendReady);
        if(actionStrip)actionStrip->refresh();
        if(uiActive&&controller&&controller->mixerBacksideEnabled()&&mixerRetry)
            mixerRetry->start();
    }
    if (event==OBS_FRONTEND_EVENT_EXIT) {
        restartRequest.exitAccepted([]{
            const QStringList args=QStringList{QString::number(QCoreApplication::applicationPid()),restartExecutable,restartCwd}+restartArgs;
            const bool launched=QProcess::startDetached(restartHelper,args,restartCwd);
            blog(launched?LOG_INFO:LOG_ERROR,"[aftermarket] Restart helper %s",launched?"launched":"could not launch");
            return launched;
        });
        frontendReady=false;
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
    restartHotkey=obs_hotkey_register_frontend("aftermarket.restart_obs","Aftermarket: Restart OBS",restartHotkeyCallback,nullptr);
    obs_frontend_add_save_callback(restartHotkeyState,nullptr);
    auto *main=static_cast<QWidget*>(obs_frontend_get_main_window());
    emergency=std::make_unique<augment::EmergencyRecovery>(
        QDir(folder).filePath("safe-mode.flag"),main,
        []{
            // Do not delete dock/menu widgets inside an active QMenu signal
            // or a keyboard event filter; the marker is already on disk.
            QTimer::singleShot(0,[] {
                restoreEmergencyUi();
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
    toolsMenu=std::make_unique<augment::ToolsMenu>(
        static_cast<QAction*>(obs_frontend_add_tools_menu_qaction("Aftermarket")));
    using Command=augment::ToolsMenu::Command;
    auto *workspaceAction=toolsMenu->action(Command::Workspace);
    if(workspaceAction)QObject::connect(workspaceAction,&QAction::triggered,controller,[]{
        if(!uiActive||!frontendReady)return;
        auto *main=qobject_cast<QMainWindow*>(static_cast<QWidget*>(obs_frontend_get_main_window()));
        if(!main)return;
        QPointer<QMainWindow> safeMain=main;
        if(!inspector)inspector=new augment::Inspector(*controller,main,
            panels.get(),propertySections.get(),setAudioMixerBackside,rowStripes.get(),compactBar.get(),actionStrip.get(),
            dockLayouts.get(),cueDeck.get(),
            [safeMain](QWidget *owner){
                if(!safeMain||!frontendReady||!uiActive)return;
                const auto folder=QFileInfo(stripConfigPath).absolutePath();
                augment::WorkspaceCheckpoints checkpoints(folder,safeMain,[folder]{return captureCheckpointScene(folder);});
                augment::WorkspaceCheckpoints::CompanionBackup companion;
                if(collectionManagerActionAvailable("backup_scene_collection"))
                    companion=[folder](const auto &saved){return backupCheckpointCompanion(folder,saved);};
                checkpoints.showManager(owner,companion);
            },collectionManagerActionAvailable("scene_collection_manager")?std::function<void()>([]{
                if(collectionManagerActionAvailable("scene_collection_manager"))
                    nativeActions->triggerHotkey(augment::NativeActionBridge::binding("frontend","","scene_collection_manager"));
            }):std::function<void()>{});
        inspector->show();inspector->raise();inspector->activateWindow();
    });
    if(auto *commandAction=toolsMenu->action(Command::CommandCenter))
        QObject::connect(commandAction,&QAction::triggered,controller,[]{
            QTimer::singleShot(0,[]{if(frontendReady&&uiActive&&commandCenter)commandCenter->open();});
        });
    auto *findAction=toolsMenu->action(Command::Find);
    blog(LOG_INFO,"[aftermarket] AFTERMARKET_QUICK_FIND_20261123 registered=%d",findAction?1:0);
    if(findAction)QObject::connect(findAction,&QAction::triggered,controller,[]{
        // The tools menu can still be closing when this signal arrives.
        QTimer::singleShot(0,[]{showQuickFind();});
    });
    if(auto *restartAction=toolsMenu->action(Command::Restart))
        QObject::connect(restartAction,&QAction::triggered,controller,[]{QTimer::singleShot(0,[]{requestRestart();});});
    auto *emergencyAction=toolsMenu->action(Command::Emergency);
    if(emergencyAction)QObject::connect(emergencyAction,&QAction::triggered,controller,[]{
        if(emergency&&!emergency->engage()&&!emergency->active())
            blog(LOG_ERROR,"[aftermarket] emergency marker could not be saved; no UI state changed");
    });
    auto *resumeAction=toolsMenu->action(Command::Resume);
    if(resumeAction)QObject::connect(resumeAction,&QAction::triggered,controller,[]{
        // A native Tools menu may still be closing; avoid reparenting it mid-signal.
        QTimer::singleShot(0,[]{
            if(emergency&&!emergency->resume())
                blog(LOG_ERROR,"[aftermarket] safe mode remains enabled; resume failed");
        });
    });
    if(emergency->active()){
        // Already suspended before any UI adapters were constructed.
        toolsMenu->setState(false,frontendReady);
        blog(LOG_WARNING,"[aftermarket] safe-mode.flag detected; Aftermarket UI disabled");
    }else startCustomUi();
    obs_frontend_add_event_callback(frontendEvent,nullptr);
    blog(LOG_INFO, "[aftermarket] recovery shortcut registered; rules: %s", qPrintable(file));
    return true;
}
MODULE_EXPORT void obs_module_unload(void) {
    obs_frontend_remove_event_callback(frontendEvent,nullptr);
    obs_frontend_remove_save_callback(quickFindHotkeyState,nullptr);
    obs_frontend_remove_save_callback(restartHotkeyState,nullptr);
    if(restartHotkey!=OBS_INVALID_HOTKEY_ID){obs_hotkey_unregister(restartHotkey);restartHotkey=OBS_INVALID_HOTKEY_ID;}
    if(quickFindHotkey!=OBS_INVALID_HOTKEY_ID){
        obs_hotkey_unregister(quickFindHotkey);
        quickFindHotkey=OBS_INVALID_HOTKEY_ID;
    }
    cleanupUi();
    delete controller; controller = nullptr;
    audio.reset();
    nativeActions.reset();
}
