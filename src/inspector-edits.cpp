#include "inspector-edits.h"
#include <obs.hpp>
#include <obs-frontend-api.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <cmath>
#include <algorithm>
namespace augment {
namespace {
QByteArray json(const QJsonObject &o){return QJsonDocument(o).toJson(QJsonDocument::Compact);}
QString utf(const char *s){return s?QString::fromUtf8(s):QString{};}
QString collection(){char *s=obs_frontend_get_current_scene_collection();auto name=utf(s);bfree(s);return name;}
bool integer(const QJsonValue &v,int lo,int hi){return v.isDouble()&&v.toDouble()>=lo&&v.toDouble()<=hi&&std::floor(v.toDouble())==v.toDouble();}
bool number(const QJsonValue &v,double lo,double hi){return v.isDouble()&&std::isfinite(v.toDouble())&&v.toDouble()>=lo&&v.toDouble()<=hi;}
bool pair(const QJsonValue &v,vec2 &out,double lo,double hi){
 const auto a=v.toArray();if(a.size()!=2||!number(a[0],lo,hi)||!number(a[1],lo,hi))return false;
 out.x=float(a[0].toDouble());out.y=float(a[1].toDouble());return true;
}
bool alignment(const QJsonValue &v){
 if(!integer(v,0,15))return false;
 int n=v.toInt();return (n&3)!=3&&(n&12)!=12;
}
obs_sceneitem_t *resolve(const InspectorItem &target,obs_source_t *owner){
 if(!owner||obs_source_removed(owner)||target.item<0||target.source.isEmpty())return nullptr;
 // Group children can resize/reposition their containing group as a side
 // effect. Until grouped transform history is qualified, use top-level items.
 auto *scene=obs_scene_from_source(owner);
 if(!scene)return nullptr;
 auto *item=obs_scene_find_sceneitem_by_id(scene,target.item);
 if(!item||obs_sceneitem_locked(item)||obs_sceneitem_is_group(item)||
    utf(obs_source_get_uuid(obs_sceneitem_get_source(item)))!=target.source)return nullptr;
 return item;
}
QByteArray snapshot(obs_sceneitem_t *item){
 if(!item)return {};
 obs_transform_info i{};obs_sceneitem_crop c{};obs_sceneitem_get_info2(item,&i);obs_sceneitem_get_crop(item,&c);
 return encodeTransform(i,c);
}
bool writeTransform(obs_sceneitem_t *item,const QByteArray &bytes){
 obs_transform_info i{};obs_sceneitem_crop c{};
 if(!item||!decodeTransform(bytes,i,c))return false;
 obs_sceneitem_defer_update_begin(item);obs_sceneitem_set_info2(item,&i);obs_sceneitem_set_crop(item,&c);obs_sceneitem_defer_update_end(item);
 return true;
}
using Filters=QList<OBSSource>;
Filters filters(obs_source_t *source){
 Filters result;
 if(source&&!obs_source_removed(source))obs_source_enum_filters(source,[](obs_source_t*,obs_source_t *f,void *p){
  static_cast<Filters*>(p)->append(OBSSource(f));
 },&result);
 return result;
}
QJsonArray chain(const Filters &list){
 QJsonArray result;for(const auto &f:list)result.append(QJsonObject{{"id",utf(obs_source_get_uuid(f))},{"enabled",obs_source_enabled(f)}});return result;
}
obs_source_t *findFilter(const Filters &list,const QString &id){
 if(id.isEmpty())return nullptr;
 for(const auto &f:list)if(utf(obs_source_get_uuid(f))==id)return f;return nullptr;
}
// Use libobs's AV compatibility contract, plus the native creation
// catalog's hidden/deprecated exclusions. Do not instantiate to probe.
bool compatible(obs_source_t *parent,const QString &type,bool creating){
 if(!parent||obs_source_removed(parent)||type.isEmpty())return false;
 bool registered=false;const char *id=nullptr;
 for(size_t i=0;obs_enum_filter_types(i,&id);++i)if(utf(id)==type){registered=true;break;}
 if(!registered)return false;
 const auto flags=obs_get_source_output_flags(type.toUtf8().constData());
 if(flags&(OBS_SOURCE_DO_NOT_DUPLICATE|OBS_SOURCE_CAP_DISABLED))return false;
 if(creating&&(flags&OBS_SOURCE_DEPRECATED))return false;
 constexpr uint32_t av=OBS_SOURCE_VIDEO|OBS_SOURCE_AUDIO|OBS_SOURCE_ASYNC;
 auto needed=flags&av;
 if((needed&OBS_SOURCE_AUDIO)&&!(needed&OBS_SOURCE_VIDEO))needed&=~OBS_SOURCE_ASYNC;
 return (obs_source_get_output_flags(parent)&needed)==needed;
}
constexpr qsizetype snapshotLimit=1024*1024;
QByteArray filterSnapshot(obs_source_t *filter){
 if(!filter||obs_source_removed(filter))return {};
 OBSDataAutoRelease saved=obs_save_source(filter);
 if(!saved)return {};
 QByteArray bytes=obs_data_get_json(saved);
 if(bytes.size()>snapshotLimit||!QJsonDocument::fromJson(bytes).isObject())return {};
 return bytes;
}
bool nameUsed(const Filters &list,const QString &name){
 for(const auto &f:list)if(utf(obs_source_get_name(f))==name)return true;
 return false;
}
bool replayMembership(obs_source_t *parent,const Filters &list,const QJsonObject &o){
 const auto id=o.value("id").toString();
 const auto desired=o.value("desired").toArray();
 const auto bytes=o.value("snapshot").toString().toUtf8();
 if(bytes.isEmpty()||bytes.size()>snapshotLimit)return false;
 OBSDataAutoRelease data=obs_data_create_from_json(bytes.constData());
 if(!data||utf(obs_data_get_string(data,"uuid"))!=id)return false;
 const auto type=utf(obs_data_get_string(data,"versioned_id"));
 const auto name=utf(obs_data_get_string(data,"name"));
 auto *live=findFilter(list,id);
 if(o.value("kind").toString()=="detach"){
  // Never remove a filter that has been edited since the history entry.
  if(!live||desired.size()!=list.size()-1||
     QJsonDocument::fromJson(filterSnapshot(live)).object()!=QJsonDocument::fromJson(bytes).object())return false;
  obs_source_filter_remove(parent,live);
  return chain(filters(parent))==desired;
 }
 if(live||desired.size()!=list.size()+1||name.isEmpty()||nameUsed(list,name)||!compatible(parent,type,false))return false;
 int index=-1;for(int i=0;i<desired.size();++i)if(desired[i].toObject().value("id").toString()==id)index=i;
 if(index<0)return false;
 // Native project loading also treats attached filters as private sources.
 // Do not reuse or overwrite a detached UUID still retained by another owner.
 OBSSourceAutoRelease collision=obs_get_source_by_uuid(id.toUtf8().constData());
 if(collision)return false;
 OBSSourceAutoRelease restored=obs_load_private_source(data);
 if(!restored||obs_source_get_type(restored)!=OBS_SOURCE_TYPE_FILTER||
    utf(obs_source_get_uuid(restored))!=id||utf(obs_source_get_id(restored))!=type)return false;
 obs_source_filter_add(parent,restored);
 if(obs_filter_get_parent(restored)!=parent)return false;
 obs_source_filter_set_index(parent,restored,size_t(desired.size()-1-index));
 obs_source_load(restored);
 if(chain(filters(parent))==desired)return true;
 obs_source_filter_remove(parent,restored);return false;
}
QJsonObject payload(const QString &kind,const QString &source,const QString &id,const QJsonValue &expected,const QJsonValue &desired){
 return {{"kind",kind},{"collection",collection()},{"source",source},{"id",id},{"expected",expected},{"desired",desired}};
}
void replay(const char *data){
 const auto o=QJsonDocument::fromJson(QByteArray(data?data:"")).object();
 if(o.value("collection").toString()!=collection())return;
 const auto source=o.value("source").toString(),id=o.value("id").toString(),kind=o.value("kind").toString();
 if(kind=="transform"){
  bool ok=false;auto item=id.toLongLong(&ok);if(!ok)return;
  InspectorItem target{source,o.value("scene").toString(),item};
  OBSSourceAutoRelease owner=obs_get_source_by_uuid(target.scene.toUtf8().constData());
  auto *live=resolve(target,owner);
  if(!live||snapshot(live)!=o.value("expected").toString().toUtf8())return;
  if(writeTransform(live,o.value("desired").toString().toUtf8()))obs_frontend_save();
  return;
 }
 OBSSourceAutoRelease parent=obs_get_source_by_uuid(source.toUtf8().constData());
 auto list=filters(parent);
 if(!parent||obs_source_removed(parent)||chain(list)!=o.value("expected").toArray())return;
 if(kind=="attach"||kind=="detach"){
  if(replayMembership(parent,list,o))obs_frontend_save();
  return;
 }
 auto *filter=findFilter(list,id);if(!filter)return;
 const auto desired=o.value("desired").toArray();
 if(desired.size()!=list.size())return;
 if(kind=="enabled"){
  for(auto row:desired)if(row.toObject().value("id").toString()==id){
   const auto on=row.toObject().value("enabled");if(!on.isBool())return;
   obs_source_set_enabled(filter,on.toBool());obs_frontend_save();return;
  }
 }else if(kind=="order"){
  for(int index=0;index<desired.size();++index)if(desired[index].toObject().value("id").toString()==id){
   // Native enumeration is reverse of libobs's internal index.
   obs_source_filter_set_index(parent,filter,size_t(desired.size()-1-index));obs_frontend_save();return;
  }
 }
}
void history(const char *name,QJsonObject before,QJsonObject after){
 const auto u=json(before),r=json(after);
 obs_frontend_add_undo_redo_action(name,replay,replay,u.constData(),r.constData(),false);
 obs_frontend_save();
}
void filterHistory(const char *name,const QString &kind,const QString &source,const QString &id,const QJsonArray &before,const QJsonArray &after){
 history(name,payload(kind,source,id,after,before),payload(kind,source,id,before,after));
}
void membershipHistory(const char *name,const QString &source,const QString &id,const QByteArray &bytes,
                       const QJsonArray &without,const QJsonArray &with,bool added){
 auto attach=payload("attach",source,id,without,with),detach=payload("detach",source,id,with,without);
 attach["snapshot"]=QString::fromUtf8(bytes);detach["snapshot"]=QString::fromUtf8(bytes);
 history(name,added?detach:attach,added?attach:detach);
}
}
QByteArray encodeTransform(const obs_transform_info &i,const obs_sceneitem_crop &c){
 return json({{"version",1},{"position",QJsonArray{i.pos.x,i.pos.y}},{"scale",QJsonArray{i.scale.x,i.scale.y}},
  {"rotation",i.rot},{"alignment",int(i.alignment)},{"boundsType",int(i.bounds_type)},{"boundsAlignment",int(i.bounds_alignment)},
  {"bounds",QJsonArray{i.bounds.x,i.bounds.y}},{"cropToBounds",i.crop_to_bounds},{"crop",QJsonArray{c.left,c.top,c.right,c.bottom}}});
}
bool decodeTransform(const QByteArray &bytes,obs_transform_info &i,obs_sceneitem_crop &c){
 if(bytes.size()>4096)return false;
 const auto o=QJsonDocument::fromJson(bytes).object();
 obs_transform_info next{};obs_sceneitem_crop crop{};
 if(o.size()!=10||o.value("version").toDouble()!=1||
    !pair(o.value("position"),next.pos,-1000000,1000000)||!pair(o.value("scale"),next.scale,-10000,10000)||
    !number(o.value("rotation"),-1000000,1000000)||!alignment(o.value("alignment"))||
    !alignment(o.value("boundsAlignment"))||!integer(o.value("boundsType"),OBS_BOUNDS_NONE,OBS_BOUNDS_MAX_ONLY)||
    !pair(o.value("bounds"),next.bounds,0,1000000)||!o.value("cropToBounds").isBool())return false;
 const auto a=o.value("crop").toArray();if(a.size()!=4)return false;
 for(const auto &v:a)if(!integer(v,0,1000000))return false;
 crop.left=a[0].toInt();crop.top=a[1].toInt();crop.right=a[2].toInt();crop.bottom=a[3].toInt();
 next.rot=float(o.value("rotation").toDouble());next.alignment=o.value("alignment").toInt();
 next.bounds_type=obs_bounds_type(o.value("boundsType").toInt());next.bounds_alignment=o.value("boundsAlignment").toInt();
 next.crop_to_bounds=o.value("cropToBounds").toBool();i=next;c=crop;return true;
}
QByteArray readItemTransform(const InspectorItem &target){
 OBSSourceAutoRelease owner=obs_get_source_by_uuid(target.scene.toUtf8().constData());
 auto bytes=snapshot(resolve(target,owner));obs_transform_info info{};obs_sceneitem_crop crop{};
 return decodeTransform(bytes,info,crop)?bytes:QByteArray{};
}
bool applyItemTransform(const InspectorItem &target,const QByteArray &bytes){
 OBSSourceAutoRelease owner=obs_get_source_by_uuid(target.scene.toUtf8().constData());
 auto *item=resolve(target,owner);const auto before=snapshot(item);
 obs_transform_info i{};obs_sceneitem_crop c{};
 if(!decodeTransform(before,i,c)||!writeTransform(item,bytes))return false;
 const auto after=snapshot(item);if(before==after)return true;
 auto undo=payload("transform",target.source,QString::number(target.item),QString::fromUtf8(after),QString::fromUtf8(before));
 auto redo=payload("transform",target.source,QString::number(target.item),QString::fromUtf8(before),QString::fromUtf8(after));
 undo["scene"]=target.scene;redo["scene"]=target.scene;
 history("Aftermarket: Load transform preset",undo,redo);return true;
}
bool setInspectorFilterEnabled(const QString &source,const QString &id,bool enabled){
 OBSSourceAutoRelease parent=obs_get_source_by_uuid(source.toUtf8().constData());
 auto list=filters(parent);auto *f=findFilter(list,id);
 if(!f||list.size()>256)return false;
 if(obs_source_enabled(f)==enabled)return true;
 const auto before=chain(list);obs_source_set_enabled(f,enabled);const auto after=chain(filters(parent));
 if(before==after)return false;
 filterHistory("Aftermarket: Toggle filter","enabled",source,id,before,after);return true;
}
QList<InspectorFilterType> inspectorFilterTypes(const QString &source){
 OBSSourceAutoRelease parent=obs_get_source_by_uuid(source.toUtf8().constData());
 QList<InspectorFilterType> result;if(!parent||obs_source_removed(parent))return result;
 const char *id=nullptr;
 for(size_t i=0;obs_enum_filter_types(i,&id);++i){
  const auto type=utf(id);if(!compatible(parent,type,true))continue;
  auto name=utf(obs_source_get_display_name(id));if(!name.isEmpty())result.append({type,name});
 }
 std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){return a.name.localeAwareCompare(b.name)<0;});
 return result;
}
bool addInspectorFilter(const QString &source,const QString &type){
 OBSSourceAutoRelease parent=obs_get_source_by_uuid(source.toUtf8().constData());
 if(!compatible(parent,type,true))return false;
 const auto list=filters(parent);if(list.size()>=256)return false;
 const auto base=utf(obs_source_get_display_name(type.toUtf8().constData()));
 if(base.isEmpty())return false;
 QString name=base;
 for(int i=2;nameUsed(list,name);++i)name=base+" "+QString::number(i);
 OBSSourceAutoRelease filter=obs_source_create_private(type.toUtf8().constData(),name.toUtf8().constData(),nullptr);
 if(!filter||obs_source_get_type(filter)!=OBS_SOURCE_TYPE_FILTER||utf(obs_source_get_id(filter))!=type)return false;
 const auto before=chain(list);
 obs_source_filter_add(parent,filter);
 if(obs_filter_get_parent(filter)!=parent)return false;
 const auto bytes=filterSnapshot(filter),id=utf(obs_source_get_uuid(filter)).toUtf8();
 const auto after=chain(filters(parent));
 if(bytes.isEmpty()||id.isEmpty()||after.size()!=before.size()+1){obs_source_filter_remove(parent,filter);return false;}
 membershipHistory("Aftermarket: Add filter",source,QString::fromUtf8(id),bytes,before,after,true);return true;
}
bool removeInspectorFilter(const QString &source,const QString &id){
 OBSSourceAutoRelease parent=obs_get_source_by_uuid(source.toUtf8().constData());
 const auto list=filters(parent);auto *filter=findFilter(list,id);
 if(!filter||list.size()>256||!compatible(parent,utf(obs_source_get_id(filter)),false))return false;
 const auto bytes=filterSnapshot(filter);if(bytes.isEmpty())return false;
 const auto before=chain(list);obs_source_filter_remove(parent,filter);
 const auto after=chain(filters(parent));if(after.size()!=before.size()-1)return false;
 membershipHistory("Aftermarket: Remove filter",source,id,bytes,after,before,false);return true;
}
bool moveInspectorFilter(const QString &source,const QString &id,bool up){
 OBSSourceAutoRelease parent=obs_get_source_by_uuid(source.toUtf8().constData());
 auto list=filters(parent);auto *f=findFilter(list,id);if(!f||list.size()>256)return false;
 const auto before=chain(list);
 obs_source_filter_set_order(parent,f,up?OBS_ORDER_MOVE_UP:OBS_ORDER_MOVE_DOWN);
 const auto after=chain(filters(parent));if(before==after)return false;
 filterHistory("Aftermarket: Reorder filter","order",source,id,before,after);return true;
}
}
