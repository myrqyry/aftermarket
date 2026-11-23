#include "dock-faces.h"
#include "dock-header-button.h"
#include <QDockWidget>
#include <QMainWindow>
#include <QPointer>
#include <QHash>
#include <QFrame>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QToolButton>
#include <QStyle>
#include <QFont>
#include <QTimer>
#include <QStyleOptionDockWidget>
#include <QEvent>
#include <QFontMetrics>

namespace augment {
namespace {
QMainWindow *mainWindow(QDockWidget *dock) {
    for(auto *p=dock?dock->parentWidget():nullptr;p;p=p->parentWidget())
        if(auto *main=qobject_cast<QMainWindow*>(p))return main;
    return nullptr;
}
QToolButton *button(QWidget *parent,const char *name,const QString &tooltip,
                    DockHeaderGlyph glyph) {
    auto *result=new DockHeaderButton(glyph,parent);
    result->setObjectName(name);
    result->setToolTip(tooltip);
    result->setAccessibleName(tooltip);
    return result;
}
}
struct DockFaces::State {
    struct Entry {
        QPointer<QDockWidget> dock;
        QPointer<QMainWindow> main;
        QPointer<QWidget> native;
        QPointer<QWidget> alternate;
        QPointer<QWidget> host;
        QPointer<QStackedWidget> stack;
        QPointer<QToolButton> flip,prev,next;
        QPointer<QWidget> accessory;
        bool wasHidden=false;
    };
    QHash<QDockWidget*,Entry> entries;
};
DockFaces::DockFaces(QObject *p):QObject(p),state(new State){}
DockFaces::~DockFaces() {
    const auto docks=state->entries.keys();
    for(auto *dock:docks)detach(dock);
}
bool DockFaces::attach(QDockWidget *dock,QWidget *back) {
    auto *main=mainWindow(dock);
    if(!main||!dock->widget()||!back||state->entries.contains(dock))return false;
    auto *native=dock->widget();
    State::Entry entry;
    entry.dock=dock;entry.main=main;entry.native=native;entry.alternate=back;
    entry.wasHidden=native->isHidden();

    // Detach from QDockWidget before replacing its widget: setWidget takes ownership
    // and may delete the previous widget in the old layout.
    native->setParent(nullptr);
    auto *host=new QWidget(dock);
    host->setObjectName("aftermarket-dock-faces");
    auto *layout=new QVBoxLayout(host);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);
    // Child controls live directly over the real Qt dock title area;
    // no transparent container covers OBS's native title rendering.
    auto *prev=button(dock,"aftermarket-tab-previous",
                      "Previous dock tab",DockHeaderGlyph::Previous);
    auto *next=button(dock,"aftermarket-tab-next",
                      "Next dock tab",DockHeaderGlyph::Next);
    auto *flip=button(dock,"aftermarket-face-flip",
                      "Show routing",DockHeaderGlyph::Flip);
    auto *stack=new QStackedWidget(host);
    stack->setObjectName("aftermarket-dock-pages");
    layout->addWidget(stack,1);

    // Native pages retain their identity, signals and exact hierarchy.
    dock->setWidget(host);
    stack->addWidget(native);
    stack->addWidget(back);
    stack->setCurrentWidget(native);
    if(!entry.wasHidden && dock->isVisible())native->show();

    entry.host=host;entry.stack=stack;
    entry.flip=flip;entry.prev=prev;entry.next=next;
    state->entries.insert(dock,entry);
    dock->installEventFilter(this);

    QPointer<QMainWindow> safeMain=main;
    QPointer<QDockWidget> safeDock=dock;
    auto updateTabs=[this,safeMain,safeDock]{
        if(!safeMain||!safeDock)return;
        QTimer::singleShot(0,this,[this,safeDock]{
            if(safeDock)positionTitleButtons(safeDock);
        });
    };
    updateTabs();
    connect(flip,&QToolButton::clicked,this,[this,dock]{
        showBack(dock,!backVisible(dock));
    });
    connect(prev,&QToolButton::clicked,this,[main,dock,updateTabs]{
        const auto others=main->tabifiedDockWidgets(dock);
        if(!others.isEmpty())others.constLast()->raise();
        updateTabs();
    });
    connect(next,&QToolButton::clicked,this,[main,dock,updateTabs]{
        const auto others=main->tabifiedDockWidgets(dock);
        if(!others.isEmpty())others.constFirst()->raise();
        updateTabs();
    });
    connect(main,&QMainWindow::tabifiedDockWidgetActivated,this,[updateTabs](QDockWidget*){updateTabs();});
    connect(dock,&QObject::destroyed,this,[this,dock]{state->entries.remove(dock);});
    connect(dock,&QDockWidget::dockLocationChanged,this,
            [updateTabs](Qt::DockWidgetArea){updateTabs();});
    connect(dock,&QDockWidget::topLevelChanged,this,
            [updateTabs](bool){updateTabs();});
    positionTitleButtons(dock);
    return true;
}
bool DockFaces::addFrontAccessory(QDockWidget *dock,QWidget *accessory){
    auto it=state->entries.find(dock);
    if(it==state->entries.end()||!accessory||it->accessory||!it->host)return false;
    accessory->setParent(dock);
    it->accessory=accessory;
    positionTitleButtons(dock);
    return true;
}
bool DockFaces::detach(QDockWidget *dock) {
    auto it=state->entries.find(dock);
    if(it==state->entries.end())return false;
    const State::Entry e=it.value();
    state->entries.erase(it);
    if(e.dock)e.dock->removeEventFilter(this);
    if(e.accessory)delete e.accessory.data();
    if(e.flip)delete e.flip.data();
    if(e.prev)delete e.prev.data();
    if(e.next)delete e.next.data();
    if(!e.dock||!e.host)return true;
    if(e.dock->widget()!=e.host)return true; // External owner replaced the dock.
    if(e.native){
        if(e.stack)e.stack->removeWidget(e.native);
        e.native->hide();
        e.native->setParent(nullptr);
        e.dock->setWidget(e.native);
        if(!e.wasHidden&&!e.dock->isHidden())e.native->show();
    }
    if(e.host)delete e.host.data();
    return true;
}
QWidget *DockFaces::front(QDockWidget *dock)const {
    return state->entries.value(dock).native;
}
QWidget *DockFaces::back(QDockWidget *dock)const {
    return state->entries.value(dock).alternate;
}
bool DockFaces::backVisible(QDockWidget *dock)const {
    const auto e=state->entries.value(dock);
    return e.stack&&e.alternate&&e.stack->currentWidget()==e.alternate;
}
bool DockFaces::showBack(QDockWidget *dock,bool visible) {
    auto it=state->entries.find(dock);
    if(it==state->entries.end()||!it->stack||!it->native||!it->alternate)return false;
    it->stack->setCurrentWidget(visible?it->alternate.data():it->native.data());
    // The native dock title already renders the real name.
    positionTitleButtons(dock);
    if(it->flip){
        it->flip->setChecked(visible);
        it->flip->setToolTip(visible?"Show original Audio Mixer":"Show routing");
        it->flip->setAccessibleName(it->flip->toolTip());
    }
    return true;
}
bool DockFaces::eventFilter(QObject *watched,QEvent *event){
    auto *dock=qobject_cast<QDockWidget*>(watched);
    if(dock&&state->entries.contains(dock)){
        switch(event->type()){
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::StyleChange:
        case QEvent::PaletteChange:
        case QEvent::FontChange:
        case QEvent::WindowTitleChange:
        case QEvent::LayoutDirectionChange:
        case QEvent::LayoutRequest:{
            QPointer<QDockWidget> safe=dock;
            QTimer::singleShot(0,this,[this,safe]{
                if(safe)positionTitleButtons(safe);
            });
            break;
        }
        default:break;
        }
    }
    return QObject::eventFilter(watched,event);
}
void DockFaces::positionTitleButtons(QDockWidget *dock){
    auto it=state->entries.find(dock);
    if(it==state->entries.end()||!it->flip||!dock->widget())return;
    auto *flip=it->flip.data();
    const auto hide=[&]{
        if(it->accessory)it->accessory->hide();
        if(it->prev)it->prev->hide();
        if(it->next)it->next->hide();
        flip->hide();
    };
    if(dock->isFloating()||(dock->features()&QDockWidget::DockWidgetVerticalTitleBar)){
        hide();return; // platform title bar is not the dock's Qt title region
    }
    const int titleHeight=dock->widget()->geometry().top();
    if(titleHeight<16||titleHeight>48){hide();return;}
    QStyleOptionDockWidget option;
    option.initFrom(dock);
    option.title=dock->windowTitle();
    option.closable=dock->features()&QDockWidget::DockWidgetClosable;
    option.floatable=dock->features()&QDockWidget::DockWidgetFloatable;
    option.movable=dock->features()&QDockWidget::DockWidgetMovable;
    option.verticalTitleBar=false;
    const QRect text=dock->style()->subElementRect(
        QStyle::SE_DockWidgetTitleBarText,&option,dock);
    if(!text.isValid()){hide();return;}
    // Reserve the real dock text and Qt's native undock/close hit area.
    int titleRight=text.left()+QFontMetrics(dock->font())
        .horizontalAdvance(dock->windowTitle())+9;
    // The native Qt title painter already includes the temporary leading
    // display spacing for a left-hand icon; never double-count it here.
    const int usable=text.right()-titleRight;
    constexpr int spacing=2;
    const bool showFlip=usable>=flip->width();
    int used=showFlip?flip->width():0;
    const bool front=it->stack&&it->stack->currentWidget()==it->native;
    const bool showAccessory=front&&it->accessory&&
        usable>=used+spacing+it->accessory->width();
    if(showAccessory)used+=spacing+it->accessory->width();
    const bool tabbed=it->main&&!it->main->tabifiedDockWidgets(dock).isEmpty();
    const int tabsWidth=it->prev&&it->next?
        it->prev->width()+it->next->width()+2*spacing:0;
    const bool showTabs=tabbed&&tabsWidth>0&&usable>=used+tabsWidth;
    if(showTabs)used+=tabsWidth;
    if(!showFlip){hide();return;}
    int x=text.right()-used+1;
    const int y=qMax(0,(titleHeight-flip->height())/2);
    auto place=[&](QWidget *widget,bool visible){
        if(!widget)return;
        if(!visible){widget->hide();return;}
        widget->move(x,y);
        widget->show();
        widget->raise();
        x+=widget->width()+spacing;
    };
    place(it->accessory,showAccessory);
    place(it->prev,showTabs);
    place(it->next,showTabs);
    place(flip,true);
}
}
