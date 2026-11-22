#pragma once
#include "action-strip.h"
#include <QList>
#include <QString>
#include <QtGlobal>
#include <obs-frontend-api.h>
#include <util/platform.h>
namespace augment {
struct RegisteredHotkey {
    int id=-1;
    QString binding;
};
// Monotonic time model: never invent elapsed time when a stream/recording
// predates plugin initialization. Recording pause does not count as elapsed.
class StatusClock {
public:
    void start(qint64 now);
    void pause(qint64 now);
    void resume(qint64 now);
    void stop();
    qint64 seconds(qint64 now) const;
private:
    qint64 runningSince_=0,accumulated_=0;
    bool known_=false,paused_=false;
};
class NativeActionBridge {
public:
    NativeActionBridge();
    ~NativeActionBridge();
    static QString binding(const QString &kind,const QString &owner,const QString &name);
    static int resolveUnique(const QList<RegisteredHotkey> &observed,const QString &binding);
    QList<HotkeyChoice> enumerateHotkeys() const;
    bool triggerHotkey(const QString &binding) const;
    void onFrontendEvent(enum obs_frontend_event event);
    StatusSnapshot snapshot();
private:
    qint64 lastMs_=0;
    quint64 lastStream_=0,lastRecord_=0;
    StatusClock recordClock_,streamClock_;
    os_cpu_usage_info_t *cpu_=nullptr;
};
}
