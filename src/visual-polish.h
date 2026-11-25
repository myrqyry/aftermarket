#pragma once
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPalette>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QToolButton>
#include <QElapsedTimer>
#include <QTimer>
#include <QEvent>
#include <functional>
#include "motion.h"

namespace augment {
// Reserved for Aftermarket-owned text-only toggle controls. Native OBS
// widgets keep their own QSS / QStyle state rendering.
inline QColor paletteBlend(const QColor &base,const QColor &highlight,int amount) {
    const int normal=100-amount;
    return QColor((base.red()*normal+highlight.red()*amount)/100,
                  (base.green()*normal+highlight.green()*amount)/100,
                  (base.blue()*normal+highlight.blue()*amount)/100);
}
inline QFont subtleHeadingFont(QFont font,double growth=0.75) {
    font.setWeight(QFont::DemiBold);
    if(font.pointSizeF()>0)font.setPointSizeF(font.pointSizeF()+growth);
    else if(font.pixelSize()>0)font.setPixelSize(font.pixelSize()+int(growth>=0.75));
    return font;
}
// Paint-only feedback for owned controls. No geometry, effects, pixmaps,
// polling or OBS calls. Timer runs only during a visible interaction.
class MotionToolButton : public QToolButton {
public:
    explicit MotionToolButton(QWidget *parent=nullptr):QToolButton(parent){
        elapsed_.start();
        clock_=new QTimer(this);
        clock_->setObjectName("aftermarket-feedback-clock");
        clock_->setInterval(motionFrameIntervalMs);
        connect(clock_,&QTimer::timeout,this,[this]{
            const auto t=elapsed_.elapsed();
            if(!allowed()||!isVisible()){track_.reset(target());clock_->stop();}
            else if(!track_.running(t))clock_->stop();
            update();
        });
        connect(this,&QToolButton::toggled,this,[this]{retarget();});
        connect(this,&QToolButton::pressed,this,[this]{retarget();});
        connect(this,&QToolButton::released,this,[this]{retarget();});
    }
    void setMotionProvider(std::function<bool()> provider){
        provider_=std::move(provider);retarget();
    }
protected:
    double feedbackAmount()const{
        return allowed()?track_.value(elapsed_.elapsed()):target();
    }
    bool event(QEvent *e)override{
        const auto type=e->type();
        const bool result=QToolButton::event(e);
        if(type==QEvent::Enter||type==QEvent::Leave||type==QEvent::Show||
           type==QEvent::Hide||type==QEvent::EnabledChange||type==QEvent::StyleChange)retarget();
        return result;
    }
private:
    QElapsedTimer elapsed_;
    QTimer *clock_=nullptr;
    MotionTrack track_;
    std::function<bool()> provider_;
    bool allowed()const{return provider_&&provider_()&&style()->styleHint(QStyle::SH_Widget_Animate,nullptr,this);}
    double target()const{return !isEnabled()?0:isDown()?18:isChecked()?12:underMouse()?6:0;}
    void retarget(){
        if(!clock_)return;
        if(!allowed()||!isVisible()){track_.reset(target());clock_->stop();}
        else{
            const auto t=elapsed_.elapsed();
            track_.retarget(target(),t,132);
            if(track_.running(t)&&!clock_->isActive())clock_->start();
        }
        update();
    }
};
class FineStateToolButton final : public MotionToolButton {
public:
    explicit FineStateToolButton(QWidget *parent=nullptr):MotionToolButton(parent){
        setProperty("aftermarket-thin-state",true);
        QFont face=font();
        face.setWeight(QFont::Normal);
        setFont(face);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto pal=palette();
        const bool checked=isChecked();
        const auto base=pal.color(isEnabled()?QPalette::Active:QPalette::Disabled,QPalette::Button);
        const auto accent=pal.color(QPalette::Highlight);
        const auto foreground=pal.color(isEnabled()?QPalette::Active:QPalette::Disabled,
                                        QPalette::ButtonText);
        const auto border=checked?accent:pal.color(QPalette::Mid);
        painter.setBrush(paletteBlend(base,accent,qRound(feedbackAmount())));
        painter.setPen(QPen(border,checked?1.0:0.7));
        painter.drawRoundedRect(QRectF(rect()).adjusted(1,1,-1,-1),5,5);
        painter.setFont(font());
        painter.setPen(foreground);
        painter.drawText(rect(),Qt::AlignCenter,text());
        if(hasFocus()){
            QStyleOptionFocusRect option;
            option.initFrom(this);
            option.rect=rect().adjusted(3,3,-3,-3);
            option.backgroundColor=base;
            style()->drawPrimitive(QStyle::PE_FrameFocusRect,&option,&painter,this);
        }
    }
};
}
