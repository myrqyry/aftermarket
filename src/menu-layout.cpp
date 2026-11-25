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
        QString label=(a->property("_aftermarketBaseText").isValid()?a->property("_aftermarketBaseText").toString():a->text()).section(QChar('\t'),0,0);
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
// A QWindow's screen can lag behind its newly assigned popup geometry,
// particularly when exec() creates its native window on the parent's screen.
// Pick the monitor from actual global geometry, never the virtual desktop.
QScreen *geometryScreen(const QRect &rect){
    QScreen *best=QGuiApplication::screenAt(rect.center());
    qint64 bestArea=0;
    for(auto *screen:QGuiApplication::screens()){
        const QRect part=screen->geometry().intersected(rect);
        const qint64 area=qint64(part.width())*part.height();
        if(area>bestArea){bestArea=area;best=screen;}
    }
    return best;
}
QStringList nativeLabels(const QMenu *menu) {
    QStringList labels;
    for(const auto *a:menu->actions())
        if(a->isVisible()&&!a->isSeparator()&&!overflowAction(a))
            labels.append(a->property("_aftermarketBaseText").isValid()?a->property("_aftermarketBaseText").toString():a->text());
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
void MenuLayout::prepare(bool enabled,const QSet<QAction*> &explicitOverflow,const QSet<QAction*> &explicitHidden,bool compact) {
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
    const bool density=compact&&(count>10||menu->sizeHint().height()>cap);
    if(!density&&explicitOverflow.isEmpty()&&explicitHidden.isEmpty())return;
    // Qt has not assigned its native popup geometry at aboutToShow.
    // Record the pointer now, but resolve the actual monitor and anchor
    // once the native popup appears. Do not let hover move either later.
#ifdef AFTERMARKET_QT642_COLUMNS
    if(qstrcmp(qVersion(),QT_VERSION_STR)==0){
        openingAnchor=QCursor::pos();
        anchoredToInvocation=QGuiApplication::screenAt(openingAnchor)!=nullptr;
    }
#endif
    prepared=true;
    menu->setProperty("_aftermarketSourceContext",sourceContextMenu(menu));
    if(!explicitOverflow.isEmpty()||!explicitHidden.isEmpty()) {
        for(auto *a:actions)originalActions.append(a);
        menu->setProperty(labelsKey,nativeLabels(menu));
    }
    if(density) {
    originalStyle=menu->styleSheet();
    const int minimumRow=qMax(16,menu->fontMetrics().height()+2);
    appliedStyle=originalStyle+QStringLiteral(
        "\nQMenu::item { padding: 2px 12px; margin: 0px; min-height: %1px; }"
        "\nQMenu::separator { height: 1px; margin: 3px 8px; }").arg(minimumRow);
    menu->setStyleSheet(appliedStyle);
    }
    for(auto *a:actions)if(explicitHidden.contains(a)&&a->isVisible()&&!a->isSeparator()) {
        menu->removeAction(a);
        moved.append(Move{a,QPointer<QMenu>{}});
    }
    if(explicitOverflow.isEmpty()||menu->property(pageKey).toBool())return;
    const auto labels=logicalMenuLabels(menu);
    const auto name=menu->objectName();
    menu->setProperty(labelsKey,labels);
    auto *page=new QMenu(QStringLiteral("More actions…"),menu);
    page->setProperty(pageKey,true);
    page->setProperty(labelsKey,labels);
    page->setProperty(nameKey,name);
    if(density)page->setStyleSheet(appliedStyle);
    page->setLayoutDirection(menu->layoutDirection());
    page->setFont(menu->font());
    menu->addMenu(page)->setProperty(actionKey,true);
    pages.append(page);
    for(auto *action:actions)if(explicitOverflow.contains(action)&&!explicitHidden.contains(action)&&action->isVisible()&&!action->isSeparator()) {
        menu->removeAction(action);page->addAction(action);moved.append({action,page});
    }
}
void MenuLayout::trackCustom(QAction *action) {
    if(!action)return;
    if(!prepared)prepared=true;
    if(!menu->property(labelsKey).isValid())menu->setProperty(labelsKey,nativeLabels(menu));
    custom.append(action);
}
void MenuLayout::present() {
#ifdef AFTERMARKET_QT642_COLUMNS
    // QMenu has no public column-height setter. Update its actual native
    // geometry cache, so paint, actionAt, mouse/keyboard and submenu paths
    // all read identical rectangles. Never use this on a different Qt ABI.
    if(!prepared||arranging||!menu->isVisible()||qstrcmp(qVersion(),QT_VERSION_STR)!=0)return;
    auto *d=QMenuPrivate::get(menu);
    if(!placementResolved){
        placementResolved=true;
        // Submenus inherit the actual triggering action's global rectangle,
        // even if Qt's provisional popup was positioned on another monitor.
        auto *cause=d->causedPopup.widget.data();
        auto *action=d->causedPopup.action.data();
        if(auto *parent=qobject_cast<QMenu*>(cause)){
            if(action){
                const QRect row=parent->actionGeometry(action);
                if(row.isValid())parentActionAnchor=QRect(parent->mapToGlobal(row.topLeft()),row.size());
            }
        }else if(auto *bar=qobject_cast<QMenuBar*>(cause)){
            if(action){
                const QRect row=bar->actionGeometry(action);
                if(row.isValid()){
                    parentActionAnchor=QRect(bar->mapToGlobal(row.topLeft()),row.size());
                    menuBarAnchor=true;
                }
            }
        }
        const QRect native=menu->geometry();
        invocationScreen=geometryScreen(parentActionAnchor.isValid()?parentActionAnchor:native);
        if(!invocationScreen)invocationScreen=menu->screen();
        const int edge=qMin(qAbs(openingAnchor.x()-native.left()),
                            qAbs(openingAnchor.x()-native.right()));
        // A cursor on a different monitor is never a valid root invocation
        // anchor, even when both monitors share the same x coordinate.
        if(parentActionAnchor.isValid()||
           QGuiApplication::screenAt(openingAnchor)!=invocationScreen||
           edge>qMax(64,menu->fontMetrics().height()*4))
            anchoredToInvocation=false;
    }
    if(!d->itemsDirty&&menu->geometry()==arrangedGeometry)return;
    // Preserve the familiar vertical layout of Source context menus. Do not
    // disable the independent optional placement rules or compact spacing.
    if(menu->property("_aftermarketSourceContext").toBool()||sourceContextMenu(menu))return;
    // Keep hierarchical menus native and compact only flat choice lists.
    // Check the actual visible actions after explicit overflow rules and
    // custom entries, so a generated More actions submenu counts too.
    const auto entries=menu->actions();
    if(std::any_of(entries.cbegin(),entries.cend(),[](const QAction *a){
        return a->isVisible()&&a->menu()!=nullptr;
    }))return;
    if(!d->widgetItems.isEmpty()||d->tearoff||d->tornoff)return;
    const auto actions=menu->actions();
    if(actions.isEmpty())return;
    arranging=true;
    // Obtain current native row sizes via the public API (also flushes dirty
    // layout). Heights and widths remain the active style's actual metrics.
    menu->actionGeometry(actions.first());
    const auto native=d->actionRects;
    if(native.size()!=actions.size()){arranging=false;return;}
    // Use the trigger's monitor, not the screen that Qt temporarily chose
    // for a tall popup before the columns changed its geometry.
    auto *screen=invocationScreen.data();
    if(!screen)screen=geometryScreen(menu->geometry());
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
    if(parentActionAnchor.isValid()){
        const QRect row=parentActionAnchor;
        const bool rtl=menu->isRightToLeft();
        if(menuBarAnchor){
            pos.setX(rtl?row.right()-size.width()+1:row.left());
            pos.setY(row.bottom()+1+size.height()-1<=work.bottom()?
                     row.bottom()+1:row.top()-size.height());
        }else{
            // Choose a side with the final compact dimensions, not Qt's
            // original tall-menu size. Preserve the triggering row height.
            const int right=row.right()+1;
            const int left=row.left()-size.width();
            const bool fitsRight=right+size.width()-1<=work.right();
            const bool fitsLeft=left>=work.left();
            if((!rtl&&fitsRight)||(rtl&&!fitsLeft&&fitsRight))pos.setX(right);
            else if(fitsLeft)pos.setX(left);
            else pos.setX(work.right()-row.right()>=row.left()-work.left()?right:left);
            pos.setY(row.top()-top);
        }
    }else if(anchoredToInvocation){
        const QPoint anchor=openingAnchor;
        // Preserve the root popup's original opening point in BOTH axes.
        // A narrow native menu can fit to the right, while the expanded
        // columns must open to the left. Retaining its old x puts the wide
        // menu hundreds of pixels away from the control that opened it.
        const bool rtl=menu->isRightToLeft();
        if((!rtl&&anchor.x()+size.width()-1<=work.right())||
           (rtl&&anchor.x()-size.width()+1<work.left()))
            pos.setX(anchor.x());
        else if(anchor.x()-size.width()+1>=work.left())
            pos.setX(anchor.x()-size.width()+1);
        else
            pos.setX(anchor.x());
        if(anchor.y()+size.height()-1<=work.bottom())
            pos.setY(anchor.y());
        else if(anchor.y()-size.height()+1>=work.top())
            pos.setY(anchor.y()-size.height()+1);
        else
            pos.setY(anchor.y()-size.height()/2);
    }
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
    anchoredToInvocation=false;openingAnchor={};
    placementResolved=false;menuBarAnchor=false;parentActionAnchor={};invocationScreen.clear();
    // Custom entries exist only while this menu is open.
    for(auto &item:custom)if(item) {
        menu->removeAction(item);
        delete item.data();
    }
    custom.clear();
    // Only restore surviving actions still owned by a generated page; do
    // not resurrect actions explicitly removed/deleted while the popup was open.
    for(auto it=originalActions.crbegin();it!=originalActions.crend();++it) {
        auto *a=it->data();if(!a||menu->actions().contains(a))continue;
        for(const auto &move:moved) {
            if(move.action!=a||(move.page&&!move.page->actions().contains(a)))continue;
            auto next=it.base(); // reverse_iterator::base points to the next original
            QAction *before=nullptr;
            for(;next!=originalActions.cend();++next)
                if(*next&&menu->actions().contains(next->data())){before=next->data();break;}
            if(move.page)move.page->removeAction(a);
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
    menu->setProperty("_aftermarketSourceContext",QVariant());
    if(!appliedStyle.isEmpty()&&menu->styleSheet()==appliedStyle)menu->setStyleSheet(originalStyle);
    appliedStyle.clear();originalStyle.clear();
}
}
