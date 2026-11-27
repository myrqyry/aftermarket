#include "health-resources.h"
#include <QDir>
#include <QFile>
#include <QLibrary>
#include <QMutexLocker>
#include <algorithm>
#include <array>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace augment {
namespace {
class ResourceProbe {
    uint64_t previousTotal=0,previousIdle=0;
    bool baseline=false;
    QLibrary nvml;
    using Device=void*;
    struct Utilization{unsigned gpu,memory;};
    using Init=int(*)();
    using Count=int(*)(unsigned*);
    using Handle=int(*)(unsigned,Device*);
    using Util=int(*)(Device,Utilization*);
    Init shutdown=nullptr;
    Util utilization=nullptr;
    std::array<Device,32> devices{};
    unsigned deviceCount=0;
public:
    ResourceProbe(){
#ifdef Q_OS_WIN
        nvml.setFileName("nvml");
#elif defined(Q_OS_LINUX)
        nvml.setFileNameAndVersion("nvidia-ml",1);
#else
        return;
#endif
        auto init=reinterpret_cast<Init>(nvml.resolve("nvmlInit_v2"));
        auto count=reinterpret_cast<Count>(nvml.resolve("nvmlDeviceGetCount_v2"));
        auto handle=reinterpret_cast<Handle>(nvml.resolve("nvmlDeviceGetHandleByIndex_v2"));
        auto query=reinterpret_cast<Util>(nvml.resolve("nvmlDeviceGetUtilizationRates"));
        auto close=reinterpret_cast<Init>(nvml.resolve("nvmlShutdown"));
        if(!init||!count||!handle||!query||!close||init()!=0)return;
        shutdown=close;utilization=query;
        unsigned n=0;if(count(&n)!=0)return;
        for(unsigned i=0;i<qMin(n,unsigned(devices.size()));++i){
            Device dev=nullptr;if(handle(i,&dev)==0&&dev)devices[deviceCount++]=dev;
        }
    }
    ~ResourceProbe(){if(shutdown)shutdown();}
    std::optional<double> cpu(){
        uint64_t total=0,idle=0;
#ifdef Q_OS_LINUX
        QFile file("/proc/stat");if(!file.open(QIODevice::ReadOnly))return {};
        const auto fields=file.readLine(512).simplified().split(' ');
        if(fields.size()<9||fields.first()!="cpu")return {};
        for(int i=1;i<=8;++i){bool ok=false;auto v=fields[i].toULongLong(&ok);if(!ok)return {};total+=v;if(i==4||i==5)idle+=v;}
#elif defined(Q_OS_WIN)
        FILETIME i,k,u;if(!GetSystemTimes(&i,&k,&u))return {};
        auto value=[](FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
        idle=value(i);total=value(k)+value(u);
#else
        return {};
#endif
        std::optional<double> result;
        if(baseline&&total>previousTotal&&idle>=previousIdle&&idle-previousIdle<=total-previousTotal)
            result=100.*(1.-double(idle-previousIdle)/double(total-previousTotal));
        previousTotal=total;previousIdle=idle;baseline=true;return result;
    }
    ResourceSample sample(){
        ResourceSample result;result.cpu=cpu();
        auto add=[&](double load,const QString &label){
            if(load<0||load>100)return;
            if(!result.gpu||load>*result.gpu){result.gpu=load;result.gpuLabel=label;}
        };
        if(utilization)for(unsigned i=0;i<deviceCount;++i){
            Utilization data{};if(utilization(devices[i],&data)==0)add(data.gpu,QString("NVIDIA %1").arg(i+1));
        }
#ifdef Q_OS_LINUX
        const QDir drm("/sys/class/drm");
        for(const auto &name:drm.entryList({"card*"},QDir::Dirs|QDir::NoDotAndDotDot)){
            QFile f(drm.filePath(name+"/device/gpu_busy_percent"));if(!f.open(QIODevice::ReadOnly))continue;
            bool ok=false;const int load=f.read(32).trimmed().toInt(&ok);if(ok)add(load,name);
        }
#endif
        return result;
    }
};
}
void HealthResources::stop(){
    {QMutexLocker lock(&mutex_);stopping_=true;wake_.wakeAll();}
    wait();
}
ResourceSample HealthResources::latest(){QMutexLocker lock(&mutex_);if(std::chrono::steady_clock::now()-sampled_>std::chrono::seconds(6))return {};return latest_;}
void HealthResources::run(){
    ResourceProbe probe;
    for(;;){
        {QMutexLocker lock(&mutex_);if(stopping_)break;}
        auto sample=probe.sample();
        QMutexLocker lock(&mutex_);
        if(stopping_)break;
        latest_=std::move(sample);sampled_=std::chrono::steady_clock::now();
        wake_.wait(&mutex_,2000);
        if(stopping_)break;
    }
}
}
