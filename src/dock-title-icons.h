#pragma once
#include <QObject>
#include <QHash>
#include <QPointer>
#include <QString>
class QMainWindow;
class QDockWidget;
class QWidget;
class QEvent;

namespace augment {
// Passive, reversible decoration of *native* OBS dock title bars.
// Does not replace the titleBarWidget, title text, native buttons, or hit areas.
class DockTitleIcons final : public QObject {
public:
    enum class Kind { Scenes, Sources, Audio, Transitions, Controls,
                      Preview, Chat, Browser, Generic };
    static Kind classify(const QString &objectName,const QString &title);
    static int glyphSizeForRoom(int freePixels);
    explicit DockTitleIcons(QMainWindow *window,QObject *parent=nullptr);
    ~DockTitleIcons() override;
    void refresh();
    int decoratedCount() const;
    QWidget *glyph(QDockWidget *dock) const;
private:
    bool eventFilter(QObject *watched,QEvent *event) override;
    void scheduleRefresh();
    void place(QDockWidget *dock,QWidget *icon);
    QPointer<QMainWindow> window_;
    QHash<QDockWidget*,QPointer<QWidget>> icons_;
    bool scheduled_=false;
};
}
