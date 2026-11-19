#pragma once
#include <QObject>
#include <QColor>
#include <QRect>
#include <QVector>
#include <QJsonObject>
#include <memory>
class QWidget;
namespace augment {
struct GradientSpec {
    bool enabled=false, sliders=true, progress=true;
    int period=1800;
    QVector<QColor> colors;
    static bool parse(const QJsonValue &,GradientSpec &);
};
QRect gradientFill(const QWidget *);
class GradientController final: public QObject {
public:
    explicit GradientController(QObject *parent=nullptr);
    ~GradientController() override;
    void configure(const GradientSpec &,bool motion);
protected:
    bool eventFilter(QObject *,QEvent *) override;
private:
    struct State;
    std::unique_ptr<State> state;
    void reconcile(QWidget *);
    void sync(QWidget *,bool frame=false);
    void refreshClock();
};
}
