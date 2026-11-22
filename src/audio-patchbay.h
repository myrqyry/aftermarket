#pragma once
#include <QWidget>
#include <QVector>
#include <QString>
#include <cstdint>
#include <functional>
class QLabel;
class QComboBox;
class QToolButton;
class QCheckBox;
class QTimer;
namespace augment {
struct RouteSource {
    QString id,name;
    uint32_t tracks=0;
    int monitor=0;
    // Read-only libobs source health; not audio level or inferred signal flow.
    bool muted=false;
    bool enabled=true;
    bool audioActive=true;
    bool operator==(const RouteSource &o) const {
        return id==o.id&&name==o.name&&tracks==o.tracks&&monitor==o.monitor
            &&muted==o.muted&&enabled==o.enabled&&audioActive==o.audioActive;
    }
};
class AudioPatchbay final : public QWidget {
public:
    using Query=std::function<QVector<RouteSource>()>;
    using ToggleTrack=std::function<bool(const QString &,int)>;
    using ChangeMonitoring=std::function<bool(const QString &,int)>;
    using SetTrack=std::function<bool(const QString &,int,bool)>;
    AudioPatchbay(Query query,ToggleTrack toggle,ChangeMonitoring monitor,
                  QWidget *parent=nullptr,SetTrack setTrack={});
    void refresh();
    int routeCount() const;
    QString selectedId() const;
    bool selectSource(const QString &id);
    bool toggleTrack(int track);
    bool connectRoute(const QString &id,int track);
    bool connectMonitor(const QString &id);
    bool toggleMonitorOutput();
    bool setMonitoring(int mode);
    bool showAllWires() const;
    int visibleCableCount() const;
private:
    class Canvas;
    Query query;
    ToggleTrack toggle;
    ChangeMonitoring monitor;
    SetTrack setTrack;
    QVector<RouteSource> routes;
    QString selected;
    Canvas *canvas=nullptr;
    QLabel *status=nullptr,*diagnostics=nullptr;
    QComboBox *sources=nullptr,*monitoring=nullptr;
    QCheckBox *allWires=nullptr;
    QToolButton *tracks[6]={};
    QToolButton *monitorOutput=nullptr;
    QTimer *timer=nullptr;
    void syncControls();
};
}
