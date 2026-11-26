#pragma once
#include "visual-polish.h"
#include <QToolButton>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QPaintEvent>

namespace augment {
enum class DockHeaderGlyph { Previous, Next, Flip, Knobs, More, Settings };

// Only used for Aftermarket-owned dock header controls. Original OBS buttons,
// docking behavior, stylesheet and palette remain untouched.
class DockHeaderButton final : public MotionToolButton {
public:
    explicit DockHeaderButton(DockHeaderGlyph glyph,QWidget *parent=nullptr)
        :MotionToolButton(parent),glyph_(glyph) {
        setProperty("aftermarket-header-button",true);
        setFixedSize(20,18);
        setAutoRaise(true);
        setFocusPolicy(Qt::StrongFocus);
        setToolButtonStyle(Qt::ToolButtonIconOnly);
        setCheckable(glyph==DockHeaderGlyph::Flip||glyph==DockHeaderGlyph::Knobs);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto pal=palette();
        const auto group=isEnabled()?QPalette::Active:QPalette::Disabled;
        const QColor foreground=pal.color(group,QPalette::ButtonText);
        const QColor accent=pal.color(group,QPalette::Highlight);
        const QColor base=pal.color(group,QPalette::Button);
        const bool active=isCheckable()&&isChecked();
        const int feedback=qRound(feedbackAmount());
        if(active||feedback>0){
            painter.setPen(QPen(active?accent:pal.color(group,QPalette::Mid),
                                active?1.0:0.7));
            painter.setBrush(paletteBlend(base,accent,feedback));
            painter.drawRoundedRect(QRectF(rect()).adjusted(1.2,1.2,-1.2,-1.2),5,5);
        }
        const qreal cx=width()/2.0,cy=height()/2.0;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(active?accent:foreground,1.45,Qt::SolidLine,
                            Qt::RoundCap,Qt::RoundJoin));
        switch(glyph_){
        case DockHeaderGlyph::Previous:
        case DockHeaderGlyph::Next: {
            const int direction=glyph_==DockHeaderGlyph::Previous?-1:1;
            const QPointF tip(cx+direction*3,cy);
            painter.drawLine(QPointF(cx-direction*3,cy-4),tip);
            painter.drawLine(tip,QPointF(cx-direction*3,cy+4));
            break;
        }
        case DockHeaderGlyph::Flip:
            // Two stacked faces; checked accent identifies the routing face.
            painter.drawRoundedRect(QRectF(cx-6.5,cy-5.5,9.5,9.5),1.5,1.5);
            painter.drawRoundedRect(QRectF(cx-2.5,cy-2.5,9.5,9.5),1.5,1.5);
            painter.drawLine(QPointF(cx+1,cy+4.6),QPointF(cx+4.7,cy+4.6));
            break;
        case DockHeaderGlyph::More:
            painter.setPen(Qt::NoPen);
            painter.setBrush(active?accent:foreground);
            for(int dy=-4;dy<=4;dy+=4)
                painter.drawEllipse(QPointF(cx,cy+dy),1.3,1.3);
            break;
        case DockHeaderGlyph::Settings: {
            // Eight compact gear teeth, matching the rounded line glyphs.
            constexpr double pi=3.141592653589793;
            QPainterPath outline;
            for(int i=0;i<32;++i){
                const qreal angle=(i-1.5)*pi/16;
                const qreal radius=(i%4==1||i%4==2)?7.0:5.4;
                const QPointF point(cx+radius*std::cos(angle),cy+radius*std::sin(angle));
                if(i==0)outline.moveTo(point);else outline.lineTo(point);
            }
            outline.closeSubpath();
            painter.setPen(QPen(foreground,1.15,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            painter.drawPath(outline);
            painter.drawEllipse(QPointF(cx,cy),2.2,2.2);
            break;
        }
        case DockHeaderGlyph::Knobs: {
            painter.drawEllipse(QPointF(cx,cy),5.5,5.5);
            painter.drawLine(QPointF(cx,cy),QPointF(cx+3.2,cy-3.2));
            painter.drawPoint(QPointF(cx-7.5,cy+4));
            painter.drawPoint(QPointF(cx+7.5,cy-4));
            break;
        }
        }
        if(hasFocus()){
            QStyleOptionFocusRect option;
            option.initFrom(this);
            option.rect=rect().adjusted(3,3,-3,-3);
            option.backgroundColor=base;
            style()->drawPrimitive(QStyle::PE_FrameFocusRect,&option,&painter,this);
        }
    }
private:
    DockHeaderGlyph glyph_;
};
}
