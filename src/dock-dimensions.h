#pragma once
#include <QObject>
#include <QHash>
#include <QPointer>
#include <QSize>
#include <QString>
class QDockWidget;
class QMainWindow;
class QMenu;
class QWidget;
namespace augment {
class Panels;
// Owns only the explicitly locked axes; never modifies dock content constraints.
class DockDimensions final : public QObject {
public:
    DockDimensions(QMainWindow *main,Panels *panels,const QString &path,QObject *parent=nullptr);
    ~DockDimensions() override;
    void observe(QDockWidget *dock);
    QSize minimum(QDockWidget *dock) const;
    bool define(QDockWidget *dock,QSize dimensions,bool lockWidth,bool lockHeight);
    bool unlock(QDockWidget *dock);
    void appendActions(QMenu *menu,QDockWidget *dock);
private:
    struct Entry {
        QPointer<QWidget> content;
        QSize originalMin,originalMax;
        QSize requested{0,0};
        QString key;
        bool loaded=false;
    };
    QHash<QDockWidget*,Entry> entries_;
    QPointer<QMainWindow> main_;
    QPointer<Panels> panels_;
    QString path_;
    bool pending_=false;
    bool available(QDockWidget *dock)const;
    QString key(QDockWidget *dock)const;
    bool save(QDockWidget *dock,QSize requested)const;
    void apply(QDockWidget *dock);
    void release(QDockWidget *dock,Entry &entry,bool width,bool height);
    void schedule();
    void edit(QDockWidget *dock);
    bool eventFilter(QObject*,QEvent*) override;
};
}
