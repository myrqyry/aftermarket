#pragma once
#include <QObject>
#include <QPointer>
#include <QString>
class QMainWindow;
class QDialog;
class QEvent;
namespace augment {
// Native OBS 32.2 Add Source picker, shown as a bounded, frameless modal
// transient anchored to its original QMainWindow. It never creates an OBS source or replaces the picker.
class SourcePicker final:public QObject {
public:
    explicit SourcePicker(QMainWindow *main,QString settingsFile,QObject *parent=nullptr);
    ~SourcePicker() override;
    bool enabled()const{return enabled_;}
    bool setEnabled(bool);
private:
    bool eventFilter(QObject*,QEvent*) override;
    void embed(QDialog*);
    QPointer<QMainWindow> main_;
    QString settingsFile_;
    bool enabled_=true;
};
}
