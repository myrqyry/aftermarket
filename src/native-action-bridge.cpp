#include "native-action-bridge.h"
#include <obs.h>
#include <obs-hotkey.h>
#include <obs-frontend-api.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QHash>
#include <algorithm>

namespace augment {
namespace {
qint64 nowMs(){
    QElapsedTimer timer;
    timer.start();
    return timer.msecsSinceReference();
}
}
void StatusClock::start(qint64 now){
    runningSince_=now;
    accumulated_=0;
    paused_=false;
    known_=true;
}
void StatusClock::pause(qint64 now){
    if(!known_||paused_)return;
    accumulated_+=qMax(qint64(0),now-runningSince_);
    paused_=true;
}
void StatusClock::resume(qint64 now){
    if(!known_||!paused_)return;
    runningSince_=now;
    paused_=false;
}
void StatusClock::stop(){known_=false;paused_=false;accumulated_=0;}
qint64 StatusClock::seconds(qint64 now)const{
    if(!known_)return -1;
    return (accumulated_+(paused_?0:qMax(qint64(0),now-runningSince_)))/1000;
}
NativeActionBridge::NativeActionBridge(){cpu_=os_cpu_usage_info_start();}
NativeActionBridge::~NativeActionBridge(){if(cpu_)os_cpu_usage_info_destroy(cpu_);}
void NativeActionBridge::onFrontendEvent(enum obs_frontend_event event){
    const auto now=nowMs();
    switch(event){
        case OBS_FRONTEND_EVENT_RECORDING_STARTED:recordClock_.start(now);break;
        case OBS_FRONTEND_EVENT_RECORDING_PAUSED:recordClock_.pause(now);break;
        case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:recordClock_.resume(now);break;
        case OBS_FRONTEND_EVENT_RECORDING_STOPPED:recordClock_.stop();lastRecord_=0;break;
        case OBS_FRONTEND_EVENT_STREAMING_STARTED:streamClock_.start(now);break;
        case OBS_FRONTEND_EVENT_STREAMING_STOPPED:streamClock_.stop();lastStream_=0;break;
        default:break;
    }
}
QString NativeActionBridge::binding(const QString &type,const QString &owner,const QString &name){
    if(name.isEmpty()||type.isEmpty()||(type!="frontend"&&owner.isEmpty()))return {};
    return "hotkey:v1:"+QString::fromUtf8(
        QJsonDocument(QJsonObject{{"registerer",type},{"owner",owner},{"name",name}})
        .toJson(QJsonDocument::Compact));
}
int NativeActionBridge::resolveUnique(const QList<RegisteredHotkey> &list,const QString &identity){
    if(identity.isEmpty())return -1;
    int result=-1;
    for(const auto &hotkey:list)if(hotkey.binding==identity){
        if(result!=-1)return -1;
        result=hotkey.id;
    }
    return result;
}
namespace {
struct FoundHotkey{
    obs_hotkey_id id=OBS_INVALID_HOTKEY_ID;
    HotkeyChoice choice;
};
QString identity(obs_hotkey_t *hotkey){
    const char *raw=obs_hotkey_get_name(hotkey);
    if(!raw||!*raw)return {};
    const QString name=QString::fromUtf8(raw);
    void *owner=obs_hotkey_get_registerer(hotkey);
    switch(obs_hotkey_get_registerer_type(hotkey)){
      case OBS_HOTKEY_REGISTERER_FRONTEND:
        return NativeActionBridge::binding("frontend","",name);
      case OBS_HOTKEY_REGISTERER_SOURCE:{
        auto *s=owner?obs_weak_source_get_source(static_cast<obs_weak_source_t*>(owner)):nullptr;
        if(!s)return {};
        const char *uuid=obs_source_get_uuid(s);
        const auto result=obs_source_removed(s)?QString{}:
            NativeActionBridge::binding("source",uuid?QString::fromUtf8(uuid):QString{},name);
        obs_source_release(s);return result;
      }
      case OBS_HOTKEY_REGISTERER_OUTPUT:{
        auto *o=owner?obs_weak_output_get_output(static_cast<obs_weak_output_t*>(owner)):nullptr;
        if(!o)return {};
        const char *id=obs_output_get_name(o);
        const auto result=NativeActionBridge::binding("output",id?QString::fromUtf8(id):QString{},name);
        obs_output_release(o);return result;
      }
      case OBS_HOTKEY_REGISTERER_ENCODER:{
        auto *o=owner?obs_weak_encoder_get_encoder(static_cast<obs_weak_encoder_t*>(owner)):nullptr;
        if(!o)return {};
        const char *id=obs_encoder_get_name(o);
        const auto result=NativeActionBridge::binding("encoder",id?QString::fromUtf8(id):QString{},name);
        obs_encoder_release(o);return result;
      }
      case OBS_HOTKEY_REGISTERER_SERVICE:{
        auto *o=owner?obs_weak_service_get_service(static_cast<obs_weak_service_t*>(owner)):nullptr;
        if(!o)return {};
        const char *id=obs_service_get_name(o);
        const auto result=NativeActionBridge::binding("service",id?QString::fromUtf8(id):QString{},name);
        obs_service_release(o);return result;
      }
    }
    return {};
}
QList<FoundHotkey> collected(){
    QList<FoundHotkey> result;
    obs_enum_hotkeys([](void *context,obs_hotkey_id id,obs_hotkey_t *hotkey){
        const auto binding=identity(hotkey);
        if(binding.isEmpty())return true;
        const char *description=obs_hotkey_get_description(hotkey);
        const char *name=obs_hotkey_get_name(hotkey);
        const QString readable=description&&*description?QString::fromUtf8(description):
            QString::fromUtf8(name?name:"");
        QString group;
        switch(obs_hotkey_get_registerer_type(hotkey)){
          case OBS_HOTKEY_REGISTERER_SOURCE:{
            auto *weak=static_cast<obs_weak_source_t*>(obs_hotkey_get_registerer(hotkey));
            auto *src=weak?obs_weak_source_get_source(weak):nullptr;
            const char *sourceName=src?obs_source_get_name(src):nullptr;
            group="Sources · "+QString::fromUtf8(sourceName&&*sourceName?sourceName:"(unnamed)");
            if(src)obs_source_release(src);
            break;
          }
          case OBS_HOTKEY_REGISTERER_OUTPUT:group="Outputs";break;
          case OBS_HOTKEY_REGISTERER_ENCODER:group="Encoders";break;
          case OBS_HOTKEY_REGISTERER_SERVICE:group="Services";break;
          default:group="Frontend & plugins";break;
        }
        static_cast<QList<FoundHotkey>*>(context)->append({id,{binding,readable,group}});
        return true;
    },&result);
    return result;
}
}
QList<HotkeyChoice> NativeActionBridge::enumerateHotkeys() const{
    auto found=collected();
    QHash<QString,int> counts;
    for(const auto &item:found)++counts[item.choice.binding];
    QList<HotkeyChoice> result;
    for(const auto &entry:found)if(counts[entry.choice.binding]==1)result.append(entry.choice);
    std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){
        const auto group=a.group.compare(b.group,Qt::CaseInsensitive);
        return group?group<0:a.label.compare(b.label,Qt::CaseInsensitive)<0;
    });
    return result;
}
bool NativeActionBridge::triggerHotkey(const QString &binding) const{
    const auto found=collected();
    QList<RegisteredHotkey> observed;
    for(const auto &h:found)observed.append({static_cast<int>(h.id),h.choice.binding});
    const int id=resolveUnique(observed,binding);
    if(id<0)return false;
    obs_hotkey_trigger_routed_callback(static_cast<obs_hotkey_id>(id),true);
    obs_hotkey_trigger_routed_callback(static_cast<obs_hotkey_id>(id),false);
    return true;
}
StatusSnapshot NativeActionBridge::snapshot(){
    StatusSnapshot status;
    status.recording=obs_frontend_recording_active();
    status.streaming=obs_frontend_streaming_active();
    status.fps=obs_get_active_fps();
    status.cpu=cpu_?qMax(0.0,os_cpu_usage_info_query(cpu_)):0;
    status.lagged=obs_get_lagged_frames();
    const auto now=nowMs();
    status.recordSeconds=status.recording?recordClock_.seconds(now):-1;
    status.streamSeconds=status.streaming?streamClock_.seconds(now):-1;
    quint64 streamBytes=0,recordBytes=0;
    if(auto *output=obs_frontend_get_streaming_output()){
        if(status.streaming){
            streamBytes=obs_output_get_total_bytes(output);
            status.dropped=qMax(0,obs_output_get_frames_dropped(output));
        }
        obs_output_release(output);
    }
    if(auto *output=obs_frontend_get_recording_output()){
        if(status.recording)recordBytes=obs_output_get_total_bytes(output);
        obs_output_release(output);
    }
    const qint64 delta=now-lastMs_;
    if(lastMs_>0&&delta>0){
        // bytes are cumulative per output, so counters resetting are
        // expected when a new stream/recording starts.
        if(status.streaming&&streamBytes>=lastStream_)
            status.streamKbps=(streamBytes-lastStream_)*8.0/delta;
        if(status.recording&&recordBytes>=lastRecord_)
            status.recordKbps=(recordBytes-lastRecord_)*8.0/delta;
    }
    lastMs_=now;
    lastStream_=streamBytes;
    lastRecord_=recordBytes;
    return status;
}
}
