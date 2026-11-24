#include "source-picker.h"
#include <QApplication>
#include <QDialog>
#include <QDockWidget>
#include <QMainWindow>
#include <QEvent>
#include <QListWidget>
#include <QScrollArea>
#include <QCheckBox>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QFontMetrics>
#include <QTimer>
#include <QFrame>
#include <QBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QFont>
#include <QStyle>
#include <QPointer>
#include <algorithm>

namespace augment {
SourcePicker::SourcePicker(QMainWindow *main,QString settingsFile,QObject *parent)
    :QObject(parent),main_(main),settingsFile_(std::move(settingsFile)) {
    if(!settingsFile_.isEmpty()){
        QSettings settings(settingsFile_,QSettings::IniFormat);
        enabled_=settings.value("sourcePicker/embedded",true).toBool();
    }
    if(qApp)qApp->installEventFilter(this);
}
SourcePicker::~SourcePicker(){
    if(qApp)qApp->removeEventFilter(this);
}
bool SourcePicker::setEnabled(bool value){
    if(!settingsFile_.isEmpty()){
        QSettings settings(settingsFile_,QSettings::IniFormat);
        settings.setValue("sourcePicker/embedded",value);
        settings.sync();
        if(settings.status()!=QSettings::NoError)return false;
    }
    enabled_=value;
    return true;
}
bool SourcePicker::eventFilter(QObject *watched,QEvent *event){
    // Qt posts Polish before QDialog::exec begins its local event loop.
    // Reparenting AFTER Show causes QDialog::exec to return prematurely.
    if(enabled_&&main_&&event->type()==QEvent::Polish&&
       watched->objectName()==QStringLiteral("OBSBasicSourceSelect")){
        if(auto *dialog=qobject_cast<QDialog*>(watched))embed(dialog);
    }
    return false;
}
void SourcePicker::embed(QDialog *dialog){
    if(!main_||!dialog||!dialog->isWindow()||dialog->isVisible())return;
    auto *list=dialog->findChild<QListWidget*>("sourceTypeList");
    auto *existing=dialog->findChild<QScrollArea*>("existingScrollArea");
    auto *visible=dialog->findChild<QCheckBox*>("sourceVisible");
    auto *add=dialog->findChild<QPushButton*>("addExistingButton");
    auto *sources=main_->findChild<QDockWidget*>("sourcesDock");
    if(!list||!existing||!visible||!add||!sources||sources->isHidden()||
       main_->width()<700||main_->height()<500)return;

    // The stock UI forces 460px of height onto its source-type list. The
    // list already scrolls, so let it use the bounded space of the overlay.
    list->setMinimumHeight(240);
    list->setMaximumWidth(270);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setTextElideMode(Qt::ElideRight);
    for(int i=0;i<list->count();++i){
        auto *item=list->item(i);
        if(item&&item->toolTip().isEmpty())item->setToolTip(item->text());
    }
    // Keep OBS's QDialog and every source/card/button signal intact. A child
    // dialog still completes its native exec() result, unlike reparenting
    // it in a queued callback after Show (which rejects exec early).
    dialog->setParent(main_,Qt::Widget);
    dialog->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    dialog->setMinimumSize(640,400);
    const int width=std::clamp(main_->width()*2/3,700,820);
    const int height=std::clamp(main_->height()*11/20,460,540);
    const QPoint origin=sources->mapTo(main_,QPoint(0,0));
    const bool dockLeft=origin.x()+sources->width()/2<main_->width()/2;
    const int wantedX=dockLeft?origin.x()+sources->width()-12:origin.x()-width+12;
    const int wantedY=origin.y()+sources->height()-height+8;
    const int x=std::clamp(wantedX,8,main_->width()-width-8);
    const int y=std::clamp(wantedY,8,main_->height()-height-8);
    dialog->setGeometry(x,y,width,height);
    // The native desktop title bar is no longer present when the dialog
    // becomes an embedded QWidget. Keep a compact, accessible title/close
    // row in the overlay; all OBS source-selection content remains native.
    if(auto *root=qobject_cast<QBoxLayout*>(dialog->layout())){
        auto *heading=new QWidget(dialog);
        heading->setObjectName("aftermarket-source-picker-heading");
        auto *row=new QHBoxLayout(heading);
        row->setContentsMargins(10,3,8,3);
        row->setSpacing(6);
        auto *title=new QLabel(dialog->windowTitle().isEmpty()?
                               QStringLiteral("Add Source"):dialog->windowTitle(),heading);
        QFont font=title->font();font.setWeight(QFont::DemiBold);
        title->setFont(font);
        row->addWidget(title,1);
        auto *close=new QToolButton(heading);
        close->setObjectName("aftermarket-source-picker-close");
        close->setToolTip("Close Add Source");
        close->setAccessibleName(close->toolTip());
        close->setAutoRaise(true);
        close->setIcon(dialog->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        row->addWidget(close);
        QObject::connect(close,&QToolButton::clicked,dialog,&QDialog::reject);
        root->insertWidget(0,heading);
    }
    QPointer<QDialog> safe=dialog;
    QTimer::singleShot(0,dialog,[safe]{
        if(safe&&safe->isVisible())safe->raise();
    });
}
}
