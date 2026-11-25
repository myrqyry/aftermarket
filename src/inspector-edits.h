#pragma once
#include <obs.h>
#include <QByteArray>
#include <QString>
#include <QList>
namespace augment {
struct InspectorItem {QString source,scene;qint64 item=-1;};
QByteArray encodeTransform(const obs_transform_info &,const obs_sceneitem_crop &);
bool decodeTransform(const QByteArray &,obs_transform_info &,obs_sceneitem_crop &);
QByteArray readItemTransform(const InspectorItem &);
bool applyItemTransform(const InspectorItem &,const QByteArray &);
bool setInspectorFilterEnabled(const QString &source,const QString &filter,bool enabled);
struct InspectorFilterType {QString id,name;};
QList<InspectorFilterType> inspectorFilterTypes(const QString &source);
bool addInspectorFilter(const QString &source,const QString &type);
bool removeInspectorFilter(const QString &source,const QString &filter);
bool moveInspectorFilter(const QString &source,const QString &filter,bool up);
}
