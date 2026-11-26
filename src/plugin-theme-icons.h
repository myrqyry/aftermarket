#pragma once

#include <QDir>
#include <QPainter>
#include <QPixmapCache>
#include <QDateTime>
#include <QWidget>
#include <QAbstractItemModel>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QEvent>
#include <QTimer>
#include <QSignalBlocker>
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
        bool valid() const {
            return !light.isEmpty() && !dark.isEmpty()
                && QSvgRenderer(light).isValid() && QSvgRenderer(dark).isValid();
        }
        // Use the monochrome light SVG as an alpha mask. Colorway's
        // fixed-ink assets remain usable by stock OBS and older consumers.
        QPixmap tinted(const QColor &foreground,QSize size,qreal dpr=1.0) const {
            if(light.isEmpty()||!foreground.isValid()||size.isEmpty()||!qIsFinite(dpr)||dpr<=0||dpr>16)return {};
            const QSize physical(qRound(size.width()*dpr),qRound(size.height()*dpr));
            if(physical.isEmpty()||physical.width()>512||physical.height()>512)return {};
            const QFileInfo info(light);
            const auto key=QStringLiteral("aftermarket:plugin-icon:")+light+":"+
                QString::number(info.lastModified().toMSecsSinceEpoch())+":"+QString::number(info.size())+":"+
                foreground.name(QColor::HexArgb)+":"+QString::number(physical.width())+"x"+
                QString::number(physical.height())+":"+QString::number(dpr,'g',12);
            QPixmap cached;
            if(QPixmapCache::find(key,&cached))return cached;
            QSvgRenderer svg(light);
            if(!svg.isValid())return {};
            QPixmap pix(physical);pix.fill(Qt::transparent);
            {QPainter painter(&pix);
             painter.setRenderHint(QPainter::Antialiasing);
             svg.render(&painter,QRectF(QPointF(0,0),QSizeF(physical)));
             painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
             painter.fillRect(pix.rect(),foreground);}
            pix.setDevicePixelRatio(dpr);
            QPixmapCache::insert(key,pix);
            return pix;
        }
        QIcon itemIcon(const QPalette &palette,QPalette::ColorRole role=QPalette::Text,
                       QPalette::ColorGroup group=QPalette::Active,qreal deviceRatio=1.0) const {
            if(!valid())return {};
            QIcon icon;
            const auto normal=palette.color(group,role);
            const auto selected=palette.color(group,QPalette::HighlightedText);
            const auto disabled=palette.color(QPalette::Disabled,role);
            // Standard Qt pixmap icon (no plugin-defined icon engine survives
            // teardown). Provide native size and common high-DPI variants.
            QList<qreal> ratios{1.0,2.0};
            if(!ratios.contains(deviceRatio))ratios.append(deviceRatio);
            for(qreal dpr : ratios){
                for(int n:{16,24,32}){
                    const QSize size(n,n);
                    icon.addPixmap(tinted(normal,size,dpr),QIcon::Normal);
                    icon.addPixmap(tinted(normal,size,dpr),QIcon::Active);
                    icon.addPixmap(tinted(selected,size,dpr),QIcon::Selected);
                    icon.addPixmap(tinted(disabled,size,dpr),QIcon::Disabled);
                }
            }
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
            dockClasses_=object.value("dockClasses").toObject();
            return;
        }
    }
    Glyph source(const QString &id) const {return glyph(sources_,id);}
    Glyph dock(const QString &id,const QString &contentClass={}) const {
        // A dynamic Source Dock title may collide with another plugin ID.
        // Recognized widget identity wins even when its artwork is missing.
        if(dockClasses_.contains(contentClass))return glyph(dockClasses_,contentClass);
        return glyph(docks_,id);
    }
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
    QJsonObject sources_,docks_,filters_,dockClasses_;
};

// Retint only registered decorations, without rebuilding a list, moving its
// selection, or changing native/unmapped icons. Persistent indexes follow
// reorder operations and invalidate automatically on removal/model reset.
class PalettePluginIcons final : public QObject {
public:
    static void bind(QWidget *widget,const QModelIndex &index,
                     const PluginThemeIcons::Glyph &glyph,QPalette::ColorRole role=QPalette::Text){
        if(!widget||!index.isValid()||!glyph.valid())return;
        PalettePluginIcons *binding=nullptr;
        for(auto *child:widget->children())
            if((binding=dynamic_cast<PalettePluginIcons*>(child)))break;
        if(!binding)binding=new PalettePluginIcons(widget);
        binding->add(index,glyph,role);
    }
protected:
    bool eventFilter(QObject *,QEvent *event) override {
        switch(event->type()){
        case QEvent::PaletteChange:case QEvent::ApplicationPaletteChange:
        case QEvent::StyleChange:case QEvent::EnabledChange:
        case QEvent::WindowActivate:case QEvent::WindowDeactivate:
        case QEvent::Show:case QEvent::ScreenChangeInternal:
            if(!queued_){queued_=true;QTimer::singleShot(0,this,[this]{queued_=false;refresh();});}
            break;
        default:break;
        }
        return false;
    }
private:
    struct Entry {QPersistentModelIndex index;PluginThemeIcons::Glyph glyph;QPalette::ColorRole role;};
    explicit PalettePluginIcons(QWidget *widget):QObject(widget),widget_(widget){
        widget->installEventFilter(this);
    }
    void add(const QModelIndex &index,const PluginThemeIcons::Glyph &glyph,QPalette::ColorRole role){
        for(auto i=entries_.begin();i!=entries_.end();){
            if(!i->index.isValid())i=entries_.erase(i);
            else if(i->index==index){i->glyph=glyph;i->role=role;apply(*i);return;}
            else ++i;
        }
        entries_.append({QPersistentModelIndex(index),glyph,role});
        apply(entries_.last());
    }
    void refresh(){
        entries_.removeIf([](const Entry &entry){return !entry.index.isValid();});
        // Decoration changes emit model signals; iterate a snapshot so a
        // consumer may rebuild its list safely during a refresh.
        const auto snapshot=entries_;
        for(const auto &entry:snapshot)apply(entry);
    }
    void apply(Entry entry){
        if(!widget_||!entry.index.isValid())return;
        auto *model=const_cast<QAbstractItemModel*>(entry.index.model());
        const auto group=widget_->isEnabled()?(widget_->isActiveWindow()?QPalette::Active:QPalette::Inactive):QPalette::Disabled;
        const auto icon=entry.glyph.itemIcon(widget_->palette(),entry.role,group,widget_->devicePixelRatioF());
        if(!icon.isNull()){
            // QListWidget forwards DecorationRole changes through itemChanged.
            // A palette repaint must never be mistaken for a filter edit.
            const QSignalBlocker block(widget_.data());
            model->setData(entry.index,icon,Qt::DecorationRole);
        }
    }
    QPointer<QWidget> widget_;
    QList<Entry> entries_;
    bool queued_=false;
};

} // namespace augment
