#pragma once
#include "audio-patchbay.h"
namespace augment {
struct ObsRouting {
    static QVector<RouteSource> list();
    static bool deviceMonitoringAvailable();
    static QVector<DeviceDestination> deviceDestinations();
    static bool setDeviceParameter(const QString &uuid,const QString &device,DeviceParameter parameter,double value);
    static bool setDeviceSend(const QString &uuid,const QString &device,bool connected);
    static bool toggleTrack(const QString &uuid,int track);
    static bool setTrack(const QString &uuid,int track,bool connected);
    static bool setMonitoring(const QString &uuid,int mode);
};
}
