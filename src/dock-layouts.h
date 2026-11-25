#pragma once

#include <QByteArray>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

class QMainWindow;
class QWidget;

namespace augment {
struct DockLayoutSnapshot {
    QString id;
    QString name;
    QStringList requiredDocks;
    QByteArray state;
};

class DockLayouts final {
public:
    explicit DockLayouts(QString configPath, QMainWindow *main);
    QList<DockLayoutSnapshot> snapshots() const { return presets_; }
    bool reload();
    QString capture(const QString &name);
    bool replace(const QString &id);
    bool rename(const QString &id, const QString &name);
    bool remove(const QString &id);
    bool restore(const QString &id);
    bool undo();
    bool canUndo() const { return !undoState_.isEmpty(); }
    void showManager(QWidget *owner,std::function<void(QWidget*)> checkpoints={});
private:
    bool write(const QList<DockLayoutSnapshot> &snapshots) const;
    QStringList dockIds() const;
    static bool validName(const QString &name);
    QString path_;
    QPointer<QMainWindow> main_;
    QList<DockLayoutSnapshot> presets_;
    QByteArray undoState_;
};
} // namespace augment
