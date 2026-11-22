#pragma once
#include "audio-patchbay.h"
#include <QHash>
#include <QString>
#include <QVector>
#include <memory>
namespace augment {
// Scoped libobs meter taps for actual source signal (not merely source enabled).
// One meter per UUID while its AudioHub patchbay is alive; callbacks publish
// atomics only and never call Qt or modify routing/audio.
class ObsSignalLevels {
public:
    ObsSignalLevels();
    ~ObsSignalLevels();
    QHash<QString,SignalReading> sample(const QVector<RouteSource> &sources);
private:
    struct State;
    std::unique_ptr<State> state;
};
}
