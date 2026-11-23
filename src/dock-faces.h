#pragma once
#include <QObject>
#include <memory>
class QDockWidget;
class QWidget;
namespace augment {
// Reversible mixer host: original OBS widget stays above an optional bottom AudioHub.
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
    bool eventFilter(QObject *watched,QEvent *event) override;
    void positionTitleButtons(QDockWidget *dock);
    struct State;
    std::unique_ptr<State> state;
};
}
