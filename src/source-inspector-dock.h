#pragma once
#include <QWidget>
#include <QPointer>
#include <QString>
#include <QTimer>
#include "quick-find.h"
class QLabel;
class QPushButton;
class QLineEdit;
class QListWidget;
class QMainWindow;
namespace augment {
// Initial native inspector: source identity, selection and pinned item
// context. Native dialogs remain the authoritative editors.
class SourceInspectorDock final:public QWidget{
public:
 explicit SourceInspectorDock(QMainWindow *);
 void refresh();
 void clearSelection();
 bool inspect(const QuickFindEntry &);
protected:
 void showEvent(QShowEvent *)override;
 void hideEvent(QHideEvent *)override;
private:
 struct Target{QString source,scene;qint64 item=-1;};
 Target target_;
 bool pinned_=false;
 QTimer refreshTimer_,coalesce_;
 QLabel *heading_,*detail_,*transform_,*usage_;
 QPushButton *pin_,*properties_,*filters_,*usageButton_;
 QLineEdit *search_;
 QListWidget *filterList_;
 QStringList filterNames_;
 void schedule();
 void setPinned(bool);
 void updateFilterList();
 void updateUsage();
};
}
