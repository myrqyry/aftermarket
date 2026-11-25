#include "property-presets.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QSaveFile>
#include <QJsonArray>
#include <QSet>
#include <algorithm>
namespace augment {
namespace {
constexpr qint64 limit=16*1024*1024;
constexpr qsizetype presetLimit=1024*1024;
bool validName(const QString &name){return !name.isEmpty()&&name==name.trimmed()&&name.size()<=100&&!name.contains(QChar(0));}
bool validType(const QString &type){return !type.isEmpty()&&type.size()<=256&&!type.contains(QChar(0));}
bool validPayload(const QByteArray &bytes){return bytes.size()<=presetLimit&&QJsonDocument::fromJson(bytes).isObject();}
bool validForType(const QString &type,const QByteArray &bytes){
    return validPayload(bytes)&&(type!="chain"||PropertyPresets::validChain(bytes));
}
}
bool PropertyPresets::readAll(QJsonObject &root)const{
    root={};QFile file(path_);
    if(!file.exists())return true;
    if(!file.open(QIODevice::ReadOnly)||file.size()>limit)return false;
    const auto bytes=file.read(limit+1);
    if(bytes.size()>limit)return false;
    const auto doc=QJsonDocument::fromJson(bytes);
    if(!doc.isObject())return false;
    const auto obj=doc.object();
    if(obj.value("version").toInt()!=1||!obj.value("types").isObject())return false;
    root=obj.value("types").toObject();
    for(auto type=root.begin();type!=root.end();++type){
        if(!validType(type.key())||!type.value().isObject())return false;
        const auto rows=type.value().toObject();
        for(auto row=rows.begin();row!=rows.end();++row)
            if(!validName(row.key())||!row.value().isString()||!validForType(type.key(),row.value().toString().toUtf8()))return false;
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
    QJsonObject root;if(!validType(type)||!validName(name)||!validForType(type,settings)||!readAll(root))return false;
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
bool PropertyPresets::validChain(const QByteArray &settings,bool allowEmpty){
    if(!validPayload(settings))return false;
    const auto root=QJsonDocument::fromJson(settings).object();
    if(!root.value("filters").isArray()||root.size()!=1)return false;
    const auto filters=root.value("filters").toArray();
    if(filters.size()>64||(!allowEmpty&&filters.isEmpty()))return false;
    QSet<QString> names;
    for(const auto &value:filters){
        if(!value.isObject())return false;
        const auto row=value.toObject();
        if(row.size()!=4||!row.value("id").isString()||!row.value("name").isString()||
           !row.value("enabled").isBool()||!row.value("settings").isString())return false;
        const auto name=row.value("name").toString();
        if(!validType(row.value("id").toString())||name.isEmpty()||name.size()>256||
           name.contains(QChar(0))||names.contains(name)||!validPayload(row.value("settings").toString().toUtf8()))return false;
        names.insert(name);
    }
    return true;
}
bool PropertyPresets::exportPreset(const QString &type,const QString &name,const QString &path)const{
    const auto payload=read(type,name);
    if(payload.isEmpty()||QFileInfo(path).exists()||QFileInfo(path).isSymLink())return false;
    const auto bytes=QJsonDocument(QJsonObject{{"format","aftermarket-preset"},{"version",1},
        {"type",type},{"name",name},{"settings",QString::fromUtf8(payload)}}).toJson(QJsonDocument::Indented);
    // NewOnly avoids clobbering even if another file appears after the picker.
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly))return false;
    if(!file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner)){file.close();file.remove();return false;}
    const bool ok=file.write(bytes)==bytes.size()&&file.flush();
    file.close();if(!ok)file.remove();return ok;
}
bool PropertyPresets::readPortable(const QString &type,const QString &path,QString &name,QByteArray &settings){
    name.clear();settings.clear();
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)||file.size()>limit)return false;
    const auto bytes=file.read(limit+1);
    if(bytes.size()>limit)return false;
    const auto doc=QJsonDocument::fromJson(bytes);
    if(!doc.isObject())return false;
    const auto obj=doc.object();
    if(obj.value("format").toString()!="aftermarket-preset"||obj.value("version").toDouble()!=1||
       obj.value("type").toString()!=type||!obj.value("name").isString()||!obj.value("settings").isString())return false;
    const auto original=obj.value("name").toString();
    const auto payload=obj.value("settings").toString().toUtf8();
    if(!validName(original)||!validType(type)||!validForType(type,payload))return false;
    name=original;settings=payload;return true;
}
bool PropertyPresets::importPreset(const QString &type,const QString &path,const QString &newName){
    QString name;QByteArray settings;
    if(!readPortable(type,path,name,settings))return false;
    return save(type,newName.isEmpty()?name:newName,settings,false);
}

}
