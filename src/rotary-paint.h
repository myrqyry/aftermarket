#pragma once
#include <QPainter>
#include <QPalette>
#include <QVector>
#include <cmath>

namespace augment {
inline QVector<QColor> rotaryThemeColors(const QPalette &palette,QPalette::ColorGroup group=QPalette::Active){
    const auto accent=palette.color(group,QPalette::Highlight);
    return {accent,accent.lighter(155),palette.color(group,QPalette::Link),accent};
}
inline QColor rotaryMutedAccent(const QColor &surface,const QColor &accent){
    return QColor::fromRgbF(surface.redF()*.58+accent.redF()*.42,
        surface.greenF()*.58+accent.greenF()*.42,surface.blueF()*.58+accent.blueF()*.42);
}
inline QConicalGradient rotaryThemeGradient(const QPointF &center,const QPalette &palette,QPalette::ColorGroup group){
    QConicalGradient gradient(center,225);
    const auto colors=rotaryThemeColors(palette,group);
    for(int i=0;i<colors.size();++i)gradient.setColorAt(double(i)/(colors.size()-1),colors.at(i));
    return gradient;
}
// Use a centered square in logical pixels, regardless of host aspect ratio.
inline QRectF rotarySquare(const QRectF &available,qreal maximum,qreal inset){
    const qreal side=qMin(maximum,qMin(available.width(),available.height()))-2*inset;
    if(side<8)return {};
    return QRectF(available.center()-QPointF(side/2.,side/2.),QSizeF(side,side));
}
inline double rotaryDragDistance(const QPointF &delta){return delta.x()-delta.y();}

// Shared gain, balance and encoder visual vocabulary. Geometry is relative
// to the available diameter; the caller supplies the value semantics and
// accent brush (which may be the existing animated gain gradient).
inline void paintRotaryFace(QPainter &p,const QRectF &ring,const QRectF &face,
                            const QPalette &palette,QPalette::ColorGroup group,
                            const QBrush &accent,double start,double sweep,
                            double activeStart,double activeSweep,double pointer,
                            double scale=1.0,bool compact=false,bool showPointer=true){
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(palette.color(group,QPalette::Mid),qMax(1.,3.*scale),Qt::SolidLine,Qt::RoundCap));
    p.drawArc(ring,qRound(start*16),qRound(sweep*16));
    p.setPen(QPen(accent,qMax(1.5,3.5*scale),Qt::SolidLine,Qt::RoundCap));
    if(std::abs(activeSweep)>.01)p.drawArc(ring,qRound(activeStart*16),qRound(activeSweep*16));
    if(!face.isValid())return;
    p.setPen(QPen(palette.color(group,QPalette::Mid),qMax(.65,scale)));
    p.setBrush(palette.color(group,QPalette::Button));p.drawEllipse(face);
    if(!showPointer)return;
    constexpr double pi=3.141592653589793;
    const auto rad=pointer*pi/180.;
    const QPointF direction(std::cos(rad),-std::sin(rad));
    p.setPen(QPen(palette.color(group,QPalette::ButtonText),qMax(1.15,2.*scale),Qt::SolidLine,Qt::RoundCap));
    p.drawLine(face.center()+direction*(face.width()*(compact?.05:.13)),face.center()+direction*(face.width()*(compact?.41:.37)));
}
}
