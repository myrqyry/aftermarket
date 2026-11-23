#pragma once
#include <QObject>
#include <QHash>
#include <QPointer>
#include <QString>
#include <QVariant>
#include <Qt>

class QWidget;
class QLabel;
class QAbstractItemView;
class QTabBar;

namespace augment {
// A bounded adapter for Qt's native item-view/tab elide modes and the
// known OBS source-name display. Ordinary Qt controls retain their own
// style painting and layout; source and model data are never rewritten.
class AppTextElision final : public QObject {
public:
    explicit AppTextElision(QWidget *mainWindow, QObject *parent=nullptr);
    ~AppTextElision() override;
    Qt::TextElideMode mode() const { return mode_; }
    bool setMode(Qt::TextElideMode mode);
    static Qt::TextElideMode currentCustomMode();
    void refresh();
private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void updateWidget(QWidget *widget);
    bool paintLabel(QLabel *label);
    void restoreLabelTooltips();
    struct LabelEntry {
        QPointer<QLabel> label;
        QString previousTooltip;
        QString appliedTooltip;
    };
    struct ViewEntry {
        QPointer<QAbstractItemView> view;
        Qt::TextElideMode original, applied;
    };
    struct TabEntry {
        QPointer<QTabBar> tab;
        Qt::TextElideMode original, applied;
    };
    QWidget *window_=nullptr;
    QVariant originalPaintProperty_;
    Qt::TextElideMode mode_=Qt::ElideRight;
    QHash<QLabel*,LabelEntry> labels_;
    QHash<QAbstractItemView*,ViewEntry> views_;
    QHash<QTabBar*,TabEntry> tabs_;
    bool updating_=false;
};
}
