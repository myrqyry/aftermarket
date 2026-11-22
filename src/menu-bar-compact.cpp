#include "menu-bar-compact.h"
#include <QAction>
#include <QActionEvent>
#include <QEvent>
#include <QMenuBar>
#include <QMenu>
#include <QFontMetrics>
#include <QShortcut>
#include <QScopedValueRollback>
#include <QTimer>
#include <QWidget>
#include <algorithm>

namespace augment {
MenuBarCompact::MenuBarCompact(QObject *parent):QObject(parent){}
MenuBarCompact::~MenuBarCompact(){
    if(bar_)setEnabled(bar_,false);
    delete hamburger_.data(); // remove the plugin-owned popup on unload
}
bool MenuBarCompact::enabled() const {return active_;}
QMenu *MenuBarCompact::hamburgerMenu() const{return hamburger_;}
void MenuBarCompact::remember(QAction *action,int index) {
    if(!action||action==hamburger_->menuAction())return;
    for(const auto &entry:originals_)if(entry==action)return;
    const QPointer<QAction> safe(action);
    if(index>=0&&index<originals_.size())originals_.insert(index,safe);
    else originals_.append(safe);
    connect(action,&QObject::destroyed,this,[this,action]{discardDestroyed(action);});
}
void MenuBarCompact::discardDestroyed(QAction *action) {
    for(int i=originals_.size()-1;i>=0;--i)
        if(!originals_[i]||originals_[i].data()==action)originals_.removeAt(i);
}
bool MenuBarCompact::setEnabled(QMenuBar *bar,bool turnOn) {
    if(turnOn){
        if(active_||!bar||bar->isNativeMenuBar()||bar->actions().isEmpty())return false;
        if(bar_&&bar_!=bar)return false;
        if(!bar_){
            bar_=bar;
            connect(bar_,&QObject::destroyed,this,[this]{
                active_=false;bar_.clear();hamburger_.clear();shortcut_.clear();
                originals_.clear();
            });
        }
        if(!hamburger_){
            hamburger_=new QMenu(QString::fromUtf8("☰"),bar);
            hamburger_->setObjectName("aftermarket-hamburger-menu");
            hamburger_->setAccessibleName("OBS menus");
            hamburger_->menuAction()->setToolTip("Open OBS menus (Alt+M)");
        }
        originals_.clear();
        for(auto *action:bar->actions())remember(action);
        if(originals_.isEmpty())return false;
        active_=true;
        bar_->installEventFilter(this);
        reconcile();
        shortcut_=new QShortcut(QKeySequence("Alt+M"),bar_);
        shortcut_->setContext(Qt::WindowShortcut);
        connect(shortcut_,&QShortcut::activated,this,[this]{
            if(!active_||!bar_||!hamburger_)return;
            const QRect rect=bar_->actionGeometry(hamburger_->menuAction());
            hamburger_->popup(bar_->mapToGlobal(QPoint(rect.left(),rect.bottom())));
        });
        return true;
    }
    if(!active_||!bar_||bar!=bar_)return false;
    // Capture last-minute plugin entries before returning the original menu bar.
    reconcile();
    active_=false;
    bar_->removeEventFilter(this);
    QScopedValueRollback<bool> busy(busy_,true);
    if(shortcut_)delete shortcut_.data();
    if(hamburger_){
        hamburger_->hide();
        for(const auto &a:originals_)if(a)hamburger_->removeAction(a);
        bar_->removeAction(hamburger_->menuAction());
    }
    for(const auto &a:originals_)if(a)bar_->addAction(a);
    originals_.clear();
    return true;
}
void MenuBarCompact::scheduleReconcile(){
    if(!active_||busy_||scheduled_)return;
    scheduled_=true;
    QTimer::singleShot(0,this,[this] {
        scheduled_=false;
        if(active_)reconcile();
    });
}
void MenuBarCompact::reconcile(){
    if(!active_||busy_||!bar_||!hamburger_)return;
    QScopedValueRollback<bool> busy(busy_,true);
    // A plugin may add a top-level QAction directly while compact mode is on.
    // Keep its identity and the insertion position relative to existing menus.
    const auto current=bar_->actions();
    for(int i=0;i<current.size();++i){
        auto *action=current.at(i);
        if(action==hamburger_->menuAction())continue;
        int before=-1;
        for(int k=i+1;k<current.size();++k){
            const auto successor=std::find_if(originals_.begin(),originals_.end(),
                [next=current[k]](const QPointer<QAction> &entry){return entry==next;});
            if(successor!=originals_.end()){
                before=int(std::distance(originals_.begin(),successor));break;
            }
            if(current[k]==hamburger_->menuAction()){
                before=0;break;
            }
        }
        remember(action,before);
        bar_->removeAction(action);
    }
    // QMenu::clear removes only action associations, never OBS-owned QAction
    // instances or their submenus. Rebuild only when top-level order changes.
    QList<QAction*> wanted;
    for(const auto &action:originals_)if(action)wanted.append(action);
    if(hamburger_->actions()!=wanted){
        hamburger_->clear();
        for(auto *action:wanted)hamburger_->addAction(action);
    }
    // Native themes with dense padding can crowd submenu indicators against
    // longer labels, such as Scene Collection.
    int minimumWidth=175;
    const QFontMetrics font(hamburger_->font());
    for(auto *action:wanted){
        auto label=action->text();
        label.remove('&');
        minimumWidth=qMax(minimumWidth,font.horizontalAdvance(label)+70);
    }
    hamburger_->setMinimumWidth(qMin(minimumWidth,440));
    if(!bar_->actions().contains(hamburger_->menuAction()))
        bar_->addMenu(hamburger_);
}
bool MenuBarCompact::eventFilter(QObject *watched,QEvent *event){
    if(active_&&watched==bar_&&
       (event->type()==QEvent::ActionAdded||event->type()==QEvent::ActionRemoved))
        scheduleReconcile();
    return false;
}
}
