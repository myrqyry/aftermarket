#include "audio-patchbay.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QToolButton>
#include <QPushButton>
#include <QTimer>
#include <QSignalBlocker>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPalette>
#include <QFontMetrics>
#include <algorithm>

namespace augment {
class AudioPatchbay::Canvas final : public QWidget {
    static constexpr int firstY=43;
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

    QRect sourceRect(int i) const {
        return QRect(sourceLeft,firstY+i*rowStep()-nodeHeight()/2,
                     sourceWidth,nodeHeight());
    }
    QRect trackRect(int t) const {
        return QRect(width()-targetRight-targetWidth,
                     firstY+(t-1)*rowStep()-nodeHeight()/2,targetWidth,nodeHeight());
    }
    int sourceAt(QPoint pt) const {
        for(int i=0;i<routes.size();++i)
            if(sourceRect(i).contains(pt))return i;
        return -1;
    }
    int trackAt(QPoint pt) const {
        for(int t=1;t<=6;++t)
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
        unsetCursor();
        update();
    }
public:
    QVector<RouteSource> routes;
    QString selected;
    bool showAll=false;
    std::function<void(const QString &)> onSource;
    std::function<void(int)> onTrack;
    std::function<void(const QString &,int)> onConnect;

    explicit Canvas(QWidget *parent=nullptr):QWidget(parent){
        setObjectName("aftermarket-route-canvas");
        setMinimumWidth(240);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setToolTip("Drag a source to a track to connect. Click a source to focus; click a track to toggle its route.");
    }
    int visibleCableCount() const {
        int count=0;
        for(const auto &src:routes)
            if(showAll||src.id==selected)
                for(int t=0;t<6;++t)if(src.tracks&(1u<<t))++count;
        return count;
    }
    void setRoutes(const QVector<RouteSource> &value,const QString &current){
        if(routes==value&&selected==current)return;
        if(dragging&&!std::any_of(value.begin(),value.end(),
            [this](const RouteSource &src){return src.id==dragId;}))resetDrag();
        routes=value;
        selected=current;
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
        const auto base=palette().color(QPalette::Button);
        const auto light=palette().color(QPalette::HighlightedText);
        const qreal destX=width()-targetRight-targetWidth;

        QFont small=p.font();
        small.setPointSizeF(qMax(7.0,small.pointSizeF()-1.0));
        p.setFont(small);
        p.setPen(muted);
        p.drawText(QRect(7,3,120,23),Qt::AlignVCenter,"Sources");
        p.drawText(QRect(width()-65,3,58,23),Qt::AlignVCenter|Qt::AlignRight,"Tracks");

        for(int t=1;t<=6;++t){
            const auto rect=trackRect(t);
            p.setPen(QPen(hoveredTarget==t?accent:muted,hoveredTarget==t?2.0:1.0));
            p.setBrush(base);
            p.drawRoundedRect(rect,6,6);
            p.setPen(fg);
            p.drawText(rect,Qt::AlignCenter,QString::number(t));
        }
        // Draw cables behind nodes. The all-wires view is intentionally
        // optional to avoid overwhelming small docks with unrelated routes.
        // In all-wires mode draw other sources first, selected wires last,
        // so the focused route stays legible. Colors still come from Qt.
        for(int pass=0;pass<2;++pass)
            for(int i=0;i<routes.size();++i){
                const auto &src=routes.at(i);
                const bool current=src.id==selected;
                if(current!=(pass==1)||(!showAll&&!current))continue;
                for(int t=0;t<6;++t){
                    if(!(src.tracks&(1u<<t)))continue;
                    QColor wire=current?accent:fg;
                    wire.setAlpha(current?225:165);
                    p.setPen(QPen(wire,current?2.3:1.55,Qt::SolidLine,Qt::RoundCap));
                    p.setBrush(Qt::NoBrush);
                    p.drawPath(cable(firstY+i*rowStep(),firstY+t*rowStep()));
                }
            }
        if(dragging){
            QColor preview=accent;
            preview.setAlpha(225);
            p.setPen(QPen(preview,2.2,Qt::DashLine,Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            const int sourceIndex=sourceAt(pressPoint);
            if(sourceIndex>=0){
                const int y=firstY+sourceIndex*rowStep();
                if(hoveredTarget>0)p.drawPath(cable(y,firstY+(hoveredTarget-1)*rowStep()));
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
            p.setPen(QPen(current?accent:muted,current?1.7:1.0));
            p.setBrush(current?accent.lighter(128):base);
            p.drawRoundedRect(rect,6,6);
            p.setPen(current?light:fg);
            const auto name=p.fontMetrics().elidedText(src.name,Qt::ElideRight,sourceWidth-15);
            p.drawText(rect.adjusted(7,0,-3,0),Qt::AlignVCenter|Qt::AlignLeft,name);
            p.setBrush(current?accent:muted);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(portX,firstY+i*rowStep()),3.5,3.5);
        }
        if(routes.isEmpty()){
            p.setPen(muted);
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
            const int target=trackAt(event->position().toPoint());
            if(target!=hoveredTarget){hoveredTarget=target;update();}
        }
        event->accept();
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
                             QWidget *parent,SetTrack s)
    :QWidget(parent),query(std::move(q)),toggle(std::move(t)),
     monitor(std::move(m)),setTrack(std::move(s)){
    setObjectName("aftermarket-audio-patchbay");
    setWindowTitle("Routing");
    auto *root=new QVBoxLayout(this);
    root->setContentsMargins(4,3,4,3);
    root->setSpacing(3);
    auto *header=new QHBoxLayout;
    header->setSpacing(5);
    auto *heading=new QLabel("Audio routes",this);
    auto font=heading->font();font.setBold(true);heading->setFont(font);
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
    auto *selector=new QHBoxLayout;
    selector->addWidget(new QLabel("Source",this));
    sources=new QComboBox(this);
    sources->setObjectName("aftermarket-route-source");
    sources->setAccessibleName("Choose an OBS audio source");
    selector->addWidget(sources,1);
    root->addLayout(selector);
    auto *scroll=new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    canvas=new Canvas(scroll);
    canvas->onSource=[this](const QString &id){selectSource(id);};
    canvas->onTrack=[this](int track){toggleTrack(track);};
    canvas->onConnect=[this](const QString &id,int track){connectRoute(id,track);};
    scroll->setWidget(canvas);
    root->addWidget(scroll,1);
    auto *trackRow=new QHBoxLayout;
    trackRow->setSpacing(3);
    trackRow->addWidget(new QLabel("Tracks",this));
    for(int i=0;i<6;++i){
        auto *button=new QToolButton(this);
        button->setObjectName(QStringLiteral("aftermarket-track-%1").arg(i+1));
        button->setCheckable(true);
        button->setText(QString::number(i+1));
        button->setToolTip(QStringLiteral("Toggle OBS audio track %1 for selected source").arg(i+1));
        button->setAccessibleName(button->toolTip());
        button->setMinimumSize(24,24);
        tracks[i]=button;
        trackRow->addWidget(button,1);
        connect(button,&QToolButton::clicked,this,[this,i]{toggleTrack(i+1);});
    }
    root->addLayout(trackRow);
    auto *monitorRow=new QHBoxLayout;
    monitorRow->setSpacing(3);
    monitorRow->addWidget(new QLabel("Monitoring",this));
    monitoring=new QComboBox(this);
    monitoring->setObjectName("aftermarket-monitor");
    monitoring->addItems({"Off","Monitor Only","Monitor and Output"});
    monitoring->setAccessibleName("Native OBS monitoring mode");
    monitorRow->addWidget(monitoring,1);
    root->addLayout(monitorRow);
    status=new QLabel("Drag a source onto a track to connect.",this);
    status->setObjectName("aftermarket-route-status");
    status->setWordWrap(true);
    root->addWidget(status);
    connect(refreshButton,&QToolButton::clicked,this,[this]{refresh();});
    connect(allWires,&QCheckBox::toggled,this,[this](bool enabled){canvas->setShowAll(enabled);});
    connect(sources,&QComboBox::currentIndexChanged,this,[this](int index){
        if(index>=0)selectSource(sources->itemData(index).toString());
    });
    connect(monitoring,&QComboBox::currentIndexChanged,this,[this](int mode){
        if(mode>=0)setMonitoring(mode);
    });
    timer=new QTimer(this);
    timer->setInterval(1400);
    connect(timer,&QTimer::timeout,this,[this]{if(isVisible())refresh();});
    timer->start();
    refresh();
}
int AudioPatchbay::routeCount() const{return routes.size();}
QString AudioPatchbay::selectedId() const{return selected;}
bool AudioPatchbay::showAllWires() const{return allWires->isChecked();}
int AudioPatchbay::visibleCableCount() const{return canvas->visibleCableCount();}
void AudioPatchbay::refresh(){
    const auto latest=query?query():routes;
    if(latest==routes&&!selected.isEmpty())return;
    routes=latest;
    auto exists=[this](const QString &id){
        return std::any_of(routes.begin(),routes.end(),[&](const RouteSource &r){return r.id==id;});
    };
    if(!exists(selected))selected=routes.isEmpty()?QString{}:routes.first().id;
    syncControls();
}
void AudioPatchbay::syncControls(){
    const QSignalBlocker sourceBlock(sources),monitorBlock(monitoring);
    sources->clear();
    for(const auto &r:routes)sources->addItem(r.name,r.id);
    int chosen=-1;
    for(int i=0;i<routes.size();++i)if(routes[i].id==selected){chosen=i;break;}
    sources->setCurrentIndex(chosen);
    sources->setEnabled(chosen>=0);
    for(int i=0;i<6;++i){
        const QSignalBlocker blocker(tracks[i]);
        tracks[i]->setEnabled(chosen>=0);
        tracks[i]->setChecked(chosen>=0&&(routes.at(chosen).tracks&(1u<<i)));
    }
    monitoring->setEnabled(chosen>=0);
    monitoring->setCurrentIndex(chosen<0?-1:routes.at(chosen).monitor);
    canvas->setRoutes(routes,selected);
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
    status->setText(ok?QStringLiteral("Updated track %1 in OBS").arg(track):
                       QStringLiteral("Source unavailable; OBS routing unchanged"));
    return ok;
}
bool AudioPatchbay::connectRoute(const QString &id,int track){
    if(id.isEmpty()||track<1||track>6)return false;
    // Read OBS again immediately before the drop. Repeated drops onto an
    // existing connection are idempotent; they must never disconnect audio.
    const auto current=query?query():routes;
    auto found=std::find_if(current.begin(),current.end(),
        [&](const RouteSource &route){return route.id==id;});
    if(found==current.end()){
        refresh();
        status->setText("Source unavailable; OBS routing unchanged");
        return false;
    }
    if(found->tracks&(1u<<(track-1))){
        refresh();
        selectSource(id);
        status->setText(QStringLiteral("Track %1 is already connected").arg(track));
        return true;
    }
    const bool ok=setTrack?setTrack(id,track,true):(toggle&&toggle(id,track));
    refresh();
    if(ok)selectSource(id);
    status->setText(ok?QStringLiteral("Connected source to OBS track %1").arg(track):
                       QStringLiteral("Source unavailable; OBS routing unchanged"));
    return ok;
}
bool AudioPatchbay::setMonitoring(int mode){
    if(selected.isEmpty()||mode<0||mode>2||!monitor)return false;
    const bool ok=monitor(selected,mode);
    refresh();
    status->setText(ok?"Updated native OBS monitoring":"Source unavailable; monitoring unchanged");
    return ok;
}
}
