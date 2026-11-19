#include "augment.h"
#include "motion.h"
#include "gradient.h"
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QPaintEvent>
#include <QtMath>
#include <QTimer>
#include <QMouseEvent>
#include <QAction>
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
#include <QSaveFile>
#include <QStyle>
#include <QSvgRenderer>
#include <QWidgetAction>
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
    bool enabled = true, inspect = false, motion = false;
    std::vector<Rule> entries;
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
            (root.contains("motion") && !root.value("motion").isBool())) return false;
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
        enabled = root.value("enabled").toBool(true);
        inspect = root.value("inspect").toBool(false);
        motion = root.value("motion").toBool(false);
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
    QMenu *menu;
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
    explicit Overlay(QMenu *m):QWidget(m),menu(m),clock_(new QTimer(this)) {
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
    void restore() {
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
        prepared_ = true;
        motionEnabled_=rules.motion;
        const auto actions = menu->actions();
        QStringList menuLabels;
        for (auto *action : actions)
            if (action->isVisible() && !action->isSeparator()) menuLabels.append(action->text());
        const auto labels=normalizedLabels(menuLabels);
        for (auto *a : actions) {
            if (rules.inspect)
                rules.log(QString("menu=%1 action=%2 text=%3").arg(menu->objectName(), a->objectName(), a->text()));
            if (!rules.enabled) continue;
            if (!a->isVisible() || a->isSeparator() || a->isCheckable() ||
                qobject_cast<QWidgetAction *>(a)) continue;
            const auto *r = rules.matchNormalized(a->text(),a->objectName(),menu->objectName(),labels);
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
    for (auto *menu : state->menus.keys()) QObject::disconnect(menu,nullptr,this,nullptr);
    const auto overlays=state->menus.values();
    state->menus.clear();
    for (auto *overlay : overlays) delete overlay;
}
namespace {
constexpr qint64 maxRuleBytes=256*1024;
bool fail(QString *error, const QString &message) {
    if (error) *error=message;
    return false;
}
bool editRules(Rules &current, const QJsonObject &entry, bool remove, QString *error, const bool *motion=nullptr, const QJsonObject *gradient=nullptr) {
    // Read the actual latest file, not the controller's last-good cache.
    QFile in(current.path);
    if (!in.open(QIODevice::ReadOnly)) return fail(error,"Cannot read rules.json");
    auto before=in.read(maxRuleBytes+1);
    in.close();
    Rules validator=current;
    if (before.size()>maxRuleBytes || !validator.parse(before))
        return fail(error,"Invalid rules.json; fix the file before editing.");
    auto root=QJsonDocument::fromJson(before).object();
    if (gradient) root["gradient"]=*gradient;
    else if (motion) root["motion"]=*motion;
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
bool Controller::setGradientSettings(const QJsonObject &spec,QString *error) {
    if(!editRules(state->rules,{},false,error,nullptr,&spec))return false;
    state->refreshGradients();return true;
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
    switch(event->type()) {
        case QEvent::Polish:
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::Resize:
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
            overlay->present();
            break;
        case QEvent::Hide: overlay->restore(); break;
        case QEvent::Resize: if (menu->isVisible()) overlay->present(); break;
        case QEvent::MouseMove: overlay->hover(menu->actionAt(static_cast<QMouseEvent*>(event)->position().toPoint()));break;
        case QEvent::Leave: overlay->hover(nullptr);break;
        case QEvent::KeyPress: {
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
