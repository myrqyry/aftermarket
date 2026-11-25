#include "inspector-edits.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"line "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);
 using namespace augment;
 obs_transform_info info{};info.pos.x=123.5;info.pos.y=-40;info.scale.x=-1.25;info.scale.y=.5;
 info.rot=32;info.alignment=5;info.bounds_type=OBS_BOUNDS_SCALE_INNER;info.bounds.x=900;info.bounds.y=400;info.crop_to_bounds=true;
 obs_sceneitem_crop crop{2,4,6,8};
 auto saved=encodeTransform(info,crop);obs_transform_info decoded{};obs_sceneitem_crop dc{};
 CHECK(decodeTransform(saved,decoded,dc));
 CHECK(decoded.pos.x==123.5&&decoded.scale.x==-1.25&&decoded.rot==32&&decoded.crop_to_bounds&&decoded.bounds.x==900&&dc.right==6);
 CHECK(!decodeTransform({},decoded,dc));
 auto broken=QJsonDocument::fromJson(saved).object();broken["rotation"]="32";
 CHECK(!decodeTransform(QJsonDocument(broken).toJson(),decoded,dc));
 broken=QJsonDocument::fromJson(saved).object();broken["boundsType"]=999;
 CHECK(!decodeTransform(QJsonDocument(broken).toJson(),decoded,dc));
 broken=QJsonDocument::fromJson(saved).object();broken["crop"]=QJsonArray{0,-1,0,0};
 CHECK(!decodeTransform(QJsonDocument(broken).toJson(),decoded,dc));
 return 0;
}
