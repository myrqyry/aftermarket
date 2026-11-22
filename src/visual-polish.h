#pragma once
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPalette>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QToolButton>

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
class FineStateToolButton final : public QToolButton {
public:
    explicit FineStateToolButton(QWidget *parent=nullptr):QToolButton(parent){
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
        const bool checked=isChecked(),hovered=underMouse()&&isEnabled();
        const auto base=pal.color(isEnabled()?QPalette::Active:QPalette::Disabled,QPalette::Button);
        const auto accent=pal.color(QPalette::Highlight);
        const auto foreground=pal.color(isEnabled()?QPalette::Active:QPalette::Disabled,
                                        QPalette::ButtonText);
        const auto border=checked?accent:pal.color(QPalette::Mid);
        painter.setBrush(paletteBlend(base,accent,checked?12:(hovered?7:0)));
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
