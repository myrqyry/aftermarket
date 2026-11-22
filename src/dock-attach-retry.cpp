#include "dock-attach-retry.h"
#include <QtGlobal>
namespace augment {
DockAttachRetry::DockAttachRetry(std::function<bool()> attach,int max,int interval,QObject *parent)
    :QObject(parent),attach_(std::move(attach)),maxAttempts_(qMax(1,max)){
    timer_.setSingleShot(true);
    timer_.setInterval(qMax(5,interval));
    connect(&timer_,&QTimer::timeout,this,[this]{attemptNow();});
}
bool DockAttachRetry::start(){
    if(pending_||!attach_)return false;
    attempts_=0;
    pending_=true;
    return attemptNow();
}
bool DockAttachRetry::attemptNow(){
    if(!pending_||!attach_)return false;
    timer_.stop();
    ++attempts_;
    if(attach_()){pending_=false;return true;}
    if(attempts_>=maxAttempts_){pending_=false;return false;}
    timer_.start();
    return false;
}
void DockAttachRetry::cancel(){pending_=false;timer_.stop();}
bool DockAttachRetry::pending() const{return pending_;}
int DockAttachRetry::attempts() const{return attempts_;}
}
