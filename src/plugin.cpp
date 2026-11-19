#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QPointer>
#include "inspector.h"
#include <util/bmem.h>
#include <QApplication>
#include <QThread>
#include <QDir>
#include <QFileInfo>
#include "augment.h"
OBS_DECLARE_MODULE()
static augment::Controller *controller = nullptr;
static QPointer<augment::Inspector> inspector;
static QPointer<QAction> toolsAction;
static void cleanupUi() {
    delete inspector.data();
    if (toolsAction) {
        // Remove our callback and menu entry before unloading the plugin code.
        delete toolsAction.data();
    }
}
static void frontendEvent(enum obs_frontend_event event, void *) {
    if (event==OBS_FRONTEND_EVENT_EXIT) cleanupUi();
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
    toolsAction=static_cast<QAction*>(obs_frontend_add_tools_menu_qaction("UI Augment Inspector"));
    if (toolsAction) QObject::connect(toolsAction.data(),&QAction::triggered,controller,[]{
        if (!inspector) inspector=new augment::Inspector(*controller,
            static_cast<QWidget*>(obs_frontend_get_main_window()));
        inspector->show();
        inspector->raise();
        inspector->activateWindow();
    });
    obs_frontend_add_event_callback(frontendEvent,nullptr);
    blog(LOG_INFO, "[ui-augment] menu instrumentation active; rules: %s", qPrintable(file));
    return true;
}
MODULE_EXPORT void obs_module_unload(void) {
    obs_frontend_remove_event_callback(frontendEvent,nullptr);
    cleanupUi();
    delete controller; controller = nullptr;
}
