#include "panels.h"
#include <QApplication>
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
};
Panels::Panels(QObject *p):QObject(p),state(new State) {
    qApp->installEventFilter(this);
}
Panels::~Panels() {
    if(qApp)qApp->removeEventFilter(this);
    delete state->manager.data();
    restoreAll();
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
    if(event->type()==QEvent::Close&&qobject_cast<QMainWindow*>(o)) {
        for(auto *dock:state->entries.keys())if(state->entries.value(dock).main==o)expand(dock);
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
