#pragma once
#include "visual-polish.h"
#include <QToolButton>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QPaintEvent>

namespace augment {
enum class DockHeaderGlyph { Previous, Next, Flip, Knobs };

// Only used for Aftermarket-owned dock header controls. Original OBS buttons,
// docking behavior, stylesheet and palette remain untouched.
class DockHeaderButton final : public QToolButton {
public:
    explicit DockHeaderButton(DockHeaderGlyph glyph,QWidget *parent=nullptr)
        :QToolButton(parent),glyph_(glyph) {
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
        const bool hover=isEnabled()&&underMouse();
        if(active||hover){
            painter.setPen(QPen(active?accent:pal.color(group,QPalette::Mid),
                                active?1.0:0.7));
            painter.setBrush(paletteBlend(base,accent,active?12:6));
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
