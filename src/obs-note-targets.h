#pragma once
#include "context-notes.h"
#include "quick-find.h"
#include "inspector-edits.h"
#include <obs.hpp>
#include <obs-frontend-api.h>
namespace augment {
inline QString noteUuid(obs_source_t *source){
 return source?QString::fromUtf8(obs_source_get_uuid(source)):QString{};
}
inline QString noteCollection(){
 char *name=obs_frontend_get_current_scene_collection();const auto result=QString::fromUtf8(name?name:"");bfree(name);return result;
}
inline QString entryNoteKey(const QuickFindEntry &entry){
 return entry.kind==QuickFindEntry::Filter?noteKey("filter",entry.id,entry.ownerId):
     noteKey(entry.kind==QuickFindEntry::Scene?"scene":"source",entry.id);
}
inline bool noteEntryExists(const QuickFindEntry &entry){
 if(entry.kind==QuickFindEntry::Filter){
  OBSSourceAutoRelease owner=obs_get_source_by_uuid(entry.ownerId.toUtf8().constData());
  if(!owner||obs_source_removed(owner))return false;
  struct Search{QString id;bool found=false;};Search search{entry.id};
  obs_source_enum_filters(owner,[](obs_source_t*,obs_source_t *f,void *p){
   auto &s=*static_cast<Search*>(p);if(!obs_source_removed(f)&&noteUuid(f)==s.id)s.found=true;
  },&search);return search.found;
 }
 OBSSourceAutoRelease source=obs_get_source_by_uuid(entry.id.toUtf8().constData());
 return source&&!obs_source_removed(source)&&
  (entry.kind==QuickFindEntry::Scene?obs_source_get_type(source)==OBS_SOURCE_TYPE_SCENE:
   obs_source_get_type(source)!=OBS_SOURCE_TYPE_FILTER);
}
inline bool noteItemExists(const InspectorItem &target){
 OBSSourceAutoRelease owner=obs_get_source_by_uuid(target.scene.toUtf8().constData());
 if(!owner||obs_source_removed(owner)||target.item<0)return false;
 auto *scene=obs_scene_from_source(owner);if(!scene)scene=obs_group_from_source(owner);
 auto *item=scene?obs_scene_find_sceneitem_by_id(scene,target.item):nullptr;
 return item&&noteUuid(obs_sceneitem_get_source(item))==target.source;
}
}
