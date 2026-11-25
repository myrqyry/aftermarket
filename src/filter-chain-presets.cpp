#include "filter-chain-presets.h"
#include "property-presets.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSet>
namespace augment {
FilterChain captureFilters(obs_source_t *source){
    FilterChain list;
    if(source)obs_source_enum_filters(source,[](obs_source_t*,obs_source_t *filter,void *data){
        static_cast<FilterChain*>(data)->append(OBSSource(filter));
    },&list);
    return list;
}
QByteArray encodeFilters(const FilterChain &filters){
    QJsonArray rows;
    for(const auto &filter:filters){
        if(!filter||obs_source_removed(filter))return {};
        OBSDataAutoRelease settings=obs_source_get_settings(filter);
        if(!settings)return {};
        rows.append(QJsonObject{{"id",QString::fromUtf8(obs_source_get_id(filter))},
            {"name",QString::fromUtf8(obs_source_get_name(filter))},{"enabled",obs_source_enabled(filter)},
            {"settings",QString::fromUtf8(obs_data_get_json(settings))}});
    }
    auto bytes=QJsonDocument(QJsonObject{{"filters",rows}}).toJson(QJsonDocument::Compact);
    return PropertyPresets::validChain(bytes,true)?bytes:QByteArray{};
}
bool prepareFilters(obs_source_t *parent,const QByteArray &bytes,FilterChain &prepared){
    prepared.clear();
    if(!parent||obs_source_removed(parent)||!PropertyPresets::validChain(bytes))return false;
    const auto rows=QJsonDocument::fromJson(bytes).object().value("filters").toArray();
    QSet<QString> registered;
    const char *id=nullptr;
    for(size_t i=0;obs_enum_filter_types(i,&id);++i)registered.insert(QString::fromUtf8(id));
    const auto caps=obs_source_get_output_flags(parent)&(OBS_SOURCE_VIDEO|OBS_SOURCE_AUDIO|OBS_SOURCE_ASYNC);
    // Preflight every entry before constructing any filter.
    for(const auto &entry:rows){
        const auto row=entry.toObject();const auto id=row.value("id").toString();
        if(!registered.contains(id))return false;
        const auto flags=obs_get_source_output_flags(id.toUtf8().constData());
        auto required=flags&(OBS_SOURCE_VIDEO|OBS_SOURCE_AUDIO|OBS_SOURCE_ASYNC);
        if((required&OBS_SOURCE_AUDIO)&&!(required&OBS_SOURCE_VIDEO))required&=~OBS_SOURCE_ASYNC;
        if((caps&required)!=required||(flags&OBS_SOURCE_DO_NOT_DUPLICATE))return false;
    }
    FilterChain next;
    for(const auto &entry:rows){
        const auto row=entry.toObject();
        const auto id=row.value("id").toString().toUtf8(),name=row.value("name").toString().toUtf8();
        OBSDataAutoRelease settings=obs_data_create_from_json(row.value("settings").toString().toUtf8().constData());
        if(!settings)return false;
        OBSSourceAutoRelease filter=obs_source_create_private(id.constData(),name.constData(),settings);
        if(!filter||obs_source_get_type(filter)!=OBS_SOURCE_TYPE_FILTER||
           QByteArray(obs_source_get_id(filter))!=id)return false;
        obs_source_set_enabled(filter,row.value("enabled").toBool());
        next.append(OBSSource(filter));
    }
    prepared=std::move(next);return true;
}
bool replaceFilters(obs_source_t *parent,const FilterChain &expected,const FilterChain &replacement){
    if(!parent||obs_source_removed(parent)||captureFilters(parent)!=expected)return false;
    for(const auto &filter:replacement)
        if(!filter||obs_source_removed(filter)||obs_filter_get_parent(filter))return false;
    for(const auto &filter:expected)obs_source_filter_remove(parent,filter);
    for(const auto &filter:replacement)obs_source_filter_add(parent,filter);
    if(captureFilters(parent)==replacement)return true;
    // Keep originals alive until attachment is verified. Roll back only
    // our own additions, then restore original object identities/order.
    for(const auto &filter:replacement)
        if(obs_filter_get_parent(filter)==parent)obs_source_filter_remove(parent,filter);
    for(const auto &filter:expected)
        if(!obs_filter_get_parent(filter))obs_source_filter_add(parent,filter);
    return false;
}
}
