#include "audio-patchbay.h"
#include "app-text-elision.h"
#include "visual-polish.h"
#include <QApplication>
#include <QMenu>
#include <QActionGroup>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QToolTip>
#include <QPushButton>
#include <QTimer>
#include <QSignalBlocker>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QPalette>
#include <QFontMetrics>
#include <QStringList>
#include <QEvent>
#include <QPointer>
#include <QIcon>
#include <QStyleOptionFocusRect>
#include <QStyle>
#include <algorithm>

namespace augment {
AudioPatchbay::SourceIcon AudioPatchbay::iconForType(const QString &type){
    const QString id=type.toLower();
    if(id.contains("browser"))return SourceIcon::Browser;
    if(id.contains("ffmpeg")||id.contains("vlc")||id.contains("media"))
        return SourceIcon::Media;
    if(id.contains("input_capture")||id.contains("microphone")||id.contains("mic_capture"))
        return SourceIcon::Microphone;
    if(id.contains("output_capture")||id.contains("desktop_audio")||id.contains("speaker"))
        return SourceIcon::Speaker;
    return SourceIcon::Waveform;
}
AudioPatchbay::ActivityState AudioPatchbay::activityForSource(const RouteSource &src){
    if(!src.enabled||src.muted)return ActivityState::Muted;
    if(!src.audioActive||!src.signalKnown)return ActivityState::Unknown;
    return src.signalActive?ActivityState::Active:ActivityState::Silent;
}
class AudioPatchbay::Canvas final : public QWidget {
    static constexpr int firstY=43;
    static constexpr int monitorTarget=7;
    static constexpr int minStep=18;
    static constexpr int maxStep=32;
    static constexpr int sourceLeft=5;
    static constexpr int sourceWidth=124;
    static constexpr int portX=sourceLeft+sourceWidth+4;
    static constexpr int targetRight=13; // circular output hit area, with clear outer margin
    static constexpr int targetWidth=28; // true circular hit target
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
            return QRect(width()-targetRight-targetWidth,2,targetWidth,targetWidth);
        return QRect(width()-targetRight-targetWidth,
                     firstY+(t-1)*rowStep()-nodeHeight()/2,targetWidth,nodeHeight());
    }
    int sourceAt(QPoint pt) const {
        for(int i=0;i<routes.size();++i)
            if(sourceRect(i).contains(pt))return i;
        return -1;
    }
    int trackAt(QPoint pt) const {
        for(int t=1;t<=monitorTarget;++t){
            const auto r=trackRect(t);
            const QPointF delta=QPointF(pt)-QPointF(r.center());
            const qreal radius=r.width()/2.0;
            if(delta.x()*delta.x()+delta.y()*delta.y()<=radius*radius)return t;
        }
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
        return AudioPatchbay::activityForSource(src)==ActivityState::Active;
    }
    ActivityState outputActivity(int target) const {
        if(target<1||target>monitorTarget)return ActivityState::Unknown;
        bool unknown=false,silent=false,muted=false;
        for(const auto &src:routes){
            const bool connected=target==monitorTarget?src.monitor!=0:
                src.monitor!=1&&(src.tracks&(1u<<(target-1)))!=0;
            if(!connected)continue;
            const auto state=AudioPatchbay::activityForSource(src);
            if(state==ActivityState::Active)return state;
            if(state==ActivityState::Silent)silent=true;
            if(state==ActivityState::Muted)muted=true;
            if(state==ActivityState::Unknown)unknown=true;
        }
        // An unmeasured route could be producing sound: don't claim the
        // entire destination is silent or muted until every route is known.
        return unknown?ActivityState::Unknown:
               silent?ActivityState::Silent:
               muted?ActivityState::Muted:ActivityState::Unknown;
    }
    bool outputLive(int target)const{return outputActivity(target)==ActivityState::Active;}
    std::function<void(const QString &)> onSource;
    std::function<void(int)> onTrack;
    std::function<void(int,const QPoint &)> onContextTarget;
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
    ActivityState sourceActivity(const QString &id) const{
        for(const auto &src:routes)if(src.id==id)return AudioPatchbay::activityForSource(src);
        return ActivityState::Unknown;
    }
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

        for(int t=1;t<=monitorTarget;++t){
            const auto rect=trackRect(t);
            const auto selectedIt=std::find_if(routes.begin(),routes.end(),
                [this](const RouteSource &src){return src.id==selected;});
            const bool connected=selectedIt!=routes.end()&&hasConnection(*selectedIt,t);
            // Signal state is the output circle's OWN outline. Connected
            // routing remains a subtle fill, never a false "active" border.
            // The hover ring uses a neutral emphasis so a hovered/silent
            // output cannot masquerade as one carrying measured audio.
            const bool live=outputActivity(t)==ActivityState::Active;
            const QColor inactive=paletteBlend(muted,fg,43);
            const QColor hovered=paletteBlend(muted,fg,68);
            p.setPen(QPen(live?accent:(hoveredTarget==t?hovered:inactive),
                          live?1.85:(hoveredTarget==t?1.3:0.95)));
            p.setBrush(connected?paletteBlend(base,accent,13):base);
            p.drawEllipse(rect);
            p.setFont(bodyFont);
            p.setPen(fg);
            if(t==monitorTarget) {
                const QPointF centre=rect.center();
                p.setPen(QPen(fg,1.45,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                p.setBrush(Qt::NoBrush);
                p.drawArc(QRectF(centre.x()-6.6,centre.y()-7.2,13.2,14.4),20*16,140*16);
                p.drawRoundedRect(QRectF(centre.x()-7.6,centre.y(),3.2,6.2),1.1,1.1);
                p.drawRoundedRect(QRectF(centre.x()+4.4,centre.y(),3.2,6.2),1.1,1.1);
            }else {
                p.drawText(rect,Qt::AlignCenter,QString::number(t));
            }
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
            // Measured source signal highlights the card perimeter,
            // replacing the old interior status dot. Selected/hovered
            // routes keep their fill and cable emphasis; they must not
            // borrow the active signal outline when silent or unknown.
            const bool live=AudioPatchbay::activityForSource(src)==ActivityState::Active;
            const QColor inactive=paletteBlend(muted,fg,43);
            const QColor hovered=paletteBlend(muted,fg,68);
            p.setPen(QPen(live?accent:(focused?hovered:inactive),
                          live?1.65:(focused?1.0:0.75)));
            p.setBrush(current?paletteBlend(base,accent,18):base);
            p.drawRoundedRect(rect,5,5);
            QFont sourceFont=bodyFont;
            sourceFont.setWeight(current?QFont::Medium:QFont::Normal);
            p.setFont(sourceFont);
            p.setPen(fg);
            const QRect textRect=rect.adjusted(22,0,-7,0);
            const QString name=p.fontMetrics().elidedText(
                src.name,AppTextElision::currentCustomMode(),textRect.width()-2);
            p.drawText(textRect,Qt::AlignVCenter|Qt::AlignLeft,name);
            // Palette-derived source-type pictogram at the left of the label.
            const QRectF icon(rect.left()+6,rect.center().y()-5,10,10);
            p.setPen(QPen(caption,1.15,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            switch(AudioPatchbay::iconForType(src.sourceType)){
              case SourceIcon::Browser:
                p.drawEllipse(icon.adjusted(0.7,0.7,-0.7,-0.7));
                p.drawLine(QPointF(icon.left()+1,icon.center().y()),
                           QPointF(icon.right()-1,icon.center().y()));
                p.drawEllipse(QRectF(icon.center().x()-2.2,icon.top()+0.7,4.4,8.6));
                break;
              case SourceIcon::Media:{
                QPainterPath play;
                play.moveTo(icon.left()+2.8,icon.top()+1.7);
                play.lineTo(icon.right()-1,icon.center().y());
                play.lineTo(icon.left()+2.8,icon.bottom()-1.7);
                play.closeSubpath();
                p.drawPath(play);
                break;
              }
              case SourceIcon::Microphone:
                p.drawRoundedRect(QRectF(icon.center().x()-1.9,icon.top()+0.2,3.8,6),1.9,1.9);
                p.drawArc(QRectF(icon.left()+1,icon.top()+1.7,8,7),185*16,170*16);
                p.drawLine(QPointF(icon.center().x(),icon.bottom()-2),
                           QPointF(icon.center().x(),icon.bottom()));
                break;
              case SourceIcon::Speaker:{
                QPainterPath cone;
                cone.moveTo(icon.left()+1,icon.top()+3.4);
                cone.lineTo(icon.left()+3.7,icon.top()+3.4);
                cone.lineTo(icon.left()+6.3,icon.top()+1);
                cone.lineTo(icon.left()+6.3,icon.bottom()-1);
                cone.lineTo(icon.left()+3.7,icon.bottom()-3.4);
                cone.lineTo(icon.left()+1,icon.bottom()-3.4);
                cone.closeSubpath();p.drawPath(cone);
                p.drawArc(QRectF(icon.left()+4,icon.top()+0.3,6,9.5),-65*16,130*16);
                break;
              }
              case SourceIcon::Waveform:
                p.drawLine(QPointF(icon.left()+2,icon.center().y()-2),
                           QPointF(icon.left()+2,icon.center().y()+2));
                p.drawLine(QPointF(icon.center().x(),icon.top()+1),
                           QPointF(icon.center().x(),icon.bottom()-1));
                p.drawLine(QPointF(icon.right()-2,icon.center().y()-3),
                           QPointF(icon.right()-2,icon.center().y()+3));
                break;
            }
            p.setBrush(focused?accent:muted);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(portX,firstY+i*rowStep()),2.3,2.3);
            p.setFont(captionFont);
        }
        if(routes.isEmpty()){
            p.setPen(caption);
            p.drawText(QRect(7,35,width()-14,55),Qt::AlignHCenter|Qt::TextWordWrap,
                       "No active OBS audio sources");
        }
    }
    void contextMenuEvent(QContextMenuEvent *event) override {
        if(trackAt(event->pos())==monitorTarget&&onContextTarget){
            onContextTarget(monitorTarget,event->globalPos());
            event->accept(); // don't propagate to OBS's Audio Mixer context menu
            return;
        }
        QWidget::contextMenuEvent(event);
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
                switch(AudioPatchbay::activityForSource(src)){
                    case ActivityState::Active:state<<"Signal detected";break;
                    case ActivityState::Silent:state<<"Measured silence";break;
                    case ActivityState::Muted:break;
                    case ActivityState::Unknown:state<<"Signal not measured";break;
                }
                setToolTip(src.name+(state.isEmpty()?QString():(" · "+state.join(" · "))));
            }else if(target>0){
                int incoming=0;
                for(const auto &src:routes)if(hasConnection(src,target))++incoming;
                QString signal;
                switch(outputActivity(target)){
                    case ActivityState::Active:signal="signal detected";break;
                    case ActivityState::Silent:signal="measured silence";break;
                    case ActivityState::Muted:signal="routed sources muted or disabled";break;
                    case ActivityState::Unknown:signal="no fresh signal";break;
                }
                if(target==monitorTarget)
                    setToolTip(QStringLiteral("Headphones · native OBS Monitor · right-click for Monitor Only and other modes · %1 connected %2 · %3 · not Track 7")
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
    monitorMenu=new QMenu(this);
    monitorMenu->setObjectName("aftermarket-monitor-mode-menu");
    monitorMenu->setTitle("Native OBS monitoring");
    auto *group=new QActionGroup(monitorMenu);
    group->setExclusive(true);
    for(int mode=0;mode<=2;++mode){
        const QString label=mode==0?"Off":mode==1?"Monitor Only":"Monitor and Output";
        auto *action=monitorMenu->addAction(label);
        action->setCheckable(true);
        action->setData(mode);
        group->addAction(action);
        connect(action,&QAction::triggered,this,[this,mode]{setMonitoring(mode);});
    }
    canvas->onContextTarget=[this](int target,const QPoint &global){
        if(target!=7||selected.isEmpty())return;
        syncMonitorMenu();
        monitorMenu->popup(global);
    };
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
bool AudioPatchbay::showAllWires() const{return canvas->showAll;}
void AudioPatchbay::setShowAllWires(bool enabled){canvas->setShowAll(enabled);}
void AudioPatchbay::populateRoutingMenu(QMenu *menu){
    if(!menu)return;
    // QMenu/QAction own the native checked, hover, focus and disabled
    // rendering. No stylesheet, Unicode refresh glyph, or fixed accent.
    auto *wires=menu->addAction("Show all wires");
    wires->setObjectName("aftermarket-show-all-wires");
    wires->setToolTip("Show connections from every OBS audio source; otherwise show only the selected source.");
    wires->setCheckable(true);
    wires->setChecked(showAllWires());
    QPointer<AudioPatchbay> safe=this;
    connect(wires,&QAction::toggled,menu,[safe](bool enabled){
        if(safe)safe->setShowAllWires(enabled);
    });
    // Take the refresh SHAPE from the active Qt style, then tint its alpha
    // mask with the actual OBS menu foreground color. The system desktop's
    // "view-refresh" icon theme can be bright blue even when OBS is dark.
    // Build on each menu opening so live OBS theme changes are reflected.
    const int iconSide=qBound(14,menu->style()->pixelMetric(
        QStyle::PM_SmallIconSize,nullptr,menu),24);
    QPixmap refreshGlyph=menu->style()->standardIcon(
        QStyle::SP_BrowserReload,nullptr,menu).pixmap(iconSide,iconSide);
    if(!refreshGlyph.isNull()){
        QPainter tint(&refreshGlyph);
        tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tint.fillRect(refreshGlyph.rect(),menu->palette().color(QPalette::Text));
    }
    auto *reload=menu->addAction(QIcon(refreshGlyph),"Refresh audio routes");
    reload->setObjectName("aftermarket-refresh-audio-routes");
    reload->setToolTip("Read current audio routes from OBS.");
    connect(reload,&QAction::triggered,menu,[safe]{
        if(safe)safe->refresh();
    });
}
int AudioPatchbay::visibleCableCount() const{return canvas->visibleCableCount();}
void AudioPatchbay::refresh(){
    auto latest=query?query():routes;
    // Audio meter state is refreshed independently; preserve its values
    // across the slower authoritative OBS routing poll. Otherwise every
    // polling tick would otherwise reset measured source activity.
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
void AudioPatchbay::syncMonitorMenu(){
    int mode=-1;
    for(const auto &src:routes)if(src.id==selected){mode=src.monitor;break;}
    monitorMenu->setEnabled(mode>=0);
    for(auto *action:monitorMenu->actions())
        action->setChecked(action->data().toInt()==mode);
}
void AudioPatchbay::syncControls(){
    syncMonitorMenu();
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
AudioPatchbay::ActivityState AudioPatchbay::sourceActivity(const QString &id)const{
    return canvas->sourceActivity(id);
}
AudioPatchbay::ActivityState AudioPatchbay::outputActivity(int target)const{
    return canvas->outputActivity(target);
}
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
    // remains accessible via right-click on the headphones destination.
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
