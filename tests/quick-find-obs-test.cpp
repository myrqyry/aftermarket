#include "quick-find.h"
#include <obs.h>
#include <QApplication>
#include <iostream>
#define REQUIRE(v) do{if(!(v)){std::cerr<<"FAILED "<<__LINE__<<": "<<#v<<"\n";return 1;}}while(false)

namespace {
const char* name(void*){return "Aftermarket Quick Find fixture";}
void* create(obs_data_t*,obs_source_t*){return new int(1);}
void destroy(void* data){delete static_cast<int*>(data);}
}
int main(int argc,char**argv){
    QApplication app(argc,argv);
    REQUIRE(obs_startup("en-US",nullptr,nullptr));
    obs_source_info info{};
    info.id="aftermarket-quick-find-input-test";
    info.type=OBS_SOURCE_TYPE_INPUT;
    info.output_flags=OBS_SOURCE_AUDIO;
    info.get_name=name;info.create=create;info.destroy=destroy;
    obs_register_source(&info);
    obs_source_info filterInfo=info;
    filterInfo.id="aftermarket-quick-find-filter-test";
    filterInfo.type=OBS_SOURCE_TYPE_FILTER;
    obs_register_source(&filterInfo);
    obs_scene_t *scene=obs_scene_create("QuickFind Scene");
    REQUIRE(scene);
    obs_data_t *settings=obs_data_create();
    obs_data_set_string(settings,"url","https://example.test/stream-endpoint");
    auto *source=obs_source_create(info.id,"Stream Browser",settings,nullptr);
    obs_data_release(settings);
    REQUIRE(source);
    REQUIRE(QString::fromUtf8(obs_source_get_unversioned_id(source))==info.id);
    const QString id=QString::fromUtf8(obs_source_get_uuid(source));
    auto *item=obs_scene_add(scene,source);
    REQUIRE(item);
    auto *filter=obs_source_create(filterInfo.id,"Optical Blur",nullptr,nullptr);
    REQUIRE(filter);
    obs_source_filter_add(source,filter);
    auto *canvas=obs_canvas_create("Portrait fixture",nullptr,SCENE_REF);
    REQUIRE(canvas);
    auto *vertical=obs_canvas_scene_create(canvas,"Portrait scene");
    REQUIRE(vertical);
    auto *verticalItem=obs_scene_add(vertical,source);REQUIRE(verticalItem);
    auto *duplicate=obs_scene_add(scene,source);REQUIRE(duplicate);
    auto *unused=obs_source_create(info.id,"Unused fixture",nullptr,nullptr);REQUIRE(unused);
    augment::startQuickFindWatch();
    const auto all=augment::collectObsQuickFind();
    bool portrait=false,unplaced=false;
    augment::QuickFindEntry sourceHit;
    for(const auto &entry:all){
      if(entry.name=="Portrait scene"){REQUIRE(!entry.mainCanvas&&entry.canvasNames.contains("Portrait fixture"));portrait=true;}
      if(entry.id==id){sourceHit=entry;REQUIRE(entry.placements.size()==3&&entry.canvasNames.contains("Portrait fixture"));REQUIRE(entry.placements[0].key!=entry.placements[1].key);}
      if(entry.name=="Unused fixture"){REQUIRE(entry.unplaced);unplaced=true;}
    }
    REQUIRE(portrait&&unplaced);
    const auto scenes=augment::QuickFind::search(all,"quickfind");
    bool foundScene=false,foundSource=false,foundFilter=false;
    augment::QuickFindEntry filterHit;
    for(const auto &entry:all){
        if(entry.kind==augment::QuickFindEntry::Scene&&entry.name=="QuickFind Scene")
            foundScene=true;
        if(entry.kind==augment::QuickFindEntry::Source&&entry.id==id){
            REQUIRE(entry.name=="Stream Browser");
            REQUIRE(entry.type==info.id);
            REQUIRE(entry.detail=="https://example.test/stream-endpoint");
            REQUIRE(entry.location.contains("QuickFind Scene"));
            REQUIRE(!entry.sceneId.isEmpty());
            foundSource=true;
        }
        if(entry.kind==augment::QuickFindEntry::Filter&&entry.name=="Optical Blur"){
            REQUIRE(entry.ownerId==id);
            filterHit=entry;
            foundFilter=true;
        }
    }
    REQUIRE(foundScene&&foundSource&&foundFilter);
    REQUIRE(!scenes.isEmpty());
    REQUIRE(augment::QuickFind::search(all,"stream-endpoint").size()==1);
    REQUIRE(augment::QuickFind::search(all,"optical blur").size()==1);
    REQUIRE(!augment::performObsQuickFind({augment::QuickFindEntry::Source, "stale",{},"","",{}, {},{}},
        augment::QuickFind::Filters));
    const auto generation=augment::quickFindGeneration();
    REQUIRE(!sourceHit.placements.isEmpty());
    for(const auto &placement:sourceHit.placements)REQUIRE(augment::validateQuickFindPlacement(sourceHit,placement));
    obs_source_filter_remove(source,filter);
    REQUIRE(augment::quickFindGeneration()!=generation);
    REQUIRE(!augment::performObsQuickFind(filterHit,augment::QuickFind::Filters));
    obs_source_release(filter);
    const auto afterFilter=augment::quickFindGeneration();
    obs_sceneitem_remove(verticalItem);
    REQUIRE(augment::quickFindGeneration()!=afterFilter);
    for(const auto &placement:sourceHit.placements)if(!placement.mainCanvas)REQUIRE(!augment::validateQuickFindPlacement(sourceHit,placement));
    augment::stopQuickFindWatch();
    obs_sceneitem_remove(duplicate);obs_scene_release(vertical);obs_canvas_remove(canvas);obs_canvas_release(canvas);obs_source_remove(unused);obs_source_release(unused);
    obs_sceneitem_remove(item);
    obs_source_remove(source);
    obs_source_release(source);
    obs_scene_release(scene);
    obs_shutdown();
    std::cout<<"AFTERMARKET_QUICK_FIND_LIBOBS_CATALOG_PASS=1\n";
}
