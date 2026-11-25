#pragma once
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QJsonObject>
namespace augment {
class PropertyPresets {
public:
    explicit PropertyPresets(QString path):path_(std::move(path)){}
    QStringList names(const QString &type)const;
    QByteArray read(const QString &type,const QString &name)const;
    bool save(const QString &type,const QString &name,const QByteArray &settings,bool overwrite);
    bool rename(const QString &type,const QString &oldName,const QString &newName);
    bool remove(const QString &type,const QString &name);
    bool exportPreset(const QString &type,const QString &name,const QString &file)const;
    static bool readPortable(const QString &type,const QString &file,QString &name,QByteArray &settings);
    bool importPreset(const QString &type,const QString &file,const QString &newName={});
    static bool validChain(const QByteArray &settings,bool allowEmpty=false);
private:
    bool readAll(QJsonObject &root)const;
    bool writeAll(const QJsonObject &root)const;
    QString path_;
};
}
