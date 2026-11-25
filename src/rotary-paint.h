#pragma once
#include <QPainter>
#include <QPalette>
#include <cmath>

namespace augment {
// Shared gain, balance and encoder visual vocabulary. Geometry is relative
// to the available diameter; the caller supplies the value semantics and
// accent brush (which may be the existing animated gain gradient).
inline void paintRotaryFace(QPainter &p,const QRectF &ring,const QRectF &face,
                            const QPalette &palette,QPalette::ColorGroup group,
                            const QBrush &accent,double start,double sweep,
                            double activeStart,double activeSweep,double pointer,
                            double scale=1.0){
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(palette.color(group,QPalette::Mid),qMax(1.,3.*scale),Qt::SolidLine,Qt::RoundCap));
    p.drawArc(ring,qRound(start*16),qRound(sweep*16));
    p.setPen(QPen(accent,qMax(1.5,3.5*scale),Qt::SolidLine,Qt::RoundCap));
    if(std::abs(activeSweep)>.01)p.drawArc(ring,qRound(activeStart*16),qRound(activeSweep*16));
    if(!face.isValid())return;
    p.setPen(QPen(palette.color(group,QPalette::Mid),qMax(.65,scale)));
    p.setBrush(palette.color(group,QPalette::Button));p.drawEllipse(face);
    constexpr double pi=3.141592653589793;
    const auto rad=pointer*pi/180.;
    const QPointF direction(std::cos(rad),-std::sin(rad));
    p.setPen(QPen(palette.color(group,QPalette::ButtonText),qMax(1.15,2.*scale),Qt::SolidLine,Qt::RoundCap));
    p.drawLine(face.center()+direction*(face.width()*.13),face.center()+direction*(face.width()*.37));
}
}
