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
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QGuiApplication>
#include <QLabel>
#include <functional>

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
        QPointer<QLabel> fade;
        bool wasHidden=false;
    };
    QHash<QDockWidget*,Entry> entries;
    std::function<bool()> motionEnabled;
};
DockFaces::DockFaces(QObject *p,std::function<bool()> motionEnabled)
    :QObject(p),state(new State) {
    state->motionEnabled=std::move(motionEnabled);
}
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
    // Only the interactive AudioBus switch belongs at the bottom.
    // The routing graph is a separate page, not an extra panel under OBS.
    auto *bottom=new QWidget(host);
    bottom->setObjectName("aftermarket-audiobus-controls");
    auto *bottomRow=new QHBoxLayout(bottom);
    bottomRow->setContentsMargins(4,0,4,1);
    bottomRow->setSpacing(2);
    auto *flip=new QToolButton(bottom);
    flip->setObjectName("aftermarket-audiobus-toggle");
    flip->setText("AudioBus");
    flip->setCheckable(true);
    flip->setAutoRaise(true);
    flip->setFixedHeight(23);
    flip->setToolTip("Show AudioBus routing");
    flip->setAccessibleName(flip->toolTip());
    flip->setFocusPolicy(Qt::StrongFocus);
    bottomRow->addWidget(flip);
    bottomRow->addStretch(1);
    auto *stack=new QStackedWidget(host);
    stack->setObjectName("aftermarket-dock-pages");
    layout->addWidget(stack,1);
    layout->addWidget(bottom,0);

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
    if(it->fade)delete it->fade.data();
    const State::Entry e=it.value();
    state->entries.erase(it);
    if(e.dock)e.dock->removeEventFilter(this);
    if(e.accessory)delete e.accessory.data();
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
    auto *stack=it->stack.data();
    auto *destination=visible?it->alternate.data():it->native.data();
    if(stack->currentWidget()==destination)return true;
    // Clear an interrupted transition rather than stacking screenshots.
    if(it->fade)delete it->fade.data();
    it->fade=nullptr;
    const bool animate=state->motionEnabled&&state->motionEnabled()&&
        stack->isVisible()&&stack->width()>0&&stack->height()>0&&
        stack->style()->styleHint(QStyle::SH_Widget_Animate,nullptr,stack);
    // A short, click-through snapshot fades away over the real new page.
    // The original OBS mixer never gets an opacity effect or a fake widget.
    const QPixmap previous=animate?stack->grab():QPixmap{};
    stack->setCurrentWidget(destination);
    if(!previous.isNull()){
        auto *overlay=new QLabel(stack);
        overlay->setObjectName("aftermarket-view-fade");
        overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
        overlay->setFocusPolicy(Qt::NoFocus);
        overlay->setPixmap(previous);
        overlay->setScaledContents(true);
        overlay->setGeometry(stack->rect());
        auto *opacity=new QGraphicsOpacityEffect(overlay);
        overlay->setGraphicsEffect(opacity);
        overlay->show();
        overlay->raise();
        auto *animation=new QPropertyAnimation(opacity,"opacity",overlay);
        animation->setObjectName("aftermarket-view-fade-motion");
        animation->setDuration(155);
        animation->setStartValue(1.0);
        animation->setEndValue(0.0);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        QPointer<QLabel> safe=overlay;
        QPointer<QDockWidget> target=dock;
        connect(animation,&QPropertyAnimation::finished,this,[this,target,safe]{
            if(target){
                auto it=state->entries.find(target);
                if(it!=state->entries.end()&&it->fade==safe)it->fade=nullptr;
            }
            if(safe)safe->deleteLater();
        });
        it->fade=overlay;
        animation->start();
    }
    // The native dock title already renders the real name.
    positionTitleButtons(dock);
    if(it->flip){
        it->flip->setChecked(visible);
        it->flip->setText(visible?"Audio Mixer":"AudioBus");
        it->flip->setToolTip(visible?"Show original Audio Mixer":"Show AudioBus routing");
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
    if(it==state->entries.end()||!dock->widget())return;
    // Only the three-dot options accessory and optional tab navigation live
    // in Qt's native title. AudioBus is a real button at the bottom of host.
    const auto hide=[&]{
        if(it->accessory)it->accessory->hide();
        if(it->prev)it->prev->hide();
        if(it->next)it->next->hide();
    };
    if(dock->isFloating()||(dock->features()&QDockWidget::DockWidgetVerticalTitleBar)){
        hide();return;
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
    const int titleRight=text.left()+
        QFontMetrics(dock->font()).horizontalAdvance(dock->windowTitle())+9;
    const int usable=text.right()-titleRight;
    constexpr int spacing=2;
    int used=0;
    const bool showAccessory=it->accessory&&usable>=it->accessory->width();
    if(showAccessory)used+=it->accessory->width();
    const bool tabbed=it->main&&!it->main->tabifiedDockWidgets(dock).isEmpty();
    const int tabsWidth=it->prev&&it->next?
        it->prev->width()+it->next->width()+2*spacing:0;
    const bool showTabs=tabbed&&tabsWidth>0&&usable>=used+tabsWidth;
    if(showTabs)used+=tabsWidth;
    int x=text.right()-used+1;
    const int y=qMax(0,(titleHeight-18)/2);
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
}
}
