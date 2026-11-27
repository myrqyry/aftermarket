#include "gradient.h"
#include "rotary-paint.h"
#include <QApplication>
#include <QDockWidget>
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
#include <QContextMenuEvent>
#include <QMenu>
#include <QCursor>
#include <QtMath>
#include <cmath>
#include <QCache>
#include <algorithm>
namespace augment {
QStringList GradientSpec::patterns(){return {"flow","reverse","bounce","breathe","shimmer","drift","aurora","tide","bloom","orbit","nebula","vortex","ripple","static"};}
QString GradientSpec::patternLabel(const QString &id){
    if(id=="reverse")return "Reverse flow";
    if(id=="bounce")return "Ping-pong";
    if(id=="breathe")return "Breathing";
    if(id=="shimmer")return "Shimmer";
    if(id=="drift")return "Drift — soft flow";
    if(id=="aurora")return "Aurora — liquid ribbons";
    if(id=="tide")return "Tide — gentle sway";
    if(id=="bloom")return "Bloom — spreading color";
    if(id=="orbit")return "Orbit — radial lights";
    if(id=="nebula")return "Nebula — fluid mesh";
    if(id=="vortex")return "Vortex — swirling colors";
    if(id=="ripple")return "Ripple — radial waves";
    if(id=="static")return "Static";
    return "Flow";
}
QString GradientSpec::patternDescription(const QString &id){
    if(id=="drift")return "Slowly accelerating and decelerating color bands with soft blends.";
    if(id=="aurora")return "Overlapping traveling waves stretch and compress the color bands.";
    if(id=="tide")return "Colors sway back and forth, gently expanding and contracting.";
    if(id=="bloom")return "Broad color blooms expand, dissolve and return in a seamless cycle.";
    if(id=="orbit")return "Soft radial pools orbit a moving center.";
    if(id=="nebula")return "Multiple independent color clouds merge in a two-dimensional field.";
    if(id=="vortex")return "A rotating spiral twists and blends colors around its center.";
    if(id=="ripple")return "Circular waves expand from a gently moving origin.";
    if(id=="reverse")return "Continuous reverse travel.";
    if(id=="bounce")return "Back-and-forth travel.";
    if(id=="breathe")return "Stationary colors with a brightness pulse.";
    if(id=="shimmer")return "A narrow traveling highlight.";
    if(id=="static")return "Stationary gradient.";
    return "Continuous forward travel.";
}
bool GradientSpec::parse(const QJsonValue &value,GradientSpec &out) {
    GradientSpec next;
    if(value.isUndefined()) {out=next;return true;}
    if(!value.isObject())return false;
    const auto o=value.toObject();
    for(auto k:{"enabled","sliders","progress","audioReactive","meters","knobs","knobMeters","titlebars","titlebarFill"})
        if(o.contains(k)&&!o.value(k).isBool())return false;
    next.titlebars=o.value("titlebars").toBool(false);
    next.titlebarFill=o.value("titlebarFill").toBool(false);
    if(o.contains("pattern")){
        if(!o.value("pattern").isString()||!patterns().contains(o.value("pattern").toString()))return false;
        next.pattern=o.value("pattern").toString();
    }
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
        if(!std::isfinite(d)||d<400||d>GradientSpec::maxPeriod||std::floor(d)!=d)return false;
        next.period=int(d);
    }
    if(o.contains("scale")){
        const auto d=o.value("scale").toDouble(-1);
        if(!std::isfinite(d)||d<minScale||d>maxScale||std::floor(d)!=d)return false;
        next.scale=int(d);
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
    return knobTarget(w,spec)||(spec.enabled&&(((spec.titlebars||spec.titlebarFill)&&qobject_cast<const QDockWidget*>(w))||(spec.sliders&&qobject_cast<const QSlider*>(w))||(spec.progress&&qobject_cast<const QProgressBar*>(w))));
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
namespace {
QRect titleAccent(const QDockWidget *dock){
    if(!dock||dock->isFloating()||dock->titleBarWidget()||!dock->widget()||
       (dock->features()&QDockWidget::DockWidgetVerticalTitleBar))return {};
    const auto body=dock->widget()->geometry();
    const int top=body.top();
    if(top<16||top>48||body.width()<32)return {};
    return QRect(body.left()+2,top-3,body.width()-4,2).intersected(dock->rect());
}
QRect titleBackground(const QDockWidget *dock){
    const auto accent=titleAccent(dock);
    if(!accent.isValid())return {};
    const auto body=dock->widget()->geometry();
    return QRect(body.left()+1,1,body.width()-2,body.top()-3).intersected(dock->rect());
}
double patternOffset(const QString &pattern,double phase){
    if(pattern=="reverse")return -phase;
    if(pattern=="bounce")return .5-.5*std::cos(phase*2*M_PI);
    if(pattern=="breathe"||pattern=="static"||pattern=="drift"||pattern=="aurora"||pattern=="tide"||pattern=="bloom")return 0;
    return phase;
}
// Keep the legacy gradients identical. Organic styles instead deform a
// continuous cyclic palette. All harmonics are integer multiples of a
// full period, so both time and spatial boundaries join without a jump.
}
QGradientStops gradientPatternStops(QVector<QColor> colors,const QString &pattern,double phase){
    if(colors.isEmpty())return {};
    if(colors.size()==1)colors.append(colors.first());
    phase=std::isfinite(phase)?phase-std::floor(phase):0.;
    // Quantize only sub-picosecond floating point wrap error.
    phase=std::round(phase*1e12)/1e12;
    if(phase>=1.)phase=0.;
    QGradientStops stops;
    const bool organic=pattern=="drift"||pattern=="aurora"||pattern=="tide"||pattern=="bloom";
    if(organic){
        if(colors.size()>2&&colors.first()==colors.last())colors.removeLast();
        constexpr double tau=6.28318530717958647692;
        constexpr int segments=48;
        const double t=tau*phase;
        auto sample=[&](double x){
            x-=std::floor(x);
            const double u=x*colors.size();
            const int i=qMin(int(u),int(colors.size())-1);
            double f=u-i;
            f=f*f*(3.-2.*f);
            const auto a=colors[i],b=colors[(i+1)%colors.size()];
            return QColor::fromRgbF(a.redF()+(b.redF()-a.redF())*f,
                                    a.greenF()+(b.greenF()-a.greenF())*f,
                                    a.blueF()+(b.blueF()-a.blueF())*f,
                                    a.alphaF()+(b.alphaF()-a.alphaF())*f);
        };
        stops.reserve(segments+1);
        for(int i=0;i<segments;++i){
            const double x=double(i)/segments;
            const double angle=tau*x;
            double u=x;
            if(pattern=="drift")u+=phase+.075*std::sin(t)+.04*std::sin(angle-t);
            else if(pattern=="aurora")u+=phase+.11*std::sin(angle-t)+.035*std::sin(2*angle+2*t);
            else if(pattern=="tide")u+=.26*std::sin(t)+.085*std::sin(angle)*std::cos(t);
            else u+=.16*std::sin(t)+.13*std::sin(angle-t);
            auto color=sample(u);
            if(pattern=="bloom"){
                const auto second=sample(u+.5);
                const double wave=.5+.5*std::cos(angle-t);
                const double blend=.48*wave*wave;
                color=QColor::fromRgbF(color.redF()+(second.redF()-color.redF())*blend,
                                      color.greenF()+(second.greenF()-color.greenF())*blend,
                                      color.blueF()+(second.blueF()-color.blueF())*blend);
            }
            stops.append({x,color});
        }
        stops.append({1.,stops.first().second});
        return stops;
    }
    if(pattern=="breathe"){
        const double pulse=.5-.5*std::cos(phase*2*M_PI);
        for(auto &c:colors)c=c.lighter(100+qRound(pulse*35));
    }
    if(pattern=="shimmer"){
        const auto base=colors.first();
        const auto glint=colors.at(1).lighter(135);
        return {{0,base},{.36,base},{.5,glint},{.64,base},{1,base}};
    }
    if(colors.first()!=colors.last())colors.append(colors.first());
    for(int i=0;i<colors.size();++i)stops.append({double(i)/(colors.size()-1),colors.at(i)});
    return stops;
}
bool isSpatialGradient(const QString &pattern){
    return pattern=="orbit"||pattern=="nebula"||pattern=="vortex"||pattern=="ripple";
}
QImage gradientFieldImage(const QVector<QColor>& input,const QString &pattern,double phase,int scale){
    if(input.isEmpty()||!isSpatialGradient(pattern))return {};
    auto colors=input;
    if(colors.size()>2&&colors.first()==colors.last())colors.removeLast();
    constexpr double tau=6.28318530717958647692;
    phase=std::isfinite(phase)?phase-std::floor(phase):0.;
    phase=std::round(phase*1e12)/1e12;
    if(phase>=1.)phase=0.;
    const double t=tau*phase;
    const double zoom=100./qBound(GradientSpec::minScale,scale,GradientSpec::maxScale);
    // Small normalized field, stretched smoothly by QPainter. Work is
    // independent of surface size and never samples video or audio frames.
    QImage image(96,48,QImage::Format_RGB32);
    if(std::all_of(colors.cbegin(),colors.cend(),[&](const QColor&c){return c==colors.first();})){
        image.fill(colors.first());return image;
    }
    struct RGB{double r,g,b;};
    QVector<RGB> rgb;rgb.reserve(colors.size());
    for(const auto&c:colors)rgb.append({double(c.red()),double(c.green()),double(c.blue())});
    auto mix=[](RGB a,RGB b,double f){return RGB{a.r+(b.r-a.r)*f,a.g+(b.g-a.g)*f,a.b+(b.b-a.b)*f};};
    auto sample=[&](double u){
        u-=std::floor(u);u*=rgb.size();
        const int i=qMin(int(u),int(rgb.size())-1);
        double f=u-i;f=f*f*(3.-2.*f);
        return mix(rgb[i],rgb[(i+1)%rgb.size()],f);
    };
    const auto pixel=[](RGB c){return qRgb(qBound(0,qRound(c.r),255),qBound(0,qRound(c.g),255),qBound(0,qRound(c.b),255));};
    const bool mesh=pattern=="orbit"||pattern=="nebula";
    const bool orbit=pattern=="orbit";
    struct Pool{double x,y,scale;RGB color;};
    QVector<Pool> pools;
    if(mesh){
        for(int i=0;i<rgb.size();++i){
            const double a=tau*i/rgb.size();
            const double theta=t+a;
            const double x=orbit?.5+.31*std::cos(theta):.5+.33*std::cos(a+t)+.09*std::sin(2*t+a);
            const double y=orbit?.5+.31*std::sin(theta):.5+.30*std::sin(a-2*t)+.07*std::cos(t+2*a);
            const double radius=(orbit?.23:.29)+.055*std::sin(t+a);
            pools.append({x,y,1./(radius*radius),rgb[i]});
        }
    }
    const double cx=.5+.13*std::cos(t),cy=.5+.13*std::sin(t);
    for(int iy=0;iy<image.height();++iy){
        auto *line=reinterpret_cast<QRgb*>(image.scanLine(iy));
        const double y=.5+((iy+.5)/image.height()-.5)*zoom;
        for(int ix=0;ix<image.width();++ix){
            const double x=.5+((ix+.5)/image.width()-.5)*zoom;
            RGB c{};
            if(mesh){
                double total=0;
                for(const auto&p:pools){
                    const double dx=x-p.x,dy=y-p.y;
                    const double z=1.+(dx*dx+dy*dy)*p.scale;
                    const double w=1./(z*z*z);
                    c.r+=p.color.r*w;c.g+=p.color.g*w;c.b+=p.color.b*w;total+=w;
                }
                c={c.r/total,c.g/total,c.b/total};
            }else{
                const double dx=x-cx,dy=y-cy,r=std::hypot(dx,dy);
                if(pattern=="vortex"){
                    const double angle=std::atan2(dy,dx)/tau;
                    const double fade=qMin(1.,r/.18);
                    c=mix(sample(phase),sample(angle+1.1*r-phase),fade*fade*(3.-2.*fade));
                }else{
                    const double wave=.5-.5*std::cos(tau*(1.7*r-phase));
                    c=sample(.12+.72*wave+.15*std::sin(t));
                }
            }
            line[ix]=pixel(c);
        }
    }
    return image;
}
QBrush spatialGradientBrush(const QVector<QColor>& colors,const QString &pattern,double phase,const QRectF &rect,int scale){
    if(rect.isEmpty()||colors.isEmpty())return {};
    // Shared bounded GUI-thread cache. Non-reactive surfaces can reuse
    // the same field; quantization is smaller than a 30 Hz animation step
    // even at the longest supported period.
    static thread_local QCache<QString,QImage> cache(64);
    const double normalized=std::isfinite(phase)?phase-std::floor(phase):0.;
    const int frame=qRound(normalized*4096)%4096;
    scale=qBound(GradientSpec::minScale,scale,GradientSpec::maxScale);
    QString key=pattern+":"+QString::number(frame)+":"+QString::number(scale);
    for(const auto&c:colors)key+=":"+QString::number(c.rgba(),16);
    auto *image=cache.object(key);
    if(!image){
        cache.insert(key,new QImage(gradientFieldImage(colors,pattern,frame/4096.,scale)));
        image=cache.object(key);
    }
    if(!image||image->isNull())return {};
    QBrush brush(*image);
    QTransform transform;transform.translate(rect.x(),rect.y());transform.scale(rect.width()/image->width(),rect.height()/image->height());
    brush.setTransform(transform);return brush;
}
namespace {
void setPatternStops(QGradient &g,const QVector<QColor>&colors,const QString &pattern,double phase){
    g.setStops(gradientPatternStops(colors,pattern,phase));
}
void setScaledRingStops(QConicalGradient &g,const QVector<QColor>&colors,const QString &pattern,double phase,int scale){
    const auto stops=gradientPatternStops(colors,pattern,phase);
    if(scale==100||stops.size()<2){g.setStops(stops);return;}
    auto mix=[](QColor a,QColor b,double f){return QColor::fromRgbF(a.redF()+(b.redF()-a.redF())*f,a.greenF()+(b.greenF()-a.greenF())*f,a.blueF()+(b.blueF()-a.blueF())*f);};
    auto sample=[&](double u){
        u-=std::floor(u);
        for(int i=1;i<stops.size();++i)if(u<=stops[i].first){
            const auto &a=stops[i-1],&b=stops[i];
            return mix(a.second,b.second,(u-a.first)/qMax(1e-9,b.first-a.first));
        }
        return stops.last().second;
    };
    const double cycles=100./scale;
    const int lo=int(cycles),hi=lo+1;
    const double blend=cycles-lo;
    const double shift=patternOffset(pattern,phase);
    // A very broad field on a closed ring tends toward a single color;
    // sample the traveling color rather than freezing a constant hue.
    const auto flat=sample(-shift);
    QGradientStops result;result.reserve(65);
    for(int i=0;i<64;++i){
        const double x=double(i)/64;
        result.append({x,mix(lo==0?flat:sample(x*lo),sample(x*hi),blend)});
    }
    result.append({1.,result.first().second});g.setStops(result);
}

}
QRect gradientFill(const QWidget *w) {
    if(!w||!w->isEnabled())return {};
    if(auto *dock=qobject_cast<const QDockWidget*>(w))return titleAccent(dock);
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
    QSize originalMinimum,originalMaximum,appliedMinimum,appliedMaximum;
    bool adjustingBounds=false;
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
    float peakDb=-60,holdDb=-60;
    qint64 holdUntil=0,clipUntil=0,previousMeterFrame=-1;
    bool meterMuted=false;
    // The native fader may be constrained to a short track by OBS/QSS.
    // Own only the constraints needed for the rotary presentation, and
    // preserve new native bounds written while it is active.
    void syncKnobBounds(bool restoring=false) {
        auto *s=qobject_cast<QSlider*>(parentWidget());
        if(!s||!knob||adjustingBounds)return;
        const auto currentMin=s->minimumSize(),currentMax=s->maximumSize();
        if(currentMin.width()!=appliedMinimum.width())originalMinimum.setWidth(currentMin.width());
        if(currentMin.height()!=appliedMinimum.height())originalMinimum.setHeight(currentMin.height());
        if(currentMax.width()!=appliedMaximum.width())originalMaximum.setWidth(currentMax.width());
        if(currentMax.height()!=appliedMaximum.height())originalMaximum.setHeight(currentMax.height());
        const int floor=integrated?68:44;
        const QSize desiredMin=restoring?originalMinimum:originalMinimum.expandedTo(QSize(floor,floor));
        QSize desiredMax=originalMaximum;
        if(integrated&&!restoring)desiredMax.setHeight(qMin(76,desiredMax.height()));
        desiredMax=desiredMax.expandedTo(desiredMin);
        if(currentMin==desiredMin&&currentMax==desiredMax){appliedMinimum=currentMin;appliedMaximum=currentMax;return;}
        adjustingBounds=true;
        // Qt may clamp an opposing bound when either constraint changes.
        // First relax maxima, then set minima, then tighten maxima.
        s->setMaximumSize(currentMax.expandedTo(desiredMax).expandedTo(desiredMin));
        s->setMinimumSize(desiredMin);
        s->setMaximumSize(desiredMax);
        appliedMinimum=s->minimumSize();appliedMaximum=s->maximumSize();
        adjustingBounds=false;s->updateGeometry();
    }
    void restoreMeter() {
        if(!integrated)return;
        integrated=false;
        if(auto *s=qobject_cast<QSlider*>(parentWidget())){
            s->setProperty("_obsUiAugmentIntegratedMeter",QVariant());
            syncKnobBounds();
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
            integrated=true;
            s->setProperty("_obsUiAugmentIntegratedMeter",true);
            syncKnobBounds();
            nativeMeter->hide();s->updateGeometry();
            peakDb=holdDb=-60;holdUntil=clipUntil=0;previousMeterFrame=-1;
            update();
        }
        syncKnobBounds();
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
            originalMinimum=appliedMinimum=s->minimumSize();
            originalMaximum=appliedMaximum=s->maximumSize();
            knob=true;
            s->setProperty("_obsUiAugmentKnob",true);
            syncKnobBounds();
        } else {
            restoreMeter();finishDrag();
            syncKnobBounds(true);knob=false;
            s->setProperty("_obsUiAugmentKnob",QVariant());
        }
        s->updateGeometry();s->update();update();
    }
    QRectF knobRect() const {
        return rotarySquare(QRectF(rect()),integrated?68:48,3.);
    }
    bool gainTarget(QSlider *s)const{
        return s&&s->parentWidget()&&s->parentWidget()->objectName()=="volMeterFrame";
    }
    bool handleGainMenu(QSlider *s,QContextMenuEvent *event){
        if(!gainTarget(s)||s->contextMenuPolicy()!=Qt::DefaultContextMenu)return false;
        QWidget *channel=s->parentWidget();
        while(channel&&!channel->inherits("VolumeControl"))channel=channel->parentWidget();
        if(!channel||channel->contextMenuPolicy()!=Qt::CustomContextMenu)return false;
        // OBS 32.2 synchronously opens its own channel popup from this
        // signal. Keep that menu and all its original actions intact.
        QPointer<QWidget> previous=QApplication::activePopupWidget();
        const QPoint point=event->reason()==QContextMenuEvent::Keyboard?QPoint{}:channel->mapFromGlobal(event->globalPos());
        QMetaObject::invokeMethod(channel,"customContextMenuRequested",Qt::DirectConnection,Q_ARG(QPoint,point));
        auto *menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if(menu&&menu!=previous&&menu->isVisible()&&!menu->actions().isEmpty()){
            auto *reset=menu->findChild<QAction*>("aftermarket-reset-gain",Qt::FindDirectChildrenOnly);
            if(!reset){
                reset=menu->addAction("Reset gain to default (0 dB)");
                reset->setObjectName("aftermarket-reset-gain");
                QObject::connect(reset,&QAction::triggered,menu,[safe=QPointer<QSlider>(s)]{
                    if(safe&&safe->isEnabled()&&safe->property("_obsUiAugmentKnob").toBool())safe->setValue(safe->maximum());
                });
            }
            reset->setEnabled(s->isEnabled()&&s->value()!=s->maximum());
        }
        event->accept();return true;
    }
    bool handleMouse(QSlider *s,QEvent *event) {
        if(event->type()==QEvent::UngrabMouse||event->type()==QEvent::WindowDeactivate) {finishDrag();return false;}
        auto *e=static_cast<QMouseEvent*>(event);
        if(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseButtonDblClick) {
            if(e->button()!=Qt::LeftButton&&e->button()!=Qt::MiddleButton)return false;
            if(!s->isEnabled()||!knobRect().adjusted(-3,-3,3,3).contains(e->position()))return true;
            // Native mixer gain uses the OBS fader deflection scale:
            // its maximum represents unity (0 dB). Never reset unrelated
            // sliders or adjustment controls to their maximum.
            if(event->type()==QEvent::MouseButtonDblClick&&e->button()==Qt::LeftButton&&
               gainTarget(s)){
                finishDrag();
                s->setValue(s->maximum());
                e->accept();return true;
            }
            if(dragging)return true;
            dragging=true;start=e->globalPosition();startValue=s->sliderPosition();
            if(s->focusPolicy()!=Qt::NoFocus)s->setFocus(Qt::MouseFocusReason);
            s->setSliderDown(true);e->accept();return true;
        }
        if(event->type()==QEvent::MouseMove) {
            if(dragging) {
                if(!(e->buttons()&(Qt::LeftButton|Qt::MiddleButton))) {finishDrag();return true;}
                const auto delta=e->globalPosition()-start;
                double movement=rotaryDragDistance(delta)/150.;
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
                return rotaryMutedAccent(surface,c);
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
        QConicalGradient g(center,225+patternOffset(spec.pattern,phase)*360);
        if(spec.enabled&&spec.sliders) {
            setScaledRingStops(g,colors,spec.pattern,phase,spec.scale);
        } else {auto c=pal.color(group,QPalette::Highlight);g.setColorAt(0,c);g.setColorAt(1,c);}
        const auto face=r.adjusted(integrated?16:8,integrated?16:8,integrated?-16:-8,integrated?-16:-8);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const auto brush=spec.enabled&&spec.sliders&&isSpatialGradient(spec.pattern)?spatialGradientBrush(colors,spec.pattern,phase,arc.adjusted(-2,-2,2,2),spec.scale):QBrush(g);
        paintRotaryFace(p,arc,face,pal,group,brush,225,-270,225,-270*f,225-270*f);
        if(s->hasFocus()) {
            p.setBrush(Qt::NoBrush);p.setPen(QPen(pal.color(group,QPalette::Highlight),1,Qt::DotLine));
            p.drawEllipse(r.adjusted(-2,-2,2,2));
        }
    }
    explicit Layer(QWidget *w):QWidget(w) {
        setObjectName(qobject_cast<QDockWidget*>(w)?"aftermarket-title-gradient":"aftermarket-gradient");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        hide();
    }
    QImage titleMask;
    QString maskTitle;
    QRect titleMaskGeometry;
    QFont titleMaskFont;
    QPalette titleMaskPalette;
    QDockWidget::DockWidgetFeatures titleMaskFeatures;
    const QStyle *titleMaskStyle=nullptr;
    qreal titleMaskDpr=0;
    Qt::LayoutDirection titleMaskDirection=Qt::LeftToRight;
    QStyle::State titleMaskState;
    void paintTitleBackground(QPainter &p,QDockWidget *dock,const QBrush &gradient){
        const qreal dpr=devicePixelRatioF();
        const QSize physical(qCeil(width()*dpr),qCeil(height()*dpr));
        if(physical.isEmpty()||physical.width()>8192||physical.height()>192)return;
        QStyleOptionDockWidget opt;
        opt.initFrom(dock);
        const auto body=dock->widget()->geometry();
        opt.rect=QRect(body.left(),0,body.width(),body.top());
        opt.title=dock->windowTitle();
        opt.closable=dock->features()&QDockWidget::DockWidgetClosable;
        opt.floatable=dock->features()&QDockWidget::DockWidgetFloatable;
        opt.movable=dock->features()&QDockWidget::DockWidgetMovable;
        opt.verticalTitleBar=false;
        if(titleMask.isNull()||maskTitle!=opt.title||titleMaskGeometry!=geometry()||
           titleMaskFont!=dock->font()||titleMaskPalette!=dock->palette()||
           titleMaskFeatures!=dock->features()||titleMaskStyle!=dock->style()||
           titleMaskDpr!=dpr||titleMaskDirection!=opt.direction||titleMaskState!=opt.state){
            maskTitle=opt.title;titleMaskGeometry=geometry();titleMaskFont=dock->font();
            titleMaskPalette=dock->palette();titleMaskFeatures=dock->features();
            titleMaskStyle=dock->style();titleMaskDpr=dpr;titleMaskDirection=opt.direction;titleMaskState=opt.state;
            auto render=[&](const QStyleOptionDockWidget &o){
                QImage image(physical,QImage::Format_ARGB32_Premultiplied);image.setDevicePixelRatio(dpr);image.fill(Qt::transparent);
                QPainter painter(&image);painter.translate(-pos());painter.setFont(dock->font());
                dock->style()->drawControl(QStyle::CE_DockWidgetTitle,&o,&painter,dock);
                return image;
            };
            const auto painted=render(opt);auto blank=opt;blank.title.clear();
            const auto background=render(blank);
            titleMask=QImage(physical,QImage::Format_ARGB32_Premultiplied);titleMask.setDevicePixelRatio(dpr);titleMask.fill(Qt::transparent);
            // The offscreen style render and the backing store can differ
            // in subpixel AA. Leave a physical-pixel guard around ink.
            titleMask.fill(Qt::white);
            for(int y=0;y<physical.height();++y)for(int x=0;x<physical.width();++x){
                if(painted.pixel(x,y)==background.pixel(x,y))continue;
                for(int yy=qMax(0,y-1);yy<=qMin(physical.height()-1,y+1);++yy)
                    for(int xx=qMax(0,x-1);xx<=qMin(physical.width()-1,x+1);++xx)
                        titleMask.setPixel(xx,yy,0);
            }
        }
        // Keep native text pixels untouched, including antialiased edges.
        // A restrained translucent wash keeps the active theme readable.
        QImage tint(physical,QImage::Format_ARGB32_Premultiplied);tint.setDevicePixelRatio(dpr);tint.fill(Qt::transparent);
        {QPainter layer(&tint);layer.setRenderHint(QPainter::SmoothPixmapTransform);layer.setOpacity(.22);layer.fillRect(rect(),gradient);
         layer.setOpacity(1);layer.setCompositionMode(QPainter::CompositionMode_DestinationIn);layer.drawImage(QPoint(0,0),titleMask);}
        p.drawImage(QPoint(0,0),tint);
    }
    void paintEvent(QPaintEvent *) override {
        if(!parentWidget())return;
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);
        QVector<QColor> colors=spec.colors;
        if(colors.isEmpty()) {
            colors=rotaryThemeColors(parentWidget()->palette());
        }
        if(energy>0)for(auto &color:colors)color=color.lighter(100+int(energy*25));
        if(knob){paintKnob(painter,colors);return;}
        // Pattern stops close the periodic palette, including the organic styles.
        const bool h=horizontal(parentWidget());
        const qreal wavelength=qBound(48.,(h?parentWidget()->width():parentWidget()->height())*.8,200.)*spec.scale/100.;
        const qreal offset=patternOffset(spec.pattern,phase)*wavelength;
        QLinearGradient gradient(h?QPointF(-offset,0):QPointF(0,-offset),
                                 h?QPointF(wavelength-offset,0):QPointF(0,wavelength-offset));
        gradient.setSpread(QGradient::RepeatSpread);
        setPatternStops(gradient,colors,spec.pattern,phase);
        const bool spatial=isSpatialGradient(spec.pattern);
        if(auto *dock=qobject_cast<QDockWidget*>(parentWidget());dock&&spec.titlebarFill){
            paintTitleBackground(painter,dock,spatial?spatialGradientBrush(colors,spec.pattern,phase,rect(),spec.scale):QBrush(gradient));return;
        }
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
            painter.fillPath(path,spatial?spatialGradientBrush(colors,spec.pattern,phase,track.groove,spec.scale):QBrush(gradient));
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
        painter.fillPath(path,spatial?spatialGradientBrush(colors,spec.pattern,phase,qobject_cast<QDockWidget*>(parentWidget())?QRectF(rect()):QRectF(parentWidget()->rect().translated(-pos())),spec.scale):QBrush(gradient));
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
       old.period==spec.period&&old.scale==spec.scale&&old.pattern==spec.pattern&&old.titlebars==spec.titlebars&&old.titlebarFill==spec.titlebarFill&&old.colors==spec.colors&&old.audioReactive==spec.audioReactive&&old.meters==spec.meters&&old.knobs==spec.knobs&&old.knobMeters==spec.knobMeters&&old.knobSources==spec.knobSources&&state->motion==motion)return;
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
        if(auto *dock=qobject_cast<QDockWidget*>(w)){
            connect(dock,&QDockWidget::windowTitleChanged,this,[this,w]{if(auto l=state->layers.value(w))l->update();});
            connect(dock,&QDockWidget::featuresChanged,this,[this,w]{sync(w);if(auto l=state->layers.value(w))l->update();refreshClock();});
            connect(dock,&QDockWidget::topLevelChanged,this,[this,w]{
                QPointer<QWidget> safe(w);
                QTimer::singleShot(0,this,[this,safe]{if(safe){sync(safe);refreshClock();}});
            });
        }
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
    if(l->knob)l->syncKnobBounds();
    auto fill=l->knob?w->rect():gradientFill(w);
    const bool fullTitle=state->spec.titlebarFill&&qobject_cast<QDockWidget*>(w);
    if(fullTitle&&w->isEnabled())fill=titleBackground(qobject_cast<QDockWidget*>(w));

    if(!w->isVisible()||!fill.isValid()) {if(l->isVisible())l->hide();return;}
    if(frame||!state->meterProvider||!l->knob||!state->spec.knobMeters) {
        const auto reading=l->knob&&state->spec.knobMeters&&state->meterProvider?state->meterProvider(w):std::optional<MeterReading>{};
        l->updateMeter(reading,state->elapsed.elapsed());
        if(l->knob)fill=w->rect();
    }
    const bool changed=l->geometry()!=fill;
    if(changed)l->setGeometry(fill);
    const bool newlyVisible=!l->isVisible();
    if(newlyVisible){if(fullTitle)l->lower();else l->raise();l->show();}
    if(fullTitle&&changed)l->lower();
    // Only the shared clock advances the phase. A parent paint must not
    // schedule another animated paint (which would create a busy loop).
    double phase=l->phase;
    float energy=l->energy;
    if(!state->motion||!state->spec.enabled||state->spec.pattern=="static") {phase=0;energy=0;l->lastFrame=-1;}
    else if(frame||newlyVisible) {
        // All non-reactive layers sample the same clock tick, enabling
        // reuse of their spatial fields even across a large dock layout.
        const auto now=frame?state->lastFrame:state->elapsed.elapsed();
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
    if(state->spec.enabled&&state->motion&&state->spec.pattern!="static")
        for(const auto &layer:state->layers)if(layer&&layer->isVisible()&&(!layer->knob||state->spec.sliders)){active=true;break;}
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
    case QEvent::LayoutRequest:case QEvent::LayoutDirectionChange:case QEvent::StyleChange:case QEvent::PaletteChange:case QEvent::Paint:
    case QEvent::ContextMenu:
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
            case QEvent::ContextMenu:return layer->handleGainMenu(slider,static_cast<QContextMenuEvent*>(e));
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
    case QEvent::Resize:case QEvent::LayoutRequest:case QEvent::EnabledChange:case QEvent::LayoutDirectionChange:case QEvent::StyleChange:case QEvent::PaletteChange:case QEvent::Paint:
        // Range, text visibility, orientation and upsideDown can change without
        // a dedicated signal. Sync on the native paint; never invalidate an
        // unchanged static layer from this path.
        if(state->layers.contains(w)) {
            sync(w);
            if(e->type()==QEvent::PaletteChange||e->type()==QEvent::StyleChange||e->type()==QEvent::EnabledChange)
                if(auto l=state->layers.value(w)){l->invalidateThumb();l->titleMask=QImage{};l->update();}
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
