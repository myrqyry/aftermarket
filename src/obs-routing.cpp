#include "obs-routing.h"
#include <obs.h>
#include <algorithm>
#include <cstring>
#include <cmath>

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
bool isMonitorFilter(obs_source_t *filter){
    const char *id=obs_source_get_unversioned_id(filter);
    return id&&std::strcmp(id,"audio_monitor")==0;
}
constexpr auto ownedKey="aftermarket_device_send";
QVector<DeviceSend> sends(obs_source_t *source){
    QVector<DeviceSend> result;
    obs_source_enum_filters(source,[](obs_source_t *,obs_source_t *filter,void *opaque){
        if(!isMonitorFilter(filter))return;
        auto *settings=obs_source_get_settings(filter);
        if(!settings)return;
        const QString device=QString::fromUtf8(obs_data_get_string(settings,"device"));
        if(!device.isEmpty()){
            static_cast<QVector<DeviceSend>*>(opaque)->append({
                device,obs_data_get_bool(settings,ownedKey),obs_source_enabled(filter),
                obs_data_get_double(settings,"volume"),obs_data_get_double(settings,"balance"),
                int(obs_data_get_int(settings,"delay"))});
        }
        obs_data_release(settings);
    },&result);
    return result;
}
}
bool ObsRouting::deviceMonitoringAvailable(){
    const char *id=nullptr;
    for(size_t i=0;obs_enum_filter_types(i,&id);++i)
        if(id&&std::strcmp(id,"audio_monitor")==0)return true;
    return false;
}
QVector<DeviceDestination> ObsRouting::deviceDestinations(){
    QVector<DeviceDestination> result;
    if(!deviceMonitoringAvailable()||!obs_get_audio())return result;
    obs_enum_audio_monitoring_devices([](void *opaque,const char *name,const char *id){
        if(!id||!*id||!name||!*name||std::strcmp(id,"default")==0)return true;
        auto &list=*static_cast<QVector<DeviceDestination>*>(opaque);
        const QString key=QString::fromUtf8(id);
        if(std::none_of(list.begin(),list.end(),[&](const auto &d){return d.id==key;}))
            list.append({key,QString::fromUtf8(name)});
        return true;
    },&result);
    return result;
}
bool ObsRouting::setDeviceSend(const QString &uuid,const QString &device,bool connected){
    if(device.isEmpty()||!deviceMonitoringAvailable())return false;
    auto *source=sourceById(uuid);
    if(!isAudioSource(source)||obs_source_removed(source)){
        if(source)obs_source_release(source);return false;
    }
    // Never mutate an existing independently-created Audio Monitor filter.
    struct Match {QString device;obs_source_t *managed=nullptr;bool external=false;};
    Match match{device};
    obs_source_enum_filters(source,[](obs_source_t *,obs_source_t *filter,void *opaque){
        auto &m=*static_cast<Match*>(opaque);
        if(!isMonitorFilter(filter))return;
        auto *settings=obs_source_get_settings(filter);
        if(settings&&QString::fromUtf8(obs_data_get_string(settings,"device"))==m.device){
            if(obs_data_get_bool(settings,ownedKey)&&!m.managed)m.managed=obs_source_get_ref(filter);
            else m.external=true;
        }
        if(settings)obs_data_release(settings);
    },&match);
    bool ok=false;
    if(match.managed){
        if(connected){
            if(!match.external){obs_source_set_enabled(match.managed,true);ok=obs_source_enabled(match.managed);}
        }else{obs_source_filter_remove(source,match.managed);ok=obs_filter_get_parent(match.managed)!=source;}
        obs_source_release(match.managed);
    }else if(!match.external&&connected){
        const auto devices=deviceDestinations();
        auto found=std::find_if(devices.begin(),devices.end(),[&](const auto &d){return d.id==device;});
        if(found!=devices.end()){
            // Reject direct same-device desktop-capture feedback.
            const QString type=QString::fromUtf8(obs_source_get_unversioned_id(source));
            auto *sourceSettings=obs_source_get_settings(source);
            const QString captured=sourceSettings?QString::fromUtf8(obs_data_get_string(sourceSettings,"device_id")):QString{};
            if(sourceSettings)obs_data_release(sourceSettings);
            if(!type.contains("output_capture")||(!captured.isEmpty()&&captured!=device&&captured!=device+".monitor"&&captured!="default")){
                auto *settings=obs_data_create();
                obs_data_set_string(settings,"device",device.toUtf8().constData());
                obs_data_set_string(settings,"deviceName",found->name.toUtf8().constData());
                obs_data_set_double(settings,"volume",50.0);
                obs_data_set_int(settings,"mute",1); // suppress inactive source; never link parent mute
                obs_data_set_bool(settings,"linked",false);
                obs_data_set_bool(settings,ownedKey,true);
                const auto title=(QString("Aftermarket send: ")+found->name).toUtf8();
                auto *filter=obs_source_create_private("audio_monitor",title.constData(),settings);
                obs_data_release(settings);
                if(filter){
                    obs_source_filter_add(source,filter);
                    ok=obs_filter_get_parent(filter)==source;
                    obs_source_release(filter);
                }
            }
        }
    }else if(!connected&&!match.external)ok=true;
    obs_source_release(source);
    return ok;
}
bool ObsRouting::setDeviceParameter(const QString &uuid,const QString &device,DeviceParameter parameter,double value){
    if(device.isEmpty()||!std::isfinite(value)||!deviceMonitoringAvailable())return false;
    const char *key=nullptr;
    switch(parameter){
    case DeviceParameter::Gain:if(value<0||value>100)return false;key="volume";break;
    case DeviceParameter::Pan:if(value< -1||value>1)return false;key="balance";break;
    case DeviceParameter::Delay:if(value<0||value>10000||std::floor(value)!=value)return false;key="delay";break;
    }
    if(!key)return false;
    auto *source=sourceById(uuid);
    if(!isAudioSource(source)||obs_source_removed(source)){
        if(source)obs_source_release(source);return false;
    }
    struct Match {QString device;obs_source_t *filter=nullptr;int count=0;};
    Match match{device};
    obs_source_enum_filters(source,[](obs_source_t*,obs_source_t *filter,void *opaque){
        auto &m=*static_cast<Match*>(opaque);
        if(!isMonitorFilter(filter))return;
        auto *settings=obs_source_get_settings(filter);
        if(settings&&QString::fromUtf8(obs_data_get_string(settings,"device"))==m.device){
            ++m.count;
            // Linked gain or source-mute mode may change parent source state
            // during an upstream update. Do not edit such filters here.
            if(obs_data_get_bool(settings,ownedKey)&&!obs_data_get_bool(settings,"linked")&&
               obs_data_get_int(settings,"mute")!=2&&!m.filter)m.filter=obs_source_get_ref(filter);
        }
        if(settings)obs_data_release(settings);
    },&match);
    bool ok=false;
    if(match.count==1&&match.filter){
        auto *settings=obs_data_create();
        if(parameter==DeviceParameter::Delay)obs_data_set_int(settings,key,int(value));
        else obs_data_set_double(settings,key,value);
        obs_source_update(match.filter,settings);
        obs_data_release(settings);
        auto *current=obs_source_get_settings(match.filter);
        if(current){
            ok=std::abs(obs_data_get_double(current,key)-value)<.001;
            obs_data_release(current);
        }
    }
    if(match.filter)obs_source_release(match.filter);
    obs_source_release(source);
    return ok;
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
            obs_source_audio_active(source),
            obs_source_get_unversioned_id(source)
                ?QString::fromUtf8(obs_source_get_unversioned_id(source)):QString{}});
        routes->last().deviceSends=sends(source);
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
