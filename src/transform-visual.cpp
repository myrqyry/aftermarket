#include "transform-visual.h"
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QCursor>
#include <QApplication>
#include <QLineF>
#include <QtMath>
namespace augment {
bool matchesFittedPreview(const QSize &canvas,const QSize &viewport,int reportedPercent){
    if(reportedPercent<0||canvas.width()<=0||canvas.height()<=0||
       viewport.width()<=20||viewport.height()<=20)return false;
    const qreal scale=qMin(qreal(viewport.width()-20)/canvas.width(),
                           qreal(viewport.height()-20)/canvas.height());
    return int(qFloor(100*scale))==reportedPercent;
}
QPolygonF previewPolygon(const QPolygonF &points,const QSize &canvas,const QSize &viewport,int inset){
    if(canvas.width()<=0||canvas.height()<=0||viewport.width()<=inset*2||viewport.height()<=inset*2)return {};
    const qreal scale=qMin(qreal(viewport.width()-inset*2)/canvas.width(),
                           qreal(viewport.height()-inset*2)/canvas.height());
    const QPointF origin((viewport.width()-canvas.width()*scale)/2.,(viewport.height()-canvas.height()*scale)/2.);
    QPolygonF mapped;for(const auto &p:points)mapped<<origin+p*scale;
    return mapped;
}
QPolygonF previewQuad(const QRectF &box,const QSize &canvas,const QSize &viewport){
    return previewPolygon({box.topLeft(),box.topRight(),box.bottomRight(),box.bottomLeft()},canvas,viewport);
}
TransformVisual::TransformVisual(QWidget *parent):QWidget(parent){
    setObjectName("aftermarket-transform-visual");
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    hide();
}
void TransformVisual::useTransientSurface(){
    // OBSQTDisplay renders into its own X11 native graphics window.
    // A child QWidget (even a native one) is layered behind that surface.
    // A transparent, input-free transient tool window stays in the same
    // window group, while native OBS preview events go straight through.
    setWindowFlags(Qt::Tool|Qt::FramelessWindowHint|
                   Qt::WindowDoesNotAcceptFocus|Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_ShowWithoutActivating);
}
void TransformVisual::setShapes(const QVector<QPolygonF> &quads){
    if(shapes==quads)return;
    shapes=quads;
    update();
}
void TransformVisual::paintEvent(QPaintEvent *){
    if(shapes.isEmpty())return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing,true);
    QColor accent=palette().color(QPalette::Link);
    if(!accent.isValid())accent=palette().color(QPalette::Highlight);
    QColor surface=palette().color(QPalette::Window);
    QColor halo=surface.lightness()<128?QColor(4,8,17,160):QColor(250,250,250,180);
    for(const auto &quad:shapes){
        if(quad.size()!=4)continue;
        QPainterPath path;path.moveTo(quad.at(0));
        for(int i=1;i<4;++i)path.lineTo(quad.at(i));
        path.closeSubpath();
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(halo,3.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.drawPath(path);
        p.setPen(QPen(accent,1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.drawPath(path);
        // Eight transform points, same locations as OBS's native hit testing.
        // Larger drawn handles never steal or fake mouse input.
        for(int i=0;i<8;++i){
            QPointF pos;
            if(i<4)pos=quad.at(i);
            else pos=(quad.at(i-4)+quad.at((i-3)%4))/2.;
            const bool corner=i<4;
            const qreal rad=corner?6.4:5.1;
            const QRectF handle(pos.x()-rad,pos.y()-rad,rad*2,rad*2);
            const bool hovered=QLineF(mapFromGlobal(QCursor::pos()),pos).length()<=12;
            const bool dragging=hovered&&(QApplication::mouseButtons()&Qt::LeftButton);
            QColor fill=surface;
            if(hovered){
                fill=accent;
                fill.setAlpha(dragging?235:160);
            }
            p.setPen(QPen(halo,3.1));p.setBrush(fill);p.drawEllipse(handle);
            p.setPen(QPen(accent,dragging?2.6:2.0));p.setBrush(fill);p.drawEllipse(handle);
        }
    }
}
}
