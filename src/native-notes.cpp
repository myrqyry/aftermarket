#include "native-notes.h"
#include "obs-note-targets.h"
#include <QApplication>
#include <QMainWindow>
#include <QListWidget>
#include <QListView>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QBoxLayout>
#include <QPainter>
#include <QPointer>
#include <QPersistentModelIndex>
#include <QScrollBar>
#include <QContextMenuEvent>
#include <QMenu>
#include <QTimer>
#include <QSet>
#include <QAbstractItemDelegate>
#include <QStyleOptionViewItem>
namespace augment {
namespace {
bool sources(QListView *v){return v&&v->objectName()=="sources"&&v->inherits("SourceTree");}
bool scenes(QListView *v){return v&&v->objectName()=="scenes"&&v->inherits("SceneTree");}
QLabel *nameLabel(QWidget *row){
 if(!row||!row->inherits("SourceTreeItem"))return nullptr;
 QLabel *found=nullptr;
 for(auto *l:row->findChildren<QLabel*>(QString{},Qt::FindDirectChildrenOnly))if(l->inherits("OBSSourceLabel")){if(found)return nullptr;found=l;}
 return found;
}
QuickFindEntry target(QListView *view,const QModelIndex &idx){
 QuickFindEntry entry;
 if(!idx.isValid()||idx.model()!=view->model())return entry;
 QString name;
 if(sources(view)){
  auto *label=nameLabel(view->indexWidget(idx));
  name=idx.data(Qt::AccessibleTextRole).toString();
  if(!label||name.isEmpty()||name!=label->text())return entry;
 }else if(scenes(view))name=idx.data(Qt::DisplayRole).toString();else return entry;
 if(name.isEmpty())return entry;
 OBSSourceAutoRelease source=obs_get_source_by_name(name.toUtf8().constData());
 if(!source||obs_source_removed(source)||name!=QString::fromUtf8(obs_source_get_name(source)))return entry;
 const bool scene=obs_source_get_type(source)==OBS_SOURCE_TYPE_SCENE;
 if(scenes(view)&&!scene)return entry;
 entry.kind=scene?QuickFindEntry::Scene:QuickFindEntry::Source;
 entry.id=noteUuid(source);entry.name=name;return entry;
}
class Badge final:public QToolButton{
public:
 explicit Badge(QWidget *parent):QToolButton(parent){
  setAutoRaise(true);setFocusPolicy(Qt::NoFocus);setFixedSize(18,18);
  setAccessibleName("Edit private note");setCursor(Qt::PointingHandCursor);
 }
 QColor ink;
protected:
 void paintEvent(QPaintEvent *)override{
  QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(ink.isValid()?ink:palette().color(QPalette::Text),1.2,Qt::SolidLine,Qt::RoundCap));
  p.setBrush(Qt::NoBrush);const QRectF r=QRectF(rect()).adjusted(3,3,-3,-3);p.drawEllipse(r);
  const auto c=r.center();p.drawPoint(c+QPointF(0,-2.5));p.drawLine(c+QPointF(0,0),c+QPointF(0,2.8));
 }
};
}
struct NativeNotes::State{
 QPointer<QMainWindow> main;
 std::shared_ptr<ContextNotes> notes;
 Edit edit;
 struct Row{QPointer<Badge> badge;QPointer<QListView> view;QPersistentModelIndex idx;QString key;};
 QList<Row> rows;
 QSet<QObject*> watched;
 bool queued=false,busy=false;
 struct Pending{QPointer<QListView> view;QPersistentModelIndex idx;QuickFindEntry entry;QString collection;};
 Pending pending;
};
NativeNotes::NativeNotes(QMainWindow *main,std::shared_ptr<ContextNotes> notes,Edit edit):state_(new State){
 state_->main=main;state_->notes=std::move(notes);state_->edit=std::move(edit);
 state_->notes->onChanged([this]{schedule();});qApp->installEventFilter(this);schedule();
}
NativeNotes::~NativeNotes(){
 state_->notes->onChanged({});qApp->removeEventFilter(this);state_->busy=true;
 for(auto &r:state_->rows)if(r.badge)delete r.badge.data();
}
void NativeNotes::schedule(){
 if(state_->queued||state_->busy)return;
 state_->queued=true;QTimer::singleShot(0,this,[this]{state_->queued=false;refresh();});
}
void NativeNotes::refresh(){
 if(state_->busy||!state_->main)return;
 state_->busy=true;
 auto watch=[&](QObject *o){
  if(!o||state_->watched.contains(o))return false;
  state_->watched.insert(o);connect(o,&QObject::destroyed,this,[this,o]{state_->watched.remove(o);});return true;
 };
 QSet<Badge*> seen;
 for(auto *view:state_->main->findChildren<QListView*>()){
  if(!sources(view)&&!scenes(view))continue;
  if(!view->model()||!view->isVisible())continue;
  if(auto *m=view->model();watch(m)){
   connect(m,&QAbstractItemModel::rowsInserted,this,[this]{schedule();});
   connect(m,&QAbstractItemModel::rowsRemoved,this,[this]{schedule();});
   connect(m,&QAbstractItemModel::rowsMoved,this,[this]{schedule();});
   connect(m,&QAbstractItemModel::modelReset,this,[this]{schedule();});
   connect(m,&QAbstractItemModel::dataChanged,this,[this]{schedule();});
   connect(m,&QAbstractItemModel::layoutChanged,this,[this]{schedule();});
  }
  if(auto *sel=view->selectionModel();watch(sel))connect(sel,&QItemSelectionModel::selectionChanged,this,[this]{schedule();});
  if(auto *bar=view->verticalScrollBar();watch(bar))connect(bar,&QScrollBar::valueChanged,this,[this]{schedule();});
  if(auto *bar=view->horizontalScrollBar();watch(bar))connect(bar,&QScrollBar::valueChanged,this,[this]{schedule();});
  for(int i=0;i<qMin(5000,view->model()->rowCount());++i){
   const auto idx=view->model()->index(i,0);
   const auto entry=target(view,idx);const auto key=entryNoteKey(entry);
   if(key.isEmpty())continue;
   const auto note=state_->notes->get(key);if(note.isEmpty())continue;
   QWidget *parent=nullptr;QBoxLayout *layout=nullptr;QLabel *label=nullptr;QRect rect;
   if(sources(view)){
    parent=view->indexWidget(idx);label=nameLabel(parent);layout=parent?qobject_cast<QBoxLayout*>(parent->layout()):nullptr;
    if(!layout||!label||layout->indexOf(label)<0)continue;
   }else{
    // Never replace the native SceneRenameDelegate or cover scene names.
    // Reserve only naturally empty space on a visible list row.
    if(view->viewMode()!=QListView::ListMode||view->findChild<QLineEdit*>()!=nullptr)continue;
    rect=view->visualRect(idx);
    if(!view->viewport()->rect().contains(rect)||rect.height()<18||
       view->fontMetrics().horizontalAdvance(entry.name)+30>rect.width())continue;
    parent=view->viewport();
   }
   Badge *badge=nullptr;
   for(auto &r:state_->rows)if(r.badge&&r.view==view&&r.idx==idx&&r.key==key&&r.badge->parentWidget()==parent){badge=r.badge;break;}
   if(!badge){
    badge=new Badge(parent);badge->setObjectName(sources(view)?"aftermarket-native-source-note":"aftermarket-native-scene-note");
    if(layout)layout->insertWidget(layout->indexOf(label)+1,badge,0,Qt::AlignVCenter);
    const QPointer<QListView> safe(view);const QPersistentModelIndex persistent(idx);
    connect(badge,&QToolButton::clicked,this,[this,safe,persistent,key]{
     if(!safe||!persistent.isValid())return;
     const auto current=target(safe,persistent);
     if(entryNoteKey(current)==key&&state_->edit)state_->edit(current);
    });
    state_->rows.append({badge,view,idx,key});
   }
   const auto tip=noteTooltip(note);if(badge->toolTip()!=tip)badge->setToolTip(tip);
   const auto group=view->isEnabled()?(view->isActiveWindow()?QPalette::Active:QPalette::Inactive):QPalette::Disabled;
   const auto fg=view->palette().color(group,view->selectionModel()->isSelected(idx)?QPalette::HighlightedText:QPalette::Text);
   if(badge->ink!=fg){badge->ink=fg;badge->update();}
   if(!layout){const auto at=QPoint(view->layoutDirection()==Qt::RightToLeft?rect.left()+3:rect.right()-20,rect.center().y()-9);if(badge->pos()!=at)badge->move(at);}
   if(badge->isHidden()){badge->show();badge->raise();}
   seen.insert(badge);
  }
 }
 for(auto it=state_->rows.begin();it!=state_->rows.end();){
  if(it->badge&&seen.contains(it->badge)){++it;continue;}
  if(it->badge)delete it->badge.data();it=state_->rows.erase(it);
 }
 state_->busy=false;
}
bool NativeNotes::eventFilter(QObject *object,QEvent *event){
 if(state_->busy||!state_->main)return false;
 const auto type=event->type();
 if(type==QEvent::Show){
  auto *menu=qobject_cast<QMenu*>(object);
  if(menu&&state_->pending.view&&state_->pending.idx.isValid()&&menu->parentWidget()==state_->main){
   const auto pending=state_->pending;state_->pending={};
   if(noteCollection()!=pending.collection||entryNoteKey(target(pending.view,pending.idx))!=entryNoteKey(pending.entry))return false;
   const auto key=entryNoteKey(pending.entry);
   auto *action=menu->addAction(state_->notes->get(key).isEmpty()?"Add note…":"Edit note…");
   action->setObjectName("aftermarket-native-note-action");
   connect(action,&QAction::triggered,this,[this,pending,key]{
    QTimer::singleShot(0,this,[this,pending,key]{
     if(!pending.view||!pending.idx.isValid()||noteCollection()!=pending.collection)return;
     const auto current=target(pending.view,pending.idx);
     if(entryNoteKey(current)==key&&state_->edit)state_->edit(current);
    });
   });menu->adjustSize();
   return false;
  }
 }
 if(type!=QEvent::ContextMenu&&type!=QEvent::Show&&type!=QEvent::Hide&&type!=QEvent::Resize&&type!=QEvent::LayoutRequest&&type!=QEvent::ChildAdded&&type!=QEvent::ChildRemoved&&type!=QEvent::PaletteChange&&type!=QEvent::StyleChange&&type!=QEvent::WindowActivate&&type!=QEvent::WindowDeactivate&&type!=QEvent::Move)return false;
 auto *widget=qobject_cast<QWidget*>(object);
 for(auto *p=widget;p;p=p->parentWidget()){
  auto *view=qobject_cast<QListView*>(p);
  if(!sources(view)&&!scenes(view))continue;
  if(type==QEvent::ContextMenu){
   auto *e=static_cast<QContextMenuEvent*>(event);
   const auto idx=view->indexAt(view->viewport()->mapFromGlobal(e->globalPos()));
   const auto entry=target(view,idx);
   if(!entryNoteKey(entry).isEmpty()){
    state_->pending={view,idx,entry,noteCollection()};
    QTimer::singleShot(0,this,[this]{state_->pending={};});
   }
  }else schedule();
  break;
 }
 return false;
}
}
