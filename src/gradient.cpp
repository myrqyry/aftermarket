#include "gradient.h"
#include <QApplication>
#include <QSlider>
#include <QProgressBar>
#include <QStyleOption>
#include <QStyle>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QHash>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QEvent>
#include <cmath>
namespace augment {
bool GradientSpec::parse(const QJsonValue &value,GradientSpec &out) {
    GradientSpec next;
    if(value.isUndefined()) {out=next;return true;}
    if(!value.isObject())return false;
    const auto o=value.toObject();
    for(auto k:{"enabled","sliders","progress"})
        if(o.contains(k)&&!o.value(k).isBool())return false;
    next.enabled=o.value("enabled").toBool(false);
    next.sliders=o.value("sliders").toBool(true);
    next.progress=o.value("progress").toBool(true);
    if(o.contains("period")) {
        auto d=o.value("period").toDouble(-1);
        if(!std::isfinite(d)||d<400||d>12000||std::floor(d)!=d)return false;
        next.period=int(d);
    }
    if(o.contains("colors")) {
        auto v=o.value("colors");
        if(!v.isArray()||v.toArray().size()==1||v.toArray().size()>6)return false;
        for(const auto &c:v.toArray()) {
            if(!c.isString())return false;
            QColor color(c.toString());if(!color.isValid()||color.alpha()!=255)return false;
            next.colors.push_back(color);
        }
    }
    out=next;return true;
}
namespace {
bool horizontal(const QWidget *w) {
    if(auto *s=qobject_cast<const QSlider*>(w))return s->orientation()==Qt::Horizontal;
    if(auto *p=qobject_cast<const QProgressBar*>(w))return p->orientation()==Qt::Horizontal;
    return true;
}
bool eligible(const QWidget *w,const GradientSpec &spec) {
    if(!spec.enabled||!w)return false;
    for(auto *p=w;p;p=p->parentWidget())
        if(p->objectName().startsWith("obs-ui-augment-"))return false;
    return (spec.sliders&&qobject_cast<const QSlider*>(w))||(spec.progress&&qobject_cast<const QProgressBar*>(w));
}
}
QRect gradientFill(const QWidget *w) {
    if(!w||!w->isEnabled())return {};
    if(auto *s=qobject_cast<const QSlider*>(w)) {
        if(s->maximum()<=s->minimum()||s->sliderPosition()<=s->minimum())return {};
        QStyleOptionSlider opt;opt.initFrom(s);
        opt.orientation=s->orientation();opt.minimum=s->minimum();opt.maximum=s->maximum();
        opt.sliderPosition=s->sliderPosition();opt.sliderValue=s->value();
        opt.singleStep=s->singleStep();opt.pageStep=s->pageStep();opt.tickPosition=s->tickPosition();opt.tickInterval=s->tickInterval();
        const bool h=horizontal(s);
        if(h)opt.state|=QStyle::State_Horizontal;else opt.state&=~QStyle::State_Horizontal;
        opt.upsideDown=h ? (s->invertedAppearance()!=(s->layoutDirection()==Qt::RightToLeft)) : !s->invertedAppearance();
        opt.direction=Qt::LeftToRight;
        opt.subControls=QStyle::SC_All;
        auto groove=s->style()->subControlRect(QStyle::CC_Slider,&opt,QStyle::SC_SliderGroove,s).intersected(s->rect());
        const auto handle=s->style()->subControlRect(QStyle::CC_Slider,&opt,QStyle::SC_SliderHandle,s);
        if(!groove.isValid()||!handle.isValid())return {};
        // Preserve the native thumb and leave a pixel of breathing room.
        if(h) {
            if(opt.upsideDown)groove.setLeft(qMax(groove.left(),handle.right()+1));
            else groove.setRight(qMin(groove.right(),handle.left()-1));
        } else {
            if(opt.upsideDown)groove.setTop(qMax(groove.top(),handle.bottom()+1));
            else groove.setBottom(qMin(groove.bottom(),handle.top()-1));
        }
        return groove.isValid()?groove:QRect{};
    }
    if(auto *p=qobject_cast<const QProgressBar*>(w)) {
        if(p->maximum()<=p->minimum()||p->value()<=p->minimum())return {};
        const bool h=horizontal(p);
        QStyleOptionProgressBar opt;opt.initFrom(p);
        if(h)opt.state|=QStyle::State_Horizontal;else opt.state&=~QStyle::State_Horizontal;
        opt.minimum=p->minimum();opt.maximum=p->maximum();opt.progress=p->value();
        opt.text=p->text();opt.textVisible=p->isTextVisible();opt.textAlignment=p->alignment();
        opt.invertedAppearance=p->invertedAppearance();opt.bottomToTop=p->textDirection()==QProgressBar::BottomToTop;
        auto fill=p->style()->subElementRect(QStyle::SE_ProgressBarContents,&opt,p).intersected(p->rect());
        if(!fill.isValid())return {};
        const auto fraction=qBound(0.,(double(p->value())-p->minimum())/(double(p->maximum())-p->minimum()),1.);
        const bool reverse=h ? (p->invertedAppearance()!=(p->layoutDirection()==Qt::RightToLeft)) : !p->invertedAppearance();
        const int length=int((h?fill.width():fill.height())*fraction);
        if(length<=0)return {};
        if(h) {const int right=fill.right();fill.setWidth(length);if(reverse)fill.moveRight(right);}
        else {const int bottom=fill.bottom();fill.setHeight(length);if(reverse)fill.moveBottom(bottom);}
        // Retain native text and borders; a slender accent stripe is safer than
        // redrawing the style's text (which may use split contrasting colors).
        if(p->isTextVisible()) {
            if(h) {if(fill.height()<8)return {};fill.setTop(fill.bottom()-qMin(3,fill.height()/5)+1);}
            else {if(fill.width()<8)return {};fill.setLeft(fill.right()-qMin(3,fill.width()/5)+1);}
        }
        return fill.isValid()?fill:QRect{};
    }
    return {};
}
namespace {
class Layer final:public QWidget {
public:
    GradientSpec spec;
    double phase=0;
    explicit Layer(QWidget *w):QWidget(w) {
        setObjectName("obs-ui-augment-gradient");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        hide();
    }
    void paintEvent(QPaintEvent *) override {
        if(!parentWidget())return;
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        QVector<QColor> colors=spec.colors;
        if(colors.isEmpty()) {
            QColor a=parentWidget()->palette().color(QPalette::Highlight);
            QColor b=a.lighter(155);
            QColor c=parentWidget()->palette().color(QPalette::Link);
            colors={a,b,c,a};
        }
        // A periodic gradient with identical endpoints has no seam when its phase wraps.
        if(colors.first()!=colors.last())colors.append(colors.first());
        const bool h=horizontal(parentWidget());
        const qreal wavelength=qBound(48.,(h?parentWidget()->width():parentWidget()->height())*.8,200.);
        const qreal offset=phase*wavelength;
        QLinearGradient gradient(h?QPointF(-offset,0):QPointF(0,-offset),
                                 h?QPointF(wavelength-offset,0):QPointF(0,wavelength-offset));
        gradient.setSpread(QGradient::RepeatSpread);
        for(int i=0;i<colors.size();++i)gradient.setColorAt(double(i)/(colors.size()-1),colors.at(i));
        QPainterPath path;
        const auto radius=qMin(width(),height())/2.;
        path.addRoundedRect(QRectF(rect()),radius,radius);
        painter.fillPath(path,gradient);
    }
};
}
struct GradientController::State {
    GradientSpec spec;
    bool motion=false;
    QHash<QWidget*,QPointer<Layer>> layers;
    QTimer timer;
    QElapsedTimer elapsed;
};
GradientController::GradientController(QObject *parent):QObject(parent),state(new State) {
    state->elapsed.start();
    state->timer.setInterval(33); // 30 Hz, shared by every visible animated track
    state->timer.setObjectName("augment-gradient-clock");
    connect(&state->timer,&QTimer::timeout,this,[this]{
        const auto keys=state->layers.keys();
        for(auto *w:keys)sync(w,true);
        refreshClock();
    });
    qApp->installEventFilter(this);
}
GradientController::~GradientController() {
    if(qApp)qApp->removeEventFilter(this);
    state->timer.stop();
    const auto keys=state->layers.keys();
    for(auto *w:keys) {
        QObject::disconnect(w,nullptr,this,nullptr);
        if(auto layer=state->layers.take(w))delete layer.data();
    }
}
void GradientController::configure(const GradientSpec &spec,bool motion) {
    const auto &old=state->spec;
    if(old.enabled==spec.enabled&&old.sliders==spec.sliders&&old.progress==spec.progress&&
       old.period==spec.period&&old.colors==spec.colors&&state->motion==motion)return;
    state->spec=spec;state->motion=motion;
    const auto keys=state->layers.keys();
    for(auto *w:keys)reconcile(w);
    if(spec.enabled)for(auto *w:QApplication::allWidgets())
        if(eligible(w,spec))reconcile(w);
    refreshClock();
}
void GradientController::reconcile(QWidget *w) {
    if(!eligible(w,state->spec)) {
        if(state->layers.contains(w)) {
            QObject::disconnect(w,nullptr,this,nullptr);
            if(auto layer=state->layers.take(w))delete layer.data();
        }
        return;
    }
    if(!state->layers.contains(w)) {
        auto *layer=new Layer(w);
        state->layers.insert(w,layer);
        connect(w,&QObject::destroyed,this,[this,w]{state->layers.remove(w);refreshClock();});
        if(auto *s=qobject_cast<QSlider*>(w)) {
            connect(s,&QSlider::valueChanged,this,[this,w]{sync(w);refreshClock();});
            connect(s,&QSlider::sliderMoved,this,[this,w]{sync(w);refreshClock();});
            connect(s,&QSlider::rangeChanged,this,[this,w]{sync(w);refreshClock();});
        }
        if(auto *p=qobject_cast<QProgressBar*>(w))
            connect(p,&QProgressBar::valueChanged,this,[this,w]{sync(w);refreshClock();});
    }
    if(auto layer=state->layers.value(w)) {layer->spec=state->spec;layer->update();}
    sync(w);
}
void GradientController::sync(QWidget *w,bool frame) {
    auto l=state->layers.value(w);
    if(!l)return;
    const auto fill=gradientFill(w);
    if(!w->isVisible()||!fill.isValid()) {if(l->isVisible())l->hide();return;}
    const bool changed=l->geometry()!=fill;
    if(changed)l->setGeometry(fill);
    const bool newlyVisible=!l->isVisible();
    if(newlyVisible){l->raise();l->show();}
    // Only the shared clock advances the phase. A parent paint must not
    // schedule another animated paint (which would create a busy loop).
    const double phase=!state->motion ? 0. : (frame||newlyVisible ?
        double(state->elapsed.elapsed()%state->spec.period)/state->spec.period : l->phase);
    if(changed||l->phase!=phase){l->phase=phase;l->update();}
}
void GradientController::refreshClock() {
    bool active=false;
    if(state->spec.enabled&&state->motion)
        for(const auto &layer:state->layers)if(layer&&layer->isVisible()){active=true;break;}
    if(active&&!state->timer.isActive())state->timer.start();
    else if(!active)state->timer.stop();
}
bool GradientController::eventFilter(QObject *o,QEvent *e) {
    if(!state->spec.enabled)return false;
    switch(e->type()) {
    case QEvent::Show:case QEvent::Hide:case QEvent::Resize:case QEvent::EnabledChange:
    case QEvent::LayoutDirectionChange:case QEvent::StyleChange:case QEvent::PaletteChange:case QEvent::Paint:break;
    default:return false;
    }
    auto *w=qobject_cast<QWidget*>(o);
    if(!eligible(w,state->spec))return false;
    switch(e->type()) {
    case QEvent::Show: {
        QPointer<QWidget> safe(w);
        QTimer::singleShot(0,this,[this,safe]{if(safe){reconcile(safe);refreshClock();}});
        break;
    }
    case QEvent::Hide:
        if(auto l=state->layers.value(w))l->hide();
        refreshClock();
        break;
    case QEvent::Resize:case QEvent::EnabledChange:case QEvent::LayoutDirectionChange:case QEvent::StyleChange:case QEvent::PaletteChange:case QEvent::Paint:
        // Range, text visibility, orientation and upsideDown can change without
        // a dedicated signal. Sync on the native paint; never invalidate an
        // unchanged static layer from this path.
        if(state->layers.contains(w)) {
            sync(w);
            if(e->type()==QEvent::PaletteChange)if(auto l=state->layers.value(w))l->update();
            refreshClock();
        }
        break;
    default:break;
    }
    return false;
}
}
