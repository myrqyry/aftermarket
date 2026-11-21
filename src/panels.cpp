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
#include <QTimer>
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
    } preview;
};
Panels::Panels(QObject *p):QObject(p),state(new State) {
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
void Panels::updatePreviewLayout() {
    const auto &entry=state->preview;
    if(!entry.main||!entry.dock||!entry.canvasLayout)return;
    const bool studio=entry.canvasLayout->count()>entry.baseWidgetCount;
    // In narrow docks, Studio Mode's side-by-side program would overflow
    // the dock (OBS creates two video widgets and a transition column).
    // Use its native portrait arrangement. Restore the original direction
    // as soon as Studio Mode or the dock is disabled.
    const auto desired=studio&&entry.dock->width()<760?
        QBoxLayout::BottomToTop:entry.originalDirection;
    if(entry.canvasLayout->direction()!=desired)
        entry.canvasLayout->setDirection(desired);
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
bool Panels::toggle(QDockWidget *dock) {
    if(collapsed(dock))return expand(dock);
    if(!canCollapse(dock))return false;
    State::Entry entry{dock->widget(),owner(dock),dock->minimumHeight(),dock->maximumHeight(),
        dock->height(),headerHeight(dock),dock->minimumWidth(),dock->width(),dock->widget()->isHidden(),{},{},{}};
    entry.destroyed=connect(dock,&QObject::destroyed,this,[this,dock]{state->entries.remove(dock);});
    entry.location=connect(dock,&QDockWidget::dockLocationChanged,this,[this,dock]{expand(dock);});
    entry.floating=connect(dock,&QDockWidget::topLevelChanged,this,[this,dock]{expand(dock);});
    state->entries.insert(dock,entry);
    // Hidden content no longer contributes a width hint; keep the column
    // width so the title does not collapse to an ellipsis.
    dock->setMinimumWidth(entry.width);
    dock->widget()->hide();
    dock->setMinimumHeight(0);
    dock->setMaximumHeight(entry.header);
    dock->updateGeometry();
    if(entry.main)entry.main->resizeDocks({dock},{entry.header},Qt::Vertical);
    return true;
}
bool Panels::expand(QDockWidget *dock) {
    if(!collapsed(dock))return false;
    const auto e=state->entries.take(dock);
    disconnect(e.destroyed);disconnect(e.location);disconnect(e.floating);
    dock->setMaximumHeight(e.maximum);
    dock->setMinimumHeight(e.minimum);
    if(dock->minimumWidth()==e.width)dock->setMinimumWidth(e.minimumWidth);
    if(e.content&&e.content==dock->widget()&&!e.contentWasHidden)e.content->show();
    dock->updateGeometry();
    if(e.main&&!dock->isFloating()&&dock->isVisible()) {
        e.main->resizeDocks({dock},{e.height},Qt::Vertical);
        e.main->resizeDocks({dock},{e.width},Qt::Horizontal);
    }
    return true;
}
void Panels::restoreAll() {
    for(auto *dock:state->entries.keys())expand(dock);
}
bool Panels::eventFilter(QObject *o,QEvent *event) {
    if(o==state->preview.dock&&event->type()==QEvent::Resize&&
       state->preview.dock&&!state->preview.dock->isFloating()) {
        state->preview.dockedSize=state->preview.dock->size();
        state->preview.dockedOrigin=state->preview.dock->mapToGlobal(QPoint());
    }
    if(state->preview.dock && (o==state->preview.dock||o==state->preview.canvas||
        o==state->preview.canvasLayout) &&
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
        for(auto *dock:state->entries.keys())if(state->entries.value(dock).main==o)expand(dock);
        restorePreview(qobject_cast<QMainWindow*>(o));
        return false;
    }
    auto *dock=qobject_cast<QDockWidget*>(o);
    if(dock) {
        if(event->type()==QEvent::Hide||event->type()==QEvent::Close)expand(dock);
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
    auto *hint=new QLabel("Middle-click a dock title to collapse or expand it. Or select a panel here. Floating and tabbed docks are left alone.",dlg);
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
    dlg->setObjectName("obs-ui-augment-panels");
    dlg->setWindowTitle("UI Augment · Panels");
    dlg->setAttribute(Qt::WA_DeleteOnClose);dlg->resize(400,380);
    auto *layout=new QVBoxLayout(dlg);layout->setContentsMargins(0,0,0,0);
    layout->addWidget(createPage(main,dlg));dlg->show();
}
}
