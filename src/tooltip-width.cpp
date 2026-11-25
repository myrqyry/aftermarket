#include "tooltip-width.h"
#include <QScreen>
#include <QScopedValueRollback>
#include <QTextDocument>
#include <QToolTip>
#include <QApplication>
#include <QComboBox>
#include <QSignalBlocker>

namespace augment {
void populateTooltipWidths(QComboBox *box,int selected){
    const QSignalBlocker blocker(box);
    box->clear();box->addItem("Native (unlimited)",0);
    for(int px=160;px<=640;px+=20)box->addItem(QString::number(px)+" px",px);
    if(box->findData(selected)<0&&selected>=160&&selected<=640)
        box->insertItem(1,QString::number(selected)+" px",selected);
    box->setCurrentIndex(box->findData(selected));
}

void TooltipWidth::restore(){
    const QScopedValueRollback<bool> guard(busy_,true);
    if(label_){
        auto *label=label_.data();
        label_.clear();
        label->setMaximumWidth(originalMax_);
        label->setWordWrap(originalWrap_);
    }
    text_.clear();
}
void TooltipWidth::configure(int width){
    if(width_==width)return;
    if(label_&&label_->isVisible())QToolTip::hideText();
    restore();width_=width;
}
void TooltipWidth::fit(QLabel *label,bool force){
    if(busy_||width_<=0||!label)return;
    const QScopedValueRollback<bool> guard(busy_,true);
    if(label_!=label){
        restore();label_=label;originalMax_=label->maximumWidth();originalWrap_=label->wordWrap();force=true;
    }
    const auto text=label->text();
    if(!force&&text_==text&&label->size()==fittedSize_&&label->wordWrap()==fittedWrap_&&label->maximumWidth()==fittedCap_)return;
    if(text_!=text&&!text_.isEmpty())originalWrap_=label->wordWrap();
    text_=text;
    // Qt resets wordWrap when reusing QTipLabel for a new tooltip.
    // Measure its original natural width before applying our constraint.
    label->setMaximumWidth(originalMax_);
    label->setWordWrap(originalWrap_);
    const auto natural=label->sizeHint();
    const auto *screen=label->screen();
    const auto available=screen?screen->availableGeometry():QRect{};
    const int cap=qMin(qMin(width_,originalMax_),available.isValid()?available.width():width_);
    if(cap<=0)return;
    const int target=qMin(cap,natural.width()+1);
    const bool wrap=originalWrap_||natural.width()+1>cap;
    label->setWordWrap(wrap);
    label->setMaximumWidth(cap);
    const int height=wrap?label->heightForWidth(target):natural.height();
    label->resize(target,qMax(natural.height(),height));
    fittedSize_=label->size();fittedWrap_=wrap;fittedCap_=cap;
    if(available.isValid()){
        auto p=label->pos();
        p.setX(qBound(available.left(),p.x(),qMax(available.left(),available.right()-label->width()+1)));
        p.setY(qBound(available.top(),p.y(),qMax(available.top(),available.bottom()-label->height()+1)));
        if(p!=label->pos())label->move(p);
    }
}
void TooltipWidth::handle(QObject *object,QEvent *event){
    if(busy_||width_<=0)return;
    const auto type=event->type();
    if(type!=QEvent::Show&&type!=QEvent::Hide&&type!=QEvent::Resize&&
       type!=QEvent::UpdateRequest&&type!=QEvent::FontChange&&type!=QEvent::StyleChange)return;
    auto *label=qobject_cast<QLabel*>(object);
    // QTipLabel is Qt's real tooltip implementation; leave custom popups,
    // tool windows and ordinary labels alone.
    if(!label||label->windowType()!=Qt::ToolTip||!label->inherits("QTipLabel"))return;
    if(type==QEvent::Hide){if(label_==label)restore();return;}
    if(type!=QEvent::Show&&!label->isVisible())return;
    fit(label,type==QEvent::Show||type==QEvent::FontChange||type==QEvent::StyleChange);
}
}
