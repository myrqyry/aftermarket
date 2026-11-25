#pragma once
#include <QPointer>
#include <QString>
#include <QLabel>
#include <QEvent>
class QComboBox;

namespace augment {
void populateTooltipWidths(QComboBox *,int selected);
// Native Qt tooltip label adapter. No replacement window, no modification of
// the source widget's tooltip, no additional timers.
class TooltipWidth {
public:
    ~TooltipWidth(){restore();}
    void configure(int width);
    void handle(QObject *,QEvent *);
private:
    void restore();
    void fit(QLabel *,bool force);
    QPointer<QLabel> label_;
    QString text_;
    QSize fittedSize_;
    int fittedCap_=0;
    bool fittedWrap_=false;
    int width_=0,originalMax_=QWIDGETSIZE_MAX;
    bool originalWrap_=false,busy_=false;
};
}
