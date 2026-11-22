#pragma once
#include "audio-patchbay.h"
namespace augment {
struct ObsRouting {
    static QVector<RouteSource> list();
    static bool toggleTrack(const QString &uuid,int track);
    static bool setMonitoring(const QString &uuid,int mode);
};
}
