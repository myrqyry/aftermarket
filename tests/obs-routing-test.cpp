#include "obs-routing.h"
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
    obs_source_set_monitoring_type(source,OBS_MONITORING_TYPE_MONITOR_ONLY);
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
    REQUIRE(record.tracks==(1u | (1u << 3)));
    REQUIRE(record.monitor==OBS_MONITORING_TYPE_MONITOR_ONLY);
    REQUIRE(record.muted);
    REQUIRE(!record.enabled);
    REQUIRE(record.audioActive==obs_source_audio_active(source));
    REQUIRE(augment::ObsRouting::setTrack(uuid,4,true));
    REQUIRE(obs_source_get_audio_mixers(source)==(1u | (1u << 3)));
    REQUIRE(augment::ObsRouting::setTrack(uuid,6,true));
    REQUIRE(obs_source_get_audio_mixers(source)==(1u | (1u << 3) | (1u << 5)));
    REQUIRE(augment::ObsRouting::setMonitoring(uuid,OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT));
    REQUIRE(obs_source_get_monitoring_type(source)==OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
    obs_source_set_name(source,"Renamed patchbay microphone");
    record=matches(augment::ObsRouting::list());
    REQUIRE(record.id==uuid&&record.name=="Renamed patchbay microphone");
    REQUIRE(record.muted&&!record.enabled);
    REQUIRE(!augment::ObsRouting::setTrack("stale-uuid",1,true));
    REQUIRE(!augment::ObsRouting::setMonitoring("stale-uuid",0));
    obs_source_remove(source);
    obs_source_release(source);
    obs_shutdown();
    std::cout<<"OBS bridge read-only source status, UUID routing and monitoring verified\n";
    return 0;
}
