#pragma once
#include <QtGlobal>
#include <QEasingCurve>
#include <QJsonObject>
#include <QString>
#include <cmath>

namespace augment {
// Retarget from the currently sampled value rather than restarting from an endpoint.
struct MotionTrack {
    double from=0, to=0;
    qint64 start=0;
    int duration=0;
    void reset(double v) {from=to=v;start=0;duration=0;}
    double value(qint64 now) const {
        if (!duration || now>=start+duration) return to;
        const auto t=qBound(0., double(now-start)/duration,1.);
        return from+(to-from)*QEasingCurve(QEasingCurve::OutCubic).valueForProgress(t);
    }
    bool running(qint64 now) const {return duration>0 && now<start+duration && from!=to;}
    void retarget(double target,qint64 now,int ms) {
        if(target==to)return;
        from=value(now);to=target;start=now;duration=ms;
    }
};
struct MotionSpec {
    QString preset="soft";
    int duration=160;
    bool enabled() const {return preset!="none";}
    static bool parse(const QJsonValue &value,MotionSpec &out) {
        out=MotionSpec{};
        if(value.isUndefined())return true;
        if(!value.isObject())return false;
        const auto o=value.toObject();
        if(o.contains("preset")) {
            if(!o.value("preset").isString())return false;
            out.preset=o.value("preset").toString();
            if(out.preset!="none" && out.preset!="soft" && out.preset!="pop" && out.preset!="spin")return false;
        }
        if(o.contains("duration")) {
            auto d=o.value("duration").toDouble(-1);
            if(!std::isfinite(d) || d<60 || d>600 || std::floor(d)!=d)return false;
            out.duration=int(d);
        }
        return true;
    }
};
}
