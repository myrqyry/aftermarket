#pragma once
#include <QPointer>
#include <QStringList>
#include <QList>
#include <QString>
#include <QSet>
#include <QRect>
class QMenu;
class QAction;

namespace augment {
// Native overflow pages share the logical scope of their original menu.
QStringList logicalMenuLabels(const QMenu *menu);
QString logicalMenuName(const QMenu *menu);
bool overflowAction(const QAction *action);

class MenuLayout {
public:
    explicit MenuLayout(QMenu *menu);
    void prepare(bool enabled,const QSet<QAction*> &explicitOverflow={});
    void present();
    void restore();
private:
    QMenu *menu;
    bool prepared=false, arranging=false;
    QRect arrangedGeometry;
    bool columnsApplied=false;
    QString originalStyle,appliedStyle;
    QList<QPointer<QAction>> originalActions;
    QList<QPointer<QMenu>> pages;
    struct Move {QPointer<QAction> action;QPointer<QMenu> page;};
    QList<Move> moved;
};
}
