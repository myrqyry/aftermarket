#include "obs-audio.h"
#include <obs.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <obs-audio-controls.h>
#include <QAbstractButton>
#include <QTimer>
#include <QWidget>
#include <atomic>
#include <chrono>
#include <map>
#include <limits>
namespace augment {
namespace {
qint64 milliseconds() {
    using namespace std::chrono;
    return duration_cast<std::chrono::milliseconds>(steady_clock::now().time_since_epoch()).count();
}
struct Probe {
    obs_weak_source_t *weak=nullptr;
    obs_volmeter_t *meter=nullptr;
    std::atomic<float> magnitude{0},peakDb{-100};
    obs_peak_meter_type peakType=SAMPLE_PEAK_METER;
    std::atomic<qint64> updated{0};
    qint64 requested=0;
    static void callback(void *data,const float db[MAX_AUDIO_CHANNELS],
                        const float peaks[MAX_AUDIO_CHANNELS],const float[MAX_AUDIO_CHANNELS]) {
        auto *p=static_cast<Probe*>(data);
        float level=0;
        for(size_t i=0;i<MAX_AUDIO_CHANNELS;++i)level=std::max(level,audioLevelFromDb(db[i]));
        float peak=-100.f;
        for(size_t i=0;i<MAX_AUDIO_CHANNELS;++i)
            if(std::isfinite(peaks[i]))peak=std::max(peak,peaks[i]);
        p->peakDb.store(peak,std::memory_order_relaxed);
        p->magnitude.store(level,std::memory_order_relaxed);
        p->updated.store(milliseconds(),std::memory_order_release);
    }
    explicit Probe(obs_source_t *source) {
        weak=obs_source_get_weak_source(source);
        meter=obs_volmeter_create(OBS_FADER_LOG);
        if(meter) {
            if(obs_volmeter_attach_source(meter,source))
                obs_volmeter_add_callback(meter,callback,this);
            else {obs_volmeter_destroy(meter);meter=nullptr;}
        }
    }
    ~Probe() {
        if(meter) {
            obs_volmeter_remove_callback(meter,callback,this);
            obs_volmeter_destroy(meter);
        }
        if(weak)obs_weak_source_release(weak);
    }
};
QString sourceName(const QWidget *widget) {
    // OBS 32.2's VolumeName holds the full, unelided QAbstractButton text.
    // Unlike VolumeControl::objectName, it is updated upon source rename.
    for(auto *w=widget;w;w=w->parentWidget()) {
        if(!w->inherits("VolumeControl"))continue;
        for(auto *button:w->findChildren<QAbstractButton*>())
            if(button->inherits("VolumeName"))return button->text();
        return {};
    }
    return {};
}
}
struct ObsAudio::State {
    std::map<QString,std::unique_ptr<Probe>> probes;
    QTimer sweep;
};
ObsAudio::ObsAudio():state(new State) {
    state->sweep.setInterval(1000);
    connect(&state->sweep,&QTimer::timeout,this,[this]{
        const auto now=milliseconds();
        for(auto it=state->probes.begin();it!=state->probes.end();) {
            if(now-it->second->requested>1200)it=state->probes.erase(it);
            else ++it;
        }
        if(state->probes.empty())state->sweep.stop();
    });
}
ObsAudio::~ObsAudio(){clear();}
void ObsAudio::clear(){state->sweep.stop();state->probes.clear();}
std::optional<float> ObsAudio::level(const QWidget *widget) {
    const auto r=sample(widget);
    if(!r)return {};
    return r->magnitude;
}
std::optional<MeterReading> ObsAudio::sample(const QWidget *widget) {
    const auto name=sourceName(widget);
    if(name.isEmpty())return {};
    const auto utf8=name.toUtf8();
    auto it=state->probes.find(name);
    if(it!=state->probes.end()) {
        auto *s=obs_weak_source_get_source(it->second->weak);
        const bool valid=s&&!obs_source_removed(s)&&QString::fromUtf8(obs_source_get_name(s))==name;
        if(s)obs_source_release(s);
        if(!valid){state->probes.erase(it);it=state->probes.end();}
    }
    if(it==state->probes.end()) {
        auto *source=obs_get_source_by_name(utf8.constData());
        if(!source)return {};
        if(!(obs_source_get_output_flags(source)&OBS_SOURCE_AUDIO)||obs_source_removed(source)) {
            obs_source_release(source);return {};
        }
        auto probe=std::make_unique<Probe>(source);
        obs_source_release(source);
        if(!probe->meter||!probe->weak)return {};
        blog(LOG_INFO,"[aftermarket] audio envelope attached to %s",utf8.constData());
        it=state->probes.emplace(name,std::move(probe)).first;
        if(!state->sweep.isActive())state->sweep.start();
    }
    auto &probe=*it->second;
    const auto now=milliseconds();
    probe.requested=now;
    if(auto *config=obs_frontend_get_profile_config()) {
        const auto type=config_get_uint(config,"Audio","PeakMeterType")==1?TRUE_PEAK_METER:SAMPLE_PEAK_METER;
        if(probe.peakType!=type){obs_volmeter_set_peak_meter_type(probe.meter,type);probe.peakType=type;}
    }
    auto *source=obs_weak_source_get_source(probe.weak);
    const bool muted=source&&obs_source_muted(source);
    const bool active=source&&!obs_source_removed(source)&&obs_source_audio_active(source);
    if(source)obs_source_release(source);
    if(!active||muted||now-probe.updated.load(std::memory_order_acquire)>450)return MeterReading{-100.f,muted,0.f};
    return MeterReading{probe.peakDb.load(std::memory_order_relaxed),false,probe.magnitude.load(std::memory_order_relaxed)};
}
}
