#include "dock-faces.h"
#include "dock-header-button.h"
#include "motion.h"
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
#include <QVariantAnimation>
#include <QPainter>
#include <QPaintEvent>
#include <QTransform>
#include <QGuiApplication>
#include <QtMath>
#include <functional>

namespace augment {
namespace {
QMainWindow *mainWindow(QDockWidget *dock) {
    for(auto *p=dock?dock->parentWidget():nullptr;p;p=p->parentWidget())
        if(auto *main=qobject_cast<QMainWindow*>(p))return main;
    return nullptr;
}
DockHeaderButton *button(QWidget *parent,const char *name,const QString &tooltip,
                    DockHeaderGlyph glyph) {
    auto *result=new DockHeaderButton(glyph,parent);
    result->setObjectName(name);
    result->setToolTip(tooltip);
    result->setAccessibleName(tooltip);
    return result;
}
// A paint-only, short two-sided flip. Neither real OBS widget is transformed
// or reassigned a graphics effect. Frame images are captured only once, not
// repeatedly on every paint; the animation remains inside the original dock.
class FlipFrame final : public QWidget {
public:
    FlipFrame(QPixmap outgoing,QPixmap incoming,QWidget *parent)
        :QWidget(parent),outgoing_(std::move(outgoing)),incoming_(std::move(incoming)){
        setObjectName("aftermarket-view-flip");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        parent->installEventFilter(this);
    }
    void setProgress(qreal value){
        progress_=qBound(qreal(0),value,qreal(1));
        update();
    }
    void cancel(){
        hide();
        if(auto *animation=findChild<QVariantAnimation*>("aftermarket-view-flip-motion"))animation->stop();
        deleteLater();
    }
protected:
    bool eventFilter(QObject *watched,QEvent *event)override{
        if(watched==parentWidget()&&(event->type()==QEvent::Resize||
            event->type()==QEvent::Hide||event->type()==QEvent::PaletteChange||
            event->type()==QEvent::StyleChange))cancel();
        return QWidget::eventFilter(watched,event);
    }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(),parentWidget()->parentWidget()->palette().color(QPalette::Window));
        if(width()<=0||height()<=0)return;
        const bool first=progress_<0.5;
        const QPixmap &face=first?outgoing_:incoming_;
        if(face.isNull())return;
        const qreal halfTurn=first?progress_*2:(1-progress_)*2;
        const qreal angle=halfTurn*M_PI_2;
        const qreal amount=qCos(angle);
        if(amount<0.01)return;
        const qreal halfWidth=width()*amount/2;
        const qreal cx=width()/2.0;
        const qreal inset=height()*0.09*qSin(angle);
        // One vertical edge recedes as the card rotates away; the other
        // recedes as the second side turns in. The perspective is intentionally
        // restrained to avoid a distracting gimmick in a working mixer.
        const qreal leftInset=first?0:inset;
        const qreal rightInset=first?inset:0;
        const QPolygonF target{
            QPointF(cx-halfWidth,leftInset),
            QPointF(cx+halfWidth,rightInset),
            QPointF(cx+halfWidth,height()-rightInset),
            QPointF(cx-halfWidth,height()-leftInset)
        };
        const qreal pixelRatio=face.devicePixelRatio();
        const QRectF bounds(0,0,face.width()/pixelRatio,face.height()/pixelRatio);
        const QPolygonF source{bounds.topLeft(),bounds.topRight(),
                               bounds.bottomRight(),bounds.bottomLeft()};
        QTransform perspective;
        if(!QTransform::quadToQuad(source,target,perspective))return;
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setTransform(perspective);
        painter.drawPixmap(bounds,face,QRectF(QPointF(0,0),face.size()/pixelRatio));
    }
private:
    QPixmap outgoing_,incoming_;
    qreal progress_=0;
};
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
        QPointer<FlipFrame> transition;
        bool wasHidden=false;
        bool titleUpdatePending=false;
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
    prev->setMotionProvider(state->motionEnabled);next->setMotionProvider(state->motionEnabled);
    auto *bottom=new QWidget(host);
    bottom->setObjectName("aftermarket-audiobus-controls");
    auto *bottomRow=new QHBoxLayout(bottom);
    bottomRow->setContentsMargins(4,0,4,1);
    bottomRow->setSpacing(2);
    auto *flip=new FineStateToolButton(bottom);
    flip->setMotionProvider(state->motionEnabled);
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
    if(it->transition)delete it->transition.data();
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
    // Keep only the latest transition during rapid user reversals.
    if(it->transition)delete it->transition.data();
    it->transition=nullptr;
    // Snapshot each face only once, with a physical-pixel limit. Native
    // page roots are supported, but separate native child windows and GPU
    // surfaces cannot be reliably captured/composited. Never substitute a
    // different animation when the flip is unsafe; switch immediately.
    const qreal dpr=stack->devicePixelRatioF();
    const bool withinBudget=qreal(stack->width())*stack->height()*dpr*dpr<=1024*1024;
    auto compositable=[](QWidget *page){
        auto specialSurface=[](QWidget *w){
            return w->testAttribute(Qt::WA_PaintOnScreen)||
                w->inherits("QOpenGLWidget")||w->inherits("OBSQTDisplay");
        };
        if(specialSurface(page)||QCoreApplication::testAttribute(Qt::AA_NativeWindows))return false;
        for(auto *child:page->findChildren<QWidget*>())
            if(child->testAttribute(Qt::WA_NativeWindow)||specialSurface(child))return false;
        return true;
    };
    const bool animate=withinBudget&&state->motionEnabled&&state->motionEnabled()&&
        stack->isVisible()&&stack->width()>0&&stack->height()>0&&
        stack->style()->styleHint(QStyle::SH_Widget_Animate,nullptr,stack)&&
        compositable(stack->currentWidget())&&compositable(destination);
    const QPixmap outgoing=animate?stack->currentWidget()->grab():QPixmap{};
    stack->setCurrentWidget(destination);
    const QPixmap incoming=animate?destination->grab():QPixmap{};
    if(!outgoing.isNull()&&!incoming.isNull()){
        auto *surface=new FlipFrame(outgoing,incoming,destination);
        surface->setGeometry(destination->rect());
        surface->show();
        surface->raise();
        auto *animation=new QVariantAnimation(surface);
        animation->setObjectName("aftermarket-view-flip-motion");
        animation->setDuration(210);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        animation->setEasingCurve(QEasingCurve::InOutSine);
        QPointer<FlipFrame> safe=surface;
        QPointer<QDockWidget> target=dock;
        connect(animation,&QVariantAnimation::valueChanged,surface,
            [this,safe](const QVariant &value) {
                if(!safe)return;
                if(!safe->isVisible()||!state->motionEnabled||!state->motionEnabled()){
                    safe->cancel();return;
                }
                // QVariantAnimation samples elapsed time on the GUI clock.
                // Paint only the two immutable pixmaps; no frame recapture,
                // layout mutation or queue of missed intermediate frames.
                safe->setProgress(value.toReal());
            });
        connect(animation,&QVariantAnimation::finished,this,[this,target,safe]{
            if(target){
                auto it=state->entries.find(target);
                if(it!=state->entries.end()&&it->transition==safe)it->transition=nullptr;
            }
            if(safe){safe->hide();safe->deleteLater();}
        });
        it->transition=surface;
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
            auto it=state->entries.find(dock);
            if(it->titleUpdatePending)break;
            it->titleUpdatePending=true;
            QPointer<QDockWidget> safe=dock;
            QTimer::singleShot(0,this,[this,safe]{
                if(!safe)return;
                auto entry=state->entries.find(safe);
                if(entry==state->entries.end())return;
                entry->titleUpdatePending=false;
                positionTitleButtons(safe);
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
