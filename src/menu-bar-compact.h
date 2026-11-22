#pragma once

#include <QObject>
#include <QPointer>
#include <QList>

class QMenuBar;
class QMenu;
class QAction;
class QShortcut;

// Reversible compact menu bar for a Qt/OBS main window.
// Only presents existing QAction identities through a different parent menu.
// No action or native submenu is recreated.
namespace augment {
class MenuBarCompact final : public QObject {
public:
    explicit MenuBarCompact(QObject *parent=nullptr);
    ~MenuBarCompact() override;
    bool setEnabled(QMenuBar *bar,bool enabled);
    bool enabled() const;
    QMenu *hamburgerMenu() const;
protected:
    bool eventFilter(QObject *watched,QEvent *event) override;
private:
    QPointer<QMenuBar> bar_;
    QPointer<QMenu> hamburger_;
    QPointer<QShortcut> shortcut_;
    QList<QPointer<QAction>> originals_;
    bool active_=false,busy_=false,scheduled_=false;
    void reconcile();
    void scheduleReconcile();
    void remember(QAction *action,int index=-1);
    void discardDestroyed(QAction *action);
};
}
