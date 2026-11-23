#include "dock-title-icons.h"
#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>
#include <QEvent>
#include <QTimer>
#include <QStyle>
#include <QStyleOptionDockWidget>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QPalette>
#include <QSet>
#include <QtGlobal>

namespace augment {
namespace {
class TitleGlyph final : public QWidget {
public:
    explicit TitleGlyph(DockTitleIcons::Kind kind,QWidget *parent)
        :QWidget(parent),kind_(kind){
        setObjectName("aftermarket-dock-title-icon");
        setFixedSize(14,14);
        setAttribute(Qt::WA_TransparentForMouseEvents,true);
        setAttribute(Qt::WA_NoSystemBackground,true);
        setFocusPolicy(Qt::NoFocus);
    }
    void setKind(DockTitleIcons::Kind kind){
        if(kind_==kind)return;
        kind_=kind;
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(width()/20.0,height()/20.0);
        const QColor stroke=palette().color(QPalette::Active,QPalette::WindowText);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(stroke,1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        auto line=[&](qreal x1,qreal y1,qreal x2,qreal y2){
            p.drawLine(QPointF(x1,y1),QPointF(x2,y2));
        };
        switch(kind_){
        case DockTitleIcons::Kind::Scenes:
            p.drawRoundedRect(QRectF(4,3.4,11,10.5),1.4,1.4);
            line(7,2,17,2);line(17,2,17,12);
            line(2,6.5,2,17);line(2,17,13,17);
            break;
        case DockTitleIcons::Kind::Sources:
            p.drawRoundedRect(QRectF(2.6,2.8,5,5),1.0,1.0);
            p.drawRoundedRect(QRectF(12.5,2.8,5,5),1.0,1.0);
            p.drawRoundedRect(QRectF(7.5,12.5,5,5),1.0,1.0);
            line(7.6,7.8,9.8,12.2);line(12.4,7.8,10.2,12.2);
            break;
        case DockTitleIcons::Kind::Audio:
            for(const qreal x : {4.0,10.0,16.0})line(x,2,x,18);
            p.drawRoundedRect(QRectF(1.8,5,4.4,4),1.1,1.1);
            p.drawRoundedRect(QRectF(7.8,11,4.4,4),1.1,1.1);
            p.drawRoundedRect(QRectF(13.8,6.4,4.4,4),1.1,1.1);
            break;
        case DockTitleIcons::Kind::Transitions:
            line(2.5,5,16.5,5);line(13,1.5,16.5,5);line(13,8.5,16.5,5);
            line(17.5,15,3.5,15);line(7,11.5,3.5,15);line(7,18.5,3.5,15);
            break;
        case DockTitleIcons::Kind::Controls:
            p.drawRoundedRect(QRectF(2,4.2,16,11.6),4,4);
            p.drawEllipse(QRectF(3.8,6,8,8));
            break;
        case DockTitleIcons::Kind::Preview:
            p.drawRoundedRect(QRectF(1.5,2.3,17,12),1.8,1.8);
            line(10,14.5,10,17.8);line(6.5,18,13.5,18);
            break;
        case DockTitleIcons::Kind::Chat:{
            QPainterPath shape;
            shape.moveTo(4,3);shape.lineTo(16,3);
            shape.quadTo(18,3,18,5);shape.lineTo(18,13);
            shape.quadTo(18,15,16,15);shape.lineTo(9,15);
            shape.lineTo(5,18);shape.lineTo(5,15);
            shape.quadTo(2,15,2,12);shape.lineTo(2,5);
            shape.quadTo(2,3,4,3);p.drawPath(shape);
            line(6,8,14,8);line(6,11,12,11);
            break;
        }
        case DockTitleIcons::Kind::Browser:
            p.drawEllipse(QRectF(2,2,16,16));
            p.drawEllipse(QRectF(7,2.5,6,15));
            line(2.5,10,17.5,10);
            break;
        case DockTitleIcons::Kind::Generic:
            p.drawRoundedRect(QRectF(2,3,16,14),1.6,1.6);
            line(7,3,7,17);line(2,7,18,7);
            break;
        }
    }
private:
    DockTitleIcons::Kind kind_;
};
}
DockTitleIcons::Kind DockTitleIcons::classify(const QString &objectName,const QString &title){
    const QString id=objectName.toLower();
    const QString label=title.toLower();
    const auto has=[&](const QString &part){
        return id.contains(part)||label.contains(part);
    };
    // Transition must be checked before Scenes ("Scene Transitions").
    if(has("transition"))return Kind::Transitions;
    if(has("mixer")||has("audio"))return Kind::Audio;
    if(has("sources")||has("source list")||id=="sourcedock")return Kind::Sources;
    if(has("scene"))return Kind::Scenes;
    if(has("control"))return Kind::Controls;
    if(has("preview")||has("program"))return Kind::Preview;
    if(has("chat")||has("message"))return Kind::Chat;
    if(has("browser")||has("web"))return Kind::Browser;
    return Kind::Generic;
}
int DockTitleIcons::glyphSizeForRoom(int freePixels){
    return freePixels>=21?14:freePixels>=14?10:0;
}
DockTitleIcons::DockTitleIcons(QMainWindow *window,QObject *parent)
    :QObject(parent),window_(window){
    if(window_)window_->installEventFilter(this);
    refresh();
}
DockTitleIcons::~DockTitleIcons(){
    if(window_)window_->removeEventFilter(this);
    for(auto it=icons_.begin();it!=icons_.end();++it){
        if(it.key())it.key()->removeEventFilter(this);
        if(it.value())delete it.value().data();
    }
    icons_.clear();
}
int DockTitleIcons::decoratedCount()const{return icons_.size();}
QWidget *DockTitleIcons::glyph(QDockWidget *dock)const{
    return icons_.value(dock);
}
void DockTitleIcons::refresh(){
    if(!window_)return;
    const auto docks=window_->findChildren<QDockWidget*>();
    for(auto *dock:docks){
        if(!dock)continue;
        const auto kind=classify(dock->objectName(),dock->windowTitle());
        if(icons_.contains(dock)&&icons_.value(dock)){
            static_cast<TitleGlyph*>(icons_.value(dock).data())->setKind(kind);
            continue;
        }
        auto *icon=new TitleGlyph(kind,dock);
        const bool registered=icons_.contains(dock);
        icons_.insert(dock,icon);
        if(!registered){
            dock->installEventFilter(this);
            connect(dock,&QObject::destroyed,this,[this,dock]{
                icons_.remove(dock); // never dereference a destroyed dock
            });
        }
    }
    for(auto it=icons_.begin();it!=icons_.end();++it)
        if(it.key()&&it.value())place(it.key(),it.value());
}
void DockTitleIcons::scheduleRefresh(){
    if(scheduled_)return;
    scheduled_=true;
    QTimer::singleShot(0,this,[this]{
        scheduled_=false;
        refresh();
    });
}
bool DockTitleIcons::eventFilter(QObject *watched,QEvent *event){
    if(watched==window_){
        switch(event->type()){
        case QEvent::ChildAdded:
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::StyleChange:
        case QEvent::PaletteChange:
        case QEvent::FontChange:
        case QEvent::LayoutRequest:
            scheduleRefresh();break;
        default:break;
        }
    }else if(auto *dock=qobject_cast<QDockWidget*>(watched)){
        if(icons_.contains(dock)){
            switch(event->type()){
            case QEvent::Resize:
            case QEvent::Move:
            case QEvent::Show:
            case QEvent::Hide:
            case QEvent::StyleChange:
            case QEvent::PaletteChange:
            case QEvent::FontChange:
            case QEvent::WindowTitleChange:
            case QEvent::LayoutRequest:
                scheduleRefresh();break;
            default:break;
            }
        }
    }
    return QObject::eventFilter(watched,event);
}
void DockTitleIcons::place(QDockWidget *dock,QWidget *icon){
    const auto conceal=[icon]{icon->hide();};
    if(!dock->isVisible()||dock->isFloating()||dock->titleBarWidget()||
       (dock->features()&QDockWidget::DockWidgetVerticalTitleBar)||!dock->widget()){
        conceal();return;
    }
    const int titleHeight=dock->widget()->geometry().top();
    if(titleHeight<16||titleHeight>48){conceal();return;}
    QStyleOptionDockWidget option;
    option.initFrom(dock);
    option.title=dock->windowTitle();
    option.closable=dock->features()&QDockWidget::DockWidgetClosable;
    option.floatable=dock->features()&QDockWidget::DockWidgetFloatable;
    option.movable=dock->features()&QDockWidget::DockWidgetMovable;
    option.verticalTitleBar=false;
    const QRect text=dock->style()->subElementRect(QStyle::SE_DockWidgetTitleBarText,&option,dock);
    if(!text.isValid()){conceal();return;}
    const int titleEnd=text.left()+QFontMetrics(dock->font())
        .horizontalAdvance(dock->windowTitle());
    const int size=glyphSizeForRoom(text.right()-titleEnd);
    // Use a miniature 10px symbol when long native labels leave limited
    // room. Hide if even that would intersect Qt's float/close hit area.
    if(!size){conceal();return;}
    if(icon->width()!=size)icon->setFixedSize(size,size);
    const int x=titleEnd+(size==14?4:2);
    icon->move(x,qMax(0,(titleHeight-icon->height())/2));
    icon->show();icon->raise();
}
}
