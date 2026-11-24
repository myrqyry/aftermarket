#include "panels.h"
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QDockWidget>
#include <QMainWindow>
#include <QPointer>
#include <QHash>
#include <QMouseEvent>
#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLayout>
#include <QBoxLayout>
#include <QSizePolicy>
#include <QFrame>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QButtonGroup>
#include <QFont>
#include "visual-polish.h"
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QPropertyAnimation>
#include <QSet>
#include <QEasingCurve>
#include <functional>
#include <algorithm>
namespace augment {
namespace {
QMainWindow *owner(QDockWidget *dock) {
    for(auto *w=dock?dock->parentWidget():nullptr;w;w=w->parentWidget())
        if(auto *main=qobject_cast<QMainWindow*>(w))return main;
    return nullptr;
}
int headerHeight(QDockWidget *dock) {
    if(!dock||!dock->widget())return 0;
    return dock->widget()->mapTo(dock,QPoint()).y();
}
}
struct Panels::State {
    struct Entry {
        QPointer<QWidget> content;
        QPointer<QMainWindow> main;
        int minimum,maximum,height,header,minimumWidth,width;
        bool contentWasHidden;
        QMetaObject::Connection destroyed,location,floating;
    };
    QHash<QDockWidget*,Entry> entries;
    QHash<QDockWidget*,QPointer<QPropertyAnimation>> heightMotions;
    QSet<QDockWidget*> observedDocks;
    struct ExpandBounds {
        int minimum,maximum,height,width;
        QPointer<QMainWindow> main;
    };
    QHash<QDockWidget*,ExpandBounds> pendingExpansion;
    std::function<bool()> motionEnabled;
    QPointer<QDialog> manager;
    struct Preview {
        QPointer<QMainWindow> main;
        QPointer<QDockWidget> dock;
        QPointer<QWidget> content, canvas;
        QPointer<QBoxLayout> canvasLayout;
        QBoxLayout::Direction originalDirection=QBoxLayout::LeftToRight;
        int baseWidgetCount=0;
        bool refreshPending=false;
        QSize dockedSize;
        QPoint dockedOrigin;
        StudioViewMode mode=StudioViewMode::Auto;
        QPointer<QWidget> studioToolbar,studioOptions,studioProgram,studioPreview;
        QPointer<QScrollArea> transitionStrip;
        QPointer<QBoxLayout> studioOptionsLayout;
        QBoxLayout::Direction originalOptionsDirection=QBoxLayout::TopToBottom;
        Qt::Alignment originalOptionsAlignment;
        QPointer<QLabel> programTitle,previewTitle;
        QFont originalProgramFont,originalPreviewFont,appliedProgramFont,appliedPreviewFont;
        bool studioPreviewHiddenByUs=false,studioProgramHiddenByUs=false;
        int originalPreviewStretch=0,originalProgramStretch=0;
    } preview;
};
Panels::Panels(QObject *p,std::function<bool()> motionEnabled):QObject(p),state(new State) {
    state->motionEnabled=std::move(motionEnabled);
    qApp->installEventFilter(this);
}
Panels::~Panels() {
    if(qApp)qApp->removeEventFilter(this);
    delete state->manager.data();
    restoreAll();
    restorePreview(state->preview.main.data());
}
bool Panels::previewDocked(QMainWindow *main)const {
    return main&&state->preview.main==main&&state->preview.dock&&state->preview.content;
}
QDockWidget *Panels::previewDock(QMainWindow *main)const {
    return previewDocked(main)?state->preview.dock.data():nullptr;
}

Panels::StudioViewMode Panels::studioViewMode(QMainWindow *main) const {
    return previewDocked(main)?state->preview.mode:StudioViewMode::Auto;
}
bool Panels::setStudioViewMode(QMainWindow *main,StudioViewMode mode) {
    if(!previewDocked(main))return false;
    state->preview.mode=mode;
    if(auto *bar=state->preview.studioToolbar.data()) {
        const QString name=mode==StudioViewMode::Auto?"auto":mode==StudioViewMode::Both?"both":
            mode==StudioViewMode::Program?"program":"preview";
        if(auto *button=bar->findChild<QToolButton*>("aftermarket-studio-"+name))
            button->setChecked(true);
    }
    updatePreviewLayout();
    return true;
}
bool Panels::setPreviewDocked(QMainWindow *main,bool enabled) {
    if(!main)return false;
    if(!enabled) {
        if(!previewDocked(main))return false;
        restorePreview(main);return !previewDocked(main);
    }
    if(previewDocked(main)) {
        state->preview.dock->show();state->preview.dock->raise();
        return true;
    }
    if(state->preview.dock)return false; // one main window is supported
    auto *central=main->centralWidget();
    const auto *canvas=central?central->findChild<QWidget*>("canvasEditor",Qt::FindDirectChildrenOnly):nullptr;
    auto *canvasLayout=canvas?qobject_cast<QBoxLayout*>(canvas->layout()):nullptr;
    if(!central||!canvas||!canvas->findChild<QWidget*>("preview")||!canvasLayout||
        !central->findChild<QWidget*>("contextContainer",Qt::FindDirectChildrenOnly))return false;
    // The OBS central widget contains both canvasEditor and the contextual
    // source toolbar. Dock them together so no large empty central region or
    // orphaned source controls remain.
    auto *content=main->takeCentralWidget();
    if(content!=central)return false;
    auto *dock=new QDockWidget("Preview",main);
    dock->setObjectName("aftermarket-preview-dock");
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setFeatures(QDockWidget::DockWidgetClosable |
                      QDockWidget::DockWidgetMovable |
                      QDockWidget::DockWidgetFloatable);
    state->preview={main,dock,content,const_cast<QWidget*>(canvas),canvasLayout,
        canvasLayout->direction(),qMin(2,canvasLayout->count()),false,{},{} };
    if(auto *nativePreview=canvas->findChild<QWidget*>("previewContainer",Qt::FindDirectChildrenOnly)){
        const int index=canvasLayout->indexOf(nativePreview);
        if(index>=0)state->preview.originalPreviewStretch=canvasLayout->stretch(index);
    }
    // A slim native toolbar is owned by Aftermarket, not OBS. It disappears
    // with the dock and never replaces or forwards any OBS transition event.
    if(auto *centralLayout=qobject_cast<QBoxLayout*>(content->layout())) {
        auto *toolbar=new QWidget(content);
        toolbar->setObjectName("aftermarket-studio-toolbar");
        auto *row=new QHBoxLayout(toolbar);
        row->setContentsMargins(6,2,6,2);row->setSpacing(4);
        auto *title=new QLabel("Studio",toolbar);
        title->setObjectName("aftermarket-studio-title");
        title->setFont(subtleHeadingFont(title->font(),0));
        row->addWidget(title);
        auto *group=new QButtonGroup(toolbar);group->setExclusive(true);
        for(const auto &item:{std::pair{"auto","Auto"},std::pair{"both","Both"},
                               std::pair{"program","Program"},std::pair{"preview","Preview"}}) {
            auto *button=new FineStateToolButton(toolbar);
            button->setObjectName(QStringLiteral("aftermarket-studio-")+item.first);
            button->setText(item.second);button->setCheckable(true);
            button->setAutoRaise(true);
            button->setToolTip(QStringLiteral("Studio display: ")+item.second);
            button->setMinimumHeight(24);
            group->addButton(button);row->addWidget(button);
            if(QString::fromLatin1(item.first)=="auto")button->setChecked(true);
        }
        auto *hint=new QLabel(toolbar);hint->setObjectName("aftermarket-studio-hint");
        row->addStretch(1);row->addWidget(hint);
        for(const auto &step:{std::pair{"left","‹"},std::pair{"right","›"}}) {
            auto *arrow=new QToolButton(toolbar);
            arrow->setObjectName(QStringLiteral("aftermarket-studio-scroll-")+step.first);
            arrow->setText(QString::fromUtf8(step.second));
            arrow->setAutoRaise(true);
            arrow->setMinimumSize(26,24);
            arrow->setToolTip("Browse native transition controls");
            row->addWidget(arrow);
            arrow->hide();
            const int direction=QString::fromLatin1(step.first)=="left"?-1:1;
            connect(arrow,&QToolButton::clicked,this,[this,direction]{
                auto *scroll=state->preview.transitionStrip.data();
                if(!scroll)return;
                auto *bar=scroll->horizontalScrollBar();
                bar->setValue(bar->value()+direction*qMax(90,scroll->viewport()->width()/2));
            });
        }
        centralLayout->insertWidget(0,toolbar);
        state->preview.studioToolbar=toolbar;
        toolbar->hide();
        connect(group,&QButtonGroup::buttonClicked,this,[this,main](QAbstractButton *clicked){
            if(!clicked||!previewDocked(main))return;
            const auto name=clicked->objectName();
            setStudioViewMode(main,name.endsWith("-both")?StudioViewMode::Both:
                name.endsWith("-program")?StudioViewMode::Program:
                name.endsWith("-preview")?StudioViewMode::Preview:StudioViewMode::Auto);
        });
    }
    connect(dock,&QDockWidget::topLevelChanged,this,[this,dock](bool floating){
        if(!floating)return;
        QPointer<QDockWidget> safe(dock);
        QTimer::singleShot(0,this,[this,safe]{
            if(!safe||state->preview.dock!=safe||!safe->isFloating())return;
            auto *screen=QGuiApplication::screenAt(safe->frameGeometry().center());
            if(!screen)screen=QGuiApplication::primaryScreen();
            if(!screen)return;
            const auto bounds=screen->availableGeometry().adjusted(8,8,-8,-8);
            QRect rect=safe->frameGeometry();
            const auto saved=state->preview.dockedSize;
            QSize initial=saved.isValid()?saved:QSize(680,420);
            const QSize maximum(qMin(bounds.width(),qMax(480,bounds.width()*4/5)),
                                qMin(bounds.height(),qMax(320,bounds.height()*4/5)));
            initial=initial.expandedTo(QSize(qMin(360,maximum.width()),qMin(240,maximum.height())))
                           .boundedTo(maximum);
            rect.setSize(initial);
            rect.moveTopLeft(state->preview.dockedOrigin);
            rect.moveLeft(qBound(bounds.left(),rect.left(),bounds.right()-rect.width()+1));
            rect.moveTop(qBound(bounds.top(),rect.top(),bounds.bottom()-rect.height()+1));
            if(rect!=safe->frameGeometry())safe->setGeometry(rect);
        });
    });
    content->hide();
    dock->setWidget(content);
    main->addDockWidget(Qt::RightDockWidgetArea,dock);
    dock->show();content->show();
    main->resizeDocks({dock},{qMax(260,main->height()*2/3)},Qt::Vertical);
    main->resizeDocks({dock},{qMax(380,main->width()/2)},Qt::Horizontal);
    state->preview.dockedSize=dock->size();
    state->preview.dockedOrigin=dock->mapToGlobal(QPoint());
    updatePreviewLayout();
    return true;
}
void Panels::restorePreview(QMainWindow *main) {
    if(!previewDocked(main))return;
    const auto item=state->preview;
    if(item.dock&&collapsed(item.dock))expand(item.dock);
    if(!item.content)return;
    auto *dock=item.dock.data();
    auto *content=item.content.data();
    content->hide();
    dock->hide();
    if(item.studioPreviewHiddenByUs&&item.studioPreview)item.studioPreview->show();
    if(item.studioProgramHiddenByUs&&item.studioProgram)item.studioProgram->show();
    restoreStudioControlStrip();
    if(item.studioOptionsLayout)item.studioOptionsLayout->setDirection(item.originalOptionsDirection);
    if(item.canvasLayout&&item.studioOptions)
        item.canvasLayout->setAlignment(item.studioOptions,item.originalOptionsAlignment);
    if(item.canvasLayout) {
        if(item.studioPreview) {
            const int idx=item.canvasLayout->indexOf(item.studioPreview);
            if(idx>=0)item.canvasLayout->setStretch(idx,item.originalPreviewStretch);
        }
        if(item.studioProgram) {
            const int idx=item.canvasLayout->indexOf(item.studioProgram);
            if(idx>=0)item.canvasLayout->setStretch(idx,item.originalProgramStretch);
        }
    }
    if(item.previewTitle&&item.previewTitle->font()==item.appliedPreviewFont)
        item.previewTitle->setFont(item.originalPreviewFont);
    if(item.programTitle&&item.programTitle->font()==item.appliedProgramFont)
        item.programTitle->setFont(item.originalProgramFont);
    if(item.studioToolbar)delete item.studioToolbar.data();
    if(item.canvasLayout)item.canvasLayout->setDirection(item.originalDirection);
    main->removeDockWidget(dock);
    // QDockWidget owns its content. Reparent it before deleting the dock;
    // setCentralWidget restores the identical original OBS widget tree.
    content->setParent(main);
    main->setCentralWidget(content);
    content->show();
    state->preview={};
    delete dock;
}
void Panels::restoreStudioControlStrip() {
    auto &entry=state->preview;
    auto *scroll=entry.transitionStrip.data();
    if(!scroll||!entry.canvasLayout)return;
    const int pos=entry.canvasLayout->indexOf(scroll);
    QWidget *widget=scroll->takeWidget(); // transfer OBS-owned widget, never delete it
    entry.canvasLayout->removeWidget(scroll);
    if(widget&&pos>=0) {
        widget->hide();
        widget->setParent(entry.canvas);
        entry.canvasLayout->insertWidget(pos,widget);
        entry.canvasLayout->setAlignment(widget,entry.originalOptionsAlignment);
        widget->show();
    }
    delete scroll;
    entry.transitionStrip=nullptr;
}
void Panels::updatePreviewLayout() {
    auto &entry=state->preview;
    if(!entry.main||!entry.dock||!entry.canvasLayout||!entry.canvas)return;
    const bool studio=entry.canvasLayout->count()>entry.baseWidgetCount;
    // OBS dynamically creates and destroys these widgets on Studio Mode
    // changes. Read the live layout, never retain a stale raw program pointer.
    QWidget *options=nullptr,*program=nullptr;
    QBoxLayout *optionsLayout=nullptr;
    if(studio&&entry.canvasLayout->count()>=4) {
        options=entry.canvasLayout->itemAt(entry.canvasLayout->count()-2)->widget();
        if(auto *scroll=qobject_cast<QScrollArea*>(options))options=scroll->widget();
        program=entry.canvasLayout->itemAt(entry.canvasLayout->count()-1)->widget();
        optionsLayout=options?qobject_cast<QBoxLayout*>(options->layout()):nullptr;
        if(!optionsLayout||!program||!program->findChild<QLabel*>()) {
            options=nullptr;program=nullptr;optionsLayout=nullptr;
        }
    }
    if(entry.studioOptions!=options) {
        restoreStudioControlStrip();
        // A new Studio Mode session receives new native controls.
        entry.studioOptions=options;
        entry.studioOptionsLayout=optionsLayout;
        if(optionsLayout) {
            entry.originalOptionsDirection=optionsLayout->direction();
            const int idx=entry.canvasLayout->indexOf(options);
            entry.originalOptionsAlignment=idx>=0?entry.canvasLayout->itemAt(idx)->alignment():Qt::Alignment{};
        }
    }
    if(entry.studioProgram!=program) {
        entry.studioProgram=program;
        entry.originalProgramStretch=program?
            entry.canvasLayout->stretch(entry.canvasLayout->indexOf(program)):0;
        entry.studioProgramHiddenByUs=false;
        entry.programTitle=nullptr;
    }
    auto *preview=entry.canvas->findChild<QWidget*>("previewContainer",Qt::FindDirectChildrenOnly);
    if(entry.studioPreview!=preview) {
        entry.studioPreview=preview;
        entry.studioPreviewHiddenByUs=false;
        entry.previewTitle=nullptr;
    }
    if(auto *bar=entry.studioToolbar.data())bar->setVisible(studio&&optionsLayout!=nullptr);
    const int width=entry.dock->width(),height=entry.dock->height();
    const bool wide=width>=900;
    const bool compact=studio&&!wide;
    auto desired=compact?QBoxLayout::BottomToTop:entry.originalDirection;
    if(entry.canvasLayout->direction()!=desired)entry.canvasLayout->setDirection(desired);
    if(optionsLayout) {
        const auto optionDirection=compact?QBoxLayout::LeftToRight:entry.originalOptionsDirection;
        if(optionsLayout->direction()!=optionDirection)optionsLayout->setDirection(optionDirection);
        if(compact&&!entry.transitionStrip&&entry.canvasLayout->indexOf(options)>=0) {
            const int position=entry.canvasLayout->indexOf(options);
            options->hide();
            entry.canvasLayout->removeWidget(options);
            auto *scroll=new QScrollArea(entry.canvas);
            scroll->setObjectName("aftermarket-studio-transition-strip");
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setWidgetResizable(false);
            scroll->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
            scroll->setMinimumWidth(0);
            scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            scroll->setFixedHeight(qBound(54,options->sizeHint().height()+
                scroll->style()->pixelMetric(QStyle::PM_ScrollBarExtent)+6,80));
            scroll->setWidget(options);
            entry.canvasLayout->insertWidget(position,scroll);
            entry.canvasLayout->setAlignment(scroll,{});
            entry.transitionStrip=scroll;
            scroll->show();
            options->show();
            QPointer<QScrollArea> safeScroll(scroll);
            connect(options,&QObject::destroyed,this,[this,safeScroll]{
                if(!safeScroll||state->preview.transitionStrip!=safeScroll)return;
                QTimer::singleShot(0,this,[this,safeScroll]{
                    if(safeScroll&&state->preview.transitionStrip==safeScroll) {
                        restoreStudioControlStrip();
                        updatePreviewLayout();
                    }
                });
            });
            auto updateArrows=[this,scroll] {
                if(state->preview.transitionStrip!=scroll)return;
                auto *bar=state->preview.studioToolbar.data();
                if(!bar)return;
                auto *sb=scroll->horizontalScrollBar();
                const int max=sb->maximum();
                for(const auto &step:{std::pair{"left",-1},std::pair{"right",1}}) {
                    auto *button=bar->findChild<QToolButton*>(
                        QStringLiteral("aftermarket-studio-scroll-")+step.first);
                    if(!button)continue;
                    button->setVisible(max>0);
                    button->setEnabled(step.second<0?sb->value()>0:sb->value()<max);
                }
            };
            // Qt recalculates the scrollbar range *during* geometry changes.
            // Never re-enter layout work from those synchronous signals.
            connect(scroll->horizontalScrollBar(),&QScrollBar::rangeChanged,this,
                [this,updateArrows](int,int){QTimer::singleShot(0,this,updateArrows);});
            connect(scroll->horizontalScrollBar(),&QScrollBar::valueChanged,this,
                [this,updateArrows](int){QTimer::singleShot(0,this,updateArrows);});
            QTimer::singleShot(0,this,updateArrows);
        } else if(!compact&&entry.transitionStrip) {
            restoreStudioControlStrip();
        }
        if(auto *scroll=entry.transitionStrip.data()) {
            if(options->size()!=options->sizeHint())options->adjustSize();
        }
    }
    if(!entry.transitionStrip) {
        if(auto *bar=entry.studioToolbar.data())
            for(const auto &name:{"left","right"})
                if(auto *button=bar->findChild<QToolButton*>(QStringLiteral("aftermarket-studio-scroll-")+name))
                    button->hide();
    }
    const bool focusAuto=width<430||height<380;
    const bool showProgram=entry.mode==StudioViewMode::Program||
        (entry.mode==StudioViewMode::Auto&&focusAuto);
    const bool showPreview=entry.mode==StudioViewMode::Preview;
    if(entry.studioPreview) {
        if(showProgram&&!entry.studioPreviewHiddenByUs&&entry.studioPreview->isVisible()){
            entry.studioPreview->hide();entry.studioPreviewHiddenByUs=true;
        } else if(!showProgram&&entry.studioPreviewHiddenByUs) {
            entry.studioPreviewHiddenByUs=false;entry.studioPreview->show();
        }
    }
    if(entry.studioProgram) {
        if(showPreview&&!entry.studioProgramHiddenByUs&&entry.studioProgram->isVisible()){
            entry.studioProgram->hide();entry.studioProgramHiddenByUs=true;
        } else if(!showPreview&&entry.studioProgramHiddenByUs) {
            entry.studioProgramHiddenByUs=false;entry.studioProgram->show();
        }
    }
    // Give the live Program a little more space in stacked mode. Focus
    // modes give the selected native display the available space instead
    // of leaving an OBS-sized empty region above/below it.
    auto applyStretch=[&](QWidget *widget,int amount){
        const int index=widget?entry.canvasLayout->indexOf(widget):-1;
        if(index>=0&&entry.canvasLayout->stretch(index)!=amount)
            entry.canvasLayout->setStretch(index,amount);
    };
    applyStretch(entry.studioPreview,compact?(showProgram?0:showPreview?6:2):entry.originalPreviewStretch);
    applyStretch(entry.studioProgram,compact?(showPreview?0:showProgram?6:3):entry.originalProgramStretch);
    auto *nativePreviewTitle=entry.studioPreview?
        entry.studioPreview->findChild<QLabel*>("previewLabel"):nullptr;
    auto *nativeProgramTitle=entry.studioProgram?
        entry.studioProgram->findChild<QLabel*>():nullptr;
    auto adaptFont=[compact](QPointer<QLabel> &tracked,QLabel *now,QFont &original,QFont &applied){
        if(tracked!=now){
            if(tracked&&tracked->font()==applied)tracked->setFont(original);
            tracked=now;
            if(now)original=now->font();
        }
        if(!tracked)return;
        if(compact){
            QFont font=original;
            font.setWeight(QFont::Medium);
            if(font.pixelSize()>0)font.setPixelSize(qMax(9,font.pixelSize()-1));
            else if(font.pointSizeF()>0)font.setPointSizeF(qMax(7.5,font.pointSizeF()-0.5));
            applied=font;
            if(tracked->font()!=font)tracked->setFont(font);
        } else if(tracked->font()==applied&&tracked->font()!=original){
            tracked->setFont(original);
        }
    };
    adaptFont(entry.previewTitle,nativePreviewTitle,entry.originalPreviewFont,entry.appliedPreviewFont);
    adaptFont(entry.programTitle,nativeProgramTitle,entry.originalProgramFont,entry.appliedProgramFont);
    if(auto *bar=entry.studioToolbar.data()) {
        if(auto *hint=bar->findChild<QLabel*>("aftermarket-studio-hint")){
            const auto mode=showPreview?"Preview focus":showProgram?"Program focus":wide?"Side by side":"Stacked";
            hint->setText(mode);
            hint->setVisible(width>=650);
        }
    }
}
void Panels::restoreExpandedBounds(QDockWidget *dock){
    if(!dock||!state->pendingExpansion.contains(dock))return;
    const auto bounds=state->pendingExpansion.take(dock);
    dock->setMaximumHeight(bounds.maximum);
    dock->setMinimumHeight(bounds.minimum);
    dock->updateGeometry();
    if(bounds.main&&!dock->isFloating()&&dock->isVisible()){
        bounds.main->resizeDocks({dock},{bounds.height},Qt::Vertical);
        bounds.main->resizeDocks({dock},{bounds.width},Qt::Horizontal);
    }
}
void Panels::stopHeightMotion(QDockWidget *dock){
    auto animation=state->heightMotions.take(dock);
    if(animation){animation->stop();delete animation.data();}
    // Emergency restore and rapid interruption must not strand a native
    // dock with its temporary expanding maximumHeight constraint.
    restoreExpandedBounds(dock);
}
void Panels::startHeightMotion(QDockWidget *dock,int start,int finish,
                              std::function<void()> completed){
    if(!dock||start==finish){if(completed)completed();return;}
    stopHeightMotion(dock);
    if(!state->observedDocks.contains(dock)){
        state->observedDocks.insert(dock);
        connect(dock,&QObject::destroyed,this,[this,dock]{
            state->observedDocks.remove(dock);
            state->heightMotions.remove(dock);
            state->pendingExpansion.remove(dock);
        });
    }
    auto *animation=new QPropertyAnimation(dock,"maximumHeight",dock);
    animation->setObjectName("aftermarket-dock-height-motion");
    animation->setDuration(165);
    animation->setEasingCurve(QEasingCurve::InOutCubic);
    animation->setStartValue(start);
    animation->setEndValue(finish);
    QPointer<QDockWidget> safe=dock;
    QPointer<QMainWindow> main=owner(dock);
    connect(animation,&QVariantAnimation::valueChanged,this,[safe,main](const QVariant &value){
        if(safe){
            // The native dock's hidden content no longer contributes a
            // minimum size. Pin both min and max to the current animated
            // height so QMainWindow cannot snap directly to the endpoint.
            safe->setMinimumHeight(value.toInt());
            safe->updateGeometry();
            if(main&&!safe->isFloating()&&safe->isVisible())
                main->resizeDocks({safe},{value.toInt()},Qt::Vertical);
        }
    });
    connect(animation,&QPropertyAnimation::finished,this,
        [this,safe,animation,completed=std::move(completed)]{
            if(safe&&state->heightMotions.value(safe)==animation){
                state->heightMotions.remove(safe);
                if(completed)completed();
            }
            animation->deleteLater();
        });
    state->heightMotions.insert(dock,animation);
    animation->start();
}
bool Panels::collapsed(QDockWidget *dock)const {return state->entries.contains(dock);}
bool Panels::canCollapse(QDockWidget *dock)const {
    if(!dock)return false;
    if(collapsed(dock))return true;
    auto *main=owner(dock);
    return main&&dock->isVisible()&&!dock->isFloating()&&dock->widget()&&
        dock->widget()->isVisible()&&!(dock->features()&QDockWidget::DockWidgetVerticalTitleBar)&&
        main->dockWidgetArea(dock)!=Qt::NoDockWidgetArea&&main->tabifiedDockWidgets(dock).isEmpty()&&
        headerHeight(dock)>=8&&headerHeight(dock)<dock->height()-4;
}
QAction *Panels::addCollapseAction(QMenu *menu,QDockWidget *dock) {
    if(!menu)return nullptr;
    const bool isCollapsed=collapsed(dock);
    auto *action=menu->addAction(isCollapsed?"Expand":"Collapse");
    action->setObjectName("aftermarket-collapse-action");
    action->setEnabled(canCollapse(dock));
    QPointer<Panels> owner=this;
    QPointer<QDockWidget> target=dock;
    connect(action,&QAction::triggered,menu,[owner,target]{
        if(owner&&target)owner->toggle(target);
    });
    return action;
}
bool Panels::toggle(QDockWidget *dock) {
    if(collapsed(dock))return expand(dock);
    if(!canCollapse(dock))return false;
    State::Entry entry{dock->widget(),owner(dock),dock->minimumHeight(),dock->maximumHeight(),
        dock->height(),headerHeight(dock),dock->minimumWidth(),dock->width(),dock->widget()->isHidden(),{},{},{}};
    entry.destroyed=connect(dock,&QObject::destroyed,this,[this,dock]{
        state->entries.remove(dock);
        state->heightMotions.remove(dock);
        state->pendingExpansion.remove(dock);
    });
    entry.location=connect(dock,&QDockWidget::dockLocationChanged,this,[this,dock]{expand(dock,false);});
    entry.floating=connect(dock,&QDockWidget::topLevelChanged,this,[this,dock]{expand(dock,false);});
    state->entries.insert(dock,entry);
    // Hidden content no longer contributes a width hint; keep the column
    // width so the title does not collapse to an ellipsis.
    dock->setMinimumWidth(entry.width);
    const bool motion=state->motionEnabled&&state->motionEnabled()&&
        dock->isVisible()&&dock->height()>entry.header+24&&
        dock->style()->styleHint(QStyle::SH_Widget_Animate,nullptr,dock);
    if(motion)dock->setMinimumHeight(entry.height);
    dock->widget()->hide();
    if(!motion)dock->setMinimumHeight(0);
    dock->setMaximumHeight(motion?entry.height:entry.header);
    dock->updateGeometry();
    if(motion) {
        if(entry.main)entry.main->resizeDocks({dock},{entry.height},Qt::Vertical);
        QPointer<QDockWidget> safe=dock;
        startHeightMotion(dock,entry.height,entry.header,[safe]{
            if(safe)safe->setMinimumHeight(0);
        });
    }else if(entry.main)entry.main->resizeDocks({dock},{entry.header},Qt::Vertical);
    return true;
}
bool Panels::expand(QDockWidget *dock,bool animate) {
    if(!collapsed(dock))return false;
    const auto e=state->entries.take(dock);
    disconnect(e.destroyed);disconnect(e.location);disconnect(e.floating);
    const int current=dock->maximumHeight();
    stopHeightMotion(dock);
    if(dock->minimumWidth()==e.width)dock->setMinimumWidth(e.minimumWidth);
    const bool motion=animate&&state->motionEnabled&&state->motionEnabled()&&
        dock->isVisible()&&!dock->isFloating()&&e.height>current+24&&
        dock->style()->styleHint(QStyle::SH_Widget_Animate,nullptr,dock);
    dock->setMinimumHeight(motion?current:e.minimum);
    dock->setMaximumHeight(motion?current:e.maximum);
    if(e.content&&e.content==dock->widget()&&!e.contentWasHidden)e.content->show();
    dock->updateGeometry();
    if(motion){
        QPointer<QDockWidget> safe=dock;
        startHeightMotion(dock,current,e.height,[this,safe]{
            if(safe)restoreExpandedBounds(safe);
        });
        state->pendingExpansion.insert(dock,{e.minimum,e.maximum,e.height,e.width,e.main});
    }else if(e.main&&!dock->isFloating()&&dock->isVisible()) {
        e.main->resizeDocks({dock},{e.height},Qt::Vertical);
        e.main->resizeDocks({dock},{e.width},Qt::Horizontal);
    }
    return true;
}
void Panels::restoreAll() {
    for(auto *dock:state->entries.keys())expand(dock,false);
    for(auto *dock:state->heightMotions.keys())stopHeightMotion(dock);
}
bool Panels::eventFilter(QObject *o,QEvent *event) {
    if(o==state->preview.dock&&event->type()==QEvent::Resize&&
       state->preview.dock&&!state->preview.dock->isFloating()) {
        state->preview.dockedSize=state->preview.dock->size();
        state->preview.dockedOrigin=state->preview.dock->mapToGlobal(QPoint());
    }
    if(state->preview.dock && (o==state->preview.dock||o==state->preview.canvas||
        o==state->preview.canvasLayout||o==state->preview.studioOptions||
        o==state->preview.studioOptionsLayout) &&
        (event->type()==QEvent::Resize||event->type()==QEvent::ChildAdded||
         event->type()==QEvent::ChildRemoved||event->type()==QEvent::LayoutRequest||
         event->type()==QEvent::Show||event->type()==QEvent::Hide)) {
        if(!state->preview.refreshPending) {
            state->preview.refreshPending=true;
            QTimer::singleShot(0,this,[this]{
                state->preview.refreshPending=false;
                updatePreviewLayout();
            });
        }
    }
    if(event->type()==QEvent::Close&&qobject_cast<QMainWindow*>(o)) {
        for(auto *dock:state->entries.keys())if(state->entries.value(dock).main==o)expand(dock,false);
        restorePreview(qobject_cast<QMainWindow*>(o));
        return false;
    }
    auto *dock=qobject_cast<QDockWidget*>(o);
    if(dock) {
        if(event->type()==QEvent::Hide||event->type()==QEvent::Close){
            expand(dock,false);
            stopHeightMotion(dock);
        }
        if(event->type()==QEvent::MouseButtonRelease) {
            auto *mouse=static_cast<QMouseEvent*>(event);
            const int header=collapsed(dock)?state->entries.value(dock).header:headerHeight(dock);
            if(mouse->button()==Qt::MiddleButton&&mouse->position().y()>=0&&mouse->position().y()<header&&
               dock->rect().contains(mouse->position().toPoint())&&toggle(dock))return true;
        }
    }
    return false;
}
QWidget *Panels::createPage(QMainWindow *main,QWidget *parent) {
    auto *dlg=new QWidget(parent);
    auto *layout=new QVBoxLayout(dlg);
    auto *hint=new QLabel("Use a dock title’s three-dot menu or middle-click its title to collapse/expand. You can also choose a panel here. Floating and tabbed docks are left alone.",dlg);
    hint->setWordWrap(true);layout->addWidget(hint);
    auto *previewRow=new QHBoxLayout;
    auto *previewToggle=new QPushButton("Make preview dockable",dlg);
    previewToggle->setObjectName("augment-preview-toggle");
    previewToggle->setToolTip("Move the existing OBS canvas into a movable, floatable, closable native dock. Session-only; restores when OBS closes.");
    auto *previewShow=new QPushButton("Show preview",dlg);
    previewShow->setObjectName("augment-preview-show");
    previewShow->setToolTip("Reopen a preview dock that was closed or hidden.");
    previewRow->addWidget(previewToggle,1);
    previewRow->addWidget(previewShow);
    layout->addLayout(previewRow);
    auto *previewStatus=new QLabel(dlg);
    previewStatus->setWordWrap(true);layout->addWidget(previewStatus);
    auto *list=new QListWidget(dlg);list->setObjectName("augment-panel-list");layout->addWidget(list,1);
    auto *status=new QLabel(dlg);status->setWordWrap(true);layout->addWidget(status);
    auto *buttons=new QHBoxLayout;
    auto *toggleButton=new QPushButton("Collapse / expand",dlg);toggleButton->setObjectName("augment-panel-toggle");
    auto *restoreButton=new QPushButton("Expand all",dlg);
    auto *refreshButton=new QPushButton("Refresh",dlg);refreshButton->setObjectName("augment-panel-refresh");
    buttons->addWidget(toggleButton);buttons->addWidget(restoreButton);buttons->addWidget(refreshButton);layout->addLayout(buttons);
    auto targets=std::make_shared<QVector<QPointer<QDockWidget>>>();
    QPointer<Panels> safe(this);
    QPointer<QMainWindow> window(main);
    auto refresh=[=]{
        if(!safe||!window)return;
        const bool docked=safe->previewDocked(window);
        const auto *canvas=window->centralWidget()?
            window->centralWidget()->findChild<QWidget*>("canvasEditor",Qt::FindDirectChildrenOnly):nullptr;
        previewToggle->setEnabled(docked||(canvas&&window->centralWidget()->findChild<QWidget*>("contextContainer",Qt::FindDirectChildrenOnly)));
        previewToggle->setText(docked?"Restore fixed preview":"Make preview dockable");
        const auto *previewDock=docked?safe->previewDock(window):nullptr;
        previewShow->setEnabled(previewDock&&!previewDock->isVisible());
        if(!docked&&canvas)
            previewStatus->setText("The existing preview stays in OBS until you enable docking. The source controls move with it.");
        else if(docked)
            previewStatus->setText(previewDock->isFloating()?"Preview is floating. Drag its title to dock.":"Preview is dockable. Drag its title, float it, or close it and reopen here.");
        else
            previewStatus->setText("OBS preview widget unavailable in this window.");
        QPointer<QDockWidget> previous;
        if(list->currentRow()>=0&&list->currentRow()<targets->size())previous=targets->at(list->currentRow());
        list->clear();targets->clear();
        auto docks=window->findChildren<QDockWidget*>();
        std::sort(docks.begin(),docks.end(),[](auto *a,auto *b){return a->windowTitle().localeAwareCompare(b->windowTitle())<0;});
        for(auto *dock:docks) {
            if(owner(dock)!=window||dock->isHidden()||dock->windowTitle().isEmpty())continue;
            const auto label=dock->windowTitle()+(safe->collapsed(dock)?"  ·  Collapsed":safe->canCollapse(dock)?"  ·  Expanded":"  ·  Unavailable");
            list->addItem(label);targets->append(dock);
            if(dock==previous)list->setCurrentRow(list->count()-1);
        }
        if(list->currentRow()<0&&list->count())list->setCurrentRow(0);
        const int row=list->currentRow();
        toggleButton->setEnabled(row>=0&&row<targets->size()&&safe->canCollapse(targets->at(row)));
    };
    connect(previewToggle,&QPushButton::clicked,dlg,[=]{
        if(!safe||!window)return;
        const bool enable=!safe->previewDocked(window);
        if(!safe->setPreviewDocked(window,enable))
            status->setText("The preview could not be moved safely in this layout.");
        else status->setText(enable?"Preview moved into a native dock.":"Original fixed preview restored.");
        refresh();
    });
    connect(previewShow,&QPushButton::clicked,dlg,[=]{
        if(safe&&window&&safe->previewDocked(window))
            safe->setPreviewDocked(window,true);
        refresh();
    });
    connect(toggleButton,&QPushButton::clicked,dlg,[=]{
        const int row=list->currentRow();
        if(!safe||row<0||row>=targets->size()||!targets->at(row))return;
        auto *dock=targets->at(row).data();
        status->setText(safe->toggle(dock)?(safe->collapsed(dock)?"Panel collapsed.":"Panel expanded."):"This panel cannot be collapsed in its current layout.");
        refresh();
    });
    connect(restoreButton,&QPushButton::clicked,dlg,[=]{if(!safe)return;safe->restoreAll();status->setText("All collapsed panels expanded.");refresh();});
    connect(refreshButton,&QPushButton::clicked,dlg,refresh);
    connect(list,&QListWidget::currentRowChanged,dlg,[=](int row){
        toggleButton->setEnabled(safe&&row>=0&&row<targets->size()&&safe->canCollapse(targets->at(row)));
    });
    refresh();return dlg;
}
void Panels::showManager(QMainWindow *main) {
    if(!main)return;
    if(state->manager){state->manager->raise();state->manager->activateWindow();return;}
    auto *dlg=new QDialog(main);state->manager=dlg;
    dlg->setObjectName("aftermarket-panels");
    dlg->setWindowTitle("Aftermarket · Panels");
    dlg->setAttribute(Qt::WA_DeleteOnClose);dlg->resize(400,380);
    auto *layout=new QVBoxLayout(dlg);layout->setContentsMargins(0,0,0,0);
    layout->addWidget(createPage(main,dlg));dlg->show();
}
}
