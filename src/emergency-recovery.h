#pragma once
#include <QObject>
#include <QString>
#include <QPointer>
#include <functional>
class QWidget;
class QShortcut;
namespace augment {
// Escape hatch independent of rules.json. The activation marker is written
// before any UI modification is rolled back; only explicit resume clears it.
class EmergencyRecovery final : public QObject {
public:
    using Restore=std::function<void()>;
    using Resume=std::function<bool()>;
    EmergencyRecovery(QString markerFile,QWidget *mainWindow,Restore restore,Resume resume,QObject *parent=nullptr);
    ~EmergencyRecovery() override;
    bool active() const;
    bool engage();
    bool resume();
    static QString hotkeyText();
protected:
    bool eventFilter(QObject *watched,QEvent *event) override;
private:
    QString marker_;
    QPointer<QShortcut> shortcut_;
    Restore restore_;
    Resume resume_;
    bool active_=false,acting_=false;
};
}
