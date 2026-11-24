#include "config-identity.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(false)
static bool write(const QString &path,const QByteArray &bytes){
 QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();
}
static QByteArray read(const QString &path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray{};}
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);
 QTemporaryDir tmp;REQUIRE(tmp.isValid());
 auto old=tmp.filePath("obs-ui-augment"),next=tmp.filePath("aftermarket");
 QString error;
 REQUIRE(QDir().mkpath(old+"/nested"));
 REQUIRE(write(old+"/rules.json","preserve settings"));
 REQUIRE(write(old+"/safe-mode.flag","safe"));
 REQUIRE(write(old+"/nested/unknown","unknown"));
 REQUIRE(QDir().mkpath(next)); // libobs or installer may create empty directory
 REQUIRE(augment::prepareConfigIdentity(next,&error));
 REQUIRE(!QFile::exists(old));
 REQUIRE(read(next+"/rules.json")=="preserve settings");
 REQUIRE(read(next+"/safe-mode.flag")=="safe");
 REQUIRE(read(next+"/nested/unknown")=="unknown");
 REQUIRE(augment::prepareConfigIdentity(next,&error));
 // A conflict must not silently overwrite either configuration.
 REQUIRE(QDir().mkpath(old));REQUIRE(write(old+"/rules.json","legacy conflict"));
 REQUIRE(!augment::prepareConfigIdentity(next,&error));
 REQUIRE(!error.isEmpty());
 REQUIRE(read(old+"/rules.json")=="legacy conflict");
 REQUIRE(read(next+"/rules.json")=="preserve settings");
 QTemporaryDir fresh;REQUIRE(augment::prepareConfigIdentity(fresh.filePath("aftermarket"),&error));
 REQUIRE(QFileInfo(fresh.filePath("aftermarket")).isDir());
 std::cout<<"Config identity migration preserves full directory, recovery marker and conflicts\n";
}
