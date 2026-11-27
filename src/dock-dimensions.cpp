#include "dock-dimensions.h"
#include "panels.h"
#include <QAction>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QEvent>
#include <QFormLayout>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QAbstractScrollArea>
#include <QTimer>

namespace augment {
namespace {
constexpr auto lockedProperty="aftermarket-dimensions-locked";
int axis(QSize s,bool width){return width?s.width():s.height();}
void setAxis(QSize &s,bool width,int v){if(width)s.setWidth(v);else s.setHeight(v);}
void bounds(QDockWidget *d,bool width,int lo,int hi){
    if(axis(d->minimumSize(),width)==lo&&axis(d->maximumSize(),width)==hi)return;
    // Lower minimum before changing maximum if necessary, to avoid Qt
    // implicitly moving the opposite bound.
    if(width){d->setMinimumWidth(0);d->setMaximumWidth(hi);d->setMinimumWidth(lo);}
    else{d->setMinimumHeight(0);d->setMaximumHeight(hi);d->setMinimumHeight(lo);}
}
}
DockDimensions::DockDimensions(QMainWindow *main,Panels *panels,const QString &path,QObject *parent)
    :QObject(parent),main_(main),panels_(panels),path_(path){}
DockDimensions::~DockDimensions(){
    for(auto it=entries_.begin();it!=entries_.end();++it){
        auto *dock=it.key();dock->removeEventFilter(this);
        if(it->content)it->content->removeEventFilter(this);
        release(dock,it.value(),true,true);
        dock->setProperty(lockedProperty,QVariant());
    }
}
QString DockDimensions::key(QDockWidget *dock)const{
    if(path_.isEmpty()||!main_||dock->objectName().isEmpty())return {};
    const auto matches=main_->findChildren<QDockWidget*>(dock->objectName());
    if(matches.size()!=1||matches.first()!=dock)return {};
    return "dimensions/"+QString::fromLatin1(dock->objectName().toUtf8().toHex());
}
bool DockDimensions::available(QDockWidget *dock)const{
    return dock&&dock->widget()&&(!panels_||panels_->dimensionsAvailable(dock));
}
void DockDimensions::observe(QDockWidget *dock){
    if(!dock)return;
    if(!entries_.contains(dock)){
        entries_.insert(dock,Entry{});
        dock->installEventFilter(this);
        connect(dock,&QObject::destroyed,this,[this,dock]{entries_.remove(dock);});
        connect(dock,&QDockWidget::topLevelChanged,this,[this]{schedule();});
    }
    auto &e=entries_[dock];
    if(e.content!=dock->widget()){
        if(e.content)e.content->removeEventFilter(this);
        e.content=dock->widget();if(e.content)e.content->installEventFilter(this);
    }
    if(!e.loaded&&available(dock)){
        e.key=key(dock);
        // An unnamed dock can be edited for this session. If its real ID
        // arrives later, load its settings only if no session edit exists.
        if(!e.key.isEmpty()){
            e.loaded=true;
            QSettings settings(path_,QSettings::IniFormat);
            auto requested=settings.value(e.key,QSize(0,0)).toSize();
            if(e.requested==QSize(0,0)&&requested.width()>=0&&requested.height()>=0&&
               requested.width()<=QWIDGETSIZE_MAX&&requested.height()<=QWIDGETSIZE_MAX){
                e.originalMin=dock->minimumSize();e.originalMax=dock->maximumSize();
                e.requested=requested;
            }
        }
    }
    if(e.requested.width()>0||e.requested.height()>0){
        if(!dock->property(lockedProperty).toBool())dock->setProperty(lockedProperty,true);
        schedule();
    }
}
QSize DockDimensions::minimum(QDockWidget *dock)const{
    if(!dock)return {};
    const auto e=entries_.value(dock);
    const auto hint=dock->minimumSizeHint().expandedTo(QSize(0,0));
    QSize independent(0,0);
    if(auto *content=dock->widget()){
        // Qt's dock hint includes explicit min/max constraints, including
        // our lock. Derive the content floor separately so a raised lock
        // can shrink again when its layout becomes smaller.
        auto inner=content->minimumSize().expandedTo(content->minimumSizeHint());
        if(content->layout()&&!qobject_cast<QAbstractScrollArea*>(content))
            inner=inner.expandedTo(content->layout()->minimumSize());
        inner=inner.expandedTo(QSize(0,0));
        // Measure title/frame insets, not unused space left by a content
        // widget with its own maximum size.
        QSize chrome(0,0);
        if(dock->isVisible()&&content->isVisible()){
            const auto g=content->geometry();
            chrome=QSize(qMax(0,g.left())+qMax(0,dock->width()-g.right()-1),
                         qMax(0,g.top())+qMax(0,dock->height()-g.bottom()-1));
            const int frame=qMax(0,dock->style()->pixelMetric(QStyle::PM_DockWidgetFrameWidth,nullptr,dock));
            if(!(dock->features()&QDockWidget::DockWidgetVerticalTitleBar))
                chrome.setWidth(qMin(chrome.width(),2*frame));
            else chrome.setHeight(qMin(chrome.height(),2*frame));
        }
        const bool vertical=dock->features()&QDockWidget::DockWidgetVerticalTitleBar;
        const int margin=qMax(0,dock->style()->pixelMetric(QStyle::PM_DockWidgetTitleMargin,nullptr,dock));
        int header=dock->fontMetrics().height()+2*margin;
        int buttons=0;
        for(auto *button:dock->findChildren<QToolButton*>(QString(),Qt::FindDirectChildrenOnly)){
            if(button->isHidden())continue;
            header=qMax(header,vertical?button->sizeHint().width():button->sizeHint().height());
            buttons+=vertical?button->sizeHint().height():button->sizeHint().width();
        }
        if(auto *title=dock->titleBarWidget()){
            auto titleMin=title->minimumSize().expandedTo(title->minimumSizeHint());
            header=qMax(header,vertical?titleMin.width():titleMin.height());
            buttons=qMax(buttons,vertical?titleMin.height():titleMin.width());
        }
        if(!dock->isFloating()){
            if(vertical)chrome.setWidth(qMax(chrome.width(),header));
            else chrome.setHeight(qMax(chrome.height(),header));
        }
        independent=inner+chrome;
        if(!dock->isFloating()){
            const int titleFloor=buttons+header+2*margin;
            if(vertical)independent.setHeight(qMax(independent.height(),titleFloor));
            else independent.setWidth(qMax(independent.width(),titleFloor));
        }
    }
    QSize floor;
    for(bool width:{true,false}){
        const bool locked=axis(e.requested,width)>0;
        const int native=locked?axis(e.originalMin,width):axis(dock->minimumSize(),width);
        setAxis(floor,width,qMax(native,qMax(axis(independent,width),locked?0:axis(hint,width))));
    }
    return floor;
}
bool DockDimensions::save(QDockWidget *dock,QSize requested)const{
    const auto id=key(dock);if(id.isEmpty())return true;
    QSettings settings(path_,QSettings::IniFormat);
    if(requested==QSize(0,0))settings.remove(id);else settings.setValue(id,requested);
    settings.sync();return settings.status()==QSettings::NoError;
}
void DockDimensions::release(QDockWidget *dock,Entry &e,bool width,bool height){
    for(bool w:{true,false})if((w?width:height)&&axis(e.requested,w)>0)
        bounds(dock,w,axis(e.originalMin,w),axis(e.originalMax,w));
}
void DockDimensions::apply(QDockWidget *dock){
    if(!available(dock)||!entries_.contains(dock))return;
    auto &e=entries_[dock];
    const auto floor=minimum(dock);
    for(bool w:{true,false}){
        const int request=axis(e.requested,w);
        if(request<=0)continue;
        const int lo=axis(floor,w),hi=axis(e.originalMax,w);
        // A native maximum that contradicts content minimum has no feasible
        // solution. Suspend this axis rather than override native limits.
        if(lo>hi){bounds(dock,w,axis(e.originalMin,w),hi);continue;}
        const int fixed=qBound(lo,request,hi);
        bounds(dock,w,fixed,fixed);
    }
    const bool locked=e.requested.width()>0||e.requested.height()>0;
    if(dock->property(lockedProperty).toBool()!=locked)dock->setProperty(lockedProperty,locked);
}
bool DockDimensions::define(QDockWidget *dock,QSize dimensions,bool lockWidth,bool lockHeight){
    if(!available(dock))return false;
    observe(dock);
    auto &e=entries_[dock];auto floor=minimum(dock);
    QSize target;
    for(bool w:{true,false}){
        const int hi=axis(e.requested,w)>0?axis(e.originalMax,w):axis(dock->maximumSize(),w);
        if(axis(floor,w)>hi)return false;
        setAxis(target,w,qBound(axis(floor,w),axis(dimensions,w),hi));
    }
    // Retain the user's original requested value only after clamping to the
    // current safe range. Future growth may temporarily raise this floor.
    const QSize requested(lockWidth?target.width():0,lockHeight?target.height():0);
    if(!save(dock,requested))return false;
    for(bool w:{true,false}){
        if(axis(e.requested,w)==0){
            setAxis(e.originalMin,w,axis(dock->minimumSize(),w));
            setAxis(e.originalMax,w,axis(dock->maximumSize(),w));
        }else if(axis(requested,w)==0)release(dock,e,w,!w);
    }
    e.requested=requested;apply(dock);
    if(dock->isFloating())dock->resize(target);
    else if(main_){
        main_->resizeDocks({dock},{target.width()},Qt::Horizontal);
        main_->resizeDocks({dock},{target.height()},Qt::Vertical);
    }
    return true;
}
bool DockDimensions::unlock(QDockWidget *dock){
    if(!dock||!entries_.contains(dock)||!save(dock,QSize(0,0)))return false;
    auto &e=entries_[dock];release(dock,e,true,true);e.requested=QSize(0,0);
    dock->setProperty(lockedProperty,false);return true;
}
void DockDimensions::schedule(){
    if(pending_)return;pending_=true;
    QTimer::singleShot(0,this,[this]{pending_=false;for(auto *dock:entries_.keys())apply(dock);});
}
bool DockDimensions::eventFilter(QObject *object,QEvent *event){
    switch(event->type()){
    case QEvent::LayoutRequest:case QEvent::StyleChange:case QEvent::FontChange:
    case QEvent::Show:case QEvent::ChildAdded:case QEvent::ChildRemoved:
        // Re-evaluate only active locks; never poll or resize unlocked docks.
        for(auto it=entries_.cbegin();it!=entries_.cend();++it)
            if((it.key()==object||it->content==object)&&(it->requested.width()>0||it->requested.height()>0)){schedule();break;}
        break;
    default:break;
    }
    return QObject::eventFilter(object,event);
}
void DockDimensions::appendActions(QMenu *menu,QDockWidget *dock){
    if(!menu||!dock)return;observe(dock);
    menu->addSeparator();
    auto *defineAction=menu->addAction("Define dock dimensions…");
    defineAction->setObjectName("aftermarket-dock-dimensions-action");
    defineAction->setEnabled(available(dock));
    auto *unlockAction=menu->addAction("Unlock dock dimensions");
    unlockAction->setObjectName("aftermarket-dock-unlock-dimensions");
    unlockAction->setEnabled(entries_.value(dock).requested!=QSize(0,0));
    QPointer<DockDimensions> self=this;QPointer<QDockWidget> target=dock;
    connect(defineAction,&QAction::triggered,menu,[self,target]{
        if(self)QTimer::singleShot(0,self,[self,target]{if(self&&target)self->edit(target);});
    });
    connect(unlockAction,&QAction::triggered,menu,[self,target]{
        if(self&&target&&!self->unlock(target))QMessageBox::warning(target,"Dock dimensions","Could not save the unlocked dimensions.");
    });
}
void DockDimensions::edit(QDockWidget *dock){
    if(!available(dock))return;
    observe(dock);
    QPointer<QDockWidget> target=dock;
    const auto floor=minimum(dock);const auto e=entries_.value(dock);
    QDialog dialog(main_);dialog.setWindowTitle("Dock dimensions — "+dock->windowTitle().trimmed());
    auto *layout=new QVBoxLayout(&dialog);
    auto *note=new QLabel("Set the dock size in logical pixels. Lock either axis to keep it fixed. Shared dock columns, rows and tab groups may affect neighboring docks.",&dialog);
    note->setWordWrap(true);layout->addWidget(note);
    auto *form=new QFormLayout;QSpinBox *size[2];QCheckBox *locks[2];
    for(int i=0;i<2;++i){
        const bool w=i==0;const int hi=axis(e.requested,w)>0?axis(e.originalMax,w):axis(dock->maximumSize(),w);
        const int lo=axis(floor,w);
        if(lo>hi){QMessageBox::information(main_,"Dock dimensions","The content minimum exceeds a native maximum. Adjust the native dock layout first.");return;}
        auto *row=new QWidget(&dialog);auto *line=new QHBoxLayout(row);line->setContentsMargins(0,0,0,0);
        size[i]=new QSpinBox(row);size[i]->setRange(lo,hi);size[i]->setValue(qBound(lo,axis(dock->size(),w),hi));size[i]->setSuffix(" px");
        locks[i]=new QCheckBox(w?"Lock width":"Lock height",row);locks[i]->setChecked(axis(e.requested,w)>0);
        line->addWidget(size[i],1);line->addWidget(locks[i]);form->addRow(w?"Width":"Height",row);
    }
    layout->addLayout(form);
    auto *minimumLabel=new QLabel(QString("Minimum for current content: %1 × %2 px").arg(floor.width()).arg(floor.height()),&dialog);layout->addWidget(minimumLabel);
    auto *current=new QPushButton("Use current size",&dialog);layout->addWidget(current);
    connect(current,&QPushButton::clicked,&dialog,[target,&size]{if(target){size[0]->setValue(target->width());size[1]->setValue(target->height());}});
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted&&target){
        if(!define(target,QSize(size[0]->value(),size[1]->value()),locks[0]->isChecked(),locks[1]->isChecked()))
            QMessageBox::warning(main_,"Dock dimensions","Could not apply the requested dimensions. The dock layout or native constraints may have changed.");
    }
}
}
