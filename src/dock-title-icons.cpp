#include "dock-title-icons.h"
#include "plugin-theme-icons.h"
#include "app-text-elision.h"
#include "dock-header-button.h"
#include "panels.h"
#include "dock-dimensions.h"
#include <QMainWindow>
#include <QLayout>
#include <QDockWidget>
#include <QMenu>
#include <QSettings>
#include <QToolButton>
#include <QWidget>
#include <QEvent>
#include <QHoverEvent>
#include <QTimer>
#include <QStyle>
#include <QAction>
#include <QActionGroup>
#include <QMetaProperty>
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
    void setPluginGlyph(const PluginThemeIcons::Glyph &glyph){
        if(pluginGlyph_==glyph)return;
        pluginGlyph_=glyph;
        pluginValid_=glyph.valid();
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto group=isEnabled()?QPalette::Active:QPalette::Disabled;
        const QColor stroke=palette().color(group,QPalette::WindowText);
        if(pluginValid_){
            const auto pix=pluginGlyph_.tinted(stroke,size(),devicePixelRatioF());
            if(!pix.isNull()){p.drawPixmap(QPoint(0,0),pix);return;}
        }
        p.scale(width()/20.0,height()/20.0);
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
    PluginThemeIcons::Glyph pluginGlyph_;
    bool pluginValid_=false;
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
    dimensions_=std::make_unique<DockDimensions>(window_,panels_,settingsPath_);
    refresh();
}
DockTitleIcons::~DockTitleIcons(){
    dimensions_.reset();
    if(window_)window_->removeEventFilter(this);
    for(auto it=icons_.begin();it!=icons_.end();++it){
        if(auto *dock=it.key()){
            dock->removeEventFilter(this);
            dock->setAttribute(Qt::WA_Hover,originalHover_.value(dock));
            if(auto *button=dynamic_cast<DockHeaderButton*>(dock->findChild<QToolButton*>("aftermarket-mixer-more",Qt::FindDirectChildrenOnly)))button->setDockHoverReveal(false);
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
    if(dimensions_)dimensions_->appendActions(menu,dock);
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
    const PluginThemeIcons pluginIcons;
    for(auto *dock:docks){
        if(!dock)continue;
        if(dimensions_)dimensions_->observe(dock);
        // These third-party docks use dynamic IDs. Match their verified
        // native Qt content types, never the user-editable dock title.
        QString contentClass;
        if(auto *content=dock->widget()){
            if(content->inherits("SourceDock"))contentClass="SourceDock";
            else if(content->inherits("QuickAccessDock"))contentClass="QuickAccessDock";
        }
        const auto kind=classify(dock->objectName(),originalTitle(dock),contentClass);
        const auto pluginGlyph=pluginIcons.dock(dock->objectName(),contentClass);
        if(icons_.contains(dock)&&icons_.value(dock)){
            auto *glyph=static_cast<TitleGlyph*>(icons_.value(dock).data());
            glyph->setKind(kind);
            glyph->setPluginGlyph(pluginGlyph);
            continue;
        }
        auto *more=new DockHeaderButton(DockHeaderGlyph::More,dock);
        more->setMotionProvider([panels=panels_]{return panels&&panels->motionAllowed();});
        more->setObjectName("aftermarket-dock-more");
        more->setToolTip("Dock options");
        more->setAccessibleName("Dock options");
        auto *menu=new QMenu(more);
        menu->setObjectName("aftermarket-dock-menu");
        more->setMenu(menu);
        more->setPopupMode(QToolButton::InstantPopup);
        more->setDockHoverReveal(true);
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
            if(auto *content=safe->widget();content&&content->inherits("SourceDock")){
                const int visibleIndex=content->metaObject()->indexOfProperty("previewVisible");
                if(visibleIndex>=0&&content->metaObject()->property(visibleIndex).isWritable()&&
                   content->property("previewSupported").toBool()){
                    auto *controls=menu->addAction("Controls only");
                    controls->setObjectName("aftermarket-source-controls-only");
                    controls->setCheckable(true);
                    controls->setChecked(!content->property("previewVisible").toBool());
                    QPointer<QWidget> widget=content;
                    connect(controls,&QAction::triggered,menu,[widget,owner](bool checked){
                        if(widget&&widget->setProperty("previewVisible",!checked)&&
                           widget->property("previewVisible").toBool()==!checked&&owner&&owner->sourceDockPreviewSave_)
                            owner->sourceDockPreviewSave_();
                    });
                }
                const int index=content->metaObject()->indexOfProperty("previewRefreshFps");
                if(index>=0&&content->metaObject()->property(index).isWritable()&&
                   content->property("previewRefreshAvailable").toBool()){
                    menu->addSeparator();
                    auto *rates=menu->addMenu("Preview refresh");
                    rates->setObjectName("aftermarket-source-preview-refresh");
                    auto *group=new QActionGroup(rates);group->setExclusive(true);
                    const int current=content->property("previewRefreshFps").toInt();
                    const QList<QPair<int,QString>> modes={{0,"Live"},{30,"30 FPS"},{15,"15 FPS"},
                        {10,"10 FPS"},{5,"5 FPS"},{2,"2 FPS"},{1,"1 FPS"},{-1,"Freeze frame"}};
                    QPointer<QWidget> widget=content;
                    for(const auto &mode:modes){
                        auto *action=rates->addAction(mode.second);action->setCheckable(true);
                        action->setChecked(current==mode.first);group->addAction(action);
                        connect(action,&QAction::triggered,rates,[widget,owner,fps=mode.first]{
                            if(widget&&widget->setProperty("previewRefreshFps",fps)&&
                               widget->property("previewRefreshFps").toInt()==fps&&owner&&owner->sourceDockPreviewSave_)
                                owner->sourceDockPreviewSave_();
                        });
                    }
                    const int qualityIndex=content->metaObject()->indexOfProperty("previewResolution");
                    if(qualityIndex>=0&&content->metaObject()->property(qualityIndex).isWritable()){
                        rates->addSeparator();
                        auto *quality=rates->addMenu("Cached resolution");
                        quality->setObjectName("aftermarket-source-preview-resolution");
                        quality->setToolTipsVisible(true);
                        quality->menuAction()->setToolTip("Longest edge of reduced-rate or frozen previews. Live uses native resolution.");
                        auto *qualityGroup=new QActionGroup(quality);qualityGroup->setExclusive(true);
                        const QList<QPair<int,QString>> choices={{2048,"Full (up to 2048 px)"},{1280,"1280 px"},
                            {640,"640 px"},{320,"320 px"}};
                        for(const auto &choice:choices){
                            auto *a=quality->addAction(choice.second);a->setCheckable(true);
                            a->setChecked(content->property("previewResolution").toInt()==choice.first);
                            qualityGroup->addAction(a);
                            connect(a,&QAction::triggered,quality,[widget,owner,limit=choice.first]{
                                if(widget&&widget->setProperty("previewResolution",limit)&&
                                   widget->property("previewResolution").toInt()==limit&&owner&&owner->sourceDockPreviewSave_)
                                    owner->sourceDockPreviewSave_();
                            });
                        }
                    }
                    rates->setToolTipsVisible(true);
                    rates->menuAction()->setToolTip("Per-dock SDR preview only; Program and source output are unchanged.");
                }
            }
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
        icon->setPluginGlyph(pluginGlyph);
        const bool registered=icons_.contains(dock);
        icons_.insert(dock,icon);
        originalTitles_.insert(dock,dock->windowTitle());
        originalActionLabels_.insert(dock,dock->toggleViewAction()->text());
        if(!registered){
            // OBS/plugins can add QDockWidget before QMainWindow has assigned
            // usable title geometry; replay placement once layout settles.
            QTimer::singleShot(90,this,[this]{refresh();});
            originalHover_.insert(dock,dock->testAttribute(Qt::WA_Hover));
            dock->setAttribute(Qt::WA_Hover,true);
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
                originalHover_.remove(dock);headerHovered_.remove(dock);
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
void DockTitleIcons::setHeaderHovered(QDockWidget *dock,bool hovered){
    headerHovered_[dock]=hovered;
    if(auto *button=dynamic_cast<DockHeaderButton*>(moreButton(dock)))button->setHeaderHovered(hovered);
    if(auto *button=dynamic_cast<DockHeaderButton*>(dock->findChild<QToolButton*>("aftermarket-mixer-more",Qt::FindDirectChildrenOnly)))button->setHeaderHovered(hovered);
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
            case QEvent::HoverEnter:case QEvent::HoverMove:{
                const auto pos=static_cast<QHoverEvent*>(event)->position();
                setHeaderHovered(dock,dock->widget()&&pos.x()>=0&&pos.x()<dock->width()&&pos.y()>=0&&pos.y()<dock->widget()->geometry().top());
                break;
            }
            case QEvent::HoverLeave:case QEvent::Leave:
                setHeaderHovered(dock,false);break;
            case QEvent::Resize:
            case QEvent::Move:
            case QEvent::Show:
            case QEvent::Hide:
            case QEvent::StyleChange:
            case QEvent::PaletteChange:
            case QEvent::FontChange:
                // Geometry and theme changes can make the full title fit
                // again even when the decorated windowTitle is unchanged.
                scheduleRefresh();break;
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
    int titleHeight=dock->widget()->geometry().top();
    if(titleHeight<14||titleHeight>48){conceal();return;}
    // Expand the native title's text area by suppressing ONLY the two Qt
    // window buttons; a working Aftermarket overflow will provide both.
    suppressNativeButtons(dock);
    if(dock->layout())dock->layout()->activate();
    titleHeight=dock->widget()->geometry().top();
    if(titleHeight<14||titleHeight>48){conceal();return;}
    if(auto *button=dynamic_cast<DockHeaderButton*>(more))button->fitTitleHeight(titleHeight);
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
    if(auto *button=dynamic_cast<DockHeaderButton*>(mixerMore)){
        button->setDockHoverReveal(true);
        button->setHeaderHovered(headerHovered_.value(dock));
    }
    // The menu needs its real width plus a single clearance gap.
    // Select decoration using measured padding (spaces are font-dependent),
    // not a nominal pixel estimate that can force an otherwise fitting name
    // to elide. Prefer the complete title over either size of icon.
    const auto *menuButton=specialized?mixerMore:more;
    constexpr int clearance=3;
    const int moreSpace=menuButton?menuButton->width()+clearance:0;
    const int available=qMax(0,text.width()-moreSpace);
    QString spacer;
    int size=0;
    for(const int candidate:{14,10}){
        QString padding;
        const int required=candidate==14?titleTextInset():14;
        while(metrics.horizontalAdvance(padding)<required)
            padding+=QLatin1Char(' ');
        if(titleWidth+metrics.horizontalAdvance(padding)<=available){
            size=candidate;spacer=padding;break;
        }
    }
    const int actual=metrics.horizontalAdvance(spacer);
    const int maxLabelWidth=qMax(0,available-actual);
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
