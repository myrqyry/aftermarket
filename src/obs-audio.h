#pragma once
#include "audio-motion.h"
#include <QObject>
#include <memory>
namespace augment {
// GUI-thread adapter; the OBS audio callback only publishes an atomic envelope.
class ObsAudio final : public QObject {
public:
    ObsAudio();
    ~ObsAudio() override;
    std::optional<MeterReading> sample(const QWidget *);
    std::optional<float> level(const QWidget *);
    void clear();
private:
    struct State;
    std::unique_ptr<State> state;
};
}
