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
private:
    bool readAll(QJsonObject &root)const;
    bool writeAll(const QJsonObject &root)const;
    QString path_;
};
}
