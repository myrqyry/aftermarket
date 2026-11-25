#include "source-inspector-dock.h"
#include <obs.hpp>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QHideEvent>
#include <QSet>
#include <graphics/vec2.h>
namespace augment {
namespace {
QString utf(const char *s){return s?QString::fromUtf8(s):QString{};}
QString uuid(obs_source_t *s){return s?utf(obs_source_get_uuid(s)):QString{};}
void label(QLabel *w,const QString &s){if(w->text()!=s)w->setText(s);}
struct Selection{QString source,scene;qint64 item=-1;int count=0,visited=0;QSet<obs_scene_t*> ancestors;};
void selected(obs_scene_t *scene,Selection &s){
 if(!scene||s.visited>=25000||s.ancestors.contains(scene)||s.ancestors.size()>=16)return;
 s.ancestors.insert(scene);
 obs_scene_enum_items(scene,[](obs_scene_t *owner,obs_sceneitem_t *item,void *p){
  auto &state=*static_cast<Selection*>(p);
  if(++state.visited>25000)return false;
  if(obs_sceneitem_selected(item)){
   ++state.count;state.source=uuid(obs_sceneitem_get_source(item));
   state.scene=uuid(obs_scene_get_source(owner));state.item=obs_sceneitem_get_id(item);
  }
  if(obs_sceneitem_is_group(item))selected(obs_sceneitem_group_get_scene(item),state);
  return state.count<=1&&state.visited<=25000;
 },&s);
 s.ancestors.remove(scene);
}
}
SourceInspectorDock::SourceInspectorDock(QMainWindow *main):QWidget(main){
 setObjectName("aftermarket-source-inspector");
 auto *root=new QVBoxLayout(this);root->setContentsMargins(6,6,6,6);
 auto *header=new QHBoxLayout;heading_=new QLabel("Select one source",this);
 heading_->setObjectName("aftermarket-inspect-name");heading_->setTextFormat(Qt::PlainText);heading_->setWordWrap(true);
 header->addWidget(heading_,1);pin_=new QPushButton("Pin",this);pin_->setCheckable(true);pin_->setObjectName("aftermarket-inspect-pin");header->addWidget(pin_);root->addLayout(header);
 detail_=new QLabel(this);detail_->setTextFormat(Qt::PlainText);detail_->setWordWrap(true);root->addWidget(detail_);
 auto *buttons=new QHBoxLayout;properties_=new QPushButton("Properties…",this);properties_->setObjectName("aftermarket-inspect-properties");
 filters_=new QPushButton("Filters…",this);filters_->setObjectName("aftermarket-inspect-filters");
 buttons->addWidget(properties_);buttons->addWidget(filters_);root->addLayout(buttons);
 transform_=new QLabel(this);transform_->setTextFormat(Qt::PlainText);transform_->setWordWrap(true);transform_->setObjectName("aftermarket-inspect-transform");root->addWidget(transform_);
 search_=new QLineEdit(this);search_->setPlaceholderText("Search filters…");search_->setClearButtonEnabled(true);root->addWidget(search_);
 filterList_=new QListWidget(this);filterList_->setObjectName("aftermarket-inspect-filter-list");root->addWidget(filterList_,1);
 usageButton_=new QPushButton("Find scene usage",this);root->addWidget(usageButton_);
 usage_=new QLabel(this);usage_->setWordWrap(true);usage_->setTextFormat(Qt::PlainText);usage_->setObjectName("aftermarket-inspect-usage");root->addWidget(usage_);
 connect(pin_,&QPushButton::toggled,this,[this](bool on){setPinned(on);schedule();});
 connect(search_,&QLineEdit::textChanged,this,[this]{updateFilterList();});
 connect(usageButton_,&QPushButton::clicked,this,[this]{updateUsage();});
 auto openNative=[this](bool filter){
  OBSSourceAutoRelease source=obs_get_source_by_uuid(target_.source.toUtf8().constData());
  if(!source||obs_source_removed(source))return;
  if(filter)obs_frontend_open_source_filters(source);else obs_frontend_open_source_properties(source);
 };
 connect(properties_,&QPushButton::clicked,this,[openNative]{openNative(false);});
 connect(filters_,&QPushButton::clicked,this,[openNative]{openNative(true);});
 refreshTimer_.setInterval(750);connect(&refreshTimer_,&QTimer::timeout,this,[this]{refresh();});
 coalesce_.setSingleShot(true);coalesce_.setInterval(40);connect(&coalesce_,&QTimer::timeout,this,[this]{refresh();});
 if(main)if(auto *tree=main->findChild<QAbstractItemView*>("sources"))
  if(tree->selectionModel())connect(tree->selectionModel(),&QItemSelectionModel::selectionChanged,this,[this]{schedule();});
 clearSelection();
}
void SourceInspectorDock::setPinned(bool pinned){
 pinned_=pinned&&!target_.source.isEmpty();
 const QSignalBlocker block(pin_);pin_->setChecked(pinned_);pin_->setText(pinned_?"Unpin":"Pin");
}
void SourceInspectorDock::clearSelection(){
 target_={};setPinned(false);label(heading_,"Select one source");label(detail_,{});label(transform_,{});label(usage_,{});
 if(!filterNames_.isEmpty()||filterList_->count()){filterNames_.clear();filterList_->clear();}pin_->setEnabled(false);properties_->setEnabled(false);filters_->setEnabled(false);usageButton_->setEnabled(false);
}
void SourceInspectorDock::schedule(){if(isVisible()&&!coalesce_.isActive())coalesce_.start();}
void SourceInspectorDock::showEvent(QShowEvent *e){QWidget::showEvent(e);schedule();refreshTimer_.start();}
void SourceInspectorDock::hideEvent(QHideEvent *e){refreshTimer_.stop();coalesce_.stop();QWidget::hideEvent(e);}
bool SourceInspectorDock::inspect(const QuickFindEntry &entry){
 if(entry.kind!=QuickFindEntry::Source)return false;
 OBSSourceAutoRelease source=obs_get_source_by_uuid(entry.id.toUtf8().constData());
 if(!source||obs_source_removed(source))return false;
 target_={entry.id,{},-1};label(usage_,{});setPinned(true);refresh();return true;
}
void SourceInspectorDock::refresh(){
 if(!isVisible())return;
 if(!pinned_){
  OBSSourceAutoRelease current=obs_frontend_preview_program_mode_active()?obs_frontend_get_current_preview_scene():obs_frontend_get_current_scene();
  Selection s;selected(obs_scene_from_source(current),s);
  if(s.count!=1||s.visited>25000){clearSelection();if(s.count>1)label(heading_,"Select one source (multiple selected)");return;}
  if(target_.source!=s.source||target_.scene!=s.scene||target_.item!=s.item)label(usage_,{});
  target_={s.source,s.scene,s.item};
 }
 OBSSourceAutoRelease source=obs_get_source_by_uuid(target_.source.toUtf8().constData());
 if(!source||obs_source_removed(source)){clearSelection();return;}
 QString description=utf(obs_source_get_display_name(obs_source_get_id(source)))+
    QString(" · %1 × %2").arg(obs_source_get_width(source)).arg(obs_source_get_height(source));
 label(heading_,utf(obs_source_get_name(source)));label(detail_,description);
 pin_->setEnabled(true);properties_->setEnabled(true);filters_->setEnabled(true);usageButton_->setEnabled(true);
 if(target_.item>=0){
  OBSSourceAutoRelease owner=obs_get_source_by_uuid(target_.scene.toUtf8().constData());
  auto *item=owner&&!obs_source_removed(owner)?obs_scene_find_sceneitem_by_id(obs_scene_from_source(owner),target_.item):nullptr;
  if(!item||uuid(obs_sceneitem_get_source(item))!=target_.source){clearSelection();return;}
  vec2 pos,scale;obs_sceneitem_get_pos(item,&pos);obs_sceneitem_get_scale(item,&scale);
  label(transform_,QString("Scene item · %1%2\nPosition %3, %4 · Scale %5, %6 · Rotation %7°")
     .arg(utf(obs_source_get_name(owner)),obs_sceneitem_locked(item)?" · Locked":"")
     .arg(pos.x,0,'f',1).arg(pos.y,0,'f',1).arg(scale.x,0,'f',2).arg(scale.y,0,'f',2).arg(obs_sceneitem_get_rot(item),0,'f',1));
 }else label(transform_,"Shared source · select a scene item for its transform");
 QStringList names;
 obs_source_enum_filters(source,[](obs_source_t*,obs_source_t *filter,void *p){
  auto *list=static_cast<QStringList*>(p);
  if(list->size()<256)list->append(utf(obs_source_get_name(filter))+(obs_source_enabled(filter)?"":" · Disabled"));
 },&names);
 if(names!=filterNames_){filterNames_=names;updateFilterList();}
}
void SourceInspectorDock::updateFilterList(){
 filterList_->clear();
 for(const auto &name:filterNames_)if(name.contains(search_->text(),Qt::CaseInsensitive))filterList_->addItem(name);
}
void SourceInspectorDock::updateUsage(){
 if(target_.source.isEmpty())return;
 const auto catalog=collectObsQuickFind();
 for(const auto &entry:catalog)if(entry.kind!=QuickFindEntry::Filter&&entry.id==target_.source){
  label(usage_,entry.location.isEmpty()?"No scene references found in the bounded catalog.":"Scene references (bounded): "+entry.location);return;
 }
 label(usage_,"Source not found in the current collection catalog.");
}
}
