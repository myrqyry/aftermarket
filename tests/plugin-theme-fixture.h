#pragma once
#include <QDir>
#include <QFile>
#include <QString>
inline bool writePluginThemeFixture(const QString &root){
    QDir dir(root);
    const QString base="icons/colorway/plugins/";
    if(!dir.mkpath(base+"normal")||!dir.mkpath(base+"selected"))return false;
    auto write=[&](const QString &path,const QByteArray &data){
        QFile file(dir.filePath(base+path));
        return file.open(QIODevice::WriteOnly)&&file.write(data)==data.size();
    };
    const QByteArray svg="<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 16 16'><rect width='16' height='16' fill='INK'/></svg>";
    for(const auto &id: {QString("source-clone"),QString("audio-monitor")}){
        auto light=svg;light.replace("INK","#e0def4");
        auto dark=svg;dark.replace("INK","#191724");
        if(!write("normal/"+id+".svg",light)||!write("selected/"+id+".svg",dark))return false;
    }
    if(!write("bindings.json",R"({"version":1,"sources":{"source-clone":"source-clone"},"docks":{"AudioMonitorDock":"audio-monitor"},"filters":{"source_record_filter":"source-clone"}})"))return false;
    QDir::setSearchPaths("theme",{root});
    return true;
}
