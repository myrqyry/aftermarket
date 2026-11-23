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
class QFontMetrics;

namespace augment {
// App-wide Qt text truncation for controls exposing native elide modes,
// plus plain, non-interactive QLabel text. Never edits source/model data or
// QAbstractButton text (OBS source identity sometimes depends on the latter).
class AppTextElision final : public QObject {
public:
    explicit AppTextElision(QWidget *mainWindow, QObject *parent=nullptr);
    ~AppTextElision() override;
    Qt::TextElideMode mode() const { return mode_; }
    bool setMode(Qt::TextElideMode mode);
    QString originalLabel(const QLabel *label) const;
    QString visibleText(const QString &text,const QFontMetrics &metrics,
                        int width) const;
    static Qt::TextElideMode currentCustomMode();
    void refresh();
private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void scheduleRefresh();
    void updateWidget(QWidget *widget);
    void updateLabel(QLabel *label);
    struct LabelEntry {
        QPointer<QLabel> label;
        QString original, applied, tooltip;
        bool addedTooltip=false;
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
    bool updating_=false,scheduled_=false;
};
}
