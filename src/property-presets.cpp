#include "property-presets.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
namespace augment {
namespace {
constexpr qint64 limit=16*1024*1024;
constexpr qsizetype presetLimit=1024*1024;
bool validName(const QString &name){return !name.isEmpty()&&name==name.trimmed()&&name.size()<=100&&!name.contains(QChar(0));}
bool validType(const QString &type){return !type.isEmpty()&&type.size()<=256&&!type.contains(QChar(0));}
bool validPayload(const QByteArray &bytes){return bytes.size()<=presetLimit&&QJsonDocument::fromJson(bytes).isObject();}
}
bool PropertyPresets::readAll(QJsonObject &root)const{
    root={};QFile file(path_);
    if(!file.exists())return true;
    if(!file.open(QIODevice::ReadOnly)||file.size()>limit)return false;
    const auto doc=QJsonDocument::fromJson(file.read(limit+1));
    if(!doc.isObject())return false;
    const auto obj=doc.object();
    if(obj.value("version").toInt()!=1||!obj.value("types").isObject())return false;
    root=obj.value("types").toObject();
    for(auto type=root.begin();type!=root.end();++type){
        if(!validType(type.key())||!type.value().isObject())return false;
        const auto rows=type.value().toObject();
        for(auto row=rows.begin();row!=rows.end();++row)
            if(!validName(row.key())||!row.value().isString()||!validPayload(row.value().toString().toUtf8()))return false;
    }
    return true;
}
bool PropertyPresets::writeAll(const QJsonObject &root)const{
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"types",root}}).toJson(QJsonDocument::Compact);
    if(bytes.size()>limit||!QDir().mkpath(QFileInfo(path_).absolutePath()))return false;
    QSaveFile file(path_);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly)||!file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner))return false;
    return file.write(bytes)==bytes.size()&&file.commit();
}
QStringList PropertyPresets::names(const QString &type)const{
    QJsonObject root;if(!readAll(root))return {};
    auto names=root.value(type).toObject().keys();names.sort(Qt::CaseInsensitive);return names;
}
QByteArray PropertyPresets::read(const QString &type,const QString &name)const{
    QJsonObject root;if(!readAll(root))return {};
    return root.value(type).toObject().value(name).toString().toUtf8();
}
bool PropertyPresets::save(const QString &type,const QString &name,const QByteArray &settings,bool overwrite){
    QJsonObject root;if(!validType(type)||!validName(name)||!validPayload(settings)||!readAll(root))return false;
    auto rows=root.value(type).toObject();if(rows.contains(name)!=overwrite)return false;
    rows.insert(name,QString::fromUtf8(settings));root.insert(type,rows);return writeAll(root);
}
bool PropertyPresets::rename(const QString &type,const QString &oldName,const QString &newName){
    QJsonObject root;if(!validName(newName)||!readAll(root))return false;
    auto rows=root.value(type).toObject();if(!rows.contains(oldName)||rows.contains(newName))return false;
    rows.insert(newName,rows.take(oldName));root.insert(type,rows);return writeAll(root);
}
bool PropertyPresets::remove(const QString &type,const QString &name){
    QJsonObject root;if(!readAll(root))return false;
    auto rows=root.value(type).toObject();if(!rows.contains(name))return false;
    rows.remove(name);if(rows.isEmpty())root.remove(type);else root.insert(type,rows);
    return writeAll(root);
}
}
