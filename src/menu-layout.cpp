#include "menu-layout.h"
#include <QMenu>
#include <QAction>
#include <QWidgetAction>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QVariant>
#include <QStyle>
#include <QMenuBar>
#include <QtMath>
#include <algorithm>
#ifdef AFTERMARKET_QT642_COLUMNS
#include <QtWidgets/private/qmenu_p.h>
#endif
namespace augment {
namespace {
constexpr auto labelsKey="_aftermarketLogicalLabels";
constexpr auto nameKey="_aftermarketLogicalName";
constexpr auto pageKey="_aftermarketOverflowPage";
constexpr auto actionKey="_aftermarketOverflowAction";
// Source actions are a task-oriented menu rather than a catalogue of choices.
// Their spatial order is useful muscle memory, so keep them in one column.
// Match the distinctive OBS command group, not the menu's row count or its
// (usually empty) QObject name. Filter-type lists remain eligible for columns.
bool sourceContextMenu(const QMenu *menu) {
    QSet<QString> labels;
    for(const auto *a:menu->actions()) {
        if(!a->isVisible()||a->isSeparator())continue;
        QString label=a->text().section(QChar('\t'),0,0);
        label.remove(QChar('&'));
        label=label.trimmed().toCaseFolded();
        if(label.endsWith(QStringLiteral("...")))label.chop(3);
        if(label.endsWith(QChar(0x2026)))label.chop(1);
        labels.insert(label);
    }
    return labels.contains(QStringLiteral("order"))
        && labels.contains(QStringLiteral("transform"))
        && labels.contains(QStringLiteral("filters"))
        && labels.contains(QStringLiteral("properties"));
}
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
void MenuLayout::prepare(bool enabled,const QSet<QAction*> &explicitOverflow) {
    if(prepared||!enabled||menu->isTearOffEnabled())return;
    const auto actions=menu->actions();
    int count=0;
    for(auto *a:actions) {
        if(qobject_cast<QWidgetAction*>(a))return;
        if(a->isVisible()&&!a->isSeparator())++count;
    }
    auto *screen=QGuiApplication::screenAt(QCursor::pos());
    if(!screen)screen=menu->screen();
    const int available=screen?screen->availableGeometry().height():800;
    const int cap=qMin(340,qMax(160,int(available*.48)));
    if(count<=10&&menu->sizeHint().height()<=cap&&explicitOverflow.isEmpty())return;
    prepared=true;
    originalStyle=menu->styleSheet();
    appliedStyle=originalStyle+QStringLiteral(
        "\nQMenu::item { padding: 2px 12px; margin: 0px; min-height: 16px; }"
        "\nQMenu::separator { height: 1px; margin: 3px 8px; }");
    menu->setStyleSheet(appliedStyle);
    if(explicitOverflow.isEmpty()||menu->property(pageKey).toBool())return;
    const auto labels=nativeLabels(menu);
    const auto name=menu->objectName();
    for(auto *a:actions)originalActions.append(a);
    menu->setProperty(labelsKey,labels);
    auto *page=new QMenu(QStringLiteral("More actions…"),menu);
    page->setProperty(pageKey,true);
    page->setProperty(labelsKey,labels);
    page->setProperty(nameKey,name);
    page->setStyleSheet(appliedStyle);
    page->setLayoutDirection(menu->layoutDirection());
    page->setFont(menu->font());
    menu->addMenu(page)->setProperty(actionKey,true);
    pages.append(page);
    for(auto *action:actions)if(explicitOverflow.contains(action)&&action->isVisible()&&!action->isSeparator()) {
        menu->removeAction(action);page->addAction(action);moved.append({action,page});
    }
}
void MenuLayout::present() {
#ifdef AFTERMARKET_QT642_COLUMNS
    // QMenu has no public column-height setter. Update its actual native
    // geometry cache, so paint, actionAt, mouse/keyboard and submenu paths
    // all read identical rectangles. Never use this on a different Qt ABI.
    if(!prepared||arranging||!menu->isVisible()||qstrcmp(qVersion(),QT_VERSION_STR)!=0)return;
    auto *d=QMenuPrivate::get(menu);
    if(!d->itemsDirty&&menu->geometry()==arrangedGeometry)return;
    // Preserve the familiar vertical layout of Source context menus. Do not
    // disable the independent optional placement rules or compact spacing.
    if(sourceContextMenu(menu))return;
    if(!d->widgetItems.isEmpty()||d->tearoff||d->tornoff)return;
    const auto actions=menu->actions();
    if(actions.isEmpty())return;
    arranging=true;
    // Obtain current native row sizes via the public API (also flushes dirty
    // layout). Heights and widths remain the active style's actual metrics.
    menu->actionGeometry(actions.first());
    const auto native=d->actionRects;
    if(native.size()!=actions.size()){arranging=false;return;}
    auto *screen=menu->screen();
    if(!screen){arranging=false;return;}
    const QRect work=screen->availableGeometry().adjusted(4,4,-4,-4);
    const int cap=qMin(340,qMax(160,int(work.height()*.48)));
    int total=0,rowWidth=0,left=100000,top=100000;
    for(const auto &r:native)if(r.isValid()){
        total+=r.height();rowWidth=qMax(rowWidth,r.width());left=qMin(left,r.x());top=qMin(top,r.y());
    }
    // The scroll offset may make native y negative. Reconstruct actual
    // top inset from the style rather than inheriting an offscreen offset.
    top=qMax(2,top+(d->scroll?qMax(0,-d->scroll->scrollOffset):0));
    left=qMax(2,left);
    const int bottom=top,right=left,gap=10;
    const int maxColumns=qMin(12,(work.width()-left-right+gap)/qMax(1,rowWidth+gap));
    const auto count=std::count_if(actions.begin(),actions.end(),[](const QAction *a){return a->isVisible()&&!a->isSeparator();});
    if((count<16&&total+top+bottom<=cap)||maxColumns<2||rowWidth<=0) {
        arrangedGeometry=menu->geometry();columnsApplied=false;arranging=false;return;
    }
    // Balance contiguous columns in original order. Prefer a nearby
    // separator boundary, but never remove or duplicate a command.
    const int cols=qBound(2,qCeil(double(total)/qMax(1,cap-top-bottom)),maxColumns);
    QList<QRect> rects(native.size());
    int col=0,y=top,tallest=0,used=0,remaining=total;
    int target=qCeil(double(remaining)/cols);
    for(int i=0;i<native.size();++i) {
        const auto r=native[i];if(!r.isValid())continue;
        if(col<cols-1&&used>0&&used+r.height()>target) {
            tallest=qMax(tallest,y);++col;y=top;used=0;
            target=qCeil(double(remaining)/(cols-col));
        }
        const int physical=menu->isRightToLeft()?cols-1-col:col;
        rects[i]=QRect(left+physical*(rowWidth+gap),y,rowWidth,r.height());
        y+=r.height();used+=r.height();remaining-=r.height();
    }
    tallest=qMax(tallest,y);
    const QSize size(left+right+cols*rowWidth+(cols-1)*gap,tallest+bottom);
    // If every row won't fit, retain native behavior; never clip choices.
    if(size.width()>work.width()||size.height()>work.height()) {
        arrangedGeometry=menu->geometry();columnsApplied=false;arranging=false;return;
    }
    QPoint pos=menu->pos();
    const QPoint cursor=QCursor::pos();
    if(!columnsApplied&&!qobject_cast<QMenu*>(d->causedPopup.widget.data())&&!qobject_cast<QMenuBar*>(d->causedPopup.widget.data())&&menu->geometry().adjusted(-8,-8,8,8).contains(cursor))
        pos.setY(cursor.y()+size.height()<=work.bottom()?cursor.y():cursor.y()-size.height());
    pos.setX(qBound(work.left(),pos.x(),work.right()-size.width()+1));
    pos.setY(qBound(work.top(),pos.y(),work.bottom()-size.height()+1));
    menu->setGeometry(QRect(pos,size));
    if(d->scroll){d->scroll->scrollTimer.stop();d->scroll->scrollOffset=0;d->scroll->scrollFlags=0;d->scroll->scrollDirection=0;}
    d->actionRects=rects;d->ncols=cols;d->itemsDirty=false;
    arrangedGeometry=menu->geometry();columnsApplied=true;arranging=false;
#else
    // Public-API fallback: compact rows, never auto-hide commands.
#endif
}
void MenuLayout::restore() {
    if(!prepared)return;
    prepared=false;
#ifdef AFTERMARKET_QT642_COLUMNS
    if(columnsApplied&&qstrcmp(qVersion(),QT_VERSION_STR)==0)QMenuPrivate::get(menu)->itemsDirty=true;
#endif
    columnsApplied=false;arrangedGeometry={};
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
