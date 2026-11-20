#include "menu-layout.h"
#include <QMenu>
#include <QAction>
#include <QWidgetAction>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QVariant>
#include <QStyle>
namespace augment {
namespace {
constexpr auto labelsKey="_aftermarketLogicalLabels";
constexpr auto nameKey="_aftermarketLogicalName";
constexpr auto pageKey="_aftermarketOverflowPage";
constexpr auto actionKey="_aftermarketOverflowAction";
QStringList nativeLabels(const QMenu *menu) {
    QStringList labels;
    for(const auto *a:menu->actions())
        if(a->isVisible()&&!a->isSeparator()&&!overflowAction(a))labels.append(a->text());
    return labels;
}
}
bool overflowAction(const QAction *action){return action&&action->property(actionKey).toBool();}
QStringList logicalMenuLabels(const QMenu *menu) {
    const auto labels=menu->property(labelsKey);
    return labels.isValid()?labels.toStringList():nativeLabels(menu);
}
QString logicalMenuName(const QMenu *menu) {
    const auto name=menu->property(nameKey);
    return name.isValid()?name.toString():menu->objectName();
}
MenuLayout::MenuLayout(QMenu *m):menu(m){}
void MenuLayout::prepare(bool enabled) {
    if(prepared||!enabled||menu->property(pageKey).toBool()||menu->isTearOffEnabled())return;
    const auto actions=menu->actions();
    int count=0;
    for(auto *a:actions) {
        if(qobject_cast<QWidgetAction*>(a))return; // preserve embedded/custom controls
        if(a->isVisible()&&!a->isSeparator())++count;
    }
    auto *screen=QGuiApplication::screenAt(QCursor::pos());
    if(!screen)screen=menu->screen();
    const int available=screen?screen->availableGeometry().height():800;
    const int cap=qMin(500,qMax(160,int(available*.57)));
    if(count<=10&&menu->sizeHint().height()<=cap)return;
    prepared=true;
    originalStyle=menu->styleSheet();
    appliedStyle=originalStyle+QStringLiteral(
        "\nQMenu::item { padding: 2px 12px; margin: 0px; min-height: 16px; }"
        "\nQMenu::separator { height: 1px; margin: 3px 8px; }");
    menu->setStyleSheet(appliedStyle);
    const auto labels=nativeLabels(menu);
    const auto name=menu->objectName();
    // Sum the actual styled rows instead of trusting sizeHint's possible columns.
    QList<int> heights;
    int total=0;
    const int icon=menu->style()->pixelMetric(QStyle::PM_SmallIconSize,nullptr,menu);
    for(auto *a:actions) {
        const auto rect=menu->actionGeometry(a);
        int h=rect.height();
        if(a->isVisible()&&!a->isSeparator())
            h=qMax(h,icon+6); // account for later optional icon reservations
        h=qMax(0,h);
        heights.append(h);total+=h;
    }
    constexpr int chrome=32;
    if(total+chrome<=cap)return;
    // Native cascades, not clipped rows or duplicate command proxies.
    // Reserve space for a More actions row and prefer original group breaks.
    // Long context menus often end in frequently used commands (e.g.
    // Rename, Filters, Properties). Keep the final four visible commands
    // on the first page whenever they fit without crowding the prefix.
    int tail=actions.size(),tailCost=0,tailCommands=0;
    for(int i=actions.size()-1;i>=0;--i) {
        if(tailCommands>=4&&!actions[i]->isSeparator())break;
        tail=i;tailCost+=heights[i];
        if(actions[i]->isVisible()&&!actions[i]->isSeparator())++tailCommands;
    }
    if(tailCommands<4||tail<6||tailCost>cap/3){tail=actions.size();tailCost=0;}
    QList<QList<QAction*>> chunks;
    int first=0;
    while(first<tail) {
        const int reserved=first==0?tailCost:0;
        int last=first,used=chrome+reserved;
        while(last<tail&&used+heights[last]+28<=cap) {
            used+=heights[last];++last;
        }
        if(last==first)++last;
        if(last<tail) {
            // Prefer a nearby natural group boundary.
            for(int i=last-1;i>first&&i>=last-4;--i)
                if(actions[i]->isSeparator()){last=i+1;break;}
        }
        // The final page need not reserve an overflow row.
        if(last<tail) {
            int remaining=chrome+reserved;
            for(int i=first;i<tail;++i)remaining+=heights[i];
            if(remaining<=cap)last=tail;
        }
        QList<QAction*> chunk;
        for(int i=first;i<last;++i)chunk.append(actions[i]);
        chunks.append(chunk);first=last;
    }
    if(chunks.size()<2)return;
    for(auto *a:actions)originalActions.append(a);
    menu->setProperty(labelsKey,labels);
    QMenu *current=menu;
    for(int i=1;i<chunks.size();++i) {
        auto *page=new QMenu(QStringLiteral("More actions…"),current);
        page->setProperty(pageKey,true);
        page->setProperty(labelsKey,labels);
        page->setProperty(nameKey,name);
        page->setStyleSheet(appliedStyle);
        page->setLayoutDirection(menu->layoutDirection());
        page->setFont(menu->font());
        auto *entry=(i==1&&tail<actions.size())?
            current->insertMenu(actions[tail],page):current->addMenu(page);
        entry->setProperty(actionKey,true);
        pages.append(page);
        for(auto *action:chunks[i]) {
            menu->removeAction(action);
            page->addAction(action);
            moved.append({action,page});
        }
        current=page;
    }
}
void MenuLayout::restore() {
    if(!prepared)return;
    prepared=false;
    // Only restore surviving actions still owned by a generated page; do
    // not resurrect actions explicitly removed/deleted while the popup was open.
    for(auto it=originalActions.crbegin();it!=originalActions.crend();++it) {
        auto *a=it->data();if(!a||menu->actions().contains(a))continue;
        for(const auto &move:moved) {
            if(move.action!=a||!move.page||!move.page->actions().contains(a))continue;
            auto next=it.base(); // reverse_iterator::base points to the next original
            QAction *before=nullptr;
            for(;next!=originalActions.cend();++next)
                if(*next&&menu->actions().contains(next->data())){before=next->data();break;}
            move.page->removeAction(a);
            menu->insertAction(before,a);
            break;
        }
    }
    for(auto it=pages.crbegin();it!=pages.crend();++it)if(auto *page=it->data()) {
        page->hide();
        if(auto *parent=qobject_cast<QMenu*>(page->parentWidget()))parent->removeAction(page->menuAction());
        page->deleteLater();
    }
    pages.clear();moved.clear();originalActions.clear();
    menu->setProperty(labelsKey,QVariant());
    if(menu->styleSheet()==appliedStyle)menu->setStyleSheet(originalStyle);
}
}
