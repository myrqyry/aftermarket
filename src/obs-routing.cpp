#include "obs-routing.h"
#include <obs.h>
#include <algorithm>

namespace augment {
namespace {
bool isAudioSource(obs_source_t *source) {
    return source && (obs_source_get_output_flags(source)&OBS_SOURCE_AUDIO) &&
           obs_source_get_type(source)!=OBS_SOURCE_TYPE_FILTER;
}
obs_source_t *sourceById(const QString &id) {
    if(id.isEmpty())return nullptr;
    const auto bytes=id.toUtf8();
    return obs_get_source_by_uuid(bytes.constData()); // owned ref
}
}
QVector<RouteSource> ObsRouting::list(){
    QVector<RouteSource> result;
    obs_enum_sources([](void *data,obs_source_t *source) {
        if(!isAudioSource(source))return true;
        const char *uuid=obs_source_get_uuid(source),*name=obs_source_get_name(source);
        if(!uuid||!*uuid||!name)return true;
        auto *routes=static_cast<QVector<RouteSource>*>(data);
        routes->append({QString::fromUtf8(uuid),QString::fromUtf8(name),
            obs_source_get_audio_mixers(source),
            static_cast<int>(obs_source_get_monitoring_type(source)),
            obs_source_muted(source),
            obs_source_enabled(source),
            obs_source_audio_active(source)});
        return true;
    },&result);
    std::sort(result.begin(),result.end(),[](const RouteSource &a,const RouteSource &b){
        const int byName=QString::localeAwareCompare(a.name,b.name);
        return byName==0?a.id<b.id:byName<0;
    });
    return result;
}
bool ObsRouting::toggleTrack(const QString &uuid,int track){
    if(track<1||track>6)return false;
    auto *source=sourceById(uuid);
    if(!source)return false;
    if(!isAudioSource(source)){obs_source_release(source);return false;}
    const uint32_t mask=obs_source_get_audio_mixers(source);
    obs_source_set_audio_mixers(source,mask^(1u<<(track-1)));
    obs_source_release(source);
    return true;
}
bool ObsRouting::setTrack(const QString &uuid,int track,bool connected){
    if(track<1||track>6)return false;
    auto *source=sourceById(uuid);
    if(!source)return false;
    if(!isAudioSource(source)){obs_source_release(source);return false;}
    const uint32_t mask=obs_source_get_audio_mixers(source);
    const uint32_t bit=1u<<(track-1);
    const uint32_t next=connected?(mask|bit):(mask&~bit);
    // Connecting an already-present wire must never disconnect it.
    if(next!=mask)obs_source_set_audio_mixers(source,next);
    obs_source_release(source);
    return true;
}
bool ObsRouting::setMonitoring(const QString &uuid,int mode){
    if(mode<OBS_MONITORING_TYPE_NONE||mode>OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT)return false;
    auto *source=sourceById(uuid);
    if(!source)return false;
    if(!isAudioSource(source)){obs_source_release(source);return false;}
    obs_source_set_monitoring_type(source,static_cast<obs_monitoring_type>(mode));
    obs_source_release(source);
    return true;
}
}
