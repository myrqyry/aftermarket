#pragma once
#include "gradient.h"
#include "rotary-paint.h"
#include <QWidget>
#include <QJsonArray>
#include <QPainter>

namespace augment {
// Paint the same palette as the mixer, without a stylesheet, fixed-size
// pixmap, animation clock, or mutation of the live OBS preview.
class PalettePreview final : public QWidget {
public:
    explicit PalettePreview(QWidget *parent):QWidget(parent){
        setObjectName("aftermarket-settings-palette-preview");
        setAccessibleName("Gradient palette preview");
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        setMinimumHeight(22);setMaximumHeight(28);
    }
    void setColors(const QString &text){
        QJsonArray array;
        if(!text.trimmed().isEmpty())
            for(const auto &part:text.split(','))array.append(part.trimmed());
        GradientSpec spec;
        valid_=GradientSpec::parse(QJsonObject{{"colors",array}},spec);
        colors_=spec.colors;
        setProperty("validPalette",valid_);update();
    }
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const auto rect=QRectF(this->rect()).adjusted(.5,.5,-.5,-.5);
        p.setPen(palette().color(QPalette::Mid));
        if(!valid_){
            p.setBrush(palette().color(QPalette::Base));p.drawRoundedRect(rect,5,5);
            p.setPen(palette().color(QPalette::Text));
            p.drawText(rect,Qt::AlignCenter,"Invalid palette");return;
        }
        auto colors=colors_.isEmpty()?rotaryThemeColors(palette()):colors_;
        if(colors.first()!=colors.last())colors.append(colors.first());
        QLinearGradient brush(rect.topLeft(),rect.topRight());
        for(int i=0;i<colors.size();++i)brush.setColorAt(double(i)/(colors.size()-1),colors.at(i));
        p.setBrush(brush);p.drawRoundedRect(rect,5,5);
    }
private:
    bool valid_=true;
    QVector<QColor> colors_;
};
}
