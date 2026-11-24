#include "row-stripes.h"
#include <QApplication>
#include <QAbstractItemView>
#include <QListView>
#include <QPalette>
#include <QHash>
#include <QPointer>
#include <QEvent>
#include <QColor>
#include <QWidget>
#include <algorithm>

namespace augment {
namespace {
QString scope(const QAbstractItemView *view){
    if(!view)return {};
    if(view->objectName()==QStringLiteral("sources")&&view->inherits("SourceTree"))
        return QStringLiteral("sources");
    if(view->objectName()==QStringLiteral("scenes")&&view->inherits("SceneTree"))
        return QStringLiteral("scenes");
    return {};
}
QColor blend(QColor base,QColor accent,int weight){
    // 100% is exactly the existing alternate color. Up to 200% amplifies
    // contrast without inventing a different hue; channels stay in gamut.
    const auto channel=[weight](int native,int alt){
        return std::clamp(native+(alt-native)*weight/100,0,255);
    };
    return QColor(channel(base.red(),accent.red()),
                  channel(base.green(),accent.green()),
                  channel(base.blue(),accent.blue()));
}
QColor chosenColor(const QJsonObject &settings,const char *key,QColor fallback){
    const QColor custom(settings.value(QLatin1String(key)).toString());
    return custom.isValid()?custom:fallback;
}
QPalette keepNativeRows(QPalette incoming,const QPalette &applied,const QPalette &original){
    for(const auto group:{QPalette::Active,QPalette::Inactive,QPalette::Disabled})
        for(const auto role:{QPalette::Base,QPalette::AlternateBase})
            if(incoming.brush(group,role)==applied.brush(group,role))
                incoming.setBrush(group,role,original.brush(group,role));
    return incoming;
}
}
struct RowStripes::State {
    struct Entry {
        QPointer<QAbstractItemView> view;
        QPalette original,applied;
        QString nativeStyleSheet,appliedStyleSheet;
        bool originalAlternating=false;
        bool applying=false,paused=false;
    };
    QHash<QAbstractItemView*,Entry> rows;
    QJsonObject settings;
};
RowStripes::RowStripes(QObject *parent):QObject(parent),state(new State){
    if(qApp)qApp->installEventFilter(this);
}
RowStripes::~RowStripes(){
    if(qApp)qApp->removeEventFilter(this);
    const auto views=state->rows.keys();
    for(auto *view:views)restore(view);
}
QJsonObject RowStripes::settings() const{return state->settings;}
void RowStripes::configure(const QJsonObject &settings){
    state->settings=settings;
    refresh();
}
void RowStripes::restore(QAbstractItemView *view){
    const auto it=state->rows.find(view);
    if(it==state->rows.end())return;
    const auto entry=it.value();state->rows.erase(it);
    if(!entry.view)return;
    // Undo only our palette roles if the native style changed underneath.
    const QPalette current=entry.view->palette();
    const QPalette target=current==entry.applied ? entry.original :
        keepNativeRows(current,entry.applied,entry.original);
    if(current!=target)entry.view->setPalette(target);
    if(entry.view->styleSheet()==entry.appliedStyleSheet &&
       entry.appliedStyleSheet!=entry.nativeStyleSheet)
        entry.view->setStyleSheet(entry.nativeStyleSheet);
    if(entry.view->alternatingRowColors()!=entry.originalAlternating)
        entry.view->setAlternatingRowColors(entry.originalAlternating);
}
void RowStripes::updateView(QAbstractItemView *view){
    const QString type=scope(view);
    if(type.isEmpty())return;
    const bool enabled=state->settings.value(type).toBool(false);
    // OBS Scenes can also switch to a tile grid; row parity has no meaning
    // there, so restore the native palette until it returns to ListMode.
    const auto *list=qobject_cast<QListView*>(view);
    if(!enabled){restore(view);return;}
    auto it=state->rows.find(view);
    if(type==QStringLiteral("scenes")&&list&&list->viewMode()!=QListView::ListMode){
        // Keep the native palette in grid mode, but retain the initial palette
        // snapshot so QSS transitions cannot turn an applied stripe into a
        // false "native" palette when OBS switches back to a list.
        if(it!=state->rows.end()&&!it->paused){
            it->paused=true;
            it->applying=true;
            const auto current=view->palette();
            const auto native=current==it->applied?it->original:
                keepNativeRows(current,it->applied,it->original);
            if(current!=native)view->setPalette(native);
            if(view->styleSheet()==it->appliedStyleSheet &&
               it->appliedStyleSheet!=it->nativeStyleSheet)
                view->setStyleSheet(it->nativeStyleSheet);
            view->setAlternatingRowColors(it->originalAlternating);
            it->applying=false;
        }
        return;
    }
    if(it==state->rows.end()){
        State::Entry e;e.view=view;
        e.original=view->palette();
        e.nativeStyleSheet=view->styleSheet();
        e.originalAlternating=view->alternatingRowColors();
        it=state->rows.insert(view,e);
        connect(view,&QObject::destroyed,this,[this,view]{state->rows.remove(view);});
    }
    if(it->applying)return;
    if(it->paused){
        it->original=keepNativeRows(view->palette(),it->applied,it->original);
        it->paused=false;
    }
    auto pal=it->original;
    const QColor nativeBase=pal.color(QPalette::Base);
    QColor nativeAlternate=pal.color(QPalette::AlternateBase);
    if(nativeAlternate==nativeBase)
        nativeAlternate=blend(nativeBase,pal.color(QPalette::Highlight),12);
    const QColor base=chosenColor(state->settings,"base",nativeBase);
    const QColor targetAlternate=chosenColor(state->settings,"alternate",nativeAlternate);
    // Existing settings without strength are unchanged at 100%.
    const int strength=std::clamp(state->settings.value("strength").toInt(100),0,200);
    const QColor alternate=blend(base,targetAlternate,strength);
    pal.setColor(QPalette::Base,base);
    pal.setColor(QPalette::AlternateBase,alternate);
    it->applied=pal;
    it->applying=true;
    if(view->palette()!=pal)view->setPalette(pal);
    if(!view->alternatingRowColors())view->setAlternatingRowColors(true);
    if(type==QStringLiteral("scenes")){
        // Colorway and other OBS themes may paint QListWidget::item:alternate
        // directly, bypassing QPalette::AlternateBase. Scope the override
        // solely to idle rows, retaining native hover and selected styles.
        const QString currentStyle=view->styleSheet();
        if(currentStyle!=it->appliedStyleSheet)
            it->nativeStyleSheet=currentStyle; // external/native style change
        QString sheet=it->nativeStyleSheet;
        const bool customBase=state->settings.contains("base");
        const bool customAlternate=state->settings.contains("alternate");
        if(customBase)
            sheet+=QStringLiteral("\nSceneTree::item:!alternate:!selected:!hover { background-color: %1; }")
                .arg(base.name());
        // Colorway's QSS paints SceneTree::item:alternate directly. Apply
        // a theme-derived strength override only when needed; at 100% with
        // no custom alternate, leave the theme's native rule alone.
        if(customAlternate||strength!=100)
            sheet+=QStringLiteral("\nSceneTree::item:alternate:!selected:!hover { background-color: %1; }")
                .arg(alternate.name());
        it->appliedStyleSheet=sheet;
        if(view->styleSheet()!=sheet)view->setStyleSheet(sheet);
    }
    it->applying=false;
}
void RowStripes::refresh(){
    if(!qApp)return;
    const auto entries=state->rows.keys();
    for(auto *view:entries)if(!view||!state->settings.value(scope(view)).toBool(false))restore(view);
    for(auto *widget:qApp->allWidgets())
        if(auto *view=qobject_cast<QAbstractItemView*>(widget))
            if(!scope(view).isEmpty())updateView(view);
}
bool RowStripes::eventFilter(QObject *object,QEvent *event){
    auto *view=qobject_cast<QAbstractItemView*>(object);
    if(!view||scope(view).isEmpty())return false;
    if(event->type()==QEvent::Show){
        if(!state->rows.contains(view))updateView(view);
    }else if(event->type()==QEvent::LayoutRequest||event->type()==QEvent::StyleChange){
        if(scope(view)==QStringLiteral("scenes"))updateView(view);
    }else if(event->type()==QEvent::PaletteChange){
        auto it=state->rows.find(view);
        if(it!=state->rows.end()&&!it->applying){
            if(it->paused){
                const QPalette original=keepNativeRows(view->palette(),it->applied,it->original);
                it->original=original;
                it->applying=true;
                if(view->palette()!=original)view->setPalette(original);
                it->applying=false;
            }else if(view->palette()!=it->applied){
                it->original=keepNativeRows(view->palette(),it->applied,it->original);
                updateView(view);
            }
        }
    }
    return false;
}
}
