#pragma once
#include <QWidget>
#include <QPointer>
#include <QString>
#include <QTimer>
#include "quick-find.h"
#include "inspector-edits.h"
#include "property-presets.h"
#include "context-notes.h"
class QLabel;
class QPushButton;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMainWindow;
class QComboBox;
class QToolButton;
namespace augment {
// Initial native inspector: source identity, selection and pinned item
// context. Native dialogs remain the authoritative editors.
class SourceInspectorDock final:public QWidget{
public:
 explicit SourceInspectorDock(QMainWindow *,QString presetFile,std::shared_ptr<ContextNotes> notes);
 void refresh();
 void clearSelection();
 bool inspect(const QuickFindEntry &);
protected:
 void showEvent(QShowEvent *)override;
 void hideEvent(QHideEvent *)override;
private:
 InspectorItem target_;
 PropertyPresets presets_;
 std::shared_ptr<ContextNotes> notes_;
 QToolButton *sourceNote_,*itemNote_;
 void editSourceNote(bool item);
 void editFilterNote(QListWidgetItem *);
 void refreshNotes();
 bool pinned_=false;
 QTimer refreshTimer_,coalesce_;
 QLabel *heading_,*detail_,*transform_,*usage_;
 QPushButton *pin_,*properties_,*filters_,*usageButton_;
 QLineEdit *search_;
 QListWidget *filterList_;
 struct FilterRow{QString id,name,type;bool enabled;bool operator==(const FilterRow &o)const{return id==o.id&&name==o.name&&type==o.type&&enabled==o.enabled;}};
 QList<FilterRow> filterRows_;
 QComboBox *transformPresets_;
 QPushButton *saveTransform_,*loadTransform_,*deleteTransform_,*up_,*down_,*addFilter_,*removeFilter_;
 QLabel *feedback_;
 bool updating_=false;
 void refreshPresetNames();
 void setTransformAvailable(bool);
 void changeFilterOrder(bool);
 void openFilterPicker();
 void confirmRemoveFilter();
 bool currentTargetStillValid()const;
 void schedule();
 void setPinned(bool);
 void updateFilterList();
 void updateUsage();
};
}
