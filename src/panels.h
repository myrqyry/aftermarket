#pragma once
#include <QObject>
#include <memory>
class QDockWidget;
class QMainWindow;
namespace augment {
class Panels final : public QObject {
public:
    explicit Panels(QObject *parent=nullptr);
    ~Panels() override;
    bool canCollapse(QDockWidget *) const;
    bool collapsed(QDockWidget *) const;
    bool toggle(QDockWidget *);
    void restoreAll();
    void showManager(QMainWindow *);
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    struct State;
    std::unique_ptr<State> state;
    bool expand(QDockWidget *);
};
}
