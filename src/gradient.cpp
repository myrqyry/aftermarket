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
#include <QMetaProperty>
#include <QVariant>
#include <QAbstractButton>
#include <QMouseEvent>
#include <cmath>
namespace augment {
bool GradientSpec::parse(const QJsonValue &value,GradientSpec &out) {
    GradientSpec next;
    if(value.isUndefined()) {out=next;return true;}
    if(!value.isObject())return false;
    const auto o=value.toObject();
    for(auto k:{"enabled","sliders","progress","audioReactive","meters","knobs"})
        if(o.contains(k)&&!o.value(k).isBool())return false;
    next.enabled=o.value("enabled").toBool(false);
    next.sliders=o.value("sliders").toBool(true);
    next.progress=o.value("progress").toBool(true);
    next.audioReactive=o.value("audioReactive").toBool(false);
    next.meters=o.value("meters").toBool(false);
    next.knobs=o.value("knobs").toBool(false);
    if(o.contains("knobSources")) {
        const auto names=o.value("knobSources");
        if(!names.isArray()||names.toArray().size()>256)return false;
        for(const auto &name:names.toArray()) {
            if(!name.isString()||name.toString().isEmpty())return false;
            next.knobSources.append(name.toString());
        }
    }
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
bool mixerWidget(const QWidget *w) {
    for(auto *p=w;p;p=p->parentWidget())if(p->inherits("VolumeControl"))return true;
    return false;
}
bool eligibleMeter(const QWidget *w,const GradientSpec &spec) {
    if(!spec.enabled||!spec.audioReactive||!spec.meters||!w||!w->inherits("VolumeMeter")||!mixerWidget(w))return false;
    const int index=w->metaObject()->indexOfProperty("foregroundNominalColor");
    return index>=0 && w->metaObject()->property(index).isWritable() &&
        w->metaObject()->property(index).metaType()==QMetaType::fromType<QColor>();
}
bool knobTarget(const QWidget *w,const GradientSpec &spec) {
    if(!spec.knobs||!qobject_cast<const QSlider*>(w))return false;
    for(auto *p=w;p;p=p->parentWidget()) {
        if(!p->inherits("VolumeControl"))continue;
        if(spec.knobSources.isEmpty())return true;
        for(auto *button:p->findChildren<QAbstractButton*>())
            if(button->inherits("VolumeName"))return spec.knobSources.contains(button->text());
        return false;
    }
    return false;
}
bool eligible(const QWidget *w,const GradientSpec &spec) {
    if((!spec.enabled&&!spec.knobs)||!w)return false;
    for(auto *p=w;p;p=p->parentWidget())
        if(p->objectName().startsWith("obs-ui-augment-"))return false;
    return knobTarget(w,spec)||(spec.enabled&&((spec.sliders&&qobject_cast<const QSlider*>(w))||(spec.progress&&qobject_cast<const QProgressBar*>(w))));
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
    float energy=0;
    qint64 lastFrame=-1;
    bool knob=false,dragging=false;
    QSize originalMinimum,appliedMinimum;
    QPointF start;
    int startValue=0;
    int paintedPosition=0,paintedMin=0,paintedMax=0;
    bool paintedInverted=false;
    void finishDrag() {
        if(!dragging)return;
        dragging=false;
        if(auto *s=qobject_cast<QSlider*>(parentWidget()))s->setSliderDown(false);
    }
    void setKnob(bool enabled) {
        if(knob==enabled)return;
        auto *s=qobject_cast<QSlider*>(parentWidget());
        if(!s)return;
        if(enabled) {
            originalMinimum=s->minimumSize();appliedMinimum=originalMinimum.expandedTo(QSize(
                qMin(44,s->maximumWidth()),qMin(44,s->maximumHeight())));
            knob=true;
            s->setProperty("_obsUiAugmentKnob",true);
            s->setMinimumSize(appliedMinimum);
            appliedMinimum=s->minimumSize();
        } else {
            finishDrag();knob=false;
            s->setProperty("_obsUiAugmentKnob",QVariant());
            // Do not undo newer constraints written by a theme/layout owner.
            QSize current=s->minimumSize();
            if(current.width()==appliedMinimum.width())current.setWidth(originalMinimum.width());
            if(current.height()==appliedMinimum.height())current.setHeight(originalMinimum.height());
            s->setMinimumSize(current);
        }
        s->updateGeometry();s->update();update();
    }
    QRectF knobRect() const {
        const qreal side=qMin(48,qMin(width(),height()))-6;
        if(side<8)return {};
        return QRectF((width()-side)/2.,(height()-side)/2.,side,side);
    }
    bool handleMouse(QSlider *s,QEvent *event) {
        if(event->type()==QEvent::UngrabMouse||event->type()==QEvent::WindowDeactivate) {finishDrag();return false;}
        auto *e=static_cast<QMouseEvent*>(event);
        if(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseButtonDblClick) {
            if(e->button()!=Qt::LeftButton&&e->button()!=Qt::MiddleButton)return false;
            if(!s->isEnabled()||!knobRect().adjusted(-3,-3,3,3).contains(e->position()))return true;
            if(dragging)return true;
            dragging=true;start=e->globalPosition();startValue=s->sliderPosition();
            if(s->focusPolicy()!=Qt::NoFocus)s->setFocus(Qt::MouseFocusReason);
            s->setSliderDown(true);e->accept();return true;
        }
        if(event->type()==QEvent::MouseMove) {
            if(dragging) {
                if(!(e->buttons()&(Qt::LeftButton|Qt::MiddleButton))) {finishDrag();return true;}
                const auto delta=e->globalPosition()-start;
                double movement=(delta.x()-delta.y())/150.;
                if(s->invertedControls())movement=-movement;
                const double span=double(s->maximum())-s->minimum();
                s->setSliderPosition(int(std::round(qBound(double(s->minimum()),startValue+movement*span,double(s->maximum())))));
            }
            e->accept();return true;
        }
        if(event->type()==QEvent::MouseButtonRelease&&(e->button()==Qt::LeftButton||e->button()==Qt::MiddleButton)) {
            finishDrag();e->accept();return true;
        }
        return false;
    }
    void paintKnob(QPainter &p,QVector<QColor> colors) {
        auto *s=qobject_cast<QSlider*>(parentWidget());if(!s)return;
        const auto r=knobRect();if(!r.isValid())return;
        const auto pal=s->palette();const auto center=r.center();
        const auto group=s->isEnabled()?QPalette::Active:QPalette::Disabled;
        const double span=double(s->maximum())-s->minimum();
        const auto fraction=span>0?qBound(0.,(s->sliderPosition()-double(s->minimum()))/span,1.):0.;
        const auto f=s->invertedAppearance()?1.-fraction:fraction;
        paintedPosition=s->sliderPosition();paintedMin=s->minimum();paintedMax=s->maximum();paintedInverted=s->invertedAppearance();
        p.setOpacity(s->isEnabled()?1.:.48);
        const QRectF arc=r.adjusted(2,2,-2,-2);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(pal.color(group,QPalette::Mid),3.,Qt::SolidLine,Qt::RoundCap));
        p.drawArc(arc,225*16,-270*16);
        QConicalGradient g(center,225+phase*360);
        if(spec.enabled&&spec.sliders) {
            if(colors.first()!=colors.last())colors.append(colors.first());
            for(int i=0;i<colors.size();++i)g.setColorAt(double(i)/(colors.size()-1),colors.at(i));
        } else {auto c=pal.color(group,QPalette::Highlight);g.setColorAt(0,c);g.setColorAt(1,c);}
        p.setPen(QPen(QBrush(g),3.5,Qt::SolidLine,Qt::RoundCap));
        if(f>0)p.drawArc(arc,225*16,int(-270*16*f));
        const auto face=r.adjusted(8,8,-8,-8);
        if(face.isValid()) {
            p.setPen(QPen(pal.color(group,QPalette::Mid),1));
            p.setBrush(pal.color(group,QPalette::Button));p.drawEllipse(face);
            const auto rad=(225-270*f)*3.141592653589793/180.;
            const QPointF direction(std::cos(rad),-std::sin(rad));
            p.setPen(QPen(pal.color(group,QPalette::ButtonText),2.,Qt::SolidLine,Qt::RoundCap));
            p.drawLine(center+direction*(face.width()*.13),center+direction*(face.width()*.37));
        }
        if(s->hasFocus()) {
            p.setBrush(Qt::NoBrush);p.setPen(QPen(pal.color(group,QPalette::Highlight),1,Qt::DotLine));
            p.drawEllipse(r.adjusted(-2,-2,2,2));
        }
    }
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
        if(energy>0)for(auto &color:colors)color=color.lighter(100+int(energy*25));
        if(knob){paintKnob(painter,colors);return;}
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
    struct Meter {QColor base,last;float energy=0;};
    QHash<QWidget*,Meter> meters;
    AudioProvider provider;
    AudioReset reset;
    qint64 lastFrame=0;
    QTimer timer;
    QElapsedTimer elapsed;
};
GradientController::GradientController(QObject *parent):QObject(parent),state(new State) {
    state->elapsed.start();
    state->timer.setInterval(33); // 30 Hz, shared by every visible animated track
    state->timer.setObjectName("augment-gradient-clock");
    connect(&state->timer,&QTimer::timeout,this,[this]{
        const auto keys=state->layers.keys();
        const auto now=state->elapsed.elapsed();
        const double dt=std::clamp(double(now-state->lastFrame)/1000.,0.,.25);
        state->lastFrame=now;
        for(auto *w:keys)sync(w,true);
        const auto meters=state->meters.keys();
        for(auto *w:meters)syncMeter(w,dt);
        refreshClock();
    });
    qApp->installEventFilter(this);
}
GradientController::~GradientController() {
    if(qApp)qApp->removeEventFilter(this);
    state->timer.stop();
    for(auto *w:state->meters.keys())removeMeter(w);
    if(state->reset)state->reset();
    const auto keys=state->layers.keys();
    for(auto *w:keys) {
        QObject::disconnect(w,nullptr,this,nullptr);
        if(auto layer=state->layers.take(w)){layer->setKnob(false);delete layer.data();}
    }
}
void GradientController::setAudioProvider(AudioProvider provider,AudioReset reset) {
    if(state->reset)state->reset();
    state->provider=std::move(provider);state->reset=std::move(reset);
    if(!state->provider)
        for(auto *w:state->meters.keys())removeMeter(w);
    if(state->motion&&state->spec.enabled&&state->spec.audioReactive&&state->spec.meters)
        for(auto *w:QApplication::allWidgets())if(eligibleMeter(w,state->spec))reconcileMeter(w);
    refreshClock();
}
void GradientController::configure(const GradientSpec &spec,bool motion) {
    const auto &old=state->spec;
    if(old.enabled==spec.enabled&&old.sliders==spec.sliders&&old.progress==spec.progress&&
       old.period==spec.period&&old.colors==spec.colors&&old.audioReactive==spec.audioReactive&&old.meters==spec.meters&&old.knobs==spec.knobs&&old.knobSources==spec.knobSources&&state->motion==motion)return;
    state->spec=spec;state->motion=motion;
    if(!motion||!spec.enabled||!spec.audioReactive)if(state->reset)state->reset();
    for(auto *w:state->meters.keys())if(!motion||!eligibleMeter(w,spec))removeMeter(w);
    if(motion&&spec.enabled&&spec.audioReactive&&spec.meters&&state->provider)
        for(auto *w:QApplication::allWidgets())if(eligibleMeter(w,spec))reconcileMeter(w);
    const auto keys=state->layers.keys();
    for(auto *w:keys)reconcile(w);
    if(spec.enabled||spec.knobs)for(auto *w:QApplication::allWidgets())
        if(eligible(w,spec))reconcile(w);
    refreshClock();
}
void GradientController::reconcile(QWidget *w) {
    if(!eligible(w,state->spec)) {
        if(state->layers.contains(w)) {
            QObject::disconnect(w,nullptr,this,nullptr);
            if(auto layer=state->layers.take(w)){layer->setKnob(false);delete layer.data();}
        }
        return;
    }
    if(!state->layers.contains(w)) {
        auto *layer=new Layer(w);
        state->layers.insert(w,layer);
        connect(w,&QObject::destroyed,this,[this,w]{state->layers.remove(w);refreshClock();});
        if(auto *s=qobject_cast<QSlider*>(w)) {
            connect(s,&QSlider::valueChanged,this,[this,w]{sync(w);if(auto l=state->layers.value(w);l&&l->knob)l->update();refreshClock();});
            connect(s,&QSlider::sliderMoved,this,[this,w]{sync(w);if(auto l=state->layers.value(w);l&&l->knob)l->update();refreshClock();});
            connect(s,&QSlider::rangeChanged,this,[this,w]{sync(w);refreshClock();});
        }
        if(auto *p=qobject_cast<QProgressBar*>(w))
            connect(p,&QProgressBar::valueChanged,this,[this,w]{sync(w);refreshClock();});
    }
    if(auto layer=state->layers.value(w)) {layer->spec=state->spec;layer->setKnob(knobTarget(w,state->spec));layer->update();}
    sync(w);
}
void GradientController::sync(QWidget *w,bool frame) {
    auto l=state->layers.value(w);
    if(!l)return;
    const auto fill=l->knob?w->rect():gradientFill(w);
    if(!w->isVisible()||!fill.isValid()) {if(l->isVisible())l->hide();return;}
    const bool changed=l->geometry()!=fill;
    if(changed)l->setGeometry(fill);
    const bool newlyVisible=!l->isVisible();
    if(newlyVisible){l->raise();l->show();}
    // Only the shared clock advances the phase. A parent paint must not
    // schedule another animated paint (which would create a busy loop).
    double phase=l->phase;
    float energy=l->energy;
    if(!state->motion||!state->spec.enabled) {phase=0;energy=0;l->lastFrame=-1;}
    else if(frame||newlyVisible) {
        const auto now=state->elapsed.elapsed();
        const double dt=l->lastFrame<0||newlyVisible?0.:std::clamp(double(now-l->lastFrame)/1000.,0.,.25);
        l->lastFrame=now;
        const bool reactive=state->spec.audioReactive&&state->provider&&mixerWidget(w)&&qobject_cast<QSlider*>(w);
        const auto level=reactive?state->provider(w):std::optional<float>{};
        if(reactive) {
            energy=smoothAudio(energy,level.value_or(0),dt);
            phase=std::fmod(phase+dt*3000.*energy/state->spec.period,1.);
        } else {
            energy=0;
            phase=double(now%state->spec.period)/state->spec.period;
        }
    }
    if(changed||l->phase!=phase||l->energy!=energy){l->phase=phase;l->energy=energy;l->update();}
}
void GradientController::reconcileMeter(QWidget *w) {
    if(!state->provider||!state->motion||!eligibleMeter(w,state->spec)||state->meters.contains(w))return;
    const auto base=w->property("foregroundNominalColor").value<QColor>();
    if(!base.isValid())return;
    state->meters.insert(w,{base,base,0});
    connect(w,&QObject::destroyed,this,[this,w]{state->meters.remove(w);refreshClock();});
}
void GradientController::removeMeter(QWidget *w) {
    if(!state->meters.contains(w))return;
    const auto meter=state->meters.take(w);
    if(w->property("foregroundNominalColor").value<QColor>()==meter.last && meter.last!=meter.base) {
        w->setProperty("foregroundNominalColor",meter.base);w->update();
    }
    QObject::disconnect(w,nullptr,this,nullptr);
}
void GradientController::syncMeter(QWidget *w,double dt) {
    auto it=state->meters.find(w);
    if(it==state->meters.end())return;
    auto &m=it.value();
    auto current=w->property("foregroundNominalColor").value<QColor>();
    if(!current.isValid())return;
    // An external theme or accessibility change owns the new base.
    if(current!=m.last)m.base=current;
    const auto level=w->isVisible()&&w->isEnabled()&&state->provider?state->provider(w):std::optional<float>{};
    m.energy=level?smoothAudio(m.energy,*level,dt):0;
    QColor next=m.energy==0 ? m.base : m.base.lighter(100+int(m.energy*45)).toRgb();
    if(next!=current){w->setProperty("foregroundNominalColor",next);w->update();}
    m.last=next;
}
void GradientController::refreshClock() {
    bool active=false;
    if(state->spec.enabled&&state->motion)
        for(const auto &layer:state->layers)if(layer&&layer->isVisible()){active=true;break;}
    if(!active&&state->motion&&state->spec.enabled&&state->spec.audioReactive)
        for(auto *w:state->meters.keys())if(w->isVisible()){active=true;break;}
    if(active&&!state->timer.isActive()){state->lastFrame=state->elapsed.elapsed();state->timer.start();}
    else if(!active)state->timer.stop();
}
bool GradientController::eventFilter(QObject *o,QEvent *e) {
    if(!state->spec.enabled&&!state->spec.knobs)return false;
    switch(e->type()) {
    case QEvent::Show:case QEvent::Hide:case QEvent::Resize:case QEvent::EnabledChange:
    case QEvent::LayoutDirectionChange:case QEvent::StyleChange:case QEvent::PaletteChange:case QEvent::Paint:
    case QEvent::MouseButtonPress:case QEvent::MouseButtonDblClick:case QEvent::MouseMove:case QEvent::MouseButtonRelease:
    case QEvent::UngrabMouse:case QEvent::WindowDeactivate:case QEvent::FocusIn:case QEvent::FocusOut:break;
    default:return false;
    }
    auto *w=qobject_cast<QWidget*>(o);
    if(eligibleMeter(w,state->spec)&&state->motion&&state->provider) {
        if(e->type()==QEvent::Show) {
            QPointer<QWidget> safe(w);
            QTimer::singleShot(0,this,[this,safe]{if(safe){reconcileMeter(safe);refreshClock();}});
        } else if(e->type()==QEvent::Hide) {removeMeter(w);refreshClock();}
        return false;
    }
    auto layer=state->layers.value(w);
    if(layer&&layer->knob) {
        if(auto *slider=qobject_cast<QSlider*>(w)) {
            switch(e->type()) {
            case QEvent::MouseButtonPress:case QEvent::MouseButtonDblClick:case QEvent::MouseMove:case QEvent::MouseButtonRelease:
            case QEvent::UngrabMouse:case QEvent::WindowDeactivate:return layer->handleMouse(slider,e);
            case QEvent::FocusIn:case QEvent::FocusOut:layer->update();return false;
            case QEvent::Hide:case QEvent::EnabledChange:if(!w->isVisible()||!w->isEnabled())layer->finishDrag();layer->update();break;
            default:break;
            }
        }
    }
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
            if(e->type()==QEvent::PaletteChange||e->type()==QEvent::StyleChange||e->type()==QEvent::EnabledChange)
                if(auto l=state->layers.value(w))l->update();
            refreshClock();
        }
        break;
    default:break;
    }
    // The child paints only the rotary presentation; OBS's original QSlider
    // remains the actual control. Suppress its native groove and thumb.
    if(e->type()==QEvent::Paint&&layer&&layer->knob&&layer->isVisible()) {
        if(auto *s=qobject_cast<QSlider*>(w))
            if(layer->paintedPosition!=s->sliderPosition()||layer->paintedMin!=s->minimum()||
                layer->paintedMax!=s->maximum()||layer->paintedInverted!=s->invertedAppearance())layer->update();
        return true;
    }
    return false;
}
}
