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
#include <QSplitter>
#include <QStackedWidget>
#include <QSlider>
#include <QList>
#include <QLayout>
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
        QPointer<QSplitter> splitter;
        bool expanded=false;
        QPointer<QToolButton> flip,prev,next;
        QPointer<QWidget> accessory;
        bool wasHidden=false;
        struct FaderConstraint {
            QPointer<QWidget> control;
            int minimum,maximum,appliedMinimum,appliedMaximum;
        };
        QList<FaderConstraint> compactFaders;
        void setCompactFaders(bool enabled){
            if(enabled&&native){
                if(auto *pages=native->findChild<QStackedWidget*>("stackedMixerArea"))
                    if(pages->currentWidget()&&
                       pages->currentWidget()->objectName()=="hMixerScrollArea")
                        enabled=false; // OBS's horizontal mixer has separate geometry.
            }
            if(!enabled){
                for(const auto &item:compactFaders)if(item.control){
                    if(item.control->maximumHeight()==item.appliedMaximum)
                        item.control->setMaximumHeight(item.maximum);
                    if(item.control->minimumHeight()==item.appliedMinimum)
                        item.control->setMinimumHeight(item.minimum);
                }
                compactFaders.clear();
            }else if(compactFaders.isEmpty()&&native){
                // Native OBS keeps a tall volMeterFrame even when its
                // volume meter is hidden behind Aftermarket's rotary gain.
                // Constrain the frame AND its children, preserving originals.
                const auto compress=[this](QWidget *widget,int cap){
                    const int originalMin=widget->minimumHeight();
                    const int originalMax=widget->maximumHeight();
                    const int compactMax=qMin(cap,originalMax);
                    const int compactMin=qMin(originalMin,qMin(72,compactMax));
                    compactFaders.append({widget,originalMin,originalMax,
                                         compactMin,compactMax});
                    widget->setMinimumHeight(compactMin);
                    widget->setMaximumHeight(compactMax);
                    widget->updateGeometry();
                };
                for(auto *widget:native->findChildren<QWidget*>()){
                    bool inVolume=false;
                    for(auto *ancestor=widget->parentWidget();ancestor&&ancestor!=native;
                        ancestor=ancestor->parentWidget()){
                        if(ancestor->inherits("VolumeControl")||
                           ancestor->objectName()=="VolumeControl"){
                            inVolume=true;break;
                        }
                    }
                    if(!inVolume)continue;
                    if(widget->objectName()=="volMeterFrame")
                        compress(widget,116);
                    else if(widget->inherits("VolumeMeter"))
                        compress(widget,110);
                    else if(auto *slider=qobject_cast<QSlider*>(widget);
                            slider&&slider->orientation()==Qt::Vertical)
                        compress(slider,100);
                }
            }
            if(native&&native->layout())native->layout()->invalidate();
            if(native)native->updateGeometry();
        }
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
                      "Show routing below mixer",DockHeaderGlyph::Flip);
    auto *splitter=new QSplitter(Qt::Vertical,host);
    splitter->setObjectName("aftermarket-mixer-split");
    splitter->setHandleWidth(4);
    splitter->setChildrenCollapsible(false);
    layout->addWidget(splitter,1);

    // Native pages retain their identity, signals and exact hierarchy.
    dock->setWidget(host);
    splitter->addWidget(native);
    splitter->addWidget(back);
    back->hide(); // The AudioHub opens BELOW the mixer, without replacing it.
    if(!entry.wasHidden && dock->isVisible())native->show();

    entry.host=host;entry.splitter=splitter;
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
    // Native OBS switches between vMixerScrollArea and hMixerScrollArea.
    // Restore original fader dimensions on horizontal layout, then reapply
    // compact vertical geometry when switching back.
    if(auto *pages=native->findChild<QStackedWidget*>("stackedMixerArea")){
        connect(pages,&QStackedWidget::currentChanged,this,[this,safeDock](int){
            if(!safeDock)return;
            auto it=state->entries.find(safeDock);
            if(it==state->entries.end())return;
            it->setCompactFaders(false);
            it->setCompactFaders(it->expanded);
            if(it->splitter&&it->expanded)
                it->splitter->setSizes({it->splitter->height()/2,
                                         it->splitter->height()/2});
        });
    }
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
    State::Entry e=it.value();
    state->entries.erase(it);
    e.setCompactFaders(false);
    if(e.dock)e.dock->removeEventFilter(this);
    if(e.accessory)delete e.accessory.data();
    if(e.flip)delete e.flip.data();
    if(e.prev)delete e.prev.data();
    if(e.next)delete e.next.data();
    if(!e.dock||!e.host)return true;
    if(e.dock->widget()!=e.host)return true; // External owner replaced the dock.
    if(e.native){
        // Reparent native before disposing of the splitter/AudioHub.
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
    return e.splitter&&e.alternate&&e.expanded;
}
bool DockFaces::showBack(QDockWidget *dock,bool visible) {
    auto it=state->entries.find(dock);
    if(it==state->entries.end()||!it->splitter||!it->native||!it->alternate)return false;
    it->expanded=visible;
    it->setCompactFaders(visible);
    it->alternate->setVisible(visible);
    if(visible){
        const int available=qMax(300,it->splitter->height());
        it->splitter->setSizes({available/2,available/2});
    }
    // The native dock title already renders the real name.
    positionTitleButtons(dock);
    if(it->flip){
        it->flip->setChecked(visible);
        it->flip->setToolTip(visible?"Hide routing below mixer":"Show routing below mixer");
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
    const bool showAccessory=it->accessory&&
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
