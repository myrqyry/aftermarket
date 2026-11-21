#pragma once
#include <QObject>
#include <memory>
class QDockWidget;
class QMainWindow;
class QWidget;
namespace augment {
class Panels final : public QObject {
public:
    explicit Panels(QObject *parent=nullptr);
    ~Panels() override;
    bool canCollapse(QDockWidget *) const;
    bool collapsed(QDockWidget *) const;
    bool toggle(QDockWidget *);
    void restoreAll();
    // Native OBS canvasEditor, including Preview/Program widgets, moved to
    // a reversible QDockWidget. No second libobs renderer is created.
    bool previewDocked(QMainWindow *) const;
    QDockWidget *previewDock(QMainWindow *) const;
    bool setPreviewDocked(QMainWindow *,bool enabled);
    void restorePreview(QMainWindow *);
    QWidget *createPage(QMainWindow *, QWidget *parent);
    void showManager(QMainWindow *);
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    struct State;
    std::unique_ptr<State> state;
    bool expand(QDockWidget *);
    void updatePreviewLayout();
};
}
