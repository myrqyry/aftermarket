#pragma once
#include "quick-find.h"
#include "context-notes.h"
#include <QObject>
#include <memory>
class QMainWindow;
namespace augment {
class NativeNotes final:public QObject {
public:
 using Edit=std::function<bool(const QuickFindEntry&)>;
 NativeNotes(QMainWindow *,std::shared_ptr<ContextNotes>,Edit);
 ~NativeNotes()override;
 void schedule();
protected:
 bool eventFilter(QObject *,QEvent *)override;
private:
 struct State;std::unique_ptr<State> state_;
 void refresh();
};
}
