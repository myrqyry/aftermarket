#pragma once
#include <QWidget>
#include <QPolygonF>
#include <QRectF>
#include <QVector>

namespace augment {
// Maps scene coordinates into OBS's fitted preview viewport. This is a
// visual-only projection. The native OBS preview owns every mouse hit test.
// Guard against guessed transform positions at custom OBS zoom levels.
bool matchesFittedPreview(const QSize &canvas,const QSize &viewport,int reportedPercent);
QPolygonF previewQuad(const QRectF &box,const QSize &canvas,const QSize &viewport);
QPolygonF previewPolygon(const QPolygonF &points,const QSize &canvas,const QSize &viewport,int inset=0);
class TransformVisual final : public QWidget {
public:
    explicit TransformVisual(QWidget *parent);
    void setShapes(const QVector<QPolygonF> &quads);
    void useTransientSurface();
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QVector<QPolygonF> shapes;
};
}
