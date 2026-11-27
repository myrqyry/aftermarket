#include "obs-routing.h"
#include "native-audio-settings.h"
#include <obs.h>
#include <QCoreApplication>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(false)

namespace {
const char *name(void *) { return "Aftermarket test audio"; }
void *create(obs_data_t *,obs_source_t *) { return new int(1); }
void destroy(void *data) { delete static_cast<int*>(data); }
}

int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    REQUIRE(obs_startup("en-US",nullptr,nullptr));
    obs_source_info sourceInfo{};
    sourceInfo.id="aftermarket-routing-test-input";
    sourceInfo.type=OBS_SOURCE_TYPE_INPUT;
    sourceInfo.output_flags=OBS_SOURCE_AUDIO;
    sourceInfo.get_name=name;
    sourceInfo.create=create;
    sourceInfo.destroy=destroy;
    obs_register_source(&sourceInfo);
    obs_source_t *source=obs_source_create(sourceInfo.id,"Test patchbay microphone",nullptr,nullptr);
    REQUIRE(source);
    const QString uuid=QString::fromUtf8(obs_source_get_uuid(source));
    REQUIRE(!uuid.isEmpty());

    obs_source_set_audio_mixers(source,1u | (1u << 3));
    const bool legacy=(obs_get_version() >> 24)<33;
    REQUIRE(augment::ObsRouting::monitorOnlyAvailable()==legacy);
#if LIBOBS_API_MAJOR_VER >= 33
    obs_source_set_monitoring_enabled(source,true);
#else
    obs_source_set_monitoring_type(source,legacy?OBS_MONITORING_TYPE_MONITOR_ONLY:OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
#endif
    obs_source_set_muted(source,true);
    obs_source_set_enabled(source,false);
    auto list=augment::ObsRouting::list();
    auto matches=[&](const QVector<augment::RouteSource> &data) {
        for(const auto &item:data)if(item.id==uuid)return item;
        return augment::RouteSource{};
    };
    auto record=matches(list);
    REQUIRE(record.id==uuid);
    REQUIRE(record.name=="Test patchbay microphone");
    REQUIRE(record.sourceType==sourceInfo.id);
    REQUIRE(record.tracks==(1u | (1u << 3)));
    REQUIRE(record.monitor==(legacy?1:2));
    REQUIRE(record.muted);
    REQUIRE(!record.enabled);
    REQUIRE(record.audioActive==obs_source_audio_active(source));
    REQUIRE(augment::ObsRouting::setTrack(uuid,4,true));
    REQUIRE(obs_source_get_audio_mixers(source)==(1u | (1u << 3)));
    REQUIRE(augment::ObsRouting::setTrack(uuid,6,true));
    REQUIRE(obs_source_get_audio_mixers(source)==(1u | (1u << 3) | (1u << 5)));
    REQUIRE(augment::ObsRouting::setMonitoring(uuid,OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT));
#if LIBOBS_API_MAJOR_VER >= 33
    REQUIRE(obs_source_get_monitoring_enabled(source));
#else
    REQUIRE(obs_source_get_monitoring_type(source)==OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
#endif
    const auto tracksBeforeMonitor=obs_source_get_audio_mixers(source);
    REQUIRE(augment::ObsRouting::setMonitoring(uuid,1)==legacy);
    REQUIRE(matches(augment::ObsRouting::list()).monitor==(legacy?1:2));
    REQUIRE(obs_source_get_audio_mixers(source)==tracksBeforeMonitor);
    REQUIRE(augment::ObsRouting::setMonitoring(uuid,0));
    REQUIRE(matches(augment::ObsRouting::list()).monitor==0);
    REQUIRE(obs_source_get_audio_mixers(source)==tracksBeforeMonitor);
    REQUIRE(augment::ObsRouting::setMonitoring(uuid,2));
    REQUIRE(matches(augment::ObsRouting::list()).monitor==2);
    obs_source_set_name(source,"Renamed patchbay microphone");
    record=matches(augment::ObsRouting::list());
    REQUIRE(record.id==uuid&&record.name=="Renamed patchbay microphone");
    REQUIRE(record.muted&&!record.enabled);
    REQUIRE(!augment::ObsRouting::setTrack("stale-uuid",1,true));
    REQUIRE(!augment::ObsRouting::setMonitoring("stale-uuid",0));
    // These are the real Advanced Audio Properties values, not a separate
    // model saved by Aftermarket. Native values survive a source rename.
    REQUIRE(augment::NativeAudioSettings::setPan(uuid,.8));
    REQUIRE(qAbs(obs_source_get_balance_value(source)-.8f)<.0001f);
    REQUIRE(augment::NativeAudioSettings::setOffsetMs(uuid,-175));
    REQUIRE(obs_source_get_sync_offset(source)==-175000000LL);
    auto props=augment::NativeAudioSettings::list();
    bool matched=false;
    for(const auto &item:props)if(item.id==uuid){
        REQUIRE(item.name=="Renamed patchbay microphone");
        REQUIRE(qAbs(item.balance-.8)<.001);
        REQUIRE(item.offsetMs==-175);
        matched=true;
    }
    REQUIRE(matched);
    REQUIRE(!augment::NativeAudioSettings::setPan("not-a-source",.5));
    REQUIRE(!augment::NativeAudioSettings::setPan(uuid,-.1));
    REQUIRE(!augment::NativeAudioSettings::setOffsetMs("not-a-source",0));

    // Missing optional backend must never create a placeholder filter.
    REQUIRE(!augment::ObsRouting::deviceMonitoringAvailable());
    REQUIRE(!augment::ObsRouting::setDeviceSend(uuid,"test-sink",true));
    // A real libobs registered filter exercises creation, settings, and ownership.
    obs_source_info filterInfo=sourceInfo;
    filterInfo.id="audio_monitor";filterInfo.type=OBS_SOURCE_TYPE_FILTER;
    obs_register_source(&filterInfo);
    REQUIRE(augment::ObsRouting::deviceMonitoringAvailable());
    REQUIRE(!augment::ObsRouting::setDeviceSend(uuid,"nonexistent-sink",true));
    auto *settings=obs_data_create();obs_data_set_string(settings,"device","external-sink");
    auto *external=obs_source_create_private("audio_monitor","External monitor",settings);
    obs_data_release(settings);REQUIRE(external);
    obs_source_filter_add(source,external);
    record=matches(augment::ObsRouting::list());
    REQUIRE(record.deviceSends.size()==1&&record.deviceSends[0].deviceId=="external-sink");
    REQUIRE(!record.deviceSends[0].managed);
    REQUIRE(!augment::ObsRouting::setDeviceSend(uuid,"external-sink",false));
    REQUIRE(!augment::ObsRouting::setDeviceParameter(uuid,"external-sink",augment::DeviceParameter::Gain,20));
    REQUIRE(obs_filter_get_parent(external)==source);
    obs_source_filter_remove(source,external);obs_source_release(external);
    // Real OBS filter settings store independently editable send parameters.
    settings=obs_data_create();
    obs_data_set_string(settings,"device","managed-sink");
    obs_data_set_bool(settings,"aftermarket_device_send",true);
    obs_data_set_double(settings,"volume",50);
    auto *managed=obs_source_create_private("audio_monitor","Managed monitor",settings);
    obs_data_release(settings);REQUIRE(managed);
    obs_source_filter_add(source,managed);
    const auto nativeGain=obs_source_get_volume(source);
    REQUIRE(augment::ObsRouting::setDeviceParameter(uuid,"managed-sink",augment::DeviceParameter::Gain,35));
    REQUIRE(augment::ObsRouting::setDeviceParameter(uuid,"managed-sink",augment::DeviceParameter::Pan,-.4));
    REQUIRE(augment::ObsRouting::setDeviceParameter(uuid,"managed-sink",augment::DeviceParameter::Delay,125));
    REQUIRE(!augment::ObsRouting::setDeviceParameter(uuid,"managed-sink",augment::DeviceParameter::Gain,150));
    record=matches(augment::ObsRouting::list());
    REQUIRE(record.deviceSends.size()==1&&record.deviceSends[0].managed);
    REQUIRE(qAbs(record.deviceSends[0].volume-35)<.001);
    REQUIRE(qAbs(record.deviceSends[0].balance+.4)<.001&&record.deviceSends[0].delayMs==125);
    REQUIRE(obs_source_get_volume(source)==nativeGain&&obs_source_muted(source));
    REQUIRE(augment::ObsRouting::setDeviceParameter(uuid,"managed-sink",augment::DeviceParameter::Muted,1));
    auto *actual=obs_source_get_settings(managed);
    REQUIRE(obs_data_get_double(actual,"volume")==0);
    obs_data_release(actual);
    record=matches(augment::ObsRouting::list());
    REQUIRE(record.deviceSends.size()==1&&record.deviceSends[0].muted&&record.deviceSends[0].volume==35);
    // Scene activity can re-enable a filter, but must not restore muted gain.
    obs_source_set_enabled(managed,false);obs_source_set_enabled(managed,true);
    REQUIRE(augment::ObsRouting::setDeviceParameter(uuid,"managed-sink",augment::DeviceParameter::Gain,42));
    actual=obs_source_get_settings(managed);
    REQUIRE(obs_data_get_double(actual,"volume")==0);
    obs_data_release(actual);
    REQUIRE(augment::ObsRouting::setDeviceParameter(uuid,"managed-sink",augment::DeviceParameter::Muted,0));
    actual=obs_source_get_settings(managed);
    REQUIRE(obs_data_get_double(actual,"volume")==42);
    obs_data_release(actual);
    REQUIRE(obs_source_muted(source));
    REQUIRE(!matches(augment::ObsRouting::list()).deviceSends[0].muted);
    REQUIRE(!augment::ObsRouting::setDeviceParameter(uuid,"managed-sink",augment::DeviceParameter::Muted,.5));
    REQUIRE(augment::ObsRouting::setDeviceSend(uuid,"managed-sink",false));
    REQUIRE(matches(augment::ObsRouting::list()).deviceSends.isEmpty());
    obs_source_release(managed);

    obs_source_remove(source);
    obs_source_release(source);
    obs_queue_task(OBS_TASK_DESTROY, [](void *){}, nullptr, true); // headless destroy-queue barrier
    obs_shutdown();
    std::cout<<"OBS bridge read-only source status, UUID routing and monitoring verified\n";
    return 0;
}
