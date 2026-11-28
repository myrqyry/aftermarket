#pragma once
#include <QStringList>
#include <functional>
namespace augment {
// Resume saved workspace instead of replaying one-shot startup actions.
inline QStringList restartArguments(const QStringList &args){
    QStringList result;
    for(int i=0;i<args.size();++i){
        const auto &arg=args[i];
        if(arg=="--profile"||arg=="--collection"||arg=="--scene"){if(i+1<args.size())++i;continue;}
        if(arg=="--startstreaming"||arg=="--startrecording"||arg=="--startreplaybuffer"||arg=="--startvirtualcam"||arg=="--minimize-to-tray")continue;
        result.append(arg);
    }
    return result;
}
// The accepted OBS EXIT callback is the only place allowed to launch.
class RestartRequest{
    bool pending_=false,inside_=false,launched_=false;
public:
    bool request(const std::function<bool()> &close){
        if(inside_)return false;
        inside_=pending_=true;launched_=false;
        close();
        pending_=inside_=false;
        return launched_;
    }
    bool exitAccepted(const std::function<bool()> &launch){
        if(!pending_)return false;
        pending_=false;
        launched_=launch();return launched_;
    }
};
}
