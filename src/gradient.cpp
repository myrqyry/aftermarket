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
#include <QCursor>
#include <QtMath>
#include <cmath>
namespace augment {
bool GradientSpec::parse(const QJsonValue &value,GradientSpec &out) {
    GradientSpec next;
    if(value.isUndefined()) {out=next;return true;}
    if(!value.isObject())return false;
    const auto o=value.toObject();
    for(auto k:{"enabled","sliders","progress","audioReactive","meters","knobs","knobMeters"})
        if(o.contains(k)&&!o.value(k).isBool())return false;
    next.enabled=o.value("enabled").toBool(false);
    next.sliders=o.value("sliders").toBool(true);
    next.progress=o.value("progress").toBool(true);
    next.audioReactive=o.value("audioReactive").toBool(false);
    next.meters=o.value("meters").toBool(false);
    next.knobs=o.value("knobs").toBool(false);
    next.knobMeters=o.value("knobMeters").toBool(true);
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
        if(p->objectName()=="aftermarket-overlay"||p->objectName()=="aftermarket-gradient"||
           p->objectName()=="aftermarket-inspector"||p->objectName()=="aftermarket-panels")return false;
    return knobTarget(w,spec)||(spec.enabled&&((spec.sliders&&qobject_cast<const QSlider*>(w))||(spec.progress&&qobject_cast<const QProgressBar*>(w))));
}
}
namespace {
QStyleOptionSlider sliderOption(const QSlider *s) {
    QStyleOptionSlider opt;opt.initFrom(s);
    opt.orientation=s->orientation();opt.minimum=s->minimum();opt.maximum=s->maximum();
    opt.sliderPosition=s->sliderPosition();opt.sliderValue=s->value();
    opt.singleStep=s->singleStep();opt.pageStep=s->pageStep();opt.tickPosition=s->tickPosition();opt.tickInterval=s->tickInterval();
    const bool h=horizontal(s);
    if(h)opt.state|=QStyle::State_Horizontal;else opt.state&=~QStyle::State_Horizontal;
    opt.upsideDown=h?(s->invertedAppearance()!=(s->layoutDirection()==Qt::RightToLeft)):!s->invertedAppearance();
    opt.direction=Qt::LeftToRight;
    opt.subControls=QStyle::SC_All;
    if(s->isSliderDown()) {opt.state|=QStyle::State_Sunken;opt.activeSubControls=QStyle::SC_SliderHandle;}
    else if(s->underMouse())opt.activeSubControls=s->style()->hitTestComplexControl(QStyle::CC_Slider,&opt,s->mapFromGlobal(QCursor::pos()),s);
    return opt;
}
struct SliderTrack {
    QRect groove,handle,fill;
};
struct ThumbPage {
    QPainterPath exposed;
    QImage edge;
};
ThumbPage exposedThumbPage(const QSlider *s,const QStyleOptionSlider &opt,const QRect &patch,const QRect &handle) {
    ThumbPage page;
    if(!patch.isValid())return page;
    const qreal dpr=s->devicePixelRatioF();
    const QSize size(qCeil(patch.width()*dpr),qCeil(patch.height()*dpr));
    auto render=[&](const QStyleOptionSlider &o){
        QImage image(size,QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(dpr);image.fill(Qt::transparent);
        QPainter painter(&image);painter.translate(-patch.topLeft());
        s->style()->drawComplexControl(QStyle::CC_Slider,&o,&painter,s);
        return image;
    };
    // The stylesheet backend draws sub/add pages even for SC_SliderHandle.
    // Compare the current native rendering to the same page with the handle
    // moved beyond this patch, preserving native handle pixels (including AA).
    auto reference=opt;
    reference.sliderPosition=reference.sliderValue=opt.maximum;
    auto far=s->style()->subControlRect(QStyle::CC_Slider,&reference,QStyle::SC_SliderHandle,s);
    if(far.intersects(patch)) {
        const bool h=opt.orientation==Qt::Horizontal;
        const int extra=2*(h?handle.width():handle.height())+4;
        if(h) {
            if(opt.upsideDown)reference.rect.adjust(-extra,0,0,0);
            else reference.rect.adjust(0,0,extra,0);
        } else {
            if(opt.upsideDown)reference.rect.adjust(0,-extra,0,0);
            else reference.rect.adjust(0,0,0,extra);
        }
        far=s->style()->subControlRect(QStyle::CC_Slider,&reference,QStyle::SC_SliderHandle,s);
    }
    if(far.intersects(patch))return page; // safe fallback: protect entire handle
    const auto native=render(opt),base=render(reference);
    page.edge=QImage(size,QImage::Format_ARGB32_Premultiplied);
    page.edge.setDevicePixelRatio(dpr);page.edge.fill(Qt::transparent);
    const QPoint center(qBound(0,qFloor((handle.center().x()-patch.x()+.5)*dpr),size.width()-1),
                        qBound(0,qFloor((handle.center().y()-patch.y()+.5)*dpr),size.height()-1));
    const QColor face=native.pixelColor(center);
    // A solid native handle's antialias pixels are a mixture of its face
    // and the old page. Recover coverage only when the observed RGB fits
    // that blend. More complex/shaded handles keep their original pixels.
    auto coverage=[&](QRgb a,QRgb b)->int {
        if(qAlpha(a)!=255||qAlpha(b)!=255||face.alpha()!=255)return -1;
        const double dr=face.red()-qRed(b),dg=face.green()-qGreen(b),db=face.blue()-qBlue(b);
        const double norm=dr*dr+dg*dg+db*db;
        if(norm<1600)return -1;
        const double t=((qRed(a)-qRed(b))*dr+(qGreen(a)-qGreen(b))*dg+(qBlue(a)-qBlue(b))*db)/norm;
        if(t<=0||t>=.995)return -1;
        if(std::abs(qRed(a)-qRed(b)-t*dr)>2||std::abs(qGreen(a)-qGreen(b)-t*dg)>2||std::abs(qBlue(a)-qBlue(b)-t*db)>2)return -1;
        return qBound(1,qRound(t*255),254);
    };
    for(int y=0;y<size.height();++y) {
        const auto *a=reinterpret_cast<const QRgb*>(native.constScanLine(y));
        const auto *b=reinterpret_cast<const QRgb*>(base.constScanLine(y));
        for(int x=0;x<size.width();++x) {
            if(a[x]!=b[x]) {
                const int alpha=coverage(a[x],b[x]);
                if(alpha<0)continue;
                QColor edge=face;edge.setAlpha(alpha);page.edge.setPixelColor(x,y,edge);
            }
            page.exposed.addRect(QRectF(patch.x()+x/dpr,patch.y()+y/dpr,1/dpr,1/dpr));
        }
    }
    return page;
}

SliderTrack sliderTrack(const QSlider *s) {
    if(!s||!s->isEnabled()||s->maximum()<=s->minimum()||s->sliderPosition()<=s->minimum())return {};
    const auto opt=sliderOption(s);
    SliderTrack track;
    track.groove=s->style()->subControlRect(QStyle::CC_Slider,&opt,QStyle::SC_SliderGroove,s).intersected(s->rect());
    track.handle=s->style()->subControlRect(QStyle::CC_Slider,&opt,QStyle::SC_SliderHandle,s);
    if(!track.groove.isValid()||!track.handle.isValid())return {};
    track.fill=track.groove;
    if(horizontal(s)) {
        if(opt.upsideDown)track.fill.setLeft(qMax(track.groove.left(),track.handle.center().x()));
        else track.fill.setRight(qMin(track.groove.right(),track.handle.center().x()));
    } else {
        if(opt.upsideDown)track.fill.setTop(qMax(track.groove.top(),track.handle.center().y()));
        else track.fill.setBottom(qMin(track.groove.bottom(),track.handle.center().y()));
    }
    return track;
}
}
QRect gradientFill(const QWidget *w) {
    if(!w||!w->isEnabled())return {};
    if(auto *s=qobject_cast<const QSlider*>(w))return sliderTrack(s).fill;
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
    QRect maskHandle,maskPatch,maskWidget;
    QStyle::State maskState;
    QStyle::SubControls maskActive;
    const QStyle *maskStyle=nullptr;
    qint64 maskPalette=0;
    qreal maskDpr=0;
    ThumbPage maskPage;
    bool maskReverse=false;
    Qt::Orientation maskOrientation=Qt::Horizontal;
    void invalidateThumb(){maskStyle=nullptr;}
    const ThumbPage &pageUnderThumb(QSlider *s,const QStyleOptionSlider &opt,const QRect &patch,const QRect &handle) {
        if(maskStyle!=s->style()||maskHandle!=handle||maskPatch!=patch||maskWidget!=s->rect()||
           maskOrientation!=opt.orientation||maskReverse!=opt.upsideDown||maskState!=opt.state||maskActive!=opt.activeSubControls||maskPalette!=s->palette().cacheKey()||maskDpr!=s->devicePixelRatioF()) {
            maskHandle=handle;maskPatch=patch;maskWidget=s->rect();maskState=opt.state;maskOrientation=opt.orientation;maskReverse=opt.upsideDown;maskActive=opt.activeSubControls;
            maskStyle=s->style();maskPalette=s->palette().cacheKey();maskDpr=s->devicePixelRatioF();
            maskPage=exposedThumbPage(s,opt,patch,handle);
        }
        return maskPage;
    }
    QPointer<QWidget> nativeMeter;
    bool integrated=false;
    QSize meterMin,meterMax,appliedMeterMin,appliedMeterMax;
    float peakDb=-60,holdDb=-60;
    qint64 holdUntil=0,clipUntil=0,previousMeterFrame=-1;
    bool meterMuted=false;
    void restoreMeter() {
        if(!integrated)return;
        integrated=false;
        if(auto *s=qobject_cast<QSlider*>(parentWidget())) {
            s->setProperty("_obsUiAugmentIntegratedMeter",QVariant());
            if(s->maximumSize()==appliedMeterMax)s->setMaximumSize(meterMax);
            if(s->minimumSize()==appliedMeterMin)s->setMinimumSize(meterMin);
            s->updateGeometry();
        }
        if(nativeMeter){nativeMeter->show();nativeMeter.clear();}
        update();
    }
    void updateMeter(const std::optional<MeterReading> &reading,qint64 now) {
        auto *s=qobject_cast<QSlider*>(parentWidget());
        if(!s||!knob||!spec.knobMeters||!reading) {restoreMeter();return;}
        if(integrated&&(!nativeMeter||nativeMeter->parentWidget()!=s->parentWidget()))restoreMeter();
        if(!integrated) {
            // Scope the replacement to the native meter paired with this slider.
            auto *frame=s->parentWidget();
            if(!frame||frame->objectName()!="volMeterFrame")return;
            for(auto *widget:frame->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly))
                if(widget->inherits("VolumeMeter")&&!widget->isHidden()){nativeMeter=widget;break;}
            if(!nativeMeter)return;
            meterMin=s->minimumSize();meterMax=s->maximumSize();
            integrated=true;
            s->setProperty("_obsUiAugmentIntegratedMeter",true);
            const QSize requested(qMin(68,meterMax.width()),qMin(68,meterMax.height()));
            s->setMinimumSize(meterMin.expandedTo(requested));
            s->setMaximumHeight(qMax(s->minimumHeight(),qMin(76,meterMax.height())));
            appliedMeterMin=s->minimumSize();appliedMeterMax=s->maximumSize();
            nativeMeter->hide();s->updateGeometry();
            peakDb=holdDb=-60;holdUntil=clipUntil=0;previousMeterFrame=-1;
            update();
        }
        // OBS reuses and repolishes its slider when the mixer orientation or
        // theme changes. Qt can reapply the native short-slider minimum after
        // integration. Preserve that new native constraint for eventual restore,
        // then reassert the rotary presentation's minimum on only those axes.
        const auto currentMin=s->minimumSize(),currentMax=s->maximumSize();
        if(currentMax!=appliedMeterMax)meterMax=currentMax;
        if(currentMin!=appliedMeterMin) {
            if(currentMin.width()!=appliedMeterMin.width()) {
                originalMinimum.setWidth(currentMin.width());
                appliedMinimum.setWidth(qMin(qMax(44,currentMin.width()),currentMax.width()));
                meterMin.setWidth(appliedMinimum.width());
            }
            if(currentMin.height()!=appliedMeterMin.height()) {
                originalMinimum.setHeight(currentMin.height());
                appliedMinimum.setHeight(qMin(qMax(44,currentMin.height()),currentMax.height()));
                meterMin.setHeight(appliedMinimum.height());
            }
        }
        const QSize requested(qMin(68,meterMax.width()),qMin(68,meterMax.height()));
        const QSize desired=meterMin.expandedTo(requested).boundedTo(meterMax);
        const int desiredMax=qMax(desired.height(),qMin(76,meterMax.height()));
        if(s->minimumSize()!=desired||s->maximumHeight()!=desiredMax) {
            s->setMinimumSize(desired);
            s->setMaximumHeight(desiredMax);
            appliedMeterMin=s->minimumSize();appliedMeterMax=s->maximumSize();
            s->updateGeometry();
        }
        const float target=reading->muted||!std::isfinite(reading->peakDb)?-60.f:std::clamp(reading->peakDb,-60.f,0.f);
        const double dt=previousMeterFrame<0?0.:std::clamp((now-previousMeterFrame)/1000.,0.,.25);
        previousMeterFrame=now;
        const auto oldPeak=peakDb,oldHold=holdDb;
        const bool oldMuted=meterMuted;const auto oldClip=clipUntil;
        meterMuted=reading->muted;
        peakDb=meterMuted?-60.f:std::max(target,peakDb-float(30*dt));
        if(meterMuted){holdDb=-60;holdUntil=clipUntil=0;}
        else {
            if(target>=holdDb||now>=holdUntil){holdDb=target;holdUntil=now+1000;}
            if(reading->peakDb>=0)clipUntil=now+1000;
            else if(now>=clipUntil)clipUntil=0;
        }
        if(oldPeak!=peakDb||oldHold!=holdDb||oldMuted!=meterMuted||oldClip!=clipUntil)update();
    }
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
            restoreMeter();finishDrag();knob=false;
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
        const qreal side=qMin(integrated?68:48,qMin(width(),height()))-6;
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
        if(integrated) {
            const QRectF meterArc=r.adjusted(2,2,-2,-2);
            auto color=[&](const char *key,const QColor &fallback){
                auto value=nativeMeter?nativeMeter->property(key).value<QColor>():QColor{};
                return value.isValid()?value:fallback;
            };
            const auto nominal=color("foregroundNominalColor",pal.color(group,QPalette::Highlight));
            const auto warning=color("foregroundWarningColor",QColor("#e9b44c"));
            const auto error=color("foregroundErrorColor",QColor("#e25858"));
            const auto pos=[](float db){return qBound(0.,(db+60.)/60.,1.);};
            auto section=[&](double from,double to,QColor c){
                if(to<=from)return;
                p.setPen(QPen(c,4.5,Qt::SolidLine,Qt::FlatCap));
                p.drawArc(meterArc,int((225-270*from)*16),int(-270*(to-from)*16));
            };
            // A meter needs a visible scale even with no signal. Theme Mid can
            // be almost identical to the dock background (e.g. Solarized Dark).
            // Tint the inactive zones with the actual native foreground colors;
            // keep enough headroom between idle and active brightness.
            const QColor surface=pal.color(group,QPalette::Window);
            auto dim=[&](QColor c){
                return QColor::fromRgbF(surface.redF()*.58+c.redF()*.42,
                    surface.greenF()*.58+c.greenF()*.42,
                    surface.blueF()*.58+c.blueF()*.42);
            };
            const double gap=.009;
            section(0,pos(-20)-gap,dim(nominal));
            section(pos(-20)+gap,pos(-9)-gap,dim(warning));
            section(pos(-9)+gap,1,dim(error));
            const double level=pos(peakDb);
            section(0,qMin(level,pos(-20)-gap),nominal);
            section(pos(-20)+gap,qMin(level,pos(-9)-gap),warning);
            section(pos(-9)+gap,level,error);
            if(clipUntil) section(0,1,error);
            if(holdDb>-60) {
                const double mark=pos(holdDb);
                section(qMax(0.,mark-.016),qMin(1.,mark+.016),
                    holdDb>=-9?error:holdDb>=-20?warning:nominal);
            }
            // The inner arc continues to indicate gain, not signal level.
        }
        const QRectF arc=r.adjusted(integrated?10:2,integrated?10:2,integrated?-10:-2,integrated?-10:-2);
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
        const auto face=r.adjusted(integrated?16:8,integrated?16:8,integrated?-16:-8,integrated?-16:-8);
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
        setObjectName("aftermarket-gradient");
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
        if(auto *s=qobject_cast<QSlider*>(parentWidget())) {
            const auto track=sliderTrack(s);
            if(!track.fill.isValid())return;
            painter.save();
            painter.translate(-pos());
            // Keep the native slider painting intact. Cover the page right
            // up to the actual painted thumb silhouette, not its bounding box.
            QPainterPath clip;
            const auto patch=track.fill.intersected(track.handle);
            QRegion outside(track.fill);outside-=QRegion(track.handle);
            for(const auto &r:outside)clip.addRect(QRectF(r));
            const auto &thumbPage=pageUnderThumb(s,sliderOption(s),patch,track.handle);
            clip.addPath(thumbPage.exposed);
            painter.setClipPath(clip);
            QPainterPath path;
            const auto radius=qMin(track.groove.width(),track.groove.height())/2.;
            path.addRoundedRect(QRectF(track.groove),radius,radius);
            gradient.setStart(gradient.start()+pos());gradient.setFinalStop(gradient.finalStop()+pos());
            painter.fillPath(path,gradient);
            if(!thumbPage.edge.isNull()) {
                painter.setClipPath(path,Qt::IntersectClip);
                painter.drawImage(patch.topLeft(),thumbPage.edge);
            }
            painter.restore();
            return;
        }
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
    MeterProvider meterProvider;
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
void GradientController::setMeterProvider(MeterProvider provider) {
    state->meterProvider=std::move(provider);
    for(auto *w:state->layers.keys())sync(w,true);
    refreshClock();
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
       old.period==spec.period&&old.colors==spec.colors&&old.audioReactive==spec.audioReactive&&old.meters==spec.meters&&old.knobs==spec.knobs&&old.knobMeters==spec.knobMeters&&old.knobSources==spec.knobSources&&state->motion==motion)return;
    state->spec=spec;state->motion=motion;
    if((!motion||!spec.enabled||!spec.audioReactive)&&!(spec.knobs&&spec.knobMeters))if(state->reset)state->reset();
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
    if(!state->meterProvider||!l->knob||!state->spec.knobMeters)l->restoreMeter();
    auto fill=l->knob?w->rect():gradientFill(w);

    if(!w->isVisible()||!fill.isValid()) {if(l->isVisible())l->hide();return;}
    if(frame||!state->meterProvider||!l->knob||!state->spec.knobMeters) {
        const auto reading=l->knob&&state->spec.knobMeters&&state->meterProvider?state->meterProvider(w):std::optional<MeterReading>{};
        l->updateMeter(reading,state->elapsed.elapsed());
        if(l->knob)fill=w->rect();
    }
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
    if(!state->provider||!state->motion||!eligibleMeter(w,state->spec)||!w->isVisible()||state->meters.contains(w))return;
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
    if(!active&&state->spec.knobs&&state->spec.knobMeters&&state->meterProvider)
        for(const auto &layer:state->layers)if(layer&&layer->knob&&layer->isVisible()){active=true;break;}
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
                if(auto l=state->layers.value(w)){l->invalidateThumb();l->update();}
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
