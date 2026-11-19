#include "augment.h"
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

QString normalized(QString text) {
    text = text.section('\t', 0, 0);
    text.replace("&&", "\x01");
    text.remove('&');
    text.replace(QChar(1), '&');
    text = text.trimmed();
    if (text.endsWith("...")) text.chop(3);
    else if (text.endsWith(QChar(0x2026))) text.chop(1);
    return text.trimmed().toCaseFolded();
}

struct Rule { QString text, objectName, menuObjectName, icon; QColor color; bool replaceExisting = false; };
struct Rules {
    QString path;
    Logger logger;
    QByteArray lastRead;
    bool enabled = true, inspect = false;
    std::vector<Rule> entries;
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
            (root.contains("inspect") && !root.value("inspect").isBool())) return false;
        std::vector<Rule> next;
        for (const auto &v : root.value("rules").toArray()) {
            if (!v.isObject()) return false;
            auto o = v.toObject();
            Rule r{normalized(o.value("text").toString()), o.value("objectName").toString(),
                   o.value("menuObjectName").toString(), o.value("icon").toString(),
                   QColor(o.value("color").toString()), o.value("replaceExisting").toBool(false)};
            if ((r.text.isEmpty() && r.objectName.isEmpty()) || !shapes.contains(r.icon) ||
                (o.contains("color") && !r.color.isValid()) ||
                (o.contains("replaceExisting") && !o.value("replaceExisting").isBool()) ||
                (o.contains("menuObjectName") && !o.value("menuObjectName").isString()) ||
                (o.contains("text") && !o.value("text").isString()) ||
                (o.contains("objectName") && !o.value("objectName").isString())) return false;
            next.push_back(std::move(r));
        }
        enabled = root.value("enabled").toBool(true);
        inspect = root.value("inspect").toBool(false);
        entries = std::move(next);
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
    const Rule *match(const QAction *a, const QMenu *menu) const {
        for (const auto &r : entries)
            if ((!r.menuObjectName.isEmpty() && menu->objectName()!=r.menuObjectName) ||
                (!r.objectName.isEmpty() && a->objectName()!=r.objectName) ||
                (!r.text.isEmpty() && normalized(a->text())!=r.text)) continue;
            else return &r;
        return nullptr;
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
    };
    QMenu *menu;
    std::vector<Slot> slots_;
    bool prepared_ = false;
public:
    explicit Overlay(QMenu *m):QWidget(m),menu(m) {
        setObjectName("obs-ui-augment-overlay");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        hide();
    }
    ~Overlay() override { restore(); }
    bool prepared() const {return prepared_;}
    void restore() {
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
        }
    }
    void prepare(Rules &rules) {
        if (prepared_) return;
        prepared_ = true;
        const auto actions = menu->actions();
        for (auto *a : actions) {
            if (rules.inspect)
                rules.log(QString("menu=%1 action=%2 text=%3").arg(menu->objectName(), a->objectName(), a->text()));
            if (!rules.enabled) continue;
            if (!a->isVisible() || a->isSeparator() || a->isCheckable() ||
                qobject_cast<QWidgetAction *>(a)) continue;
            const auto *r = rules.match(a, menu);
            if (!r || (!a->icon().isNull() && !r->replaceExisting) ||
                a->property("_obsUiAugmentReservation").toBool()) continue;
            int extent = qBound(12,menu->style()->pixelMetric(QStyle::PM_SmallIconSize,nullptr,menu),32);
            QPixmap blank(extent,extent);
            blank.fill(Qt::transparent);
            QIcon reservation(blank);
            slots_.push_back({a,reservation.cacheKey(),a->icon(),a->isIconVisibleInMenu(),r->icon,r->color});
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
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        for (const auto &s : slots_) {
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
            const auto svg = QString("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='%1' stroke-opacity='%2' stroke-width='1.8' stroke-linecap='round' stroke-linejoin='round'>%3</svg>")
                .arg(color.name(),QString::number(color.alphaF()),shapes.value(s.icon));
            QSvgRenderer renderer(svg.toUtf8());
            p.save();
            p.setClipRect(row);
            renderer.render(&p,target);
            p.restore();
        }
    }
};
}

struct Controller::State {
    Rules rules;
    QHash<QMenu*,Overlay*> menus;
    explicit State(QString path, Logger logger):rules(std::move(path), std::move(logger)) {}
};
Controller::Controller(QString rulesFile, QObject *parent, Logger logger):QObject(parent),state(new State(std::move(rulesFile), std::move(logger))) {
    qApp->installEventFilter(this);
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
void Controller::attach(QMenu *menu) {
    if (state->menus.contains(menu)) return;
    // Don't attach children while a menu is still being constructed.
    auto *overlay = new Overlay(menu);
    state->menus.insert(menu,overlay);
    connect(menu,&QObject::destroyed,this,[this,menu]{ state->menus.remove(menu);});
    connect(menu,&QMenu::aboutToShow,this,[this,overlay]{
        state->rules.refresh();
        overlay->prepare(state->rules);
    });
    connect(menu,&QMenu::hovered,overlay,[overlay]{overlay->update();});
}
bool Controller::eventFilter(QObject *object,QEvent *event) {
    switch(event->type()) {
        case QEvent::Polish:
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::Resize:
        case QEvent::ActionRemoved:
        case QEvent::ActionChanged:
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
                state->rules.refresh();
                overlay->prepare(state->rules);
                menu->adjustSize();
            }
            overlay->present();
            break;
        case QEvent::Hide: overlay->restore(); break;
        case QEvent::Resize: if (menu->isVisible()) overlay->present(); break;
        case QEvent::ActionChanged:
        case QEvent::ActionRemoved: overlay->update(); break;
        default: break;
    }
    return false;
}
}
