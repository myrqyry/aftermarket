#pragma once
#include <QObject>
#include <memory>
class QDockWidget;
class QWidget;
namespace augment {
// Reversible two-face host: the original OBS widget is moved, never recreated.
class DockFaces final : public QObject {
public:
    explicit DockFaces(QObject *parent=nullptr);
    ~DockFaces() override;
    bool attach(QDockWidget *dock,QWidget *back);
    bool detach(QDockWidget *dock);
    bool addFrontAccessory(QDockWidget *dock,QWidget *accessory);
    QWidget *front(QDockWidget *dock) const;
    QWidget *back(QDockWidget *dock) const;
    bool backVisible(QDockWidget *dock) const;
    bool showBack(QDockWidget *dock,bool visible=true);
private:
    struct State;
    std::unique_ptr<State> state;
};
}
