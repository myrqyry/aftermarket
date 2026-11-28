#include "quick-find.h"
#include "source-performance.h"
#include <obs.h>
#include <obs-frontend-api.h>
#include <QHash>
#include <QSet>
#include <QPair>
#include <QStringList>
#include <algorithm>
#include <atomic>
#include <memory>

namespace augment {
namespace {
QString utf(const char *p){return p?QString::fromUtf8(p):QString{};}
QString uuid(obs_source_t *source){return source?utf(obs_source_get_uuid(source)):QString{};}

struct Canvas {
    QString id,name; bool main=false;
};
Canvas canvasOf(obs_source_t *source){
    auto *canvas=obs_source_get_canvas(source);
    if(!canvas)return {};
    Canvas result{utf(obs_canvas_get_uuid(canvas)),utf(obs_canvas_get_name(canvas)),bool(obs_canvas_get_flags(canvas)&MAIN)};
    if(result.main)result.name="Main";
    obs_canvas_release(canvas);return result;
}
using Source=std::shared_ptr<obs_source_t>;
QList<Source> allPublicSources(){
    QList<Source> sources;
    obs_enum_all_sources([](void *p,obs_source_t *source){
        if(!obs_obj_is_private(source)&&!obs_source_removed(source))
            if(auto *ref=obs_source_get_ref(source))
                static_cast<QList<Source>*>(p)->append(Source(ref,obs_source_release));
        return true;
    },&sources);
    return sources;
}
struct Context {
    QHash<QString,QList<QuickFindPlacement>> placements;
    int visited=0;
    bool complete=true;
};
struct Visit {
    Context *context;
    QString rootId,key;
    QStringList path;
    QSet<QString> ancestors;
    Canvas canvas;
    bool revealable=true;
    int depth=0;
};
void recurse(obs_scene_t *scene,Visit &visit);
bool sceneItemCallback(obs_scene_t *scene,obs_sceneitem_t *item,void *payload){
    auto &v=*static_cast<Visit*>(payload);
    if(++v.context->visited>25000){v.context->complete=false;return false;}
    auto *source=obs_sceneitem_get_source(item);
    if(!source)return true;
    const auto id=uuid(source);
    if(id.isEmpty())return true;
    QStringList path=v.path;path.append(utf(obs_source_get_name(source)));
    const auto key=v.key+"/"+QString::number(obs_sceneitem_get_id(item))+":"+id;
    auto &locations=v.context->placements[id];
    if(locations.size()<256)locations.append({v.rootId,uuid(obs_scene_get_source(scene)),key,path.join(" / "),
        v.canvas.id,v.canvas.name,obs_sceneitem_get_id(item),v.canvas.main,v.revealable});
    if(v.depth>=24){v.context->complete=false;return true;}
    if(v.ancestors.contains(id))return true;
    const bool group=obs_sceneitem_is_group(item);
    auto *nested=group?obs_sceneitem_group_get_scene(item):obs_scene_from_source(source);
    if(nested&&nested!=scene){
        Visit child{v.context,v.rootId,key,path,v.ancestors,v.canvas,v.revealable&&group,v.depth+1};
        child.ancestors.insert(id);recurse(nested,child);
    }
    return true;
}
void recurse(obs_scene_t *scene,Visit &v){if(scene&&v.context->complete)obs_scene_enum_items(scene,sceneItemCallback,&v);}
void applyPlacement(QuickFindEntry &entry,const Context &context,const QString &id){
    entry.placements=context.placements.value(id);
    QStringList paths;
    for(const auto &p:entry.placements){
        if(!paths.contains(p.path))paths.append(p.path);
        if(!p.canvasId.isEmpty()&&!entry.canvasIds.contains(p.canvasId)){entry.canvasIds.append(p.canvasId);entry.canvasNames.append(p.canvasName);}
        if(p.mainCanvas&&entry.sceneId.isEmpty())entry.sceneId=p.rootId;
    }
    entry.location=paths.join(" · ").left(320);
}
std::atomic<quint64> generation{1};
QList<obs_weak_source_t*> watchedScenes;
constexpr const char *globalSignals[]={"source_create","source_create_canvas","source_remove","source_destroy","source_rename","source_update","source_filter_add","source_filter_remove","canvas_create","canvas_remove","canvas_rename"};
constexpr const char *sceneSignals[]={"item_add","item_remove","reorder","refresh"};
bool watching=false;
void changed(void*,calldata_t*){generation.fetch_add(1,std::memory_order_relaxed);}
void disconnectScenes(){
    for(auto *weak:watchedScenes){
        if(auto *source=obs_weak_source_get_source(weak)){
            for(auto signal:sceneSignals)signal_handler_disconnect(obs_source_get_signal_handler(source),signal,changed,nullptr);
            obs_source_release(source);
        }
        obs_weak_source_release(weak);
    }
    watchedScenes.clear();
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
        QuickFindEntry hit{QuickFindEntry::Filter,filterId,parentId,
             utf(obs_source_get_name(filter)),utf(obs_source_get_unversioned_id(filter)),{},
             utf(obs_source_get_name(owner)),{}};
        hit.typeName=utf(obs_source_get_display_name(obs_source_get_id(filter)));
        applyPlacement(hit,*collector->context,parentId);
        const auto canvas=canvasOf(owner);
        if(obs_source_is_scene(owner)&&!canvas.id.isEmpty()&&!hit.canvasIds.contains(canvas.id)){
            hit.canvasIds.append(canvas.id);hit.canvasNames.append(canvas.name);
        }
        hit.location=utf(obs_source_get_name(owner))+(hit.location.isEmpty()?"":" · "+hit.location);
        collector->entries.append(hit);
    },&c);
}
void addSource(Collector &c,obs_source_t *source,bool scene){
    if(!source||c.entries.size()>=5000)return;
    const QString id=uuid(source);
    if(id.isEmpty()||c.seen.contains(id))return;
    c.seen.insert(id);
    QuickFindEntry hit{scene?QuickFindEntry::Scene:QuickFindEntry::Source,id,{},
        utf(obs_source_get_name(source)),scene?"scene":utf(obs_source_get_unversioned_id(source)),
        scene?QString{}:localInput(source),{},{},!scene&&
        (obs_source_get_output_flags(source)&OBS_SOURCE_ASYNC_VIDEO)==OBS_SOURCE_ASYNC_VIDEO&&SourcePerformance::available()};
    hit.typeName=scene?"Scene":utf(obs_source_get_display_name(obs_source_get_id(source)));
    applyPlacement(hit,*c.context,id);
    if(scene){
        const auto canvas=canvasOf(source);hit.mainCanvas=canvas.main;
        if(!canvas.id.isEmpty()&&!hit.canvasIds.contains(canvas.id)){hit.canvasIds.append(canvas.id);hit.canvasNames.append(canvas.name);}
    }
    hit.unplaced=!scene&&c.context->complete&&hit.placements.isEmpty()&&!obs_source_active(source)&&!obs_source_showing(source);
    c.entries.append(hit);addFilters(c,source);
}
}
void startQuickFindWatch(){
    if(watching)return;watching=true;
    for(auto signal:globalSignals)signal_handler_connect(obs_get_signal_handler(),signal,changed,nullptr);
    refreshQuickFindWatch();
}
void refreshQuickFindWatch(){
    if(!watching)return;
    disconnectScenes();
    for(const auto &source:allPublicSources()){
        if(!obs_source_is_scene(source.get())&&!obs_source_is_group(source.get()))continue;
        for(auto signal:sceneSignals)signal_handler_connect(obs_source_get_signal_handler(source.get()),signal,changed,nullptr);
        watchedScenes.append(obs_source_get_weak_source(source.get()));
    }
}
void stopQuickFindWatch(){
    if(!watching)return;watching=false;disconnectScenes();
    for(auto signal:globalSignals)signal_handler_disconnect(obs_get_signal_handler(),signal,changed,nullptr);
}
quint64 quickFindGeneration(){return generation.load(std::memory_order_relaxed);}
QList<QuickFindEntry> collectObsQuickFind(){
    Context context;
    auto sources=allPublicSources();
    // Canvas enumeration covers actual named canvases, with all-source
    // discovery also retaining scenes supplied by third-party plugins.
    QList<obs_canvas_t*> canvases;
    if(auto *main=obs_get_main_canvas())canvases.append(main);
    obs_enum_canvases([](void *p,obs_canvas_t *canvas){
        auto &list=*static_cast<QList<obs_canvas_t*>*>(p);
        if(!list.contains(canvas))if(auto *ref=obs_canvas_get_ref(canvas))list.append(ref);
        return true;
    },&canvases);
    QSet<QString> roots;
    auto visitRoot=[&](obs_source_t *source){
        const auto id=uuid(source);
        if(id.isEmpty()||roots.contains(id)||obs_source_removed(source)||obs_obj_is_private(source)||obs_source_is_group(source))return;
        roots.insert(id);const auto canvas=canvasOf(source);
        Visit visit{&context,id,id,{utf(obs_source_get_name(source))},{id},canvas,true,0};
        recurse(obs_scene_from_source(source),visit);
    };
    // Do not traverse source trees while the canvas registry lock is held.
    for(auto *canvas:canvases){
        QList<Source> scenes;
        obs_canvas_enum_scenes(canvas,[](void *p,obs_source_t *source){
            if(auto *ref=obs_source_get_ref(source))static_cast<QList<Source>*>(p)->append(Source(ref,obs_source_release));
            return true;
        },&scenes);
        for(const auto &scene:scenes)visitRoot(scene.get());
        obs_canvas_release(canvas);
    }
    for(const auto &source:sources)if(obs_source_is_scene(source.get()))visitRoot(source.get());
    Collector collector;collector.context=&context;
    for(const auto &source:sources){
        auto *s=source.get();
        const auto type=obs_source_get_type(s);
        if(type==OBS_SOURCE_TYPE_INPUT||obs_source_is_group(s)||type==OBS_SOURCE_TYPE_SCENE)
            addSource(collector,s,obs_source_is_scene(s)&&!obs_source_is_group(s));
    }
    return collector.entries;
}

bool validateQuickFindPlacement(const QuickFindEntry &entry,const QuickFindPlacement &placement){
    if(placement.rootId.isEmpty()||placement.key.section('/',0,0)!=placement.rootId)return false;
    Source root(obs_get_source_by_uuid(placement.rootId.toUtf8().constData()),obs_source_release);
    if(!root||obs_source_removed(root.get())||canvasOf(root.get()).id!=placement.canvasId)return false;
    auto *scene=obs_scene_from_source(root.get());
    const auto segments=placement.key.split('/');
    if(segments.size()<2)return false;
    QList<Source> retained;
    for(int i=1;i<segments.size();++i){
        bool ok=false;auto id=segments[i].section(':',0,0).toLongLong(&ok);
        const auto sourceId=segments[i].section(':',1);
        auto *item=ok&&scene?obs_scene_find_sceneitem_by_id(scene,id):nullptr;
        if(!item||uuid(obs_sceneitem_get_source(item))!=sourceId)return false;
        if(i==segments.size()-1)return id==placement.itemId&&uuid(obs_scene_get_source(scene))==placement.parentId&&
            sourceId==(entry.kind==QuickFindEntry::Filter?entry.ownerId:entry.id);
        auto *s=obs_sceneitem_get_source(item);
        if(auto *ref=obs_source_get_ref(s))retained.append(Source(ref,obs_source_release));else return false;
        scene=obs_sceneitem_is_group(item)?obs_sceneitem_group_get_scene(item):obs_scene_from_source(s);
    }
    return false;
}

bool performObsQuickFind(const QuickFindEntry &entry,QuickFind::Action action){
    if(action==QuickFind::PinSource||action==QuickFind::Performance||action==QuickFind::RevealPlacement)return false; // handled by the optional source-dock adapter
    if(action==QuickFind::Default)
        action=entry.kind==QuickFindEntry::Filter|| (entry.kind==QuickFindEntry::Scene&&!entry.mainCanvas)?QuickFind::Filters:
            (entry.kind==QuickFindEntry::Scene&&entry.mainCanvas)||
            (entry.kind==QuickFindEntry::Source&&!entry.sceneId.isEmpty()&&entry.placements.size()==1)?
                QuickFind::SceneJump:QuickFind::Properties;
    if(action==QuickFind::SceneJump){
        if(entry.kind==QuickFindEntry::Filter|| (entry.kind==QuickFindEntry::Scene&&!entry.mainCanvas))return false;
        const QString target=entry.kind==QuickFindEntry::Scene?entry.id:entry.sceneId;
        if(target.isEmpty())return false;
        if(entry.kind==QuickFindEntry::Source&&!entry.placements.isEmpty()&&entry.revealKey.isEmpty()&&entry.placements.size()!=1)return false;
        if(entry.kind==QuickFindEntry::Source&&entry.placements.size()==1&&entry.revealKey.isEmpty()&&!validateQuickFindPlacement(entry,entry.placements.first()))return false;
        if(!entry.revealKey.isEmpty()){
            auto it=std::find_if(entry.placements.begin(),entry.placements.end(),[&](const auto &p){return p.key==entry.revealKey;});
            if(it==entry.placements.end()||!it->mainCanvas||!validateQuickFindPlacement(entry,*it))return false;
        }
        const auto key=target.toUtf8();
        auto *scene=obs_get_source_by_uuid(key.constData());
        if(!scene)return false;
        if(obs_source_get_type(scene)!=OBS_SOURCE_TYPE_SCENE||
           obs_source_removed(scene)){obs_source_release(scene);return false;}
        if(!canvasOf(scene).main){obs_source_release(scene);return false;}
        obs_frontend_source_list list{};obs_frontend_get_scenes(&list);
        bool present=false;for(size_t i=0;i<list.sources.num;++i)if(list.sources.array[i]==scene)present=true;
        obs_frontend_source_list_free(&list);
        if(!present){obs_source_release(scene);return false;}
        if(obs_frontend_preview_program_mode_active())
            obs_frontend_set_current_preview_scene(scene);
        else obs_frontend_set_current_scene(scene);
        obs_source_release(scene);
        return true;
    }
    if(action==QuickFind::Properties&&entry.kind!=QuickFindEntry::Source)return false;

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
