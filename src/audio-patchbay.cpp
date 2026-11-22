#include "audio-patchbay.h"
#include "visual-polish.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QToolButton>
#include <QToolTip>
#include <QPushButton>
#include <QTimer>
#include <QSignalBlocker>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPalette>
#include <QFontMetrics>
#include <QStringList>
#include <QEvent>
#include <QStyleOptionFocusRect>
#include <QStyle>
#include <algorithm>

namespace augment {
class AudioPatchbay::Canvas final : public QWidget {
    static constexpr int firstY=43;
    static constexpr int monitorTarget=7;
    static constexpr int minStep=18;
    static constexpr int maxStep=32;
    static constexpr int sourceLeft=5;
    static constexpr int sourceWidth=117;
    static constexpr int portX=sourceLeft+sourceWidth+4;
    static constexpr int targetRight=7;
    static constexpr int targetWidth=38;
    int rowStep() const {
        return qBound(minStep,(height()-58)/qMax(6,int(routes.size())),maxStep);
    }
    int nodeHeight() const {return qMin(26,rowStep()-2);}
    int pressedSource=-1;
    QPoint pressPoint,dragPoint;
    bool dragging=false;
    QString dragId;
    int hoveredTarget=-1;
    QString hoveredSource;

    // Hover is a temporary visual query, never an OBS routing mutation.
    QString focusedSource() const {
        return hoveredSource.isEmpty()?selected:hoveredSource;
    }
    bool hasConnection(const RouteSource &src,int target) const {
        return target==monitorTarget?src.monitor!=0:
            target>=1&&target<=6&&(src.tracks&(1u<<(target-1)));
    }
    bool visibleWire(const RouteSource &src,int target) const {
        if(hoveredTarget>0&&!dragging)
            return target==hoveredTarget&&hasConnection(src,target);
        return (showAll||src.id==focusedSource())&&hasConnection(src,target);
    }
    QRect sourceRect(int i) const {
        return QRect(sourceLeft,firstY+i*rowStep()-nodeHeight()/2,
                     sourceWidth,nodeHeight());
    }
    QRect trackRect(int t) const {
        // Keep the monitor jack pinned in the graph header even when OBS's
        // stock Audio Mixer dock is too short to show all six track rows.
        if(t==monitorTarget)
            return QRect(width()-targetRight-targetWidth,3,targetWidth,23);
        return QRect(width()-targetRight-targetWidth,
                     firstY+(t-1)*rowStep()-nodeHeight()/2,targetWidth,nodeHeight());
    }
    int sourceAt(QPoint pt) const {
        for(int i=0;i<routes.size();++i)
            if(sourceRect(i).contains(pt))return i;
        return -1;
    }
    int trackAt(QPoint pt) const {
        for(int t=1;t<=monitorTarget;++t)
            if(trackRect(t).contains(pt))return t;
        return -1;
    }
    QPainterPath cable(int fromY,int toY) const {
        const qreal endX=width()-targetRight-targetWidth;
        const qreal mid=portX+(endX-portX)/2.0;
        QPainterPath result(QPointF(portX,fromY));
        result.cubicTo(mid,fromY,mid,toY,endX,toY);
        return result;
    }
    void resetDrag(){
        dragging=false;
        pressedSource=-1;
        dragId.clear();
        hoveredTarget=-1;
        hoveredSource.clear();
        unsetCursor();
        update();
    }
public:
    QVector<RouteSource> routes;
    QString selected;
    bool showAll=false;
    static bool sourceLive(const RouteSource &src){
        return src.enabled&&!src.muted&&src.audioActive&&src.signalKnown&&src.signalActive;
    }
    bool outputLive(int target) const {
        if(target<1||target>monitorTarget)return false;
        return std::any_of(routes.begin(),routes.end(),[target](const RouteSource &src){
            if(!sourceLive(src))return false;
            if(target==monitorTarget)return src.monitor!=0;
            return src.monitor!=1&&(src.tracks&(1u<<(target-1)))!=0;
        });
    }
    std::function<void(const QString &)> onSource;
    std::function<void(int)> onTrack;
    std::function<void(const QString &,int)> onConnect;

    explicit Canvas(QWidget *parent=nullptr):QWidget(parent){
        setObjectName("aftermarket-route-canvas");
        setMinimumWidth(240);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setToolTip("Click a source to select; drag it to a numbered track or headphones. Headphones use the native OBS Monitor, not Track 7.");
    }
    bool sourceSignal(const QString &id) const {
        for(const auto &src:routes)if(src.id==id)return sourceLive(src);
        return false;
    }
    bool outputSignal(int target)const{return outputLive(target);}
    int visibleCableCount() const {
        int count=0;
        for(const auto &src:routes)
            for(int target=1;target<=monitorTarget;++target)
                if(visibleWire(src,target))++count;
        return count;
    }
    void setRoutes(const QVector<RouteSource> &value,const QString &current){
        if(routes==value&&selected==current)return;
        if(dragging&&!std::any_of(value.begin(),value.end(),
            [this](const RouteSource &src){return src.id==dragId;}))resetDrag();
        routes=value;
        selected=current;
        if(!hoveredSource.isEmpty()&&!std::any_of(routes.begin(),routes.end(),
            [this](const RouteSource &src){return src.id==hoveredSource;}))
            hoveredSource.clear();
        setMinimumHeight(58+qMax(routes.size(),6)*minStep);
        updateGeometry();
        update();
    }
    void setShowAll(bool enabled){
        if(showAll==enabled)return;
        showAll=enabled;
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto fg=palette().color(QPalette::Text);
        const auto accent=palette().color(QPalette::Highlight);
        const auto muted=palette().color(QPalette::Mid);
        const auto caption=paletteBlend(muted,fg,60); // readable on dark and light themes
        const auto base=palette().color(QPalette::Button);
        const QFont bodyFont=font();
        QFont captionFont=bodyFont;
        captionFont.setWeight(QFont::Normal);
        if(captionFont.pointSizeF()>0)
            captionFont.setPointSizeF(qMax(7.0,captionFont.pointSizeF()-0.75));
        else if(captionFont.pixelSize()>0)
            captionFont.setPixelSize(qMax(9,captionFont.pixelSize()-1));
        p.setFont(captionFont);
        p.setPen(caption);
        p.drawText(QRect(7,3,120,23),Qt::AlignVCenter,"Sources");
        p.drawText(QRect(width()-113,3,64,23),Qt::AlignVCenter|Qt::AlignRight,"Outputs");

        for(int t=1;t<=monitorTarget;++t){
            const auto rect=trackRect(t);
            const auto selectedIt=std::find_if(routes.begin(),routes.end(),
                [this](const RouteSource &src){return src.id==selected;});
            const bool connected=selectedIt!=routes.end()&&hasConnection(*selectedIt,t);
            p.setPen(QPen((hoveredTarget==t||connected)?accent:muted,
                          (hoveredTarget==t||connected)?1.0:0.7));
            p.setBrush(connected?paletteBlend(base,accent,13):base);
            p.drawRoundedRect(rect,5,5);
            p.setFont(bodyFont);
            p.setPen(fg);
            if(t==monitorTarget) {
                const QPointF centre=rect.center();
                p.setPen(QPen(fg,1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                p.setBrush(Qt::NoBrush);
                // Draw headset vectors; avoid font glyph fallbacks and theme-specific icons.
                p.drawArc(QRectF(centre.x()-7,centre.y()-7,14,14),20*16,140*16);
                p.drawRoundedRect(QRectF(centre.x()-8.2,centre.y(),3.7,7),1.2,1.2);
                p.drawRoundedRect(QRectF(centre.x()+4.5,centre.y(),3.7,7),1.2,1.2);
            }else {
                p.drawText(rect.adjusted(0,0,-5,0),Qt::AlignCenter,QString::number(t));
            }
            // Presence of current measured signal, independent of route outline.
            p.setPen(Qt::NoPen);
            p.setBrush(outputLive(t)?accent:muted);
            p.drawEllipse(QPointF(rect.right()-5.2,rect.center().y()),2.1,2.1);
            p.setFont(captionFont);
        }
        // Draw cables behind nodes. The all-wires view is intentionally
        // optional to avoid overwhelming small docks with unrelated routes.
        // In all-wires mode draw other sources first, selected wires last,
        // so the focused route stays legible. Colors still come from Qt.
        for(int pass=0;pass<2;++pass)
            for(int i=0;i<routes.size();++i){
                const auto &src=routes.at(i);
                const bool current=src.id==focusedSource();
                if(current!=(pass==1))continue;
                for(int track=1;track<=monitorTarget;++track){
                    if(!visibleWire(src,track))continue;
                    // Muted/disabled source routes remain wired in OBS; their
                    // dashed strokes indicate status, not disconnected wiring.
                    const bool healthy=src.enabled&&!src.muted;
                    QColor wire=current?accent:fg;
                    wire.setAlpha(current?(healthy?225:155):(healthy?145:90));
                    p.setPen(QPen(wire,current?1.25:0.8,
                                  healthy?Qt::SolidLine:Qt::DashLine,Qt::RoundCap));
                    p.setBrush(Qt::NoBrush);
                    p.drawPath(cable(firstY+i*rowStep(),trackRect(track).center().y()));
                }
            }
        if(dragging){
            QColor preview=accent;
            preview.setAlpha(225);
            p.setPen(QPen(preview,1.25,Qt::DashLine,Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            const int sourceIndex=sourceAt(pressPoint);
            if(sourceIndex>=0){
                const int y=firstY+sourceIndex*rowStep();
                if(hoveredTarget>0)p.drawPath(cable(y,trackRect(hoveredTarget).center().y()));
                else{
                    QPainterPath draft(QPointF(portX,y));
                    draft.cubicTo(portX+28,y,dragPoint.x()-28,dragPoint.y(),dragPoint.x(),dragPoint.y());
                    p.drawPath(draft);
                }
            }
        }
        for(int i=0;i<routes.size();++i){
            const auto &src=routes.at(i);
            const bool current=src.id==selected;
            const auto rect=sourceRect(i);
            const bool focused=src.id==focusedSource();
            p.setPen(QPen(focused?accent:muted,focused?0.95:0.65));
            p.setBrush(current?paletteBlend(base,accent,18):base);
            p.drawRoundedRect(rect,5,5);
            QFont sourceFont=bodyFont;
            sourceFont.setWeight(current?QFont::Medium:QFont::Normal);
            p.setFont(sourceFont);
            p.setPen(fg);
            const QString name=p.fontMetrics().elidedText(src.name,Qt::ElideRight,sourceWidth-27);
            p.drawText(rect.adjusted(15,0,-5,0),Qt::AlignVCenter|Qt::AlignLeft,name);
            // Status dot: bright only when a recent measured audio peak
            // is present. Muted/disabled/unknown sources remain subdued.
            p.setPen(Qt::NoPen);
            p.setBrush(sourceLive(src)?accent:muted);
            p.drawEllipse(QPointF(rect.left()+8,rect.center().y()),2.5,2.5);
            p.setBrush(focused?accent:muted);
            p.drawEllipse(QPointF(portX,firstY+i*rowStep()),2.6,2.6);
            p.setFont(captionFont);
        }
        if(routes.isEmpty()){
            p.setPen(caption);
            p.drawText(QRect(7,35,width()-14,55),Qt::AlignHCenter|Qt::TextWordWrap,
                       "No active OBS audio sources");
        }
    }
    void mousePressEvent(QMouseEvent *event) override {
        if(event->button()!=Qt::LeftButton){
            QWidget::mousePressEvent(event);
            return;
        }
        const QPoint pos=event->position().toPoint();
        pressedSource=sourceAt(pos);
        pressPoint=pos;
        dragPoint=pos;
        dragging=false;
        dragId=(pressedSource>=0)?routes.at(pressedSource).id:QString();
        if(pressedSource<0){
            const int track=trackAt(pos);
            if(track>0&&onTrack)onTrack(track);
        }
        event->accept();
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if(pressedSource>=0&&(event->buttons()&Qt::LeftButton)){
            const QPoint current=event->position().toPoint();
            if(!dragging&&(current-pressPoint).manhattanLength()>=QApplication::startDragDistance())
                dragging=true;
            if(dragging){
                dragPoint=current;
                hoveredTarget=trackAt(current);
                setCursor(Qt::CrossCursor);
                update();
            }
        }else{
            const QPoint point=event->position().toPoint();
            const int target=trackAt(point);
            const int sourceIndex=sourceAt(point);
            const QString sourceId=sourceIndex>=0?routes.at(sourceIndex).id:QString();
            if(target!=hoveredTarget||sourceId!=hoveredSource){
                hoveredTarget=target;
                hoveredSource=sourceId;
                update();
            }
            if(sourceIndex>=0){
                const auto &src=routes.at(sourceIndex);
                QStringList state;
                if(src.muted)state<<"Muted";
                if(!src.enabled)state<<"Disabled";
                if(!src.audioActive)state<<"OBS audio inactive";
                if(src.signalKnown)state<<(sourceLive(src)?"Signal detected":"No recent signal");
                else state<<"Signal not measured";
                setToolTip(src.name+(state.isEmpty()?QString():(" · "+state.join(" · "))));
            }else if(target>0){
                int incoming=0;
                for(const auto &src:routes)if(hasConnection(src,target))++incoming;
                const QString signal=outputLive(target)?"signal detected":"no measured signal";
                if(target==monitorTarget)
                    setToolTip(QStringLiteral("Headphones · native OBS Monitor · %1 connected %2 · %3 · not Track 7")
                        .arg(incoming).arg(incoming==1?"source":"sources",signal));
                else
                    setToolTip(QStringLiteral("Track %1 · %2 connected %3 · %4")
                        .arg(target).arg(incoming).arg(incoming==1?"source":"sources",signal));
            }else{
                setToolTip("Drag a source to an output or headphones; hover to inspect signal paths.");
            }
        }
        event->accept();
    }
    void leaveEvent(QEvent *event) override {
        if(!dragging && (hoveredTarget>0 || !hoveredSource.isEmpty())){
            hoveredTarget=-1;
            hoveredSource.clear();
            update();
        }
        QWidget::leaveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if(event->button()!=Qt::LeftButton){
            QWidget::mouseReleaseEvent(event);return;
        }
        const int target=trackAt(event->position().toPoint());
        const QString origin=dragId;
        const bool wasDragging=dragging;
        const bool clickedSource=pressedSource>=0;
        resetDrag();
        if(wasDragging&&target>0&&!origin.isEmpty()){
            if(onConnect)onConnect(origin,target);
        }else if(!wasDragging&&clickedSource){
            if(onSource)onSource(origin);
        }
        event->accept();
    }
    void keyPressEvent(QKeyEvent *event) override {
        if(event->key()==Qt::Key_Escape&&dragging){
            resetDrag();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }
};
AudioPatchbay::AudioPatchbay(Query q,ToggleTrack t,ChangeMonitoring m,
                             QWidget *parent,SetTrack s,SignalProvider levelProvider)
    :QWidget(parent),query(std::move(q)),toggle(std::move(t)),
     monitor(std::move(m)),setTrack(std::move(s)),levels(std::move(levelProvider)){
    setObjectName("aftermarket-audio-patchbay");
    setWindowTitle("Routing");
    auto *root=new QVBoxLayout(this);
    root->setContentsMargins(4,3,4,3);
    root->setSpacing(3);
    auto *header=new QHBoxLayout;
    header->setSpacing(5);
    auto *heading=new QLabel("Audio routes",this);
    heading->setObjectName("aftermarket-route-heading");
    auto font=heading->font();
    font.setWeight(QFont::DemiBold);
    heading->setFont(font);
    header->addWidget(heading);
    header->addStretch();
    allWires=new QCheckBox("Show all wires",this);
    allWires->setObjectName("aftermarket-show-all-wires");
    allWires->setToolTip("Show connections from every OBS audio source; otherwise show only the selected source.");
    header->addWidget(allWires);
    auto *refreshButton=new QToolButton(this);
    refreshButton->setText("↻");
    refreshButton->setToolTip("Refresh native OBS audio routing");
    refreshButton->setAccessibleName("Refresh audio routes");
    header->addWidget(refreshButton);
    root->addLayout(header);
    auto *scroll=new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    canvas=new Canvas(scroll);
    canvas->onSource=[this](const QString &id){selectSource(id);};
    canvas->onTrack=[this](int track){
        if(track==7)toggleMonitorOutput();
        else toggleTrack(track);
    };
    canvas->onConnect=[this](const QString &id,int track){connectRoute(id,track);};
    scroll->setWidget(canvas);
    root->addWidget(scroll,1);
    auto *monitorRow=new QHBoxLayout;
    monitorRow->setSpacing(3);
    selectedLabel=new QLabel("Source",this);
    selectedLabel->setObjectName("aftermarket-selected-source");
    selectedLabel->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    monitorRow->addWidget(selectedLabel,1);
    monitoring=new QComboBox(this);
    monitoring->setObjectName("aftermarket-monitor");
    monitoring->addItems({"Off","Monitor Only","Monitor and Output"});
    monitoring->setAccessibleName("Native OBS monitoring mode");
    monitoring->setToolTip("Monitor Only sends audio to the OBS Monitoring Device without normal program output. Monitor and Output sends to both. Choose the monitoring device in OBS Settings > Audio > Advanced.");
    monitorRow->addWidget(monitoring,2);
    root->addLayout(monitorRow);
    connect(refreshButton,&QToolButton::clicked,this,[this]{refresh();});
    connect(allWires,&QCheckBox::toggled,this,[this](bool enabled){canvas->setShowAll(enabled);});
    connect(monitoring,&QComboBox::currentIndexChanged,this,[this](int mode){
        if(mode>=0)setMonitoring(mode);
    });
    timer=new QTimer(this);
    timer->setInterval(950);
    connect(timer,&QTimer::timeout,this,[this]{if(isVisible())refresh();});
    timer->start();
    signalTimer=new QTimer(this);
    signalTimer->setInterval(160);
    connect(signalTimer,&QTimer::timeout,this,[this]{
        if(isVisible())refreshLevels();
    });
    signalTimer->start();
    refresh();
}
int AudioPatchbay::routeCount() const{return routes.size();}
QString AudioPatchbay::selectedId() const{return selected;}
bool AudioPatchbay::showAllWires() const{return allWires->isChecked();}
int AudioPatchbay::visibleCableCount() const{return canvas->visibleCableCount();}
void AudioPatchbay::refresh(){
    auto latest=query?query():routes;
    // Audio meter state is refreshed independently; preserve its values
    // across the slower authoritative OBS routing poll. Otherwise every
    // polling tick would unnecessarily reset a user-open monitoring combo.
    for(auto &incoming:latest)
        for(const auto &old:routes)if(old.id==incoming.id){
            incoming.signalKnown=old.signalKnown;
            incoming.signalActive=old.signalActive;
            break;
        }
    if(latest==routes&&!selected.isEmpty())return;
    routes=latest;
    auto exists=[this](const QString &id){
        return std::any_of(routes.begin(),routes.end(),[&](const RouteSource &r){return r.id==id;});
    };
    if(!exists(selected))selected=routes.isEmpty()?QString{}:routes.first().id;
    refreshLevels();
    syncControls();
}
void AudioPatchbay::syncControls(){
    const QSignalBlocker monitorBlock(monitoring);
    int chosen=-1;
    for(int i=0;i<routes.size();++i)if(routes[i].id==selected){chosen=i;break;}
    selectedLabel->setText("Monitor");
    selectedLabel->setToolTip(chosen<0?"Click a source in the graph":
        "OBS monitoring mode for "+routes.at(chosen).name);
    monitoring->setEnabled(chosen>=0);
    monitoring->setCurrentIndex(chosen<0?-1:routes.at(chosen).monitor);
    canvas->setRoutes(routes,selected);
}
void AudioPatchbay::refreshLevels(){
    if(!levels)return;
    const auto measured=levels(routes);
    bool changed=false;
    for(auto &src:routes){
        const auto value=measured.value(src.id);
        if(src.signalKnown!=value.known||src.signalActive!=value.active){
            src.signalKnown=value.known;
            src.signalActive=value.active;
            changed=true;
        }
    }
    if(changed)canvas->setRoutes(routes,selected);
}
bool AudioPatchbay::sourceSignal(const QString &id)const{return canvas->sourceSignal(id);}
bool AudioPatchbay::outputSignal(int track)const{return canvas->outputSignal(track);}
void AudioPatchbay::notify(const QString &message,bool failed){
    canvas->setStatusTip(message);
    canvas->setAccessibleDescription(message);
    if(failed&&isVisible())
        QToolTip::showText(canvas->mapToGlobal(QPoint(12,20)),message,canvas);
}
bool AudioPatchbay::selectSource(const QString &id){
    const auto found=std::find_if(routes.begin(),routes.end(),
        [&](const RouteSource &route){return route.id==id;});
    if(found==routes.end())return false;
    selected=id;
    syncControls();
    return true;
}
bool AudioPatchbay::toggleTrack(int track){
    if(selected.isEmpty()||track<1||track>6||!toggle)return false;
    const bool ok=toggle(selected,track);
    refresh();
    notify(ok?QStringLiteral("Updated track %1 in OBS").arg(track):
                       QStringLiteral("Source unavailable; OBS routing unchanged"));
    return ok;
}
bool AudioPatchbay::connectRoute(const QString &id,int track){
    if(track==7)return connectMonitor(id);
    if(id.isEmpty()||track<1||track>6)return false;
    // Read OBS again immediately before the drop. Repeated drops onto an
    // existing connection are idempotent; they must never disconnect audio.
    const auto current=query?query():routes;
    auto found=std::find_if(current.begin(),current.end(),
        [&](const RouteSource &route){return route.id==id;});
    if(found==current.end()){
        refresh();
        notify("Source unavailable; OBS routing unchanged");
        return false;
    }
    if(found->tracks&(1u<<(track-1))){
        refresh();
        selectSource(id);
        notify(QStringLiteral("Track %1 is already connected").arg(track));
        return true;
    }
    const bool ok=setTrack?setTrack(id,track,true):(toggle&&toggle(id,track));
    refresh();
    if(ok)selectSource(id);
    notify(ok?QStringLiteral("Connected source to OBS track %1").arg(track):
                       QStringLiteral("Source unavailable; OBS routing unchanged"));
    return ok;
}
bool AudioPatchbay::connectMonitor(const QString &id){
    if(id.isEmpty()||!monitor)return false;
    // OBS remains authoritative. A repeated drop must not toggle a
    // Monitor Only source into an output path (or disconnect it).
    const auto current=query?query():routes;
    const auto found=std::find_if(current.begin(),current.end(),
        [&](const RouteSource &route){return route.id==id;});
    if(found==current.end()){
        refresh();
        notify("Source unavailable; OBS monitoring unchanged");
        return false;
    }
    if(found->monitor!=0){
        refresh();
        selectSource(id);
        notify("Source is already connected to native OBS Monitor");
        return true;
    }
    // Monitor and Output preserves normal output tracks. Monitor Only
    // remains a separate explicit option in the selected-source Monitoring control.
    const bool ok=monitor(id,2);
    refresh();
    if(ok)selectSource(id);
    notify(ok?"Connected source to native OBS Monitor and Output":
                        "Source unavailable; OBS monitoring unchanged");
    return ok;
}
bool AudioPatchbay::toggleMonitorOutput(){
    if(selected.isEmpty()||!monitor)return false;
    const auto current=query?query():routes;
    const auto found=std::find_if(current.begin(),current.end(),
        [&](const RouteSource &route){return route.id==selected;});
    if(found==current.end()){
        refresh();
        notify("Source unavailable; OBS monitoring unchanged");
        return false;
    }
    const int mode=found->monitor==0?2:0;
    const bool ok=monitor(selected,mode);
    refresh();
    notify(ok?(mode==0?"Disconnected from native OBS Monitor":
                                  "Connected to native OBS Monitor and Output"):
                        "Source unavailable; OBS monitoring unchanged");
    return ok;
}
bool AudioPatchbay::setMonitoring(int mode){
    if(selected.isEmpty()||mode<0||mode>2||!monitor)return false;
    const bool ok=monitor(selected,mode);
    refresh();
    notify(ok?"Updated native OBS monitoring":"Source unavailable; monitoring unchanged");
    return ok;
}
}
