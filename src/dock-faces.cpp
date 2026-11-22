#include "dock-faces.h"
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
#include <QTimer>

namespace augment {
namespace {
QMainWindow *mainWindow(QDockWidget *dock) {
    for(auto *p=dock?dock->parentWidget():nullptr;p;p=p->parentWidget())
        if(auto *main=qobject_cast<QMainWindow*>(p))return main;
    return nullptr;
}
QToolButton *button(QWidget *parent,const char *name,QString tooltip,Qt::ArrowType arrow=Qt::NoArrow) {
    auto *result=new QToolButton(parent);
    result->setObjectName(name);
    result->setAutoRaise(true);
    result->setFixedSize(27,25);
    result->setToolTip(tooltip);
    result->setAccessibleName(tooltip);
    result->setFocusPolicy(Qt::StrongFocus);
    if(arrow!=Qt::NoArrow)result->setArrowType(arrow);
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
        QPointer<QLabel> faceLabel;
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
    auto *bar=new QWidget(host);
    bar->setObjectName("aftermarket-dock-controls");
    auto *controls=new QHBoxLayout(bar);
    controls->setContentsMargins(4,0,4,0);
    controls->setSpacing(2);
    auto *title=new QLabel("Front",bar);
    title->setObjectName("aftermarket-dock-label");
    title->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    controls->addWidget(title,1);
    auto *prev=button(bar,"aftermarket-tab-previous","Previous dock tab",Qt::LeftArrow);
    auto *next=button(bar,"aftermarket-tab-next","Next dock tab",Qt::RightArrow);
    auto *flip=button(bar,"aftermarket-face-flip","Show backside");
    flip->setIcon(bar->style()->standardIcon(QStyle::SP_BrowserReload));
    controls->addWidget(prev);
    controls->addWidget(next);
    controls->addWidget(flip);
    layout->addWidget(bar);
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
    entry.flip=flip;entry.prev=prev;entry.next=next;entry.faceLabel=title;
    state->entries.insert(dock,entry);

    QPointer<QMainWindow> safeMain=main;
    QPointer<QDockWidget> safeDock=dock;
    QPointer<QToolButton> safePrev=prev,safeNext=next;
    auto updateTabs=[safeMain,safeDock,safePrev,safeNext]{
        if(!safeMain||!safeDock||!safePrev||!safeNext)return;
        const bool tabbed=!safeMain->tabifiedDockWidgets(safeDock).isEmpty();
        safePrev->setVisible(tabbed);
        safeNext->setVisible(tabbed);
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
    connect(dock,&QDockWidget::dockLocationChanged,this,[this,updateTabs](Qt::DockWidgetArea){
        QTimer::singleShot(0,this,updateTabs);
    });
    return true;
}
bool DockFaces::detach(QDockWidget *dock) {
    auto it=state->entries.find(dock);
    if(it==state->entries.end())return false;
    const State::Entry e=it.value();
    state->entries.erase(it);
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
    if(it->faceLabel)it->faceLabel->setText(visible?(it->alternate->windowTitle().isEmpty()?"Back":it->alternate->windowTitle()):"Front");
    if(it->flip){
        it->flip->setToolTip(visible?"Show original dock":"Show backside");
        it->flip->setAccessibleName(it->flip->toolTip());
    }
    return true;
}
}
