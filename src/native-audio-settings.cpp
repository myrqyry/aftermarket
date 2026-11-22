#include "native-audio-settings.h"
#include <obs.h>
#include <QtGlobal>
#include <cmath>
#include <algorithm>
namespace augment {
namespace {
bool audioSource(obs_source_t *source){
    return source&&!obs_source_removed(source)&&
           (obs_source_get_output_flags(source)&OBS_SOURCE_AUDIO)&&
           obs_source_get_type(source)!=OBS_SOURCE_TYPE_FILTER;
}
obs_source_t *findSource(const QString &uuid){
    if(uuid.isEmpty())return nullptr;
    auto *source=obs_get_source_by_uuid(uuid.toUtf8().constData()); // owned
    if(!audioSource(source)){if(source)obs_source_release(source);return nullptr;}
    return source;
}
}
QList<AudioPropertyState> NativeAudioSettings::list(){
    QList<AudioPropertyState> result;
    obs_enum_sources([](void *data,obs_source_t *source){
        if(!audioSource(source))return true;
        const auto uuid=obs_source_get_uuid(source);
        const auto name=obs_source_get_name(source);
        if(!uuid||!*uuid||!name||!*name)return true;
        const auto offset=obs_source_get_sync_offset(source);
        const int ms=qBound(-999999,int(std::llround(double(offset)/1000000.0)),999999);
        static_cast<QList<AudioPropertyState>*>(data)->append({
            QString::fromUtf8(uuid),QString::fromUtf8(name),
            qBound(0.0,double(obs_source_get_balance_value(source)),1.0),ms,
            obs_source_get_speaker_layout(source)!=SPEAKERS_MONO
        });
        return true;
    },&result);
    return result;
}
bool NativeAudioSettings::setPan(const QString &uuid,double balance){
    if(!std::isfinite(balance)||balance<0||balance>1)return false;
    auto *source=findSource(uuid);
    if(!source)return false;
    if(obs_source_get_speaker_layout(source)==SPEAKERS_MONO){
        obs_source_release(source);return false;
    }
    if(std::abs(double(obs_source_get_balance_value(source))-balance)>0.0001)
        obs_source_set_balance_value(source,float(balance));
    obs_source_release(source);
    return true;
}
bool NativeAudioSettings::setOffsetMs(const QString &uuid,int ms){
    if(ms<-999999||ms>999999)return false;
    auto *source=findSource(uuid);
    if(!source)return false;
    const int64_t nanoseconds=int64_t(ms)*1000000LL;
    if(obs_source_get_sync_offset(source)!=nanoseconds)
        obs_source_set_sync_offset(source,nanoseconds);
    obs_source_release(source);
    return true;
}
}
