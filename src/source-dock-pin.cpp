#include "source-dock-pin.h"
#include <QDialog>
#include <QGridLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QPointer>
#include <QList>
namespace augment {
QPushButton *prepareSourceDockPin(QDialog *dialog,const QString &source,
    const QString &title,const QString &addLabel,SourceDockPinOptions options){
    if(!dialog||source.isEmpty()||title.isEmpty()||addLabel.isEmpty())return nullptr;
    QGridLayout *form=nullptr;
    for(auto *grid:dialog->findChildren<QGridLayout*>()){
        if(grid->columnCount()!=15||grid->rowCount()<2)continue;
        auto *item=grid->itemAtPosition(1,0);
        if(!item||!qobject_cast<QComboBox*>(item->widget()))continue;
        if(form)return nullptr;
        form=grid;
    }
    if(!form)return nullptr;
    auto widget=[form](int col)->QWidget*{
        auto *item=form->itemAtPosition(1,col);return item?item->widget():nullptr;
    };
    auto *combo=qobject_cast<QComboBox*>(widget(0));
    auto *titleEdit=qobject_cast<QLineEdit*>(widget(1));
    auto *windowEdit=qobject_cast<QLineEdit*>(widget(2));
    auto *add=qobject_cast<QPushButton*>(widget(14));
    if(!combo||!combo->isEditable()||!titleEdit||!windowEdit||!add||
       add->text()!=addLabel||!add->isEnabled()||!titleEdit->text().isEmpty()||
       !windowEdit->text().isEmpty()||!combo->currentText().isEmpty())return nullptr;
    QList<QCheckBox*> checks;
    for(int col=3;col<=13;++col){
        auto *check=qobject_cast<QCheckBox*>(widget(col));
        if(!check)return nullptr;
        checks.append(check);
    }
    int index=-1;
    for(int i=2;i<combo->count();++i){
        if(combo->itemText(i)==source&&combo->itemData(i).toByteArray()==source.toUtf8()){
            if(index!=-1)return nullptr;
            index=i;
        }
    }
    if(index<0)return nullptr;
    QPointer<QPushButton> safeAdd=add;
    combo->setCurrentIndex(index);
    if(combo->currentIndex()!=index||combo->currentText()!=source)return nullptr;
    titleEdit->setText(title);
    const bool states[]={true,options.preview,options.audio,options.audio,
        options.media,false,true,true,true,false,options.scene};
    for(int i=0;i<checks.size();++i)checks[i]->setChecked(states[i]);
    return safeAdd;
}
}
