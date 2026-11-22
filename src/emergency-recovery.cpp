#include "emergency-recovery.h"
#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QSaveFile>
#include <QShortcut>
#include <QWidget>
#include <QScopedValueRollback>

namespace augment {
QString EmergencyRecovery::hotkeyText(){return QStringLiteral("Ctrl+Alt+Shift+F12");}
EmergencyRecovery::EmergencyRecovery(QString markerFile,QWidget *main,Restore restore,Resume resume,QObject *parent)
    :QObject(parent),marker_(std::move(markerFile)),restore_(std::move(restore)),resume_(std::move(resume)){
    active_=QFileInfo::exists(marker_); // any marker content safely means disabled
    if(main){
        shortcut_=new QShortcut(QKeySequence(hotkeyText()),main);
        shortcut_->setObjectName("aftermarket-emergency-shortcut");
        shortcut_->setContext(Qt::ApplicationShortcut);
        connect(shortcut_,&QShortcut::activated,this,[this]{engage();});
    }
    if(qApp)qApp->installEventFilter(this);
}
EmergencyRecovery::~EmergencyRecovery(){
    if(qApp)qApp->removeEventFilter(this);
    delete shortcut_.data();
}
bool EmergencyRecovery::active() const{return active_;}
bool EmergencyRecovery::engage(){
    if(active_||acting_)return false;
    QScopedValueRollback<bool> busy(acting_,true);
    QSaveFile file(marker_);
    if(!file.open(QIODevice::WriteOnly))return false;
    if(file.write("Aftermarket emergency recovery mode\n")<0||!file.commit())return false;
    active_=true;
    if(restore_)restore_();
    return true;
}
bool EmergencyRecovery::resume(){
    if(!active_||acting_)return false;
    QScopedValueRollback<bool> busy(acting_,true);
    if(resume_&&!resume_())return false;
    // Keep the startup guard in place until recovery is truly disabled.
    if(!QFile::remove(marker_)){
        if(restore_)restore_();
        return false;
    }
    active_=false;
    return true;
}
bool EmergencyRecovery::eventFilter(QObject *,QEvent *event){
    if(event->type()!=QEvent::KeyPress)return false;
    auto *key=static_cast<QKeyEvent*>(event);
    const auto modifiers=Qt::ControlModifier|Qt::AltModifier|Qt::ShiftModifier;
    if(key->key()!=Qt::Key_F12||key->isAutoRepeat()||
       (key->modifiers()&modifiers)!=modifiers)return false;
    // Capture even if an Aftermarket popup has grabbed keyboard focus.
    engage();
    key->accept();
    return true;
}
}
