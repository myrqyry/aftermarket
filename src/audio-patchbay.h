#pragma once
#include <QWidget>
#include <QVector>
#include <QHash>
#include <QString>
#include <QStringList>
#include <cstdint>
#include <functional>
class QLabel;
class QMenu;
class QTimer;
namespace augment {
struct SignalReading {
    bool known=false;
    bool active=false;
    bool operator==(const SignalReading &o)const{return known==o.known&&active==o.active;}
};
struct RouteSource {
    QString id,name;
    uint32_t tracks=0;
    int monitor=0;
    bool muted=false;
    bool enabled=true;
    bool audioActive=true;
    QString sourceType; // OBS registered source ID, never inferred from its display name
    bool signalKnown=false;
    bool signalActive=false;
    bool operator==(const RouteSource &o) const {
        return id==o.id&&name==o.name&&tracks==o.tracks&&monitor==o.monitor
            &&muted==o.muted&&enabled==o.enabled&&audioActive==o.audioActive
            &&sourceType==o.sourceType
            &&signalKnown==o.signalKnown&&signalActive==o.signalActive;
    }
};
class AudioPatchbay final : public QWidget {
public:
    enum class SourceIcon { Browser, Media, Microphone, Speaker, Waveform };
    enum class ActivityState { Unknown, Silent, Muted, Active };
    static ActivityState activityForSource(const RouteSource &source);
    static SourceIcon iconForType(const QString &type);
    using Query=std::function<QVector<RouteSource>()>;
    using ToggleTrack=std::function<bool(const QString &,int)>;
    using ChangeMonitoring=std::function<bool(const QString &,int)>;
    using SetTrack=std::function<bool(const QString &,int,bool)>;
    using SignalProvider=std::function<QHash<QString,SignalReading>(const QVector<RouteSource>&)>;
    AudioPatchbay(Query query,ToggleTrack toggle,ChangeMonitoring monitor,
                  QWidget *parent=nullptr,SetTrack setTrack={},SignalProvider levels={});
    using SourceOrderProvider=std::function<QStringList()>;
    void setSourceOrderProvider(SourceOrderProvider provider);
    void refresh();
    void refreshLevels();
    int routeCount() const;
    QString selectedId() const;
    bool selectSource(const QString &id);
    bool toggleTrack(int track);
    bool connectRoute(const QString &id,int track);
    bool connectMonitor(const QString &id);
    bool toggleMonitorOutput();
    bool setMonitoring(int mode);
    bool showAllWires() const;
    void setShowAllWires(bool enabled);
    // Append native, theme-aware routing actions to the existing dock menu.
    // Action state is read afresh each time the title menu is opened.
    void populateRoutingMenu(QMenu *menu);
    int visibleCableCount() const;
    bool sourceSignal(const QString &id) const;
    bool outputSignal(int track) const; // 1–6 native track; 7 OBS Monitor
    ActivityState sourceActivity(const QString &id) const;
    ActivityState outputActivity(int target) const;
protected:
    void showEvent(QShowEvent *event) override;
private:
    class Canvas;
    Query query;
    ToggleTrack toggle;
    ChangeMonitoring monitor;
    SetTrack setTrack;
    SignalProvider levels;
    SourceOrderProvider sourceOrder;
    QVector<RouteSource> routes;
    QString selected;
    Canvas *canvas=nullptr;
    QMenu *monitorMenu=nullptr;
    QTimer *timer=nullptr,*signalTimer=nullptr;
    void syncControls();
    void syncMonitorMenu();
    void notify(const QString &message,bool failed=false);
};
}
