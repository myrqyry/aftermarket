#include "dock-title-icons.h"
#include "app-text-elision.h"
#include "dock-header-button.h"
#include "panels.h"
#include <QMainWindow>
#include <QDockWidget>
#include <QMenu>
#include <QSettings>
#include <QToolButton>
#include <QWidget>
#include <QEvent>
#include <QTimer>
#include <QStyle>
#include <QAction>
#include <QScopedValueRollback>
#include <QApplication>
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
DockTitleIcons::Kind DockTitleIcons::classify(const QString &objectName,const QString &title,const QString &contentClass){
    if(contentClass=="SourceDock")return Kind::Sources;
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
DockTitleIcons::DockTitleIcons(QMainWindow *window,Panels *panels,
                               const QString &settingsPath,QObject *parent)
    :QObject(parent),window_(window),panels_(panels),settingsPath_(settingsPath){
    if(!settingsPath_.isEmpty()){
        const QSettings settings(settingsPath_,QSettings::IniFormat);
        if(settings.value("title/ellipsis","end").toString()=="middle")
            ellipsisMode_=Qt::ElideMiddle;
    }
    if(window_)window_->installEventFilter(this);
    applicationElision_=std::make_unique<AppTextElision>(window_,this);
    applicationElision_->setMode(ellipsisMode_);
    refresh();
}
DockTitleIcons::~DockTitleIcons(){
    if(window_)window_->removeEventFilter(this);
    for(auto it=icons_.begin();it!=icons_.end();++it){
        if(auto *dock=it.key()){
            dock->removeEventFilter(this);
            restoreTitle(dock);
            restoreNativeButtons(dock);
        }
        if(it.value())delete it.value().data();
    }
    icons_.clear();
    for(const auto &button:moreButtons_)if(button)delete button.data();
    moreButtons_.clear();
    originalTitles_.clear();
    originalActionLabels_.clear();
    displayedTitles_.clear();
}
int DockTitleIcons::decoratedCount()const{return icons_.size();}
bool DockTitleIcons::titleIndented(QDockWidget *dock) const{
    return dock&&displayedTitles_.contains(dock)&&
           dock->windowTitle()==displayedTitles_.value(dock);
}
QString DockTitleIcons::originalTitle(QDockWidget *dock)const{
    return originalTitles_.value(dock,dock?dock->windowTitle():QString{});
}
void DockTitleIcons::restoreTitle(QDockWidget *dock){
    if(!dock)return;
    if(titleIndented(dock)){
        QScopedValueRollback<bool> updating(adjustingTitle_,true);
        dock->setWindowTitle(originalTitles_.value(dock));
    }
    displayedTitles_.remove(dock);
    if(auto *action=dock->toggleViewAction()){
        if(originalActionLabels_.contains(dock))
            action->setText(originalActionLabels_.value(dock));
    }
}
QDockWidget::DockWidgetFeatures DockTitleIcons::actualDockFeatures(QDockWidget *dock) const {
    return originalDockFeatures_.value(dock,dock?dock->features():
                                             QDockWidget::NoDockWidgetFeatures);
}
void DockTitleIcons::suppressNativeButtons(QDockWidget *dock){
    if(!dock||dock->isFloating()||dock->titleBarWidget())return;
    constexpr auto managed=QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable;
    const auto before=dock->features();
    if(!(before&managed))return;
    if(!originalDockFeatures_.contains(dock))
        originalDockFeatures_.insert(dock,before);
    const QScopedValueRollback<bool> guard(adjustingFeatures_,true);
    dock->setFeatures(before & ~managed);
}
void DockTitleIcons::restoreNativeButtons(QDockWidget *dock){
    if(!dock)return;
    auto it=originalDockFeatures_.find(dock);
    if(it==originalDockFeatures_.end())return;
    const auto original=it.value();
    originalDockFeatures_.erase(it);
    const QScopedValueRollback<bool> guard(adjustingFeatures_,true);
    // Keep externally changed non-window features (movable/vertical title),
    // but restore the native float/close capabilities we temporarily hid.
    constexpr auto managed=QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable;
    const auto restored=(dock->features() & ~managed) | (original & managed);
    if(dock->features()!=restored)dock->setFeatures(restored);
}
void DockTitleIcons::appendDockWindowActions(QMenu *menu,QDockWidget *dock){
    if(!menu||!dock)return;
    const auto native=actualDockFeatures(dock);
    auto *popout=menu->addAction(dock->style()->standardIcon(QStyle::SP_TitleBarNormalButton),
                                 "Pop out");
    popout->setObjectName("aftermarket-dock-popout-action");
    popout->setEnabled(!dock->isFloating() &&
                        bool(native&QDockWidget::DockWidgetFloatable));
    auto *close=menu->addAction(dock->style()->standardIcon(QStyle::SP_DockWidgetCloseButton),
                                "Close");
    close->setObjectName("aftermarket-dock-close-action");
    // OBS deliberately makes its five built-in docks non-closable.
    // Do not silently override their native feature flags.
    close->setEnabled(bool(native&QDockWidget::DockWidgetClosable));
    QPointer<DockTitleIcons> owner=this;
    QPointer<QDockWidget> target=dock;
    QObject::connect(popout,&QAction::triggered,menu,[owner,target]{
        // Delay until Qt finishes dispatching the action from a QMenu that
        // may be inside the widget being detached.
        if(!owner)return;
        QTimer::singleShot(0,owner,[owner,target]{
            if(!owner||!target||target->isFloating()||
               !(owner->actualDockFeatures(target)&QDockWidget::DockWidgetFloatable))return;
            owner->restoreNativeButtons(target);
            target->setFloating(true);
            owner->scheduleRefresh();
        });
    });
    QObject::connect(close,&QAction::triggered,menu,[owner,target]{
        if(!owner)return;
        QTimer::singleShot(0,owner,[owner,target]{
            if(!owner||!target)return;
            if(!(owner->actualDockFeatures(target)&
                 QDockWidget::DockWidgetClosable))return;
            owner->restoreNativeButtons(target);
            target->close(); // native closeEvent / visibilityChanged lifecycle
            owner->scheduleRefresh();
        });
    });
}
QWidget *DockTitleIcons::glyph(QDockWidget *dock)const{
    return icons_.value(dock);
}
QToolButton *DockTitleIcons::moreButton(QDockWidget *dock)const{
    return moreButtons_.value(dock);
}
Qt::TextElideMode DockTitleIcons::ellipsisMode()const{return ellipsisMode_;}
bool DockTitleIcons::setEllipsisMode(Qt::TextElideMode mode){
    if(mode!=Qt::ElideRight&&mode!=Qt::ElideMiddle)return false;
    if(mode==ellipsisMode_)return true;
    if(!settingsPath_.isEmpty()){
        QSettings settings(settingsPath_,QSettings::IniFormat);
        settings.setValue("title/ellipsis",mode==Qt::ElideMiddle?"middle":"end");
        settings.sync();
        if(settings.status()!=QSettings::NoError)return false;
    }
    ellipsisMode_=mode;
    if(applicationElision_)applicationElision_->setMode(mode);
    refresh();
    return true;
}
void DockTitleIcons::setSourceDockSettingsAction(std::function<QAction*()> action){
    sourceDockSettingsAction_=std::move(action);
}
void DockTitleIcons::refresh(){
    if(!window_)return;
    const auto docks=window_->findChildren<QDockWidget*>();
    for(auto *dock:docks){
        if(!dock)continue;
        const auto kind=classify(dock->objectName(),originalTitle(dock),
            dock->widget()&&dock->widget()->inherits("SourceDock")?"SourceDock":QString{});
        if(icons_.contains(dock)&&icons_.value(dock)){
            static_cast<TitleGlyph*>(icons_.value(dock).data())->setKind(kind);
            continue;
        }
        auto *more=new DockHeaderButton(DockHeaderGlyph::More,dock);
        more->setObjectName("aftermarket-dock-more");
        more->setToolTip("Dock options");
        more->setAccessibleName("Dock options");
        auto *menu=new QMenu(more);
        menu->setObjectName("aftermarket-dock-menu");
        more->setMenu(menu);
        more->setPopupMode(QToolButton::InstantPopup);
        QPointer<QDockWidget> safe=dock;
        QPointer<Panels> panel=panels_;
        QPointer<DockTitleIcons> owner=this;
        connect(menu,&QMenu::aboutToShow,more,[menu,safe,panel,owner]{
            menu->clear();
            if(!safe||!owner)return;
            if(panel)panel->addCollapseAction(menu,safe);
            else {
                auto *unavailable=menu->addAction("Collapse");
                unavailable->setEnabled(false);
            }
            menu->addSeparator();
            owner->appendDockWindowActions(menu,safe);
            if(safe->widget()&&safe->widget()->inherits("SourceDock")&&owner->sourceDockSettingsAction_){
                QPointer<QAction> native=owner->sourceDockSettingsAction_();
                if(native&&native->isEnabled()){
                    menu->addSeparator();
                    auto *settings=menu->addAction("Configure source docks…");
                    settings->setObjectName("aftermarket-source-dock-settings");
                    connect(settings,&QAction::triggered,menu,[native]{
                        if(native)QTimer::singleShot(0,native,[native]{if(native&&native->isEnabled())native->trigger();});
                    });
                }
            }
        });
        moreButtons_.insert(dock,more);
        auto *icon=new TitleGlyph(kind,dock);
        const bool registered=icons_.contains(dock);
        icons_.insert(dock,icon);
        originalTitles_.insert(dock,dock->windowTitle());
        originalActionLabels_.insert(dock,dock->toggleViewAction()->text());
        if(!registered){
            // OBS/plugins can add QDockWidget before QMainWindow has assigned
            // usable title geometry; replay placement once layout settles.
            QTimer::singleShot(90,this,[this]{refresh();});
            dock->installEventFilter(this);
            connect(dock,&QDockWidget::topLevelChanged,this,
                    [this](bool){scheduleRefresh();});
            connect(dock,&QDockWidget::featuresChanged,this,
                    [this,dock](QDockWidget::DockWidgetFeatures features){
                if(adjustingFeatures_)return;
                auto it=originalDockFeatures_.find(dock);
                if(it!=originalDockFeatures_.end()){
                    // Preserve changes made to nonmanaged flags by OBS and
                    // other plugins while our buttons are hidden.
                    constexpr auto managed=QDockWidget::DockWidgetClosable |
                                           QDockWidget::DockWidgetFloatable;
                    if(features==QDockWidget::NoDockWidgetFeatures||
                       (features&managed))
                        it.value()=features; // OBS Lock Docks / unlock
                    else
                        it.value()=(it.value()&managed)|(features&~managed);
                }
                scheduleRefresh();
            });
            connect(dock,&QObject::destroyed,this,[this,dock]{
                icons_.remove(dock);
                originalDockFeatures_.remove(dock);
                moreButtons_.remove(dock);
                originalTitles_.remove(dock);
                originalActionLabels_.remove(dock);
                displayedTitles_.remove(dock); // never dereference destroyed dock
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
                if(!adjustingTitle_&&
                   (!displayedTitles_.contains(dock)||
                    dock->windowTitle()!=displayedTitles_.value(dock))){
                    // A genuine OBS/plugin rename supersedes the previous
                    // decorative title; never discard it on teardown.
                    originalTitles_[dock]=dock->windowTitle();
                    originalActionLabels_[dock]=dock->windowTitle();
                    displayedTitles_.remove(dock);
                    scheduleRefresh();
                }
                break;
            case QEvent::LayoutRequest:
                scheduleRefresh();break;
            default:break;
            }
            if(event->type()==QEvent::Show||event->type()==QEvent::Hide)
                scheduleRefresh();
        }
    }
    return QObject::eventFilter(watched,event);
}
void DockTitleIcons::place(QDockWidget *dock,QWidget *icon){
    auto *more=moreButton(dock);
    const auto conceal=[this,dock,icon,more]{
        if(icon)icon->hide();
        if(more)more->hide();
        restoreTitle(dock);
        restoreNativeButtons(dock);
    };
    if(!dock||!icon||!dock->isVisible()||dock->isFloating()||
       dock->titleBarWidget()||
       (dock->features()&QDockWidget::DockWidgetVerticalTitleBar)||!dock->widget()){
        conceal();return;
    }
    const int titleHeight=dock->widget()->geometry().top();
    if(titleHeight<16||titleHeight>48){conceal();return;}
    // Expand the native title's text area by suppressing ONLY the two Qt
    // window buttons; a working Aftermarket overflow will provide both.
    suppressNativeButtons(dock);
    QStyleOptionDockWidget option;
    option.initFrom(dock);
    option.title=originalTitle(dock);
    option.closable=dock->features()&QDockWidget::DockWidgetClosable;
    option.floatable=dock->features()&QDockWidget::DockWidgetFloatable;
    option.movable=dock->features()&QDockWidget::DockWidgetMovable;
    option.verticalTitleBar=false;
    const QRect text=dock->style()->subElementRect(
        QStyle::SE_DockWidgetTitleBarText,&option,dock);
    if(!text.isValid()){conceal();return;}
    const QFontMetrics metrics(dock->font());
    const QString title=originalTitle(dock);
    const int titleWidth=metrics.horizontalAdvance(title);
    // Reserve the native title control for all native-titled docks, even
    // when a long label must elide. The original title and Docks menu action
    // remain unchanged; only the painted display text is shortened.
    auto *mixerMore=dock->findChild<QToolButton*>("aftermarket-mixer-more",
                                                  Qt::FindDirectChildrenOnly);
    const bool specialized=mixerMore&&mixerMore->isVisible();
    const int moreSpace=more?more->width()+4:0;
    const int fullRoom=text.width()-titleWidth-moreSpace;
    const int size=glyphSizeForRoom(fullRoom);

    QString spacer;
    if(size){
        const int required=size==14?titleTextInset():14;
        while(metrics.horizontalAdvance(spacer)<required)
            spacer+=QLatin1Char(' ');
    }
    const int actual=metrics.horizontalAdvance(spacer);
    const int maxLabelWidth=qMax(0,text.width()-actual-moreSpace-4);
    const QString renderedTitle=titleWidth<=maxLabelWidth?title:
        metrics.elidedText(title,ellipsisMode_,maxLabelWidth);
    if(renderedTitle.isEmpty()){conceal();return;}
    const QString displayed=spacer+renderedTitle;
    if(dock->windowTitle()!=displayed){
        QScopedValueRollback<bool> updating(adjustingTitle_,true);
        dock->setWindowTitle(displayed);
    }
    displayedTitles_[dock]=displayed;
    if(auto *action=dock->toggleViewAction()){
        if(originalActionLabels_.contains(dock))
            action->setText(originalActionLabels_.value(dock));
    }
    if(size){
        if(icon->width()!=size)icon->setFixedSize(size,size);
        icon->move(text.left()+2,qMax(0,(titleHeight-icon->height())/2));
        icon->show();icon->raise();
    }else icon->hide();

    if(!more)return;
    const int x=text.right()-more->width()+1;
    if(!specialized&&x>=text.left()+actual+metrics.horizontalAdvance(renderedTitle)+3){
        more->move(x,qMax(0,(titleHeight-more->height())/2));
        more->show();more->raise();
    }else more->hide();
    if(!more->isVisible()&&!specialized){
        // A dock without an accessible menu must retain native buttons.
        conceal();
    }
}
}
