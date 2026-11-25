#include "source-inspector-dock.h"
#include <obs.hpp>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QMap>
#include <QScopedValueRollback>
#include <QStyle>
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
SourceInspectorDock::SourceInspectorDock(QMainWindow *main,QString presetFile):QWidget(main),presets_(std::move(presetFile)){
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
 auto *presetRow=new QHBoxLayout;
 transformPresets_=new QComboBox(this);transformPresets_->setObjectName("aftermarket-inspect-transform-presets");
 transformPresets_->setEditable(true);transformPresets_->setInsertPolicy(QComboBox::NoInsert);
 transformPresets_->lineEdit()->setPlaceholderText("Transform preset name");
 presetRow->addWidget(transformPresets_,1);
 saveTransform_=new QPushButton(this);saveTransform_->setObjectName("aftermarket-inspect-transform-save");saveTransform_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));saveTransform_->setToolTip("Save new transform preset");saveTransform_->setAccessibleName("Save transform preset");
 loadTransform_=new QPushButton(this);loadTransform_->setObjectName("aftermarket-inspect-transform-load");loadTransform_->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));loadTransform_->setToolTip("Load named transform preset");loadTransform_->setAccessibleName("Load transform preset");
 deleteTransform_=new QPushButton(this);deleteTransform_->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));deleteTransform_->setToolTip("Delete named transform preset");deleteTransform_->setAccessibleName("Delete transform preset");
 for(auto *button:{saveTransform_,loadTransform_,deleteTransform_})button->setFixedWidth(qMax(button->sizeHint().height(),style()->pixelMetric(QStyle::PM_SmallIconSize)+14));
 presetRow->addWidget(saveTransform_);presetRow->addWidget(loadTransform_);presetRow->addWidget(deleteTransform_);root->addLayout(presetRow);
 feedback_=new QLabel(this);feedback_->setTextFormat(Qt::PlainText);feedback_->setWordWrap(true);feedback_->setObjectName("aftermarket-inspect-feedback");root->addWidget(feedback_);
 refreshPresetNames();
 connect(saveTransform_,&QPushButton::clicked,this,[this]{
  if(!currentTargetStillValid())return;
  const auto name=transformPresets_->currentText().trimmed();
  const auto data=readItemTransform(target_);
  if(data.isEmpty()||!presets_.save("transform:v1",name,data,false)){
   label(feedback_,"Choose a new preset name and an unlocked top-level scene item.");return;
  }
  refreshPresetNames();transformPresets_->setCurrentText(name);label(feedback_,"Transform preset saved.");
 });
 connect(loadTransform_,&QPushButton::clicked,this,[this]{
  if(!currentTargetStillValid())return;
  const auto data=presets_.read("transform:v1",transformPresets_->currentText().trimmed());
  if(data.isEmpty()||!applyItemTransform(target_,data)){label(feedback_,"Preset unavailable or target changed.");return;}
  label(feedback_,"Transform loaded. Undo from OBS Edit menu.");refresh();
 });
 connect(deleteTransform_,&QPushButton::clicked,this,[this]{
  const auto name=transformPresets_->currentText().trimmed();
  if(!presets_.names("transform:v1").contains(name))return;
  auto *prompt=new QMessageBox(QMessageBox::Question,"Delete transform preset","Delete this named transform preset?",QMessageBox::Yes|QMessageBox::No,this);
  prompt->setDefaultButton(QMessageBox::No);prompt->setAttribute(Qt::WA_DeleteOnClose);
  connect(prompt,&QMessageBox::finished,this,[this,name](int result){
   if(result!=QMessageBox::Yes)return;
   if(presets_.remove("transform:v1",name)){refreshPresetNames();label(feedback_,"Preset deleted.");}
  });prompt->open();
 });
 search_=new QLineEdit(this);search_->setPlaceholderText("Search filters…");search_->setClearButtonEnabled(true);root->addWidget(search_);
 filterList_=new QListWidget(this);filterList_->setObjectName("aftermarket-inspect-filter-list");root->addWidget(filterList_,1);
 auto *orderRow=new QHBoxLayout;
 up_=new QPushButton(this);up_->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));up_->setObjectName("aftermarket-inspect-filter-up");up_->setAccessibleName("Move filter up");
 down_=new QPushButton(this);down_->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));down_->setObjectName("aftermarket-inspect-filter-down");down_->setAccessibleName("Move filter down");
 addFilter_=new QPushButton("+",this);addFilter_->setObjectName("aftermarket-inspect-filter-add");addFilter_->setAccessibleName("Add filter");addFilter_->setToolTip("Add a compatible OBS filter");
 removeFilter_=new QPushButton(this);removeFilter_->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));removeFilter_->setObjectName("aftermarket-inspect-filter-remove");removeFilter_->setAccessibleName("Remove selected filter");removeFilter_->setToolTip("Remove selected filter");
 orderRow->addWidget(up_);orderRow->addWidget(down_);orderRow->addStretch();orderRow->addWidget(addFilter_);orderRow->addWidget(removeFilter_);root->addLayout(orderRow);
 connect(addFilter_,&QPushButton::clicked,this,[this]{openFilterPicker();});
 connect(removeFilter_,&QPushButton::clicked,this,[this]{confirmRemoveFilter();});
 connect(up_,&QPushButton::clicked,this,[this]{changeFilterOrder(true);});
 connect(down_,&QPushButton::clicked,this,[this]{changeFilterOrder(false);});
 connect(filterList_,&QListWidget::itemChanged,this,[this](QListWidgetItem *item){
  if(updating_||!item)return;
  if(!currentTargetStillValid()){schedule();return;}
  const auto id=item->data(Qt::UserRole).toString();
  if(!setInspectorFilterEnabled(target_.source,id,item->checkState()==Qt::Checked))label(feedback_,"Filter no longer available.");
  schedule();
 });
 connect(filterList_,&QListWidget::currentItemChanged,this,[this]{const bool selected=filterList_->currentItem()!=nullptr;up_->setEnabled(selected);down_->setEnabled(selected);removeFilter_->setEnabled(selected);});
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
 if(!filterRows_.isEmpty()||filterList_->count()){filterRows_.clear();updateFilterList();}setTransformAvailable(false);addFilter_->setEnabled(false);removeFilter_->setEnabled(false);up_->setEnabled(false);down_->setEnabled(false);pin_->setEnabled(false);properties_->setEnabled(false);filters_->setEnabled(false);usageButton_->setEnabled(false);
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
  auto *scene=owner&&!obs_source_removed(owner)?obs_scene_from_source(owner):nullptr;
  if(!scene&&owner&&!obs_source_removed(owner))scene=obs_group_from_source(owner);
  auto *item=scene?obs_scene_find_sceneitem_by_id(scene,target_.item):nullptr;
  if(!item||uuid(obs_sceneitem_get_source(item))!=target_.source){clearSelection();return;}
  setTransformAvailable(!obs_sceneitem_locked(item)&&!obs_sceneitem_is_group(item)&&obs_scene_from_source(owner)!=nullptr);
  vec2 pos,scale;obs_sceneitem_get_pos(item,&pos);obs_sceneitem_get_scale(item,&scale);
  label(transform_,QString("Scene item · %1%2\nPosition %3, %4 · Scale %5, %6 · Rotation %7°")
     .arg(utf(obs_source_get_name(owner)),obs_sceneitem_locked(item)?" · Locked":"")
     .arg(pos.x,0,'f',1).arg(pos.y,0,'f',1).arg(scale.x,0,'f',2).arg(scale.y,0,'f',2).arg(obs_sceneitem_get_rot(item),0,'f',1));
 }else{setTransformAvailable(false);label(transform_,"Shared source · select a scene item for its transform");}
 QList<FilterRow> rows;
 obs_source_enum_filters(source,[](obs_source_t*,obs_source_t *filter,void *p){
  auto *list=static_cast<QList<FilterRow>*>(p);
  if(list->size()<257)list->append({uuid(filter),utf(obs_source_get_name(filter)),obs_source_enabled(filter)});
 },&rows);
 addFilter_->setEnabled(rows.size()<256);
 if(rows.size()>256)rows.clear();
 if(rows!=filterRows_){filterRows_=rows;updateFilterList();}
}
bool SourceInspectorDock::currentTargetStillValid()const{
 if(target_.source.isEmpty())return false;
 if(pinned_)return true;
 OBSSourceAutoRelease current=obs_frontend_preview_program_mode_active()?obs_frontend_get_current_preview_scene():obs_frontend_get_current_scene();
 Selection s;selected(obs_scene_from_source(current),s);
 return s.count==1&&s.visited<=25000&&s.source==target_.source&&s.scene==target_.scene&&s.item==target_.item;
}
void SourceInspectorDock::refreshPresetNames(){
 const auto current=transformPresets_->currentText();
 const QSignalBlocker block(transformPresets_);
 transformPresets_->clear();transformPresets_->addItems(presets_.names("transform:v1"));transformPresets_->setCurrentText(current);
}
void SourceInspectorDock::setTransformAvailable(bool available){
 transformPresets_->setEnabled(available);saveTransform_->setEnabled(available);
 loadTransform_->setEnabled(available);deleteTransform_->setEnabled(available);
}
void SourceInspectorDock::changeFilterOrder(bool up){
 auto *item=filterList_->currentItem();if(!item||!currentTargetStillValid())return;
 if(!moveInspectorFilter(target_.source,item->data(Qt::UserRole).toString(),up))label(feedback_,"No further filter in this native category.");
 else label(feedback_,"Filter reordered. Undo from OBS Edit menu.");
 refresh();
}
void SourceInspectorDock::openFilterPicker(){
 if(!currentTargetStillValid())return;
 if(auto *existing=findChild<QInputDialog*>("aftermarket-inspect-filter-picker");existing&&existing->isVisible())return;
 const auto source=target_.source;
 const auto types=inspectorFilterTypes(source);
 QMap<QString,QString> choices;
 for(const auto &type:types){
  auto name=type.name;if(choices.contains(name))name+=" ("+type.id+")";
  choices.insert(name,type.id);
 }
 if(choices.isEmpty()){label(feedback_,"No compatible filter types available.");return;}
 auto *picker=new QInputDialog(this);
 picker->setObjectName("aftermarket-inspect-filter-picker");picker->setWindowTitle("Add OBS filter");picker->setLabelText("Filter type");
 picker->setComboBoxItems(choices.keys());picker->setComboBoxEditable(false);
 picker->setAttribute(Qt::WA_DeleteOnClose);
 connect(picker,&QInputDialog::accepted,this,[this,picker,source,choices]{
  if(!isVisible()||target_.source!=source||!currentTargetStillValid())return;
  const auto type=choices.value(picker->textValue());
  label(feedback_,addInspectorFilter(source,type)?"Filter added. Undo from OBS Edit menu.":"Filter unavailable or could not be created.");
  refresh();
 });picker->open();
}
void SourceInspectorDock::confirmRemoveFilter(){
 auto *row=filterList_->currentItem();
 if(!row||!currentTargetStillValid())return;
 if(auto *existing=findChild<QMessageBox*>("aftermarket-inspect-filter-remove-confirm");existing&&existing->isVisible())return;
 const auto source=target_.source,id=row->data(Qt::UserRole).toString();
 auto *prompt=new QMessageBox(QMessageBox::Question,"Remove filter","Remove "+row->text()+" from this shared source?",QMessageBox::Yes|QMessageBox::No,this);
 prompt->setTextFormat(Qt::PlainText);prompt->setObjectName("aftermarket-inspect-filter-remove-confirm");
 prompt->setDefaultButton(QMessageBox::No);prompt->setAttribute(Qt::WA_DeleteOnClose);
 connect(prompt,&QMessageBox::finished,this,[this,source,id](int result){
  if(result!=QMessageBox::Yes||!isVisible()||target_.source!=source||!currentTargetStillValid())return;
  label(feedback_,removeInspectorFilter(source,id)?"Filter removed. Undo from OBS Edit menu.":"Cannot safely remove this filter; use the native Filters dialog.");
  refresh();
 });prompt->open();
}
void SourceInspectorDock::updateFilterList(){
 const auto current=filterList_->currentItem()?filterList_->currentItem()->data(Qt::UserRole).toString():QString{};
 const QScopedValueRollback<bool> guard(updating_,true);
 const QSignalBlocker blocker(filterList_);
 filterList_->clear();
 for(const auto &row:filterRows_)if(row.name.contains(search_->text(),Qt::CaseInsensitive)){
  auto *item=new QListWidgetItem(row.name,filterList_);
  item->setData(Qt::UserRole,row.id);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);
  item->setCheckState(row.enabled?Qt::Checked:Qt::Unchecked);
  if(!current.isEmpty()&&current==row.id)filterList_->setCurrentItem(item);
 }
 const bool selected=filterList_->currentItem()!=nullptr;up_->setEnabled(selected);down_->setEnabled(selected);removeFilter_->setEnabled(selected);
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
