#pragma once
#include <obs.h>
#include <QByteArray>
#include <QString>
namespace augment {
struct InspectorItem {QString source,scene;qint64 item=-1;};
QByteArray encodeTransform(const obs_transform_info &,const obs_sceneitem_crop &);
bool decodeTransform(const QByteArray &,obs_transform_info &,obs_sceneitem_crop &);
QByteArray readItemTransform(const InspectorItem &);
bool applyItemTransform(const InspectorItem &,const QByteArray &);
bool setInspectorFilterEnabled(const QString &source,const QString &filter,bool enabled);
bool moveInspectorFilter(const QString &source,const QString &filter,bool up);
}
