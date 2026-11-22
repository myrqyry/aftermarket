#include "obs-signal-levels.h"
#include <obs.h>
#include <obs-audio-controls.h>
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <algorithm>
#include <cmath>
namespace augment {
namespace {
qint64 nowMs(){
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
struct Probe {
    obs_weak_source_t *weak=nullptr;
    obs_volmeter_t *meter=nullptr;
    std::atomic<float> peakDb{-100.f};
    std::atomic<qint64> updated{0};
    static void callback(void *user,const float[MAX_AUDIO_CHANNELS],
                         const float peaks[MAX_AUDIO_CHANNELS],const float[MAX_AUDIO_CHANNELS]){
        auto *self=static_cast<Probe*>(user);
        float peak=-100.f;
        for(size_t i=0;i<MAX_AUDIO_CHANNELS;++i)
            if(std::isfinite(peaks[i]))peak=std::max(peak,peaks[i]);
        self->peakDb.store(peak,std::memory_order_relaxed);
        self->updated.store(nowMs(),std::memory_order_release);
    }
    explicit Probe(obs_source_t *source){
        weak=obs_source_get_weak_source(source);
        meter=obs_volmeter_create(OBS_FADER_LOG);
        if(meter){
            if(obs_volmeter_attach_source(meter,source))
                obs_volmeter_add_callback(meter,callback,this);
            else {obs_volmeter_destroy(meter);meter=nullptr;}
        }
    }
    ~Probe(){
        if(meter){
            obs_volmeter_remove_callback(meter,callback,this);
            obs_volmeter_destroy(meter);
        }
        if(weak)obs_weak_source_release(weak);
    }
};
}
struct ObsSignalLevels::State {
    std::map<QString,std::unique_ptr<Probe>> probes;
};
ObsSignalLevels::ObsSignalLevels():state(new State){}
ObsSignalLevels::~ObsSignalLevels()=default;
QHash<QString,SignalReading> ObsSignalLevels::sample(const QVector<RouteSource> &routes){
    QHash<QString,SignalReading> result;
    const auto now=nowMs();
    for(const auto &route:routes){
        if(route.id.isEmpty())continue;
        auto it=state->probes.find(route.id);
        if(it!=state->probes.end()){
            auto *source=it->second->weak?obs_weak_source_get_source(it->second->weak):nullptr;
            const bool valid=source&&!obs_source_removed(source)&&
                QString::fromUtf8(obs_source_get_uuid(source))==route.id;
            if(source)obs_source_release(source);
            if(!valid){
                state->probes.erase(it);
                it=state->probes.end();
            }
        }
        if(it==state->probes.end()){
            auto *source=obs_get_source_by_uuid(route.id.toUtf8().constData());
            if(!source)continue;
            if(obs_source_removed(source)||!(obs_source_get_output_flags(source)&OBS_SOURCE_AUDIO)){
                obs_source_release(source);continue;
            }
            auto probe=std::make_unique<Probe>(source);
            obs_source_release(source);
            if(!probe->meter||!probe->weak)continue;
            it=state->probes.emplace(route.id,std::move(probe)).first;
        }
        const auto updated=it->second->updated.load(std::memory_order_acquire);
        // Fresh peak from real audio. Silence is a known measured state;
        // stale taps are unknown (rather than falsely claiming silence).
        const bool known=updated>0&&(now-updated)<=650;
        const bool audible=known&&it->second->peakDb.load(std::memory_order_relaxed)>-54.f;
        result.insert(route.id,{known,audible});
    }
    for(auto it=state->probes.begin();it!=state->probes.end();){
        if(!result.contains(it->first))it=state->probes.erase(it);
        else ++it;
    }
    return result;
}
}
