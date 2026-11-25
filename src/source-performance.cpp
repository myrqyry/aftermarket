#include "source-performance.h"
#include <obs.h>
#include <cstring>
namespace augment {
namespace {
constexpr auto type="net.nagater.obs-frame-interleave-filter.filter";
constexpr auto owner="aftermarket_frame_interleave";
struct Match{
    int count=0;
    bool external=false;
    obs_source_t *managed=nullptr;
    ~Match(){if(managed)obs_source_release(managed);}
    void collect(obs_source_t *source){
        obs_source_enum_filters(source,[](obs_source_t*,obs_source_t *filter,void *opaque){
            auto &m=*static_cast<Match*>(opaque);
            if(strcmp(obs_source_get_id(filter),type)!=0)return;
            ++m.count;
            auto *s=obs_source_get_settings(filter);
            const bool owned=s&&obs_data_get_bool(s,owner);
            if(s)obs_data_release(s);
            if(!owned)m.external=true;
            else if(!m.managed)m.managed=obs_source_get_ref(filter);
        },this);
    }
    bool blocked()const{return external||count>1;}
};
bool eligible(obs_source_t *source){
    return source&&!obs_source_removed(source)&&obs_source_get_type(source)==OBS_SOURCE_TYPE_INPUT&&
        (obs_source_get_output_flags(source)&OBS_SOURCE_ASYNC_VIDEO)==OBS_SOURCE_ASYNC_VIDEO;
}
}
bool SourcePerformance::available(){
    const char *id=nullptr;
    for(size_t i=0;obs_enum_filter_types(i,&id);++i)if(id&&strcmp(id,type)==0)return true;
    return false;
}
SourcePerformanceState SourcePerformance::inspect(const QString &uuid){
    SourcePerformanceState state;
    auto *source=obs_get_source_by_uuid(uuid.toUtf8().constData());
    if(!source)return state;
    state.eligible=available()&&eligible(source);
    if(state.eligible){
        Match m;m.collect(source);state.blocked=m.blocked();state.managed=m.managed!=nullptr;
        if(m.managed){
            auto *s=obs_source_get_settings(m.managed);
            state.intervalMs=int(obs_data_get_int(s,"interleave_ms"));
            state.enabled=obs_source_enabled(m.managed);obs_data_release(s);
        }
    }
    obs_source_release(source);return state;
}
bool SourcePerformance::setInterval(const QString &uuid,int ms){
    if(ms<0||ms>1000||!available())return false;
    auto *source=obs_get_source_by_uuid(uuid.toUtf8().constData());
    if(!eligible(source)){if(source)obs_source_release(source);return false;}
    bool ok=false;
    {
        Match m;m.collect(source);
        if(!m.blocked()){
            if(ms==0){
                if(m.managed){obs_source_filter_remove(source,m.managed);ok=obs_filter_get_parent(m.managed)!=source;}
                else ok=true;
            }else{
                auto *settings=obs_data_create();
                obs_data_set_int(settings,"interleave_ms",ms);
                if(m.managed){
                    obs_source_update(m.managed,settings);
                    obs_source_set_enabled(m.managed,true);
                    auto *actual=obs_source_get_settings(m.managed);
                    ok=obs_data_get_int(actual,"interleave_ms")==ms&&obs_source_enabled(m.managed);
                    obs_data_release(actual);
                }else{
                    obs_data_set_bool(settings,owner,true);
                    auto *filter=obs_source_create_private(type,"Aftermarket · Off-air frame limit",settings);
                    if(filter){
                        obs_source_filter_add(source,filter);
                        ok=obs_filter_get_parent(filter)==source;
                        obs_source_release(filter);
                    }
                }
                obs_data_release(settings);
            }
        }
    }
    obs_source_release(source);return ok;
}
}
