#include "app-text-elision.h"
#include <QApplication>
#include <QAbstractItemView>
#include <QTabBar>
#include <QLabel>
#include <QComboBox>
#include <QAbstractButton>
#include <QPushButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QStylePainter>
#include <QStyleOptionComboBox>
#include <QStyleOptionButton>
#include <QStyle>
#include <QFontMetrics>
#include <QEvent>
#include <QTimer>
#include <QScopedValueRollback>
#include <QTextDocument>
#include <QPointer>
#include <QWidget>

namespace augment {
namespace {
constexpr auto customModeProperty="aftermarket-global-ellipsis-middle";
}
AppTextElision::AppTextElision(QWidget *mainWindow,QObject *parent)
    :QObject(parent),window_(mainWindow){
    if(qApp)originalPaintProperty_=qApp->property(customModeProperty);
    if(qApp)qApp->installEventFilter(this);
    refresh();
}
AppTextElision::~AppTextElision(){
    if(qApp)qApp->removeEventFilter(this);
    if(qApp)qApp->setProperty(customModeProperty,originalPaintProperty_);
    QScopedValueRollback<bool> guard(updating_,true);
    for(const auto &item:labels_)if(item.label){
        if(item.label->text()==item.applied)
            item.label->setText(item.original);
        if(item.addedTooltip&&item.label->toolTip()==item.original)
            item.label->setToolTip(item.tooltip);
    }
    for(const auto &item:views_)if(item.view&&item.view->textElideMode()==item.applied)
        item.view->setTextElideMode(item.original);
    for(const auto &item:tabs_)if(item.tab&&item.tab->elideMode()==item.applied)
        item.tab->setElideMode(item.original);
}
bool AppTextElision::setMode(Qt::TextElideMode mode){
    if(mode!=Qt::ElideMiddle&&mode!=Qt::ElideRight)return false;
    if(mode_==mode)return true;
    mode_=mode;
    if(qApp)qApp->setProperty(customModeProperty,
                             mode==Qt::ElideMiddle?QVariant(true):originalPaintProperty_);
    for(auto *widget:QApplication::allWidgets())
        if(widget)widget->update(); // custom OBS/Aftermarket painters
    if(mode_==Qt::ElideRight){
        // Default is NOT an application-wide override; restore the
        // widget-specific behavior OBS and third-party plugins chose.
        QScopedValueRollback<bool> guard(updating_,true);
        for(const auto &item:labels_)if(item.label){
            if(item.label->text()==item.applied)
                item.label->setText(item.original);
            if(item.addedTooltip&&item.label->toolTip()==item.original)
                item.label->setToolTip(item.tooltip);
        }
        for(const auto &item:views_)if(item.view&&item.view->textElideMode()==item.applied)
            item.view->setTextElideMode(item.original);
        for(const auto &item:tabs_)if(item.tab&&item.tab->elideMode()==item.applied)
            item.tab->setElideMode(item.original);
        labels_.clear();views_.clear();tabs_.clear();
    }else refresh();
    return true;
}
Qt::TextElideMode AppTextElision::currentCustomMode(){
    return qApp&&qApp->property(customModeProperty).toBool()?
        Qt::ElideMiddle:Qt::ElideRight;
}
QString AppTextElision::visibleText(const QString &text,
                                  const QFontMetrics &metrics,int width)const{
    return width<=0?text:metrics.elidedText(text,mode_,width);
}
QString AppTextElision::originalLabel(const QLabel *label)const{
    if(!label)return {};
    auto it=labels_.constFind(const_cast<QLabel*>(label));
    return it==labels_.cend()?label->text():it->original;
}
void AppTextElision::scheduleRefresh(){
    if(scheduled_||!qApp)return;
    scheduled_=true;
    QTimer::singleShot(0,this,[this]{
        scheduled_=false;
        refresh();
    });
}
bool AppTextElision::eventFilter(QObject *watched,QEvent *event){
    if(updating_||!event||mode_!=Qt::ElideMiddle)return false;
    const auto type=event->type();
    switch(type){
    case QEvent::Show:
    case QEvent::Polish:
    case QEvent::Resize:
    case QEvent::FontChange:
    case QEvent::StyleChange:
    case QEvent::LayoutRequest:
    case QEvent::ChildAdded:
        scheduleRefresh();
        break;
    case QEvent::Paint:
        // Never alter QAction, QAbstractButton or QComboBox values: some
        // OBS-native source identity paths depend on those exact strings.
        // Draw shortened DISPLAY labels using each widget's own QStyle.
        if(auto *combo=qobject_cast<QComboBox*>(watched)){
            if(!combo->isEditable()&&combo->currentIndex()>=0){
                QStyleOptionComboBox option;
                option.initFrom(combo);
                option.editable=false;
                option.frame=combo->hasFrame();
                option.currentText=combo->currentText();
                option.currentIcon=combo->itemIcon(combo->currentIndex());
                option.iconSize=combo->iconSize();
                const QRect content=combo->style()->subControlRect(
                    QStyle::CC_ComboBox,&option,QStyle::SC_ComboBoxEditField,combo);
                int width=content.width()-4;
                if(!option.currentIcon.isNull())width-=option.iconSize.width()+4;
                const QString shown=visibleText(option.currentText,
                                                 combo->fontMetrics(),width);
                if(shown!=option.currentText){
                    option.currentText=shown;
                    QStylePainter painter(combo);
                    painter.drawComplexControl(QStyle::CC_ComboBox,option);
                    painter.drawControl(QStyle::CE_ComboBoxLabel,option);
                    return true;
                }
            }
        }else if(auto *button=qobject_cast<QAbstractButton*>(watched)){
            // Tool buttons (including OBS mixer sources) use specialized
            // native painters. Do not override them with generic controls.
            auto *push=qobject_cast<QPushButton*>(button);
            auto *check=qobject_cast<QCheckBox*>(button);
            auto *radio=qobject_cast<QRadioButton*>(button);
            if((push||check||radio)&&!button->text().isEmpty()){
                QStyleOptionButton option;
                option.initFrom(button);
                option.text=button->text();
                option.icon=button->icon();
                option.iconSize=button->iconSize();
                if(button->isChecked())option.state|=QStyle::State_On;
                else option.state|=QStyle::State_Off;
                if(button->isDown())option.state|=QStyle::State_Sunken;
                if(push){
                    if(push->isDefault())option.features|=QStyleOptionButton::DefaultButton;
                    if(push->autoDefault())option.features|=QStyleOptionButton::AutoDefaultButton;
                    if(push->isFlat())option.features|=QStyleOptionButton::Flat;
                    if(push->menu())option.features|=QStyleOptionButton::HasMenu;
                }
                const QStyle::SubElement sub=push?QStyle::SE_PushButtonContents:
                    check?QStyle::SE_CheckBoxContents:QStyle::SE_RadioButtonContents;
                const QStyle::ControlElement control=push?QStyle::CE_PushButton:
                    check?QStyle::CE_CheckBox:QStyle::CE_RadioButton;
                const QRect content=button->style()->subElementRect(sub,&option,button);
                int width=content.width()-4;
                if(!option.icon.isNull())width-=option.iconSize.width()+4;
                const QString shown=visibleText(option.text,button->fontMetrics(),width);
                if(shown!=option.text){
                    option.text=shown;
                    QStylePainter painter(button);
                    painter.drawControl(control,option);
                    return true;
                }
            }
        }else if(auto *label=qobject_cast<QLabel*>(watched)){
            // Catch external QLabel::setText before the actual widget paint.
            updateLabel(label);
        }
        break;
    default:break;
    }
    return false;
}
void AppTextElision::refresh(){
    if(!qApp||updating_||mode_!=Qt::ElideMiddle)return;
    for(auto *widget:QApplication::allWidgets())updateWidget(widget);
}
void AppTextElision::updateWidget(QWidget *widget){
    if(!widget||!qApp||updating_||mode_!=Qt::ElideMiddle)return;
    if(auto *view=qobject_cast<QAbstractItemView*>(widget)){
        auto it=views_.find(view);
        if(it==views_.end()){
            views_.insert(view,{view,view->textElideMode(),mode_});
            QObject::connect(view,&QObject::destroyed,this,[this,view]{views_.remove(view);});
            it=views_.find(view);
        }else if(view->textElideMode()!=it->applied)
            it->original=view->textElideMode();
        if(view->textElideMode()!=mode_){
            const QScopedValueRollback<bool> guard(updating_,true);
            view->setTextElideMode(mode_);
        }
        it->applied=mode_;
    }
    if(auto *tab=qobject_cast<QTabBar*>(widget)){
        auto it=tabs_.find(tab);
        if(it==tabs_.end()){
            tabs_.insert(tab,{tab,tab->elideMode(),mode_});
            QObject::connect(tab,&QObject::destroyed,this,[this,tab]{tabs_.remove(tab);});
            it=tabs_.find(tab);
        }else if(tab->elideMode()!=it->applied)
            it->original=tab->elideMode();
        if(tab->elideMode()!=mode_){
            const QScopedValueRollback<bool> guard(updating_,true);
            tab->setElideMode(mode_);
        }
        it->applied=mode_;
    }
    if(auto *label=qobject_cast<QLabel*>(widget))updateLabel(label);
}
void AppTextElision::updateLabel(QLabel *label){
    if(!label||updating_||mode_!=Qt::ElideMiddle||!label->isVisible()||
       label->wordWrap()||label->textFormat()==Qt::RichText||
       (label->textInteractionFlags() & (Qt::TextSelectableByMouse |
                                         Qt::TextSelectableByKeyboard |
                                         Qt::TextEditable)))
        return;
    auto text=label->text();
    auto it=labels_.find(label);
    if(it==labels_.end()){
        if(text.isEmpty()||text.contains('\n')||
           (label->textFormat()==Qt::AutoText&&Qt::mightBeRichText(text)))return;
        labels_.insert(label,{label,text,text,label->toolTip(),false});
        QObject::connect(label,&QObject::destroyed,this,[this,label]{labels_.remove(label);});
        it=labels_.find(label);
    }else if(text!=it->applied){
        // OBS updated the label; the external full string is authoritative.
        const QString oldFull=it->original;
        it->original=text;
        it->applied=text;
        if(it->addedTooltip){
            if(label->toolTip()==oldFull){
                const QScopedValueRollback<bool> guard(updating_,true);
                label->setToolTip(it->tooltip);
            }
            it->addedTooltip=false;
        }
    }
    const auto &full=it->original;
    if(full.isEmpty()||full.contains('\n')||
       (label->textFormat()==Qt::AutoText&&Qt::mightBeRichText(full)))return;
    const int available=label->contentsRect().width()-2*label->margin()-
        qMax(0,label->indent());
    if(available<=0)return;
    const QString visible=label->fontMetrics().elidedText(full,mode_,available);
    if(visible!=label->text()){
        const QScopedValueRollback<bool> guard(updating_,true);
        label->setText(visible);
    }
    it->applied=visible;
    // Expose the exact full label on hover only when it is elided and the
    // widget has no user/OBS tooltip already.
    if(visible!=full&&label->toolTip().isEmpty()){
        const QScopedValueRollback<bool> guard(updating_,true);
        label->setToolTip(full);
        it->addedTooltip=true;
    }else if(visible==full&&it->addedTooltip&&label->toolTip()==full){
        const QScopedValueRollback<bool> guard(updating_,true);
        label->setToolTip(it->tooltip);
        it->addedTooltip=false;
    }
}
}
