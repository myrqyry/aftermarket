#include <obs-module.h>
#include <util/bmem.h>
#include <QApplication>
#include <QThread>
#include <QDir>
#include <QFileInfo>
#include "augment.h"
OBS_DECLARE_MODULE()
static augment::Controller *controller = nullptr;
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
    blog(LOG_INFO, "[ui-augment] menu instrumentation active; rules: %s", qPrintable(file));
    return true;
}
MODULE_EXPORT void obs_module_unload(void) {
    delete controller; controller = nullptr;
}
