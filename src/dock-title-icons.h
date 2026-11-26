#pragma once
#include <QObject>
#include <QHash>
#include <QDockWidget>
#include <memory>
#include <functional>
#include <QPointer>
#include <QString>
class QMainWindow;
class QDockWidget;
class QWidget;
class QToolButton;
class QMenu;
class QEvent;
class QAction;

namespace augment {
class Panels;
class AppTextElision;
// Passive, reversible decoration of *native* OBS dock title bars.
// Does not replace the titleBarWidget, title text, native buttons, or hit areas.
class DockTitleIcons final : public QObject {
public:
    enum class Kind { Scenes, Sources, Audio, Transitions, Controls,
                      Preview, Chat, Browser, Generic };
    static Kind classify(const QString &objectName,const QString &title,const QString &contentClass={});
    void setSourceDockSettingsAction(std::function<QAction*()> action);
    void setSourceDockPreviewSave(std::function<void()> save){sourceDockPreviewSave_=std::move(save);}
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
    // Shared by ordinary dock overflows and the mixer's specialized overflow.
    // These are the original QDockWidget operations, not surrogate dock state.
    void appendDockWindowActions(QMenu *menu,QDockWidget *dock);
private:
    bool eventFilter(QObject *watched,QEvent *event) override;
    void scheduleRefresh();
    void place(QDockWidget *dock,QWidget *icon);
    QPointer<QMainWindow> window_;
    QPointer<Panels> panels_;
    std::unique_ptr<AppTextElision> applicationElision_;
    QString settingsPath_;
    Qt::TextElideMode ellipsisMode_=Qt::ElideRight;
    QHash<QDockWidget*,QPointer<QWidget>> icons_;
    QHash<QDockWidget*,QPointer<QToolButton>> moreButtons_;
    QHash<QDockWidget*,bool> originalHover_;
    QHash<QDockWidget*,bool> headerHovered_;
    void setHeaderHovered(QDockWidget *dock,bool hovered);
    QHash<QDockWidget*,QString> originalTitles_;
    QHash<QDockWidget*,QDockWidget::DockWidgetFeatures> originalDockFeatures_;
    QHash<QDockWidget*,QString> displayedTitles_;
    QHash<QDockWidget*,QString> originalActionLabels_;
    bool adjustingTitle_=false;
    bool adjustingFeatures_=false;
    bool scheduled_=false;
    std::function<QAction*()> sourceDockSettingsAction_;
    std::function<void()> sourceDockPreviewSave_;
    void restoreTitle(QDockWidget *dock);
    QDockWidget::DockWidgetFeatures actualDockFeatures(QDockWidget *dock) const;
    void suppressNativeButtons(QDockWidget *dock);
    void restoreNativeButtons(QDockWidget *dock);
};
}
