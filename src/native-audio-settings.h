#pragma once
#include "front-audio-controls.h"
namespace augment {
// UUID-scoped bridge to OBS Advanced Audio Properties. Never stores a second mix.
struct NativeAudioSettings {
    static QList<AudioPropertyState> list();
    static bool setPan(const QString &uuid,double balance);
    static bool setOffsetMs(const QString &uuid,int milliseconds);
};
}
