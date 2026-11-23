#pragma once
#include <QObject>
#include <QHash>
#include <QPointer>
#include <QString>
class QMainWindow;
class QDockWidget;
class QWidget;
class QToolButton;
class QMenu;
class QEvent;

namespace augment {
class Panels;
// Passive, reversible decoration of *native* OBS dock title bars.
// Does not replace the titleBarWidget, title text, native buttons, or hit areas.
class DockTitleIcons final : public QObject {
public:
    enum class Kind { Scenes, Sources, Audio, Transitions, Controls,
                      Preview, Chat, Browser, Generic };
    static Kind classify(const QString &objectName,const QString &title);
    static int glyphSizeForRoom(int freePixels);
    static constexpr int titleTextInset(){return 20;}
    bool titleIndented(QDockWidget *dock) const;
    QString originalTitle(QDockWidget *dock) const;
    explicit DockTitleIcons(QMainWindow *window,Panels *panels=nullptr,
                            const QString &settingsPath={},QObject *parent=nullptr);
    ~DockTitleIcons() override;
    void refresh();
    int decoratedCount() const;
    QWidget *glyph(QDockWidget *dock) const;
    QToolButton *moreButton(QDockWidget *dock) const;
    Qt::TextElideMode ellipsisMode() const;
    bool setEllipsisMode(Qt::TextElideMode mode);
    void appendEllipsisMenu(QMenu *menu);
private:
    bool eventFilter(QObject *watched,QEvent *event) override;
    void scheduleRefresh();
    void place(QDockWidget *dock,QWidget *icon);
    QPointer<QMainWindow> window_;
    QPointer<Panels> panels_;
    QString settingsPath_;
    Qt::TextElideMode ellipsisMode_=Qt::ElideRight;
    QHash<QDockWidget*,QPointer<QWidget>> icons_;
    QHash<QDockWidget*,QPointer<QToolButton>> moreButtons_;
    QHash<QDockWidget*,QString> originalTitles_;
    QHash<QDockWidget*,QString> displayedTitles_;
    QHash<QDockWidget*,QString> originalActionLabels_;
    bool adjustingTitle_=false;
    bool scheduled_=false;
    void restoreTitle(QDockWidget *dock);
};
}
