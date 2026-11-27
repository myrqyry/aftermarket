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
    void setPattern(const QString &pattern){pattern_=pattern;update();}
    void setScale(int scale){scale_=scale;update();}
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
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);
        const auto rect=QRectF(this->rect()).adjusted(.5,.5,-.5,-.5);
        p.setPen(palette().color(QPalette::Mid));
        if(!valid_){
            p.setBrush(palette().color(QPalette::Base));p.drawRoundedRect(rect,5,5);
            p.setPen(palette().color(QPalette::Text));
            p.drawText(rect,Qt::AlignCenter,"Invalid palette");return;
        }
        auto colors=colors_.isEmpty()?rotaryThemeColors(palette()):colors_;
        QLinearGradient brush(rect.topLeft(),rect.topLeft()+QPointF(rect.width()*scale_/100.,0));
        brush.setSpread(QGradient::RepeatSpread);
        brush.setStops(gradientPatternStops(colors,pattern_,.2));
        p.setBrush(isSpatialGradient(pattern_)?spatialGradientBrush(colors,pattern_,.2,rect,scale_):QBrush(brush));p.drawRoundedRect(rect,5,5);
    }
private:
    QString pattern_="flow";
    int scale_=100;
    bool valid_=true;
    QVector<QColor> colors_;
};
}
