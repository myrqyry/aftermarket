#pragma once
#include <QObject>
#include <QColor>
#include <QRect>
#include <QVector>
#include <QJsonObject>
#include <QStringList>
#include <memory>
#include "audio-motion.h"
class QWidget;
namespace augment {
struct GradientSpec {
    bool enabled=false, sliders=true, progress=true, audioReactive=false, meters=false, knobs=false, knobMeters=true, titlebars=false;
    QString pattern="flow";
    static QStringList patterns();
    static QString patternLabel(const QString &);
    QStringList knobSources;
    int period=1800;
    QVector<QColor> colors;
    static bool parse(const QJsonValue &,GradientSpec &);
};
QRect gradientFill(const QWidget *);
class GradientController final: public QObject {
public:
    explicit GradientController(QObject *parent=nullptr);
    ~GradientController() override;
    void setMeterProvider(MeterProvider);
    void setAudioProvider(AudioProvider,AudioReset);
    void configure(const GradientSpec &,bool motion);
protected:
    bool eventFilter(QObject *,QEvent *) override;
private:
    struct State;
    std::unique_ptr<State> state;
    void reconcile(QWidget *);
    void sync(QWidget *,bool frame=false);
    void refreshClock();
    void reconcileMeter(QWidget *);
    void syncMeter(QWidget *,double dt);
    void removeMeter(QWidget *);
};
}
