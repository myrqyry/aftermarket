#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
class QWidget;
namespace augment {
struct MeterReading { float peakDb=-100; bool muted=false; float magnitude=0; };
using MeterProvider = std::function<std::optional<MeterReading>(const QWidget *)>;
using AudioProvider = std::function<std::optional<float>(const QWidget *)>;
using AudioReset = std::function<void()>;
inline float audioLevelFromDb(float db) {
    if(!std::isfinite(db))return 0;
    return std::clamp((db+60.f)/54.f,0.f,1.f);
}
inline float smoothAudio(float current,float target,double dt) {
    target=std::isfinite(target)?std::clamp(target,0.f,1.f):0.f;
    const double tau=target>current?.055:.22;
    const float next=current+(target-current)*float(1-std::exp(-std::clamp(dt,0.,.25)/tau));
    return std::abs(next-target)<.008f?target:next;
}
}
