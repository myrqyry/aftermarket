#pragma once
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QStringList>
#include <QHash>
#include <functional>
#include <optional>
#include <cstdint>
class QMainWindow;
class QToolButton;
namespace augment {
struct HealthSample {
    uint64_t rendered=0,renderMissed=0,encoded=0,encodeSkipped=0;
    bool valid=true,stream=false,reconnecting=false;
    uintptr_t streamId=0;
    uint64_t networkDropped=0;
    std::optional<double> obsCpu,systemCpu,gpu,renderBudget,congestion;
    QString gpuLabel;
};
class HealthPolicy {
public:
    QStringList update(const HealthSample&);
    void reset(){previous_.reset();states_.clear();}
private:
    struct State {int high=0,hold=0;};
    QHash<QString,State> states_;
    std::optional<HealthSample> previous_;
};
class HealthStats final:public QObject {
public:
    using Provider=std::function<HealthSample()>;
    HealthStats(QMainWindow*,QString settingsPath,Provider,bool automatic=true);
    ~HealthStats() override;
    bool enabled()const{return enabled_;}
    bool setEnabled(bool);
    void poll();
private:
    QPointer<QToolButton> button_;
    QTimer timer_;
    QString settingsPath_;
    Provider provider_;
    HealthPolicy policy_;
    bool enabled_=true,automatic_=true;
};
}
