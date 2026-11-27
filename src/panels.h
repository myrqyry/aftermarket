#pragma once
#include <QObject>
#include <memory>
#include <functional>
class QDockWidget;
class QMenu;
class QAction;
class QMainWindow;
class QWidget;
namespace augment {
class Panels final : public QObject {
public:
    explicit Panels(QObject *parent=nullptr,std::function<bool()> motionEnabled={});
    ~Panels() override;
    bool motionAllowed() const;
    bool canCollapse(QDockWidget *) const;
    bool collapsed(QDockWidget *) const;
    bool dimensionsAvailable(QDockWidget *) const;
    bool toggle(QDockWidget *);
    // One shared native action for title menus and the mixer overflow.
    QAction *addCollapseAction(QMenu *menu,QDockWidget *dock);
    void restoreAll();
    enum class StudioViewMode {Auto,Both,Program,Preview};
    bool setStudioViewMode(QMainWindow *,StudioViewMode);
    StudioViewMode studioViewMode(QMainWindow *) const;
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
    bool expand(QDockWidget *,bool animate=true);
    void stopHeightMotion(QDockWidget *);
    void restoreExpandedBounds(QDockWidget *);
    void startHeightMotion(QDockWidget *,int start,int finish,
                           std::function<void()> completed);
    void updatePreviewLayout();
    void restoreStudioControlStrip();
};
}
