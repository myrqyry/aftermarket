#pragma once
#include <QWidget>
#include <QVector>
#include <QString>
#include <cstdint>
#include <functional>
class QLabel;
class QComboBox;
class QToolButton;
class QTimer;
namespace augment {
struct RouteSource {
    QString id,name;
    uint32_t tracks=0;
    int monitor=0;
    bool operator==(const RouteSource &o) const {
        return id==o.id&&name==o.name&&tracks==o.tracks&&monitor==o.monitor;
    }
};
class AudioPatchbay final : public QWidget {
public:
    using Query=std::function<QVector<RouteSource>()>;
    using ToggleTrack=std::function<bool(const QString &,int)>;
    using ChangeMonitoring=std::function<bool(const QString &,int)>;
    AudioPatchbay(Query query,ToggleTrack toggle,ChangeMonitoring monitor,QWidget *parent=nullptr);
    void refresh();
    int routeCount() const;
    QString selectedId() const;
    bool selectSource(const QString &id);
    bool toggleTrack(int track);
    bool setMonitoring(int mode);
private:
    class Canvas;
    Query query;
    ToggleTrack toggle;
    ChangeMonitoring monitor;
    QVector<RouteSource> routes;
    QString selected;
    Canvas *canvas=nullptr;
    QLabel *status=nullptr;
    QComboBox *sources=nullptr,*monitoring=nullptr;
    QToolButton *tracks[6]={};
    QTimer *timer=nullptr;
    void syncControls();
};
}
