#include "quick-find.h"
#include "source-performance.h"
#include <obs.h>
#include <obs-frontend-api.h>
#include <QHash>
#include <QSet>
#include <QPair>
#include <QStringList>
#include <algorithm>

namespace augment {
namespace {
QString utf(const char *p){return p?QString::fromUtf8(p):QString{};}
QString uuid(obs_source_t *source){return source?utf(obs_source_get_uuid(source)):QString{};}

struct Context {
    QHash<QString,QStringList> paths;
    QHash<QString,QString> sceneId;
    int visited=0;
};
struct Visit {
    Context *context;
    QString rootId;
    QString rootName;
    QStringList path;
    QSet<QString> ancestors;
    int depth=0;
};
void recurse(obs_scene_t *scene,Visit &visit);

bool sceneItemCallback(obs_scene_t *,obs_sceneitem_t *item,void *payload){
    auto &v=*static_cast<Visit*>(payload);
    if(++v.context->visited>25000)return false;
    auto *source=obs_sceneitem_get_source(item);
    if(!source)return true;
    const auto id=uuid(source);
    if(id.isEmpty())return true;
    const auto name=utf(obs_source_get_name(source));
    QStringList path=v.path;
    path.append(name);
    const QString route=path.join(" / ").left(300);
    auto &locations=v.context->paths[id];
    if(!locations.contains(route)&&locations.size()<16)locations.append(route);
    if(!v.context->sceneId.contains(id))v.context->sceneId[id]=v.rootId;
    if(v.depth>=10||v.ancestors.contains(id))return true;
    obs_scene_t *nested=obs_sceneitem_is_group(item)
        ?obs_sceneitem_group_get_scene(item)
        :obs_scene_from_source(source);
    if(nested&&nested!=obs_sceneitem_get_scene(item)){
        Visit child{v.context,v.rootId,v.rootName,path,v.ancestors,v.depth+1};
        child.ancestors.insert(id);
        recurse(nested,child);
    }
    return true;
}
void recurse(obs_scene_t *scene,Visit &v){
    if(scene)obs_scene_enum_items(scene,sceneItemCallback,&v);
}
QString localInput(obs_source_t *source){
    obs_data_t *settings=obs_source_get_settings(source);
    if(!settings)return {};
    QString detail;
    for(const auto *key:{"local_file","file","path","input","url"}){
        const auto value=utf(obs_data_get_string(settings,key));
        if(value.isEmpty()||value.size()>2048)continue;
        detail=value;
        break;
    }
    obs_data_release(settings);
    return detail;
}
struct Collector {
    QList<QuickFindEntry> entries;
    QSet<QString> seen;
    const Context *context=nullptr;
};
void addFilters(Collector &c,obs_source_t *source){
    const QString parent=uuid(source);
    if(parent.isEmpty())return;
    obs_source_enum_filters(source,[](obs_source_t *owner,obs_source_t *filter,void *param){
        auto *collector=static_cast<Collector*>(param);
        if(collector->entries.size()>=5000)return;
        const QString filterId=uuid(filter);
        const QString parentId=uuid(owner);
        if(filterId.isEmpty()||parentId.isEmpty())return;
        const QString key=parentId+"/"+filterId;
        if(collector->seen.contains(key))return;
        collector->seen.insert(key);
        collector->entries.append({QuickFindEntry::Filter,filterId,parentId,
             utf(obs_source_get_name(filter)),utf(obs_source_get_unversioned_id(filter)),{},
             utf(obs_source_get_name(owner)),{}});
    },&c);
}
void addSource(Collector &c,obs_source_t *source,bool scene){
    if(!source||c.entries.size()>=5000)return;
    const QString id=uuid(source);
    if(id.isEmpty()||c.seen.contains(id))return;
    c.seen.insert(id);
    const auto paths=c.context->paths.value(id);
    const QString location=paths.join(" · ").left(320);
    c.entries.append({scene?QuickFindEntry::Scene:QuickFindEntry::Source,id,{},
        utf(obs_source_get_name(source)),scene?"scene":utf(obs_source_get_unversioned_id(source)),
        scene?QString{}:localInput(source),location,c.context->sceneId.value(id),!scene&&
        (obs_source_get_output_flags(source)&OBS_SOURCE_ASYNC_VIDEO)==OBS_SOURCE_ASYNC_VIDEO&&SourcePerformance::available()});
    addFilters(c,source);
}
}

QList<QuickFindEntry> collectObsQuickFind(){
    Context context;
    obs_enum_scenes([](void *payload,obs_source_t *source){
        auto &c=*static_cast<Context*>(payload);
        Visit visit{&c,uuid(source),utf(obs_source_get_name(source)),
                    {utf(obs_source_get_name(source))},{uuid(source)},0};
        recurse(obs_scene_from_source(source),visit);
        return c.visited<=25000;
    },&context);
    Collector collector;collector.context=&context;
    obs_enum_scenes([](void *payload,obs_source_t *source){
        addSource(*static_cast<Collector*>(payload),source,true);
        return true;
    },&collector);
    obs_enum_sources([](void *payload,obs_source_t *source){
        addSource(*static_cast<Collector*>(payload),source,
                  obs_source_get_type(source)==OBS_SOURCE_TYPE_SCENE);
        return true;
    },&collector);
    return collector.entries;
}

bool performObsQuickFind(const QuickFindEntry &entry,QuickFind::Action action){
    if(action==QuickFind::PinSource||action==QuickFind::Performance)return false; // handled by the optional source-dock adapter
    if(action==QuickFind::Default)
        action=entry.kind==QuickFindEntry::Filter?QuickFind::Filters:
            entry.kind==QuickFindEntry::Scene||
            (entry.kind==QuickFindEntry::Source&&!entry.sceneId.isEmpty())?
                QuickFind::SceneJump:QuickFind::Properties;
    if(action==QuickFind::SceneJump){
        if(entry.kind==QuickFindEntry::Filter)return false;
        const QString target=entry.kind==QuickFindEntry::Scene?entry.id:entry.sceneId;
        if(target.isEmpty())return false;
        const auto key=target.toUtf8();
        auto *scene=obs_get_source_by_uuid(key.constData());
        if(!scene)return false;
        if(obs_source_get_type(scene)!=OBS_SOURCE_TYPE_SCENE||
           obs_source_removed(scene)){obs_source_release(scene);return false;}
        if(obs_frontend_preview_program_mode_active())
            obs_frontend_set_current_preview_scene(scene);
        else obs_frontend_set_current_scene(scene);
        obs_source_release(scene);
        return true;
    }
    if(action==QuickFind::Properties&&entry.kind!=QuickFindEntry::Source)return false;
    if(action==QuickFind::Filters&&entry.kind==QuickFindEntry::Scene)return false;
    const QString target=entry.kind==QuickFindEntry::Filter?entry.ownerId:entry.id;
    if(target.isEmpty())return false;
    const QByteArray key=target.toUtf8();
    auto *source=obs_get_source_by_uuid(key.constData());
    if(!source)return false;
    bool valid=!obs_source_removed(source);
    if(valid&&entry.kind==QuickFindEntry::Filter){
        // The source may survive after its filter has been deleted. Never
        // treat a stale filter search result as permission to open a different
        // filter or silently perform another action.
        struct Found { QString filterId; bool attached=false; } found{entry.id,false};
        obs_source_enum_filters(source,[](obs_source_t *,obs_source_t *filter,void *context){
            auto &f=*static_cast<Found*>(context);
            if(uuid(filter)==f.filterId)f.attached=true;
        },&found);
        valid=found.attached;
    }
    if(valid){
        if(action==QuickFind::Properties)obs_frontend_open_source_properties(source);
        else if(action==QuickFind::Filters)obs_frontend_open_source_filters(source);
    }
    obs_source_release(source);
    return valid;
}
} // namespace augment
