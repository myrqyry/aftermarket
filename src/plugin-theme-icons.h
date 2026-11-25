#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QRegularExpression>
#include <QSvgRenderer>

namespace augment {

// OBS registers the theme search paths. Keep the registry and its two inks
// in the same directory so another theme root cannot supply mismatched art.
class PluginThemeIcons {
public:
    struct Glyph {
        QString light;
        QString dark;
        bool operator==(const Glyph &other) const {
            return light == other.light && dark == other.dark;
        }
        QString forForeground(const QColor &foreground) const {
            return qGray(foreground.rgb()) >= 128 ? light : dark;
        }
        bool valid() const {
            return !light.isEmpty() && !dark.isEmpty()
                && QSvgRenderer(light).isValid() && QSvgRenderer(dark).isValid();
        }
        QIcon itemIcon(const QPalette &palette) const {
            if(!valid())return {};
            QIcon icon;
            icon.addFile(forForeground(palette.color(QPalette::Text)),{},QIcon::Normal);
            icon.addFile(forForeground(palette.color(QPalette::HighlightedText)),{},QIcon::Selected);
            return icon;
        }
    };

    PluginThemeIcons() {
        for(const auto &root : QDir::searchPaths("theme")){
            const QDir directory(QDir(root).filePath("icons/colorway/plugins"));
            QFile file(directory.filePath("bindings.json"));
            if(!file.exists())continue;
            if(!file.open(QIODevice::ReadOnly) || file.size()>64*1024)return;
            const auto document=QJsonDocument::fromJson(file.readAll());
            const auto object=document.object();
            if(!document.isObject() || object.value("version").toInt()!=1 ||
               !object.value("sources").isObject() || !object.value("docks").isObject() ||
               !object.value("filters").isObject())return;
            directory_=directory;
            sources_=object.value("sources").toObject();
            docks_=object.value("docks").toObject();
            filters_=object.value("filters").toObject();
            return;
        }
    }
    Glyph source(const QString &id) const {return glyph(sources_,id);}
    Glyph dock(const QString &id) const {return glyph(docks_,id);}
    Glyph filter(const QString &id) const {return glyph(filters_,id);}
private:
    Glyph glyph(const QJsonObject &bindings,const QString &id) const {
        const auto name=bindings.value(id).toString();
        static const QRegularExpression safe(QStringLiteral("^[a-z0-9-]+$"));
        if(name.isEmpty() || !safe.match(name).hasMatch())return {};
        const auto light=directory_.filePath("normal/"+name+".svg");
        const auto dark=directory_.filePath("selected/"+name+".svg");
        if(!QFileInfo(light).isFile() || !QFileInfo(dark).isFile())return {};
        return {light,dark};
    }
    QDir directory_;
    QJsonObject sources_,docks_,filters_;
};

} // namespace augment
