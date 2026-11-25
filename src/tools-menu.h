#pragma once
#include <QPointer>
#include <QList>
#include <QAction>
#include <QMenu>
namespace augment {
// Owns one OBS-registered Tools action and its plugin-owned submenu.
// Never moves or deletes third-party/native actions.
class ToolsMenu final {
public:
    enum class Command { Workspace, Find, Emergency, Resume };
    explicit ToolsMenu(QAction *anchor);
    ~ToolsMenu();
    ToolsMenu(const ToolsMenu&)=delete;
    ToolsMenu& operator=(const ToolsMenu&)=delete;
    QAction *action(Command command)const;
    void setState(bool active,bool frontendReady);
private:
    QPointer<QAction> anchor_;
    QPointer<QMenu> menu_;
    QList<QPointer<QAction>> actions_,separators_;
};
}
