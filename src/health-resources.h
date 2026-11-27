#pragma once
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <optional>
#include <chrono>
namespace augment {
struct ResourceSample {std::optional<double> cpu,gpu;QString gpuLabel;};
class HealthResources final:public QThread {
public:
    ~HealthResources()override{stop();}
    void stop();
    ResourceSample latest();
protected:
    void run()override;
private:
    QMutex mutex_;
    QWaitCondition wake_;
    bool stopping_=false;
    ResourceSample latest_;
    std::chrono::steady_clock::time_point sampled_{};
};
}
