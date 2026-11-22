#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QPointer>
#include "inspector.h"
#include "panels.h"
#include "property-sections.h"
#include "row-stripes.h"
#include "dock-faces.h"
#include "audio-patchbay.h"
#include "obs-routing.h"
#include <QDockWidget>
#include <QMainWindow>
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
static QPointer<QAction> toolsAction, panelsAction;
static std::unique_ptr<augment::Panels> panels;
static std::unique_ptr<augment::PropertySections> propertySections;
static std::unique_ptr<augment::RowStripes> rowStripes;
static std::unique_ptr<augment::DockFaces> dockFaces;
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
        [](const QString &id,int mode){return augment::ObsRouting::setMonitoring(id,mode);}
    );
    if(!dockFaces->attach(dock,patchbay)){delete patchbay;return false;}
    return true;
}
static void cleanupUi() {
    delete inspector.data();
    if(panels)panels->restoreAll();
    dockFaces.reset();
    rowStripes.reset();
    propertySections.reset();
    transforms.reset(); // Restore original OBS accessibility colors before exit-save.
    panels.reset();
    delete panelsAction.data();
    if (toolsAction) {
        // Remove our callback and menu entry before unloading the plugin code.
        delete toolsAction.data();
    }
}
static void frontendEvent(enum obs_frontend_event event, void *) {
    if(event==OBS_FRONTEND_EVENT_FINISHED_LOADING&&controller&&controller->mixerBacksideEnabled())
        if(!setAudioMixerBackside(true))blog(LOG_WARNING,"[ui-augment] mixer backside unavailable in this layout");
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
    QDir().mkpath(QFileInfo(file).absolutePath());
    controller = new augment::Controller(file, nullptr, [](const QString &message) {
        blog(LOG_INFO, "[ui-augment] %s", qPrintable(message));
    });
    audio=std::make_unique<augment::ObsAudio>();
    controller->setAudioProvider([](const QWidget *w){return audio?audio->level(w):std::optional<float>{};},
        []{if(audio)audio->clear();});
    controller->setMeterProvider([](const QWidget *w){return audio?audio->sample(w):std::optional<augment::MeterReading>{};});
    propertySections=std::make_unique<augment::PropertySections>();
    propertySections->setEnabled(controller->collapsiblePropertyGroups());
    rowStripes=std::make_unique<augment::RowStripes>();
    rowStripes->configure(controller->listRowSettings());
    dockFaces=std::make_unique<augment::DockFaces>();
    toolsAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction("UI Augment Inspector"));
    if (toolsAction) QObject::connect(toolsAction.data(),&QAction::triggered,controller,[]{
        if (!inspector) inspector=new augment::Inspector(*controller,
            static_cast<QWidget*>(obs_frontend_get_main_window()),panels.get(),propertySections.get(),setAudioMixerBackside,rowStripes.get());
        inspector->show();
        inspector->raise();
        inspector->activateWindow();
    });
    panels=std::make_unique<augment::Panels>();
    transforms=std::make_unique<augment::ObsTransformStyle>(
        static_cast<QWidget*>(obs_frontend_get_main_window()),
        []{return controller&&controller->modernTransformFrame();});
    panelsAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction("UI Augment Panels"));
    if(panelsAction) QObject::connect(panelsAction.data(),&QAction::triggered,controller,[]{
        if(!inspector)inspector=new augment::Inspector(*controller,static_cast<QWidget*>(obs_frontend_get_main_window()),panels.get(),propertySections.get(),setAudioMixerBackside,rowStripes.get());
        inspector->openPanels();inspector->show();inspector->raise();inspector->activateWindow();
    });
    obs_frontend_add_event_callback(frontendEvent,nullptr);
    blog(LOG_INFO, "[ui-augment] menu instrumentation active; rules: %s", qPrintable(file));
    return true;
}
MODULE_EXPORT void obs_module_unload(void) {
    obs_frontend_remove_event_callback(frontendEvent,nullptr);
    cleanupUi();
    delete controller; controller = nullptr;
    audio.reset();
}
