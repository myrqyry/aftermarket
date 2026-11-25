#pragma once
#include "quick-find.h"
#include <QObject>
#include <QPointer>
#include <functional>
class QMainWindow;
class QMenuBar;
class QLineEdit;
class QListWidget;
class QDialog;
class QMessageBox;
namespace augment {
struct CommandChoice {QString id,label,group;bool confirm=false;};
class CommandCenter final:public QObject {
public:
 using SourceRunner=std::function<bool(const QuickFindEntry&)>;
 using Commands=std::function<QList<CommandChoice>()>;
 using Runner=std::function<bool(const QString&)>;
 CommandCenter(QMainWindow *,QuickFind::Provider,SourceRunner,Commands,Runner);
 ~CommandCenter()override;
 void open(const QString &initial={});
 void detach();
protected:
 bool eventFilter(QObject *,QEvent *)override;
private:
 void scheduleLayout();
 void layoutField();
 void updateResults();
 void activate();
 QPointer<QMainWindow> main_;
 QPointer<QMenuBar> bar_;
 QPointer<QLineEdit> field_,input_;
 QPointer<QDialog> popup_;
 QPointer<QListWidget> list_;
 QPointer<QMessageBox> confirm_;
 QuickFind::Provider sourceProvider_;
 SourceRunner sourceRunner_;
 Commands commandProvider_;
 Runner runner_;
 QList<QuickFindEntry> sources_,hits_;
 QList<CommandChoice> commands_,commandHits_;
 bool sourceLoaded_=false,commandsLoaded_=false,scheduled_=false,commandMode_=false;
};
}
