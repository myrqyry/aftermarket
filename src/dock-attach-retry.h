#pragma once
#include <QObject>
#include <QTimer>
#include <functional>
namespace augment {
// Bounded, cancelable attempt to attach to a plugin/dock that may initialize
// after OBS_FRONTEND_EVENT_FINISHED_LOADING. Never modifies the dock itself.
class DockAttachRetry final : public QObject {
public:
    explicit DockAttachRetry(std::function<bool()> attach,int maxAttempts=8,
                             int intervalMs=2000,QObject *parent=nullptr);
    bool start();
    bool attemptNow();
    void cancel();
    bool pending() const;
    int attempts() const;
private:
    std::function<bool()> attach_;
    QTimer timer_;
    int attempts_=0,maxAttempts_=8;
    bool pending_=false;
};
}
