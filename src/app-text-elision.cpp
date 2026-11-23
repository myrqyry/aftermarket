#include "app-text-elision.h"
#include <QApplication>
#include <QAbstractItemView>
#include <QTabBar>
#include <QLabel>
#include <QStyle>
#include <QFontMetrics>
#include <QEvent>
#include <QScopedValueRollback>
#include <QTextDocument>
#include <QPointer>
#include <QWidget>
#include <QPainter>

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
    restoreLabelTooltips();
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
        restoreLabelTooltips();
        for(const auto &item:views_)if(item.view&&item.view->textElideMode()==item.applied)
            item.view->setTextElideMode(item.original);
        for(const auto &item:tabs_)if(item.tab&&item.tab->elideMode()==item.applied)
            item.tab->setElideMode(item.original);
        views_.clear();tabs_.clear();
    }else refresh();
    return true;
}
Qt::TextElideMode AppTextElision::currentCustomMode(){
    return qApp&&qApp->property(customModeProperty).toBool()?
        Qt::ElideMiddle:Qt::ElideRight;
}
bool AppTextElision::eventFilter(QObject *watched,QEvent *event){
    if(updating_||!event||mode_!=Qt::ElideMiddle)return false;
    const auto type=event->type();
    switch(type){
    case QEvent::Show:
    case QEvent::Polish:
        // New widgets adopt the native mode once; Qt automatically
        // relayouts/re-elides them when their geometry or font changes.
        // Do not scan the entire OBS widget tree for every Resize or
        // LayoutRequest: those events can fire constantly during docking.
        if(auto *widget=qobject_cast<QWidget*>(watched))
            updateWidget(widget);
        break;
    case QEvent::Paint:
        // Standard Qt buttons, checkboxes, comboboxes and labels keep
        // their own paintEvent, QStyle and style-sheet integration.
        // Only OBS's known simple source-name label needs an explicit
        // display adapter; lists and tabs already use Qt's elide APIs.
        if(auto *label=qobject_cast<QLabel*>(watched)){
            if(label->inherits("OBSSourceLabel")||
               label->objectName()=="OBSSourceLabel")
                return paintLabel(label);
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
    // Only repaint the supported source-name label, not every QLabel in OBS.
    if(auto *label=qobject_cast<QLabel*>(widget)){
        if(label->inherits("OBSSourceLabel")||
           label->objectName()=="OBSSourceLabel")label->update();
    }
}
void AppTextElision::restoreLabelTooltips(){
    const QScopedValueRollback<bool> guard(updating_,true);
    for(const auto &item:labels_)if(item.label){
        if(item.label->toolTip()==item.appliedTooltip)
            item.label->setToolTip(item.previousTooltip);
    }
    labels_.clear();
}
bool AppTextElision::paintLabel(QLabel *label){
    if(!label||updating_||mode_!=Qt::ElideMiddle||!label->isVisible()||
       label->wordWrap()||label->textFormat()==Qt::RichText||
       label->frameWidth()>0||label->pixmap(Qt::ReturnByValue).isNull()==false||
       (label->textInteractionFlags() & (Qt::TextSelectableByMouse |
                                         Qt::TextSelectableByKeyboard |
                                         Qt::TextEditable)))
        return false;
    const QString full=label->text();
    if(full.isEmpty()||full.contains('\n')||
       (label->textFormat()==Qt::AutoText&&Qt::mightBeRichText(full)))return false;
    QRect content=label->contentsRect();
    const int margin=label->margin();
    content.adjust(margin,margin,-margin,-margin);
    const int indent=qMax(0,label->indent());
    if(label->alignment() & Qt::AlignRight)content.adjust(0,0,-indent,0);
    else content.adjust(indent,0,0,0);
    if(content.width()<=0)return false;
    const auto metrics=label->fontMetrics();
    const QString visible=metrics.elidedText(full,mode_,content.width());
    if(visible==full){
        auto it=labels_.find(label);
        if(it!=labels_.end()){
            if(label->toolTip()==it->appliedTooltip){
                const QScopedValueRollback<bool> guard(updating_,true);
                label->setToolTip(it->previousTooltip);
            }
            labels_.erase(it);
        }
        return false; // OBS/Qt paints the unchanged full label naturally
    }
    if(!labels_.contains(label)&&label->toolTip().isEmpty()){
        labels_.insert(label,{label,label->toolTip(),full});
        connect(label,&QObject::destroyed,this,[this,label]{labels_.remove(label);});
        const QScopedValueRollback<bool> guard(updating_,true);
        label->setToolTip(full);
    }else if(auto it=labels_.find(label);it!=labels_.end()&&
             label->toolTip()==it->appliedTooltip&&it->appliedTooltip!=full){
        it->appliedTooltip=full;
        const QScopedValueRollback<bool> guard(updating_,true);
        label->setToolTip(full);
    }
    // Only the *painted* glyphs are shortened. QLabel::text and sizeHint
    // remain intact, so an ordinary label can still take free layout space.
    QPainter painter(label);
    label->style()->drawItemText(&painter,content,
        label->alignment()|Qt::TextSingleLine,label->palette(),label->isEnabled(),
        visible,QPalette::WindowText);
    return true;
}
}
