#include "augment.h"
#include "menu-layout.h"
#include "theme-picker.h"
#include "motion.h"
#include "gradient.h"
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QPaintEvent>
#include <QtMath>
#include <QTimer>
#include <QMouseEvent>
#include <QAction>
#include <QComboBox>
#include <QApplication>
#include <QColor>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QProcess>
#include <QSaveFile>
#include <QStyle>
#include <QSvgRenderer>
#include <QWidgetAction>
#include <QPushButton>
#include <QDebug>
#include <algorithm>
#include <vector>

namespace augment {
namespace {
const QByteArray defaults = R"JSON({
  "enabled": true,
  "inspect": false,
  "rules": [
    {"text":"Add","icon":"plus","color":"#90d4a6"},
    {"text":"Add Source","icon":"plus","color":"#90d4a6"},
    {"text":"New Group","icon":"group"},
    {"text":"Rename","icon":"pencil"},
    {"text":"Remove","icon":"trash","color":"#f08b8b","replaceExisting":true},
    {"text":"Copy","icon":"copy"},
    {"text":"Paste","icon":"clipboard"},
    {"text":"Paste (Reference)","icon":"clipboard"},
    {"text":"Paste (Duplicate)","icon":"clipboard"},
    {"text":"Duplicate","icon":"copy"},
    {"text":"Copy Filters","icon":"copy"},
    {"text":"Paste Filters","icon":"clipboard"},
    {"text":"Transform","icon":"move","color":"#c0aff0"},
    {"text":"Edit Transform","icon":"move"},
    {"text":"Order","icon":"order"},
    {"text":"Group Selected Items","icon":"group"},
    {"text":"Filters","icon":"sliders","color":"#9cc4f5"},
    {"text":"Properties","icon":"gear"},
    {"text":"Interact","icon":"pointer"},
    {"text":"Open Source Projector","icon":"monitor"},
    {"text":"Save Source Screenshot","icon":"camera"},
    {"text":"Set Color","icon":"palette"},
    {"text":"Scale Filtering","icon":"sliders"},
    {"text":"Blending Mode","icon":"layers"},
    {"text":"Blending Method","icon":"layers"},
    {"text":"Show Transition","icon":"arrow-in"},
    {"text":"Hide Transition","icon":"arrow-out"},
    {"text":"Resize output (source size)","icon":"resize"}
  ]
})JSON";

const QHash<QString, QString> shapes = {
    {"plus", "<path d='M12 5v14M5 12h14'/>"},
    {"pencil", "<path d='m15 5 4 4M4 20l5-1 12-12-4-4L5 15z'/>"},
    {"trash", "<path d='M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13M10 10v7M14 10v7'/>"},
    {"copy", "<rect x='8' y='8' width='12' height='12' rx='2'/><path d='M16 8V4H4v12h4'/>"},
    {"clipboard", "<rect x='5' y='5' width='14' height='16' rx='2'/><rect x='9' y='3' width='6' height='4' rx='1'/><path d='M9 12h6M9 16h6'/>"},
    {"move", "<path d='M12 3v18M3 12h18M9 6l3-3 3 3M9 18l3 3 3-3M6 9l-3 3 3 3M18 9l3 3-3 3'/>"},
    {"order", "<path d='M5 4v16m-3-3 3 3 3-3M12 5h9M12 11h6M12 17h3'/>"},
    {"group", "<rect x='3' y='3' width='18' height='18' rx='2' stroke-dasharray='3 3'/><rect x='6' y='6' width='5' height='5'/><rect x='13' y='13' width='5' height='5'/>"},
    {"sliders", "<path d='M4 6h5m4 0h7M4 12h9m4 0h3M4 18h2m4 0h10M9 3v6M13 9v6M6 15v6'/>"},
    {"gear", "<path d='m10 3-1 3-3 1-3 3 2 3v3l3 3 3-1 2 3 3-1 1-3 3-1 1-3-3-2V7l-3-2-3 1z'/><circle cx='12' cy='12' r='3'/>"},
    {"pointer", "<path d='m5 3 14 10-7 1-3 7z'/>"},
    {"camera", "<path d='M4 6h4l2-3h4l2 3h4v14H4z'/><circle cx='12' cy='13' r='4'/>"},
    {"palette", "<path d='M12 3a9 9 0 1 0 0 18h2a2 2 0 0 0 1-4h-1a2 2 0 0 1 0-4h3a4 4 0 0 0 0-8z'/><circle cx='7' cy='11' r='1'/><circle cx='10' cy='7' r='1'/><circle cx='15' cy='7' r='1'/>"},
    {"layers", "<path d='m12 3 9 5-9 5-9-5zM3 12l9 5 9-5M3 16l9 5 9-5'/>"},
    {"arrow-in", "<path d='M4 12h12m-5-5 5 5-5 5M20 4v16'/>"},
    {"arrow-out", "<path d='M4 4v16M8 12h12m-5-5 5 5-5 5'/>"},
    {"resize", "<path d='M4 9V4h5M15 20h5v-5M4 4l7 7m2 2 7 7'/>"},
    {"monitor", "<rect x='3' y='4' width='18' height='13' rx='2'/><path d='M12 17v4M8 21h8'/>"}
};

} // namespace

QString normalizeActionText(QString text) {
    text = text.section('\t', 0, 0);
    text.replace("&&", "\x01");
    text.remove('&');
    text.replace(QChar(1), '&');
    text = text.trimmed();
    if (text.endsWith("...")) text.chop(3);
    else if (text.endsWith(QChar(0x2026))) text.chop(1);
    return text.trimmed().toCaseFolded();
}

QString presetColorLabel(int index) {
    static const QStringList palette={
        "Red","Yellow","Green","Cyan","Blue","Magenta","Dark gray","White"};
    return index>=1&&index<=palette.size()?palette.at(index-1):QString{};
}
// Only a single explicitly checked native action establishes selection.
 // Multiple checked commands may represent independent toggles; never infer
 // a single active choice from the first one or from action order.
QString submenuSelection(const QMenu *submenu) {
    if(!submenu)return {};
    const QAction *selected=nullptr;
    for(const auto *a:submenu->actions())if(a->isVisible()&&a->isEnabled()&&a->isCheckable()&&a->isChecked()&&!a->isSeparator()) {
        if(selected)return {};
        selected=a;
    }
    if(!selected) {
        // OBS 32.2 ColorSelect embeds eight QPushButtons in a QWidgetAction.
        // Its selected preset is encoded as a 2px border, not a checked
        // QAction. Recognize this exact native signal without inspecting OBS
        // source data or guessing from mouse coordinates.
        int selectedPreset=0;
        for(const auto *action:submenu->actions()) {
            const auto *widgetAction=qobject_cast<const QWidgetAction*>(action);
            const auto *widget=widgetAction?widgetAction->defaultWidget():nullptr;
            if(!widget||widget->objectName()!="ColorSelect")continue;
            for(const auto *button:widget->findChildren<QPushButton*>()) {
                const auto id=button->property("bgColor").toInt();
                if(!button->objectName().startsWith("preset")||id<1||id>8)continue;
                QString css=button->styleSheet().toLower();css.remove(QChar(' '));
                if(!css.contains("border:2pxsolidblack"))continue;
                if(selectedPreset)return {}; // ambiguous, don't invent a value
                selectedPreset=id;
            }
        }
        return presetColorLabel(selectedPreset);
    }
    QString label=selected->text().section(QChar(9),0,0);
    label.replace("&&",QString(QChar(1)));label.remove('&');label.replace(QChar(1),'&');
    label=label.trimmed();
    if(!label.isEmpty())return label;
    const auto data=selected->data();
    const auto color=data.canConvert<QColor>()?data.value<QColor>():QColor();
    if(color.isValid())return color.name();
    // The OBS color picker may expose anonymous checked swatch icons.
    // Sample only opaque, near-solid center pixels; otherwise leave blank.
    if(!selected->icon().isNull()) {
        const auto pm=selected->icon().pixmap(18,18);
        if(!pm.isNull()) {
            const auto image=pm.toImage();
            const auto mid=image.pixelColor(image.width()/2,image.height()/2);
            const auto around=image.pixelColor(qMax(0,image.width()/2-2),image.height()/2);
            if(mid.alpha()>225&&around.alpha()>225&&
                qAbs(mid.red()-around.red())<10&&qAbs(mid.green()-around.green())<10&&qAbs(mid.blue()-around.blue())<10)
                return mid.name();
        }
    }
    return {};
}
QStringList iconNames() {
    auto names = shapes.keys();
    names.sort();
    return names;
}
namespace {
QByteArray iconSvg(const QString &name, const QColor &color) {
    return QString("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='%1' stroke-opacity='%2' stroke-width='1.8' stroke-linecap='round' stroke-linejoin='round'>%3</svg>")
        .arg(color.name(), QString::number(color.alphaF()), shapes.value(name)).toUtf8();
}
QStringList normalizedLabels(const QStringList &labels) {
    QStringList result;
    for (const auto &label : labels) {
        auto n=normalizeActionText(label);
        if (!n.isEmpty() && !result.contains(n)) result.append(n);
    }
    result.sort();
    return result;
}
QStringList anchorLabels(const QJsonObject &o) {
    QStringList a;
    for (const auto &v : o.value("menuContains").toArray()) a.append(v.toString());
    return normalizedLabels(a);
}
QJsonObject canonicalSelector(const QJsonObject &o) {
    QJsonObject result;
    auto text=normalizeActionText(o.value("text").toString());
    if (!text.isEmpty()) result["text"]=text;
    for (const auto *key: {"objectName","menuObjectName"})
        if (!o.value(key).toString().isEmpty()) result[key]=o.value(key);
    const auto anchors=anchorLabels(o);
    if (!anchors.isEmpty()) result["menuContains"]=QJsonArray::fromStringList(anchors);
    return result;
}
struct Rule {
    QString text, objectName, menuObjectName, icon;
    QColor color;
    bool replaceExisting = false;
    QStringList anchors;
    QJsonObject raw;
    MotionSpec motion;
    bool matches(const QString &label, const QString &id, const QString &menuId, const QStringList &labels) const {
        if ((!menuObjectName.isEmpty() && menuObjectName!=menuId) ||
            (!objectName.isEmpty() && objectName!=id) ||
            (!text.isEmpty() && normalizeActionText(label)!=text)) return false;
        return std::all_of(anchors.begin(), anchors.end(), [&labels](const QString &a){return labels.contains(a);});
    }
};
struct Rules {
    QString path;
    Logger logger;
    QByteArray lastRead;
    bool enabled = true, inspect = false, motion = false, compactMenus = true, showSubmenuSelections = false;
    std::vector<Rule> entries, overflowEntries, hiddenEntries;
    struct Custom {
        QString id,label,program,icon,menuObjectName;
        QStringList args,anchors;
        QJsonObject raw;
        bool matches(const QString &name,const QStringList &labels) const {
            if(!menuObjectName.isEmpty()&&menuObjectName!=name)return false;
            return std::all_of(anchors.begin(),anchors.end(),[&labels](const QString &anchor){return labels.contains(anchor);});
        }
    };
    std::vector<Custom> customEntries;
    GradientSpec gradient;
    QJsonObject gradientRaw;
    explicit Rules(QString file, Logger sink): path(std::move(file)), logger(std::move(sink)) {
        if (!QFileInfo::exists(path)) {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QSaveFile out(path);
            if (out.open(QIODevice::WriteOnly) && out.write(defaults)==defaults.size()) out.commit();
        }
        parse(defaults);
    }
    void log(const QString &message) const {
        if (logger) logger(message);
        else qInfo().noquote() << "[ui-augment]" << message;
    }
    bool parse(const QByteArray &bytes) {
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(bytes, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;
        auto root = doc.object();
        if (!root.value("rules").isArray() || (root.contains("enabled") && !root.value("enabled").isBool()) ||
            (root.contains("inspect") && !root.value("inspect").isBool()) ||
            (root.contains("motion") && !root.value("motion").isBool()) || (root.contains("compactMenus") && !root.value("compactMenus").isBool()) ||
            (root.contains("showSubmenuSelections")&&!root.value("showSubmenuSelections").isBool())) return false;
        GradientSpec nextGradient;
        if(!GradientSpec::parse(root.value("gradient"),nextGradient))return false;
        std::vector<Rule> next;
        for (const auto &v : root.value("rules").toArray()) {
            if (!v.isObject()) return false;
            auto o = v.toObject();
            Rule r{normalizeActionText(o.value("text").toString()), o.value("objectName").toString(),
                   o.value("menuObjectName").toString(), o.value("icon").toString(),
                   QColor(o.value("color").toString()), o.value("replaceExisting").toBool(false), anchorLabels(o), o};
            if ((r.text.isEmpty() && r.objectName.isEmpty()) || !shapes.contains(r.icon) ||
                (o.contains("color") && !r.color.isValid()) ||
                (o.contains("replaceExisting") && !o.value("replaceExisting").isBool()) ||
                (o.contains("menuObjectName") && !o.value("menuObjectName").isString()) ||
                (o.contains("text") && !o.value("text").isString()) ||
                (o.contains("objectName") && !o.value("objectName").isString())) return false;
            if (o.contains("menuContains")) {
                if (!o.value("menuContains").isArray()) return false;
                for (const auto &v : o.value("menuContains").toArray())
                    if (!v.isString() || normalizeActionText(v.toString()).isEmpty()) return false;
            }
            if (!MotionSpec::parse(o.value("animation"),r.motion)) return false;
            next.push_back(std::move(r));
        }
        std::vector<Rule> nextOverflow;
        if(root.contains("menuOverflow")&&!root.value("menuOverflow").isArray())return false;
        for(const auto &v:root.value("menuOverflow").toArray()) {
            if(!v.isObject())return false;
            const auto o=v.toObject();
            for(const auto *key:{"text","objectName","menuObjectName"})
                if(o.contains(key)&&!o.value(key).isString())return false;
            if(o.contains("menuContains")){
                if(!o.value("menuContains").isArray())return false;
                for(const auto &label:o.value("menuContains").toArray())
                    if(!label.isString()||normalizeActionText(label.toString()).isEmpty())return false;
            }
            Rule r{normalizeActionText(o.value("text").toString()),o.value("objectName").toString(),
                   o.value("menuObjectName").toString(),{},QColor{},false,anchorLabels(o),o};
            if(r.text.isEmpty()&&r.objectName.isEmpty())return false;
            nextOverflow.push_back(std::move(r));
        }
        std::vector<Rule> nextHidden;
        if(root.contains("menuHidden")&&!root.value("menuHidden").isArray())return false;
        for(const auto &v:root.value("menuHidden").toArray()) {
            if(!v.isObject())return false;
            const auto o=v.toObject();
            for(const auto *key:{"text","objectName","menuObjectName"})
                if(o.contains(key)&&!o.value(key).isString())return false;
            if(o.contains("menuContains")){
                if(!o.value("menuContains").isArray())return false;
                for(const auto &label:o.value("menuContains").toArray())
                    if(!label.isString()||normalizeActionText(label.toString()).isEmpty())return false;
            }
            Rule r{normalizeActionText(o.value("text").toString()),o.value("objectName").toString(),
                o.value("menuObjectName").toString(),{},QColor{},false,anchorLabels(o),o};
            if(r.text.isEmpty()&&r.objectName.isEmpty())return false;
            nextHidden.push_back(std::move(r));
        }
        std::vector<Custom> nextCustom;
        QSet<QString> customIds;
        if(root.contains("menuCustom")&&!root.value("menuCustom").isArray())return false;
        for(const auto &v:root.value("menuCustom").toArray()){
            if(!v.isObject())return false;
            const auto o=v.toObject();
            const auto id=o.value("id").toString(),label=o.value("label").toString(),program=o.value("program").toString();
            if(id.trimmed().isEmpty()||id.size()>96||customIds.contains(id)
                ||label.trimmed().isEmpty()||label.size()>120
                ||program.trimmed().isEmpty()||program.size()>512
                ||!o.value("args").isArray())return false;
            const auto icon=o.value("icon").toString();
            if(o.contains("icon")&&(!o.value("icon").isString()||!icon.isEmpty()&&!shapes.contains(icon)))return false;
            if(o.contains("menuObjectName")&&!o.value("menuObjectName").isString())return false;
            if(o.contains("menuContains")) {
                if(!o.value("menuContains").isArray())return false;
                for(const auto &anchor:o.value("menuContains").toArray())
                    if(!anchor.isString()||normalizeActionText(anchor.toString()).isEmpty())return false;
            }
            const auto anchors=anchorLabels(o);
            if(o.value("menuObjectName").toString().isEmpty()&&anchors.isEmpty())return false;
            QStringList args;
            if(o.value("args").toArray().size()>32)return false;
            for(const auto &a:o.value("args").toArray()){
                if(!a.isString()||a.toString().size()>2048||a.toString().contains(QChar(0)))return false;
                args.append(a.toString());
            }
            if(program.contains(QChar(0))||program.contains(QChar('\n')))return false;
            customIds.insert(id);
            nextCustom.push_back({id,label,program,icon,o.value("menuObjectName").toString(),args,anchors,o});
        }
        overflowEntries=std::move(nextOverflow);
        hiddenEntries=std::move(nextHidden);
        customEntries=std::move(nextCustom);
        enabled = root.value("enabled").toBool(true);
        inspect = root.value("inspect").toBool(false);
        motion = root.value("motion").toBool(false);
        compactMenus = root.value("compactMenus").toBool(true);
        showSubmenuSelections = root.value("showSubmenuSelections").toBool(false);
        entries = std::move(next);
        gradient=nextGradient;
        gradientRaw=root.value("gradient").toObject();
        return true;
    }
    void refresh() {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return;
        auto bytes = file.read(256*1024+1);
        if (bytes == lastRead) return;
        lastRead = bytes;
        if (bytes.size()>256*1024 || !parse(bytes))
            log("Invalid rules; retaining last valid configuration");
    }
    const Rule *matchNormalized(const QString &text, const QString &id, const QString &menuId, const QStringList &labels) const {
        for (const auto &r : entries)
            if (r.matches(text,id,menuId,labels)) return &r;
        return nullptr;
    }
    const Rule *matchTarget(const QString &text, const QString &id, const QString &menuId, const QStringList &labels) const {
        return matchNormalized(text,id,menuId,normalizedLabels(labels));
    }
};

class Overlay final : public QWidget {
    struct Slot {
        QPointer<QAction> action;
        qint64 key;
        QIcon originalIcon;
        bool originallyVisible;
        QString icon;
        QColor color;
        MotionSpec motion;
        MotionTrack hover;
        qint64 delay=0;
        QColor cachedColor;
        QPixmap cachedIcon;
        qreal cachedDpr=0;
    };
    struct SubmenuStatus {
        QPointer<QAction> action;
        QString original, applied;
        QMetaObject::Connection connection;
    };
    std::vector<SubmenuStatus> submenuStatus_;
    QMenu *menu;
    MenuLayout layout;
    std::vector<Slot> slots_;
    bool prepared_ = false;
    bool motionEnabled_=false;
    QElapsedTimer elapsed_;
    QTimer *clock_;
    QPointer<QAction> hovered_;
    qint64 now() const {return elapsed_.isValid()?elapsed_.elapsed():0;}
    QRect iconRegion(const Slot &s) const {
        if(!s.action || !menu->actions().contains(s.action))return {};
        const QRect row=menu->actionGeometry(s.action);
        if(!row.isValid() || !rect().contains(row))return {};
        return menu->layoutDirection()==Qt::RightToLeft ?
            QRect(row.right()-25,row.top(),26,row.height()) : QRect(row.left(),row.top(),26,row.height());
    }
    bool moving(const Slot &s,qint64 t) const {
        return motionEnabled_ && s.motion.enabled() && (t<s.delay+s.motion.duration || s.hover.running(t));
    }
    void tick() {
        if (!isVisible()) {clock_->stop();return;}
        const auto t=now();
        bool active=false;
        // Repaint the final frame too. Qt repaints the parent backing store for
        // transparent children; the native menu owns all non-icon pixels.
        for (const auto &s:slots_) {
            if(!s.motion.enabled())continue;
            const auto region=iconRegion(s);
            if(region.isValid())update(region);
            active|=moving(s,t);
        }
        if(!active)clock_->stop();
    }
public:
    explicit Overlay(QMenu *m):QWidget(m),menu(m),layout(m),clock_(new QTimer(this)) {
        clock_->setObjectName("augment-motion-clock");
        clock_->setTimerType(Qt::PreciseTimer);
        clock_->setInterval(16);
        connect(clock_,&QTimer::timeout,this,[this]{tick();});
        setObjectName("obs-ui-augment-overlay");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        hide();
    }
    ~Overlay() override { restore(); }
    bool prepared() const {return prepared_;}
    void restoreLayout(){layout.restore();}
    void layoutVisible(){layout.present();}
    void closePopup(){
        restore();
        // Native activation and QMenu::exec finish after Hide.
        QTimer::singleShot(0,this,[this]{if(!menu->isVisible())layout.restore();});
    }
    void restore() {
        for(auto &status:submenuStatus_) {
            QObject::disconnect(status.connection);
            if(!status.action)continue;
            if(status.action->text()==status.applied)status.action->setText(status.original);
            status.action->setProperty("_aftermarketBaseText",QVariant());
        }
        submenuStatus_.clear();
        clock_->stop();elapsed_.invalidate();hovered_.clear();motionEnabled_=false;
        hide();
        auto old = std::move(slots_);
        slots_.clear();
        prepared_ = false;
        for (auto &s : old) {
            if (!s.action) continue;
            if (s.action->icon().cacheKey()==s.key) {
                s.action->setIcon(s.originalIcon);
                s.action->setIconVisibleInMenu(s.originallyVisible);
            }
            s.action->setProperty("_obsUiAugmentReservation",QVariant());
            s.action->setProperty("_obsUiAugmentOriginalIcon",QVariant());
        }
    }
    void prepare(Rules &rules) {
        if (prepared_) return;
        layout.restore();
        QSet<QAction*> selected,hidden;
        const auto labelsBefore=normalizedLabels(logicalMenuLabels(menu));
        if(rules.enabled&&!menu->property("_aftermarketOverflowPage").toBool()) {
            for(auto *a:menu->actions())if(a->isVisible()&&!a->isSeparator()) {
                for(const auto &r:rules.overflowEntries)
                    if(r.matches(a->text(),a->objectName(),menu->objectName(),labelsBefore)){selected.insert(a);break;}
                for(const auto &r:rules.hiddenEntries)
                    if(r.matches(a->text(),a->objectName(),menu->objectName(),labelsBefore)){hidden.insert(a);break;}
            }
        }
        layout.prepare(rules.enabled,selected,hidden,rules.compactMenus);
        if(rules.enabled&&!menu->property("_aftermarketOverflowPage").toBool()) {
            for(const auto &entry:rules.customEntries) {
                if(!entry.matches(menu->objectName(),labelsBefore))continue;
                auto *a=new QAction(entry.label,menu);
                a->setProperty("_aftermarketCustomId",entry.id);
                a->setObjectName("aftermarket-custom-"+entry.id);
                if(!entry.icon.isEmpty())a->setIcon(previewIcon(entry.icon,menu->palette().color(QPalette::Text)));
                const QString program=entry.program;
                const QStringList args=entry.args;
                const auto log=rules.logger;
                connect(a,&QAction::triggered,menu,[program,args,log]{
                    if(!QProcess::startDetached(program,args)) {
                        if(log)log("Custom menu command failed to launch: "+program);
                        else qWarning().noquote()<<"[ui-augment] Custom menu command failed to launch:"<<program;
                    }
                });
                layout.trackCustom(a);
                menu->addAction(a);
            }
        }
        prepared_ = true;
        motionEnabled_=rules.motion;
        const auto actions = menu->actions();
        const auto labels=normalizedLabels(logicalMenuLabels(menu));
        for (auto *a : actions) {
            if (rules.inspect)
                rules.log(QString("menu=%1 action=%2 text=%3").arg(menu->objectName(), a->objectName(), a->text()));
            if (!rules.enabled) continue;
            if (overflowAction(a) || a->property("_aftermarketCustomId").isValid() || !a->isVisible() || a->isSeparator() || a->isCheckable() ||
                qobject_cast<QWidgetAction *>(a)) continue;
            const auto *r = rules.matchNormalized(a->text(),a->objectName(),logicalMenuName(menu),labels);
            if (!r || (!a->icon().isNull() && !r->replaceExisting) ||
                a->property("_obsUiAugmentReservation").toBool()) continue;
            int extent = qBound(12,menu->style()->pixelMetric(QStyle::PM_SmallIconSize,nullptr,menu),32);
            QPixmap blank(extent,extent);
            blank.fill(Qt::transparent);
            QIcon reservation(blank);
            slots_.push_back({a,reservation.cacheKey(),a->icon(),a->isIconVisibleInMenu(),r->icon,r->color,r->motion,{},qMin<qint64>(72,slots_.size()*9)});
            a->setProperty("_obsUiAugmentOriginalIcon",!a->icon().isNull());
            a->setProperty("_obsUiAugmentReservation",true);
            a->setIcon(reservation);
            a->setIconVisibleInMenu(true);
        }
        if(rules.enabled&&rules.showSubmenuSelections&&!menu->property("_aftermarketOverflowPage").toBool()) {
            for(auto *action:menu->actions())if(action->menu()&&!action->property("_aftermarketOverflowAction").toBool()) {
                auto *child=action->menu();
                submenuStatus_.push_back({action,action->text(),{},{}});
                const auto index=submenuStatus_.size()-1;
                auto updateStatus=[this,index]{
                    if(index>=submenuStatus_.size())return;
                    auto &slot=submenuStatus_[index];
                    if(!slot.action||!slot.action->menu())return;
                    auto current=slot.action->text();
                    if(!slot.applied.isEmpty()&&current!=slot.applied&&current!=slot.original)
                        slot.original=current; // host changed the label, respect it
                    const auto choice=submenuSelection(slot.action->menu());
                    const auto next=choice.isEmpty()?slot.original:slot.original+"  ·  "+choice;
                    if(current!=next)slot.action->setText(next);
                    slot.applied=next;
                    slot.action->setProperty("_aftermarketBaseText",slot.original);
                };
                updateStatus();
                submenuStatus_[index].connection=connect(child,&QMenu::aboutToShow,this,[this,index] {
                    // Qt/OBS may refresh check marks during aboutToShow. Read
                    // them on the following event-loop turn, not beforehand.
                    QTimer::singleShot(0,this,[this,index] {
                        if(!menu->isVisible()||index>=submenuStatus_.size())return;
                        auto &slot=submenuStatus_[index];
                        if(!slot.action||!slot.action->menu())return;
                        const auto choice=submenuSelection(slot.action->menu());
                        const auto next=choice.isEmpty()?slot.original:slot.original+"  ·  "+choice;
                        if(slot.action->text()==slot.applied&&slot.action->text()!=next) {
                            slot.action->setText(next);
                            slot.applied=next;
                            menu->adjustSize();
                        }
                    });
                });
            }
        }
        if (rules.inspect && !slots_.empty())
            rules.log(QString("Painted menu candidates: %1").arg(slots_.size()));
    }
    void present() {
        if (slots_.empty()) return;
        setGeometry(menu->rect());
        raise(); show(); update();
        if(motionEnabled_ && !elapsed_.isValid()) {
            elapsed_.start();
            if(std::any_of(slots_.begin(),slots_.end(),[](const Slot &s){return s.motion.enabled();}))
                clock_->start();
        }
    }
    void hover(QAction *action) {
        if (action && !action->isEnabled()) action=nullptr;
        if (hovered_==action) return;
        hovered_=action;
        if (!motionEnabled_ || !elapsed_.isValid()) {update();return;}
        auto t=now();
        bool active=false;
        for(auto &s:slots_) {
            if(!s.motion.enabled())continue;
            s.hover.retarget(s.action==action?1.:0.,t,s.motion.duration);
            active|=moving(s,t);
        }
        if(active && isVisible() && !clock_->isActive())clock_->start();
        update();
    }
protected:
    void paintEvent(QPaintEvent *event) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto t=now();
        for (auto &s : slots_) {
            if (!s.action || !s.action->isVisible() || s.action->isCheckable() ||
                s.action->icon().cacheKey()!=s.key || !menu->actions().contains(s.action)) continue;
            const QRect row = menu->actionGeometry(s.action);
            if (!row.isValid() || !rect().contains(row)) continue;
            const int size = qMin(16, row.height()-4);
            if (size<8) continue;
            const int x = menu->layoutDirection()==Qt::RightToLeft ? row.right()-6-size : row.left()+6;
            const QRect target(x,row.center().y()-size/2,size,size);
            QColor color = s.color.isValid() ? s.color : menu->palette().color(QPalette::Text);
            if (!s.action->isEnabled()) color.setAlpha(100);
            const auto region=iconRegion(s);
            if(!event->region().intersects(region))continue;
            const auto dpr=devicePixelRatioF();
            if(s.cachedIcon.isNull() || s.cachedColor!=color || s.cachedDpr!=dpr) {
                s.cachedColor=color;s.cachedDpr=dpr;
                const int physical=qCeil(32*dpr);
                s.cachedIcon=QPixmap(physical,physical);
                s.cachedIcon.setDevicePixelRatio(dpr);
                s.cachedIcon.fill(Qt::transparent);
                QPainter cache(&s.cachedIcon);
                cache.setRenderHint(QPainter::Antialiasing);
                QSvgRenderer svg(iconSvg(s.icon,color));
                svg.render(&cache,QRectF(0,0,32,32));
            }
            qreal alpha=1,scale=1,shift=0,angle=0;
            if(motionEnabled_ && s.motion.enabled() && elapsed_.isValid()) {
                const auto progress=qBound(0.,double(t-s.delay)/s.motion.duration,1.);
                const auto entrance=QEasingCurve(QEasingCurve::OutCubic).valueForProgress(progress);
                const auto hover=s.hover.value(t);
                alpha=entrance;
                shift=-3*(1-entrance);
                if(menu->layoutDirection()==Qt::RightToLeft)shift=-shift;
                if(s.motion.preset=="pop")scale=.72+.28*entrance+.18*hover;
                else if(s.motion.preset=="spin") {scale=.88+.12*entrance;angle=-18*(1-entrance)+28*hover;}
                else scale=.90+.10*entrance+.10*hover;
            }
            p.save();
            p.setClipRect(region);
            p.setOpacity(alpha);
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            p.translate(QRectF(target).center()+QPointF(shift,0));
            p.rotate(angle);
            p.scale(scale,scale);
            p.drawPixmap(QRectF(-size/2.,-size/2.,size,size),s.cachedIcon,QRectF(s.cachedIcon.rect()));
            p.restore();
        }
    }
};
}

QIcon previewIcon(const QString &name, const QColor &color) {
    if (!shapes.contains(name) || !color.isValid()) return {};
    QPixmap pixmap(24,24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    QSvgRenderer svg(iconSvg(name,color));
    svg.render(&painter);
    painter.end();
    return QIcon(pixmap);
}

struct Controller::State {
    Rules rules;
    QHash<QMenu*,Overlay*> menus;
    GradientController gradients;
    QFileSystemWatcher watcher;
    QTimer reload;
    void refreshGradients(){auto spec=rules.gradient;spec.enabled &= rules.enabled;spec.knobs &= rules.enabled;gradients.configure(spec,rules.motion);}
    explicit State(QString path, Logger logger):rules(std::move(path), std::move(logger)) {}
};
Controller::Controller(QString rulesFile, QObject *parent, Logger logger):QObject(parent),state(new State(std::move(rulesFile), std::move(logger))) {
    qApp->installEventFilter(this);
    state->rules.refresh();state->refreshGradients();
    // Watch both in-place writes and QSaveFile/rename replacements. Re-arm
    // after a replacement, and debounce transient partial writes.
    state->reload.setSingleShot(true);state->reload.setInterval(80);
    auto arm=[this]{
        const auto path=state->rules.path;
        if(QFileInfo::exists(path)&&!state->watcher.files().contains(path))state->watcher.addPath(path);
    };
    state->watcher.addPath(QFileInfo(state->rules.path).absolutePath());arm();
    connect(&state->watcher,&QFileSystemWatcher::fileChanged,this,[this]{state->reload.start();});
    connect(&state->watcher,&QFileSystemWatcher::directoryChanged,this,[this]{state->reload.start();});
    connect(&state->reload,&QTimer::timeout,this,[this,arm]{arm();state->rules.refresh();state->refreshGradients();});
    for (auto *w : QApplication::allWidgets())
        if (auto *menu = qobject_cast<QMenu*>(w)) attach(menu);
}
Controller::~Controller() {
    if (qApp) qApp->removeEventFilter(this);
    closeThemePickers();
    for (auto *menu : state->menus.keys()) QObject::disconnect(menu,nullptr,this,nullptr);
    const auto overlays=state->menus.values();
    state->menus.clear();
    for (auto *overlay : overlays) overlay->restore();
    for (auto *overlay : overlays) overlay->restoreLayout();
    for (auto *overlay : overlays) delete overlay;
}
namespace {
constexpr qint64 maxRuleBytes=256*1024;
bool fail(QString *error, const QString &message) {
    if (error) *error=message;
    return false;
}
bool editRules(Rules &current, const QJsonObject &entry, bool remove, QString *error, const bool *motion=nullptr, const QJsonObject *gradient=nullptr, const QJsonObject *expected=nullptr, const bool *compactMenus=nullptr, const bool *overflow=nullptr, const bool *hidden=nullptr, const QJsonObject *custom=nullptr,const QString *removeCustom=nullptr,const bool *submenuSelections=nullptr) {
    // Read the actual latest file, not the controller's last-good cache.
    QFile in(current.path);
    if (!in.open(QIODevice::ReadOnly)) return fail(error,"Cannot read rules.json");
    auto before=in.read(maxRuleBytes+1);
    in.close();
    Rules validator=current;
    if (before.size()>maxRuleBytes || !validator.parse(before))
        return fail(error,"Invalid rules.json; fix the file before editing.");
    auto root=QJsonDocument::fromJson(before).object();
    if (gradient) {
        if(expected&&root.value("gradient").toObject()!=*expected)
            return fail(error,"Settings changed elsewhere. Reload before applying.");
        root["gradient"]=*gradient;
    }
    else if (motion) root["motion"]=*motion;
    else if (compactMenus) root["compactMenus"]=*compactMenus;
    else if (submenuSelections) root["showSubmenuSelections"]=*submenuSelections;
    else if(hidden) {
        const auto selector=canonicalSelector(entry);
        if(!selector.contains("text")&&!selector.contains("objectName"))return fail(error,"No valid action selector.");
        QJsonArray next;
        for(const auto &v:root.value("menuHidden").toArray())
            if(canonicalSelector(v.toObject())!=selector)next.append(v);
        if(*hidden)next.append(entry);
        root["menuHidden"]=next;
    }
    else if(custom||removeCustom){
        const auto id=custom?custom->value("id").toString():*removeCustom;
        if(id.isEmpty())return fail(error,"Custom entry needs an ID.");
        QJsonArray next;bool found=false;
        for(const auto &v:root.value("menuCustom").toArray()) {
            if(v.toObject().value("id").toString()==id){found=true;continue;}
            next.append(v);
        }
        if(removeCustom&&!found)return fail(error,"No custom entry with this ID.");
        if(custom)next.append(*custom);
        root["menuCustom"]=next;
    }
    else if (overflow) {
        const auto selector=canonicalSelector(entry);
        if(!selector.contains("text")&&!selector.contains("objectName"))return fail(error,"No valid action selector.");
        QJsonArray next;
        for(const auto &v:root.value("menuOverflow").toArray())
            if(canonicalSelector(v.toObject())!=selector)next.append(v);
        // Keep the original escaped labels, not the normalized comparison key.
        if(*overflow)next.append(entry);
        root["menuOverflow"]=next;
    }
    else {
    auto selector=canonicalSelector(entry);
    if (!selector.contains("text") && !selector.contains("objectName"))
        return fail(error,"No valid action selector.");
    QJsonArray next;
    if (!remove) next.append(entry); // narrow overrides win over broader rules
    bool found=false;
    for (const auto &value : root.value("rules").toArray()) {
        if (canonicalSelector(value.toObject())==selector) {found=true; continue;}
        next.append(value);
    }
    if (remove && !found) return fail(error,"No exact override exists for this selector.");
    root["rules"]=next;
    }
    auto after=QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (after.size()>maxRuleBytes || !validator.parse(after))
        return fail(error,"Invalid rule; nothing was written.");
    // Do not overwrite an edit made while the rule was being prepared.
    QFile check(current.path);
    if (!check.open(QIODevice::ReadOnly) || check.read(maxRuleBytes+1)!=before)
        return fail(error,"rules.json changed; try again.");
    check.close();
    QSaveFile out(current.path);
    if (!out.open(QIODevice::WriteOnly) || out.write(after)!=after.size() || !out.commit())
        return fail(error,"Could not atomically save rules.json.");
    current.refresh();
    if (error) error->clear();
    return true;
}
}
void Controller::setMeterProvider(MeterProvider provider) {state->gradients.setMeterProvider(std::move(provider));}
void Controller::setAudioProvider(AudioProvider provider,AudioReset reset) {
    state->gradients.setAudioProvider(std::move(provider),std::move(reset));
}
QJsonObject Controller::gradientSettings() const {state->rules.refresh();return state->rules.gradientRaw;}
bool Controller::setGradientSettings(const QJsonObject &spec,QString *error,const QJsonObject *expected) {
    if(!editRules(state->rules,{},false,error,nullptr,&spec,expected))return false;
    state->refreshGradients();return true;
}
bool Controller::showSubmenuSelections() const {state->rules.refresh();return state->rules.showSubmenuSelections;}
bool Controller::setShowSubmenuSelections(bool enabled,QString *error) {
    return editRules(state->rules,{},false,error,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&enabled);
}
bool Controller::compactMenusEnabled() const {state->rules.refresh();return state->rules.compactMenus;}
bool Controller::setCompactMenus(bool enabled,QString *error) {
    return editRules(state->rules,{},false,error,nullptr,nullptr,nullptr,&enabled);
}
bool Controller::hasHiddenRule(const QJsonObject &selector) const {
    state->rules.refresh();
    for(const auto &r:state->rules.hiddenEntries)
        if(canonicalSelector(r.raw)==canonicalSelector(selector))return true;
    return false;
}
bool Controller::setHiddenRule(const QJsonObject &selector,bool hidden,QString *error){
    return editRules(state->rules,selector,false,error,nullptr,nullptr,nullptr,nullptr,nullptr,&hidden);
}
QJsonArray Controller::allHiddenRules() const {
    state->rules.refresh();QJsonArray result;
    for(const auto &r:state->rules.hiddenEntries)result.append(r.raw);
    return result;
}
QJsonArray Controller::allCustomActions() const {
    state->rules.refresh();QJsonArray result;
    for(const auto &r:state->rules.customEntries)result.append(r.raw);
    return result;
}
QJsonArray Controller::hiddenRulesForMenu(const QString &name,const QStringList &labels) const {
    state->rules.refresh();
    QJsonArray entries;
    const auto normalized=normalizedLabels(labels);
    for(const auto &r:state->rules.hiddenEntries)
        if((r.menuObjectName.isEmpty()||r.menuObjectName==name)&&
            std::all_of(r.anchors.begin(),r.anchors.end(),[&](const QString &anchor){return normalized.contains(anchor);}))
            entries.append(r.raw);
    return entries;
}
QJsonArray Controller::customActionsForMenu(const QString &name,const QStringList &labels) const {
    state->rules.refresh();
    QJsonArray entries;
    const auto normalized=normalizedLabels(labels);
    for(const auto &r:state->rules.customEntries)if(r.matches(name,normalized))entries.append(r.raw);
    return entries;
}
bool Controller::setCustomAction(const QJsonObject &entry,QString *error){
    return editRules(state->rules,{},false,error,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&entry);
}
bool Controller::removeCustomAction(const QString &id,QString *error){
    return editRules(state->rules,{},false,error,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&id);
}
bool Controller::hasOverflowRule(const QJsonObject &selector) const {
    state->rules.refresh();
    for(const auto &r:state->rules.overflowEntries)
        if(canonicalSelector(r.raw)==canonicalSelector(selector))return true;
    return false;
}
bool Controller::setOverflowRule(const QJsonObject &selector,bool enabled,QString *error) {
    return editRules(state->rules,selector,false,error,nullptr,nullptr,nullptr,nullptr,&enabled);
}
bool Controller::motionEnabled() const {state->rules.refresh();return state->rules.motion;}
bool Controller::setMotionEnabled(bool enabled, QString *error) {
    if(!editRules(state->rules,{},false,error,&enabled))return false;
    state->refreshGradients();return true;
}
bool Controller::saveRule(const QJsonObject &entry, QString *error) {
    return editRules(state->rules,entry,false,error);
}
bool Controller::removeRule(const QJsonObject &selector, QString *error) {
    return editRules(state->rules,selector,true,error);
}
QJsonObject Controller::effectiveRule(const QJsonObject &target, const QStringList &labels) const {
    state->rules.refresh();
    const auto *r=state->rules.matchTarget(target.value("text").toString(),target.value("objectName").toString(),
        target.value("menuObjectName").toString(),labels);
    return r ? r->raw : QJsonObject{};
}
void Controller::attach(QMenu *menu) {
    if (state->menus.contains(menu)) return;
    // Don't attach children while a menu is still being constructed.
    auto *overlay = new Overlay(menu);
    state->menus.insert(menu,overlay);
    connect(menu,&QObject::destroyed,this,[this,menu]{ state->menus.remove(menu);});
    connect(menu,&QMenu::aboutToShow,this,[this,overlay]{
        state->rules.refresh();state->refreshGradients();
        overlay->prepare(state->rules);
    });
    connect(menu,&QMenu::hovered,overlay,[overlay](QAction *action){overlay->hover(action);});
}
bool Controller::eventFilter(QObject *object,QEvent *event) {
    if((event->type()==QEvent::MouseButtonPress||event->type()==QEvent::KeyPress)&&qobject_cast<QComboBox*>(object)) {
        state->rules.refresh();
        if(handleThemePicker(object,event,state->rules.enabled&&state->rules.compactMenus))return true;
    }
    switch(event->type()) {
        case QEvent::Polish:
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::Resize:
        case QEvent::Paint:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::ActionRemoved:
        case QEvent::ActionChanged:
        case QEvent::MouseMove:
        case QEvent::Leave:
        case QEvent::KeyPress:
            break;
        default: return false;
    }
    auto *menu = qobject_cast<QMenu*>(object);
    if (!menu) return false;
    if (event->type()==QEvent::Polish || event->type()==QEvent::Show) attach(menu);
    auto *overlay = state->menus.value(menu,nullptr);
    if (!overlay) return false;
    switch(event->type()) {
        case QEvent::Show:
            if (!overlay->prepared()) {
                state->rules.refresh();state->refreshGradients();
                overlay->prepare(state->rules);
                menu->adjustSize();
            }
            overlay->layoutVisible();overlay->present();
            break;
        case QEvent::Hide: overlay->closePopup(); break;
        case QEvent::Resize: if (menu->isVisible()) overlay->present(); break;
        case QEvent::Paint:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease: overlay->layoutVisible();break;
        case QEvent::MouseMove: overlay->layoutVisible();overlay->hover(menu->actionAt(static_cast<QMouseEvent*>(event)->position().toPoint()));break;
        case QEvent::Leave: overlay->hover(nullptr);break;
        case QEvent::KeyPress: {
            overlay->layoutVisible();
            QPointer<QMenu> safe(menu);
            QTimer::singleShot(0,overlay,[overlay,safe]{if(safe && safe->isVisible())overlay->hover(safe->activeAction());});
            break;
        }
        case QEvent::ActionChanged:
        case QEvent::ActionRemoved: overlay->update(); break;
        default: break;
    }
    return false;
}
}
