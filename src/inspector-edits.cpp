#include "inspector-edits.h"
#include <obs.hpp>
#include <obs-frontend-api.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <cmath>
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
 if(chain(list)!=o.value("expected").toArray())return;
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
bool moveInspectorFilter(const QString &source,const QString &id,bool up){
 OBSSourceAutoRelease parent=obs_get_source_by_uuid(source.toUtf8().constData());
 auto list=filters(parent);auto *f=findFilter(list,id);if(!f||list.size()>256)return false;
 const auto before=chain(list);
 obs_source_filter_set_order(parent,f,up?OBS_ORDER_MOVE_UP:OBS_ORDER_MOVE_DOWN);
 const auto after=chain(filters(parent));if(before==after)return false;
 filterHistory("Aftermarket: Reorder filter","order",source,id,before,after);return true;
}
}
