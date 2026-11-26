#include "front-audio-controls.h"
#include "rotary-paint.h"
#include <QApplication>
#include <QWidget>
#include <QBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QAbstractButton>
#include <QSlider>
#include <QDial>
#include <QSpinBox>
#include <QLabel>
#include <QPainter>
#include <QStylePainter>
#include <QStyleOptionButton>
#include <QTextLayout>
#include <QMenu>
#include <QToolBar>
#include <QPushButton>
#include <QToolButton>
#include <QAbstractButton>
#include <QWidgetAction>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QCursor>
#include <QStringList>
#include <QSaveFile>
#include <QFile>
#include <QSignalBlocker>
#include <QPointer>
#include <QSet>
#include <QMap>
#include <QLayout>
#include <QSizePolicy>
#include <QtMath>
#include <algorithm>

namespace augment {
namespace {
constexpr int maxOffsetMs=999999;
// Reuse the gain dial's palette and drag direction with a single compact scale.
constexpr int compactDialSize=32;
class CompactRotary : public QDial {
public:
    explicit CompactRotary(QWidget *parent,bool endless=false):QDial(parent),endless_(endless){
        setNotchesVisible(false);
        setFocusPolicy(Qt::StrongFocus);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const auto pal=palette();
        const auto group=isEnabled()?QPalette::Active:QPalette::Disabled;
        const double side=qMin(width(),height())-4.;
        if(side<8)return;
        const QRectF outer((width()-side)/2.,(height()-side)/2.,side,side);
        const auto ring=outer.adjusted(4.2,4.2,-4.2,-4.2);
        const auto face=outer.adjusted(7,7,-7,-7);
        const auto accent=pal.color(group,QPalette::Highlight);
        p.setOpacity(isEnabled()?1.:.48);
        // Only the inner value scale remains. No second decorative ring.
        auto dialPalette=pal;
        dialPalette.setColor(group,QPalette::Mid,rotaryMutedAccent(pal.color(group,QPalette::Window),accent));
        const QBrush brush(rotaryThemeGradient(outer.center(),pal,group));
        if(endless_){
            const double angle=90.-((value()%360+360)%360);
            paintRotaryFace(p,ring,face,dialPalette,group,brush,90,-360,angle+68,-68,angle,.8,true,false);
        }else{
            const double span=double(maximum())-minimum();
            const double fraction=span>0?qBound(0.,(sliderPosition()-double(minimum()))/span,1.):.5;
            const double angle=225.-270.*(invertedAppearance()?1.-fraction:fraction);
            paintRotaryFace(p,ring,face,dialPalette,group,brush,225,-270,90,angle-90,angle,.8,true);
            p.setPen(QPen(pal.color(group,QPalette::ButtonText),1.,Qt::SolidLine,Qt::RoundCap));
            p.drawLine(QPointF(outer.center().x(),outer.top()-.4),QPointF(outer.center().x(),outer.top()+1.8));
        }
        if(hasFocus()){
            p.setBrush(Qt::NoBrush);p.setPen(QPen(accent,.8,Qt::DotLine));
            p.drawEllipse(outer.adjusted(-1.5,-1.5,1.5,1.5));
        }
    }
    void mousePressEvent(QMouseEvent *e) override {
        if(!isEnabled()||(e->button()!=Qt::LeftButton&&e->button()!=Qt::MiddleButton)){e->ignore();return;}
        if(dragging){e->accept();return;}
        if(focusPolicy()!=Qt::NoFocus)setFocus(Qt::MouseFocusReason);
        last=e->globalPosition();remainder=0;dragging=true;
        setSliderDown(true);e->accept();
    }
    void mouseDoubleClickEvent(QMouseEvent *e) override {
        if(e->button()!=Qt::LeftButton||!isEnabled()){e->ignore();return;}
        finish();
        setValue(endless_?0:minimum()+(maximum()-minimum())/2);
        e->accept();
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if(!dragging){e->ignore();return;}
        if(!(e->buttons()&(Qt::LeftButton|Qt::MiddleButton))){finish();e->accept();return;}
        const auto now=e->globalPosition();
        const double rate=endless_?10.:(double(maximum())-minimum())/150.;
        const double delta=rotaryDragDistance(now-last)*rate*(e->modifiers().testFlag(Qt::ShiftModifier)?.1:1.)*(invertedControls()?-1.:1.);
        last=now;
        remainder+=delta;
        const auto steps=qint64(remainder);
        remainder-=steps;
        if(steps){
            const auto next=qBound(qint64(minimum()),qint64(value())+steps,qint64(maximum()));
            setValue(int(next));
            if(next==minimum()||next==maximum())remainder=0;
        }
        e->accept();
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        if((e->button()==Qt::LeftButton||e->button()==Qt::MiddleButton)&&dragging){finish();e->accept();}
        else e->ignore();
    }
    bool event(QEvent *e) override {
        if(e->type()==QEvent::UngrabMouse||e->type()==QEvent::Hide||
           e->type()==QEvent::WindowDeactivate||
           (e->type()==QEvent::EnabledChange&&!isEnabled()))finish();
        return QDial::event(e);
    }
private:
    bool endless_,dragging=false;
    QPointF last;
    double remainder=0;
    void finish(){if(dragging){dragging=false;remainder=0;setSliderDown(false);}}
};
// Absolute offset stays independent of the painted angular position.
class SyncEncoder final : public CompactRotary {
public:
    explicit SyncEncoder(QWidget *parent):CompactRotary(parent,true){
        setRange(-maxOffsetMs,maxOffsetMs);
        setSingleStep(10);setPageStep(100);
    }
protected:
    void wheelEvent(QWheelEvent *e) override {
        const int delta=e->angleDelta().y();
        if(!delta){e->ignore();return;}
        const int scale=step(e->modifiers());
        if(scale!=wheelScale){wheelRemainder=0;wheelScale=scale;}
        const qint64 total=qint64(wheelRemainder)+delta;
        wheelRemainder=int(total%120);
        adjust(total/120*scale);e->accept();
    }
    void keyPressEvent(QKeyEvent *e) override {
        const int scale=step(e->modifiers());
        switch(e->key()){
        case Qt::Key_Up:case Qt::Key_Right:adjust(scale);break;
        case Qt::Key_Down:case Qt::Key_Left:adjust(-scale);break;
        case Qt::Key_PageUp:adjust(scale*10);break;
        case Qt::Key_PageDown:adjust(-scale*10);break;
        default:QWidget::keyPressEvent(e);return;
        }
        e->accept();
    }
private:
    int wheelRemainder=0,wheelScale=10;
    static int step(Qt::KeyboardModifiers mods){return mods.testFlag(Qt::ShiftModifier)?1:10;}
    void adjust(qint64 delta){
        if(delta)setValue(int(qBound(qint64(minimum()),qint64(value())+delta,qint64(maximum()))));
    }
};
QString sourceName(QWidget *volume){
    for(auto *button:volume->findChildren<QAbstractButton*>())
        if(button->inherits("VolumeName")||button->objectName()=="VolumeName")
            return button->text();
    for(auto *label:volume->findChildren<QLabel*>())
        if(label->inherits("OBSSourceLabel")||label->objectName()=="OBSSourceLabel")
            return label->text();
    return {};
}
bool volumeControl(QWidget *widget){
    // OBS Studio 32.x calls its native mixer widget VolControl. Retain the
    // legacy/integration spelling for existing plugin-owned test layouts.
    return widget&&(widget->inherits("VolControl")||
                     widget->objectName()=="VolControl"||
                     widget->inherits("VolumeControl")||
                     widget->objectName()=="VolumeControl");
}
enum class ChannelStatus { Unknown, Pinned, Active, Inactive };
ChannelStatus statusKind(const QString &text){
    const auto value=text.trimmed();
    if(value=="Pinned")return ChannelStatus::Pinned;
    if(value=="Active")return ChannelStatus::Active;
    if(value=="Inactive")return ChannelStatus::Inactive;
    return ChannelStatus::Unknown;
}
QColor channelTitleColor(const QWidget *widget,ChannelStatus state){
    const auto pal=widget->palette();
    const auto group=widget->isEnabled()?QPalette::Active:QPalette::Disabled;
    if(state==ChannelStatus::Pinned)return pal.color(group,QPalette::Highlight);
    if(state==ChannelStatus::Inactive)return pal.color(QPalette::Disabled,QPalette::ButtonText);
    return pal.color(group,QPalette::ButtonText);
}
bool paintChannelTitle(QLabel *label,QAbstractButton *button){
    if(!label||!button||!button->inherits("VolumeName"))return false;
    const auto state=statusKind(button->property("aftermarket-channel-state").toString());
    const bool two=button->property("aftermarket-two-line-title").toBool();
    if(state==ChannelStatus::Unknown&&!two)return false;
    QPainter p(label);
    auto font=label->font();
    if(state==ChannelStatus::Pinned)font.setBold(true);
    p.setFont(font);p.setPen(channelTitleColor(button,state));
    const QFontMetrics metrics(font);
    QRect area=label->contentsRect();
    if(area.width()<=0)return true;
    const QString full=button->text();
    if(!two){
        p.drawText(area,Qt::AlignLeft|Qt::AlignVCenter,metrics.elidedText(full,Qt::ElideMiddle,area.width()));
        return true;
    }
    // Wrap once at a word boundary (or anywhere for a long token);
    // the second row is elided, never an unbounded third line.
    QTextLayout text(full,font);
    QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    text.setTextOption(option);text.beginLayout();
    auto line=text.createLine();
    int used=0;
    if(line.isValid()){line.setLineWidth(area.width());used=line.textLength();}
    text.endLayout();
    const int h=metrics.lineSpacing();
    const int y=area.top()+qMax(0,(area.height()-2*h)/2);
    p.drawText(QRect(area.left(),y,area.width(),h),Qt::AlignLeft|Qt::AlignVCenter,full.left(used).trimmed());
    if(used<full.size())p.drawText(QRect(area.left(),y+h,area.width(),h),Qt::AlignLeft|Qt::AlignVCenter,
        metrics.elidedText(full.mid(used).trimmed(),Qt::ElideMiddle,area.width()));
    return true;
}
// OBS 32.2 VolumeName is a QAbstractButton with a child QLabel and
// explicitly paints PE_IndicatorArrowDown (not a menu-indicator subcontrol).
// Retain its exact native bevel and indicator bounds, replacing only the
// painted arrow. Its native click, menu, focus, label and drag paths remain.
bool paintVolumeNameMenu(QAbstractButton *button){
    if(!button||!button->inherits("VolumeName")||
       !button->findChild<QLabel*>(QString(),Qt::FindDirectChildrenOnly))return false;
    QStyleOptionButton opt;opt.initFrom(button);
    const auto *style=button->style();
    const int padding=opt.rect.right()-style->subElementRect(QStyle::SE_PushButtonContents,&opt,button).right();
    const int w=style->pixelMetric(QStyle::PM_MenuButtonIndicator,&opt,button);
    const int y=button->property("aftermarket-two-line-title").toBool()?
        qMax(0,(button->fontMetrics().lineSpacing()-w)/2):opt.rect.center().y()-w/2;
    const QRect indicator(opt.rect.right()-w-padding/2,y,w,w);
    if(w<7||!opt.rect.contains(indicator))return false;
    QStylePainter p(button);
    p.drawControl(QStyle::CE_PushButtonBevel,opt);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(opt.palette.color(button->isEnabled()?QPalette::Active:QPalette::Disabled,QPalette::ButtonText));
    // Use a pixel-aligned dot at this tiny native indicator size.
    // Guaranteed gaps also on binary, non-antialiased paint targets.
    const int step=qMin(4,qMax(2,(w-1)/3));
    const int diameter=step>=3?2:1;
    const auto color=p.brush().color();
    for(int i=-1;i<=1;++i)
        p.fillRect(QRect(indicator.center().x()-diameter/2,
                         indicator.center().y()+i*step-diameter/2,
                         diameter,diameter),color);
    return true;
}
QWidget *sourceNameLabel(QWidget *volume){
    if(!volume)return nullptr;
    for(auto *candidate:volume->findChildren<QWidget*>()){
        // OBS uses OBSSourceLabel (QLabel); older integrations may use a
        // VolumeName button. Recognize only source-name controls, not meter,
        // balance, or sync labels that share the same channel hierarchy.
        if(candidate->inherits("OBSSourceLabel")||
           candidate->objectName()=="OBSSourceLabel") {
            if(qobject_cast<QLabel*>(candidate))return candidate;
        }
        if(candidate->inherits("VolumeName")||
           candidate->objectName()=="VolumeName") {
            if(qobject_cast<QAbstractButton*>(candidate))return candidate;
        }
    }
    return nullptr;
}
}
struct FrontAudioControls::State {
    struct Row {
        QPointer<QWidget> volume,controls;
        QPointer<QSlider> panSlider;
        QPointer<QDial> panDial,offsetDial;
        QPointer<QSpinBox> offsetSpin;
        QPointer<QLabel> panLabel,offsetLabel;
        QString id;
        QPointer<QWidget> nameHandle;
        QCursor originalCursor;
        bool hadCursor=false;
        // Preserve all native layout slots and widget identities.
        QPointer<QBoxLayout> root,body,header,center,nameRow,columnButtons,columnBody,columnCenter;
        int originalButtonSpacing=-1,appliedButtonSpacing=-1;
        int nameIndex=-1,nameStretch=0;
        QPointer<QWidget> frame;
        QPointer<QLabel> db,status;
        int dbIndex=0,dbStretch=0,frameIndex=0,frameStretch=0,controlsIndex=0,controlsStretch=0;
        Qt::Alignment dbAlignment,frameAlignment,controlsAlignment;
        QSize controlsMin,controlsMax;
        QSizePolicy controlsPolicy;
        bool statusDecorated=false,statusWasHidden=false;
        QPointer<QLabel> nameText;
        QPointer<QBoxLayout> statusHeader;
        struct SpacerBaseline {QSpacerItem *item;QSize size;QSizePolicy policy;};
        QList<SpacerBaseline> statusSpacers;
        int titleMin=0,titleMax=QWIDGETSIZE_MAX;
        bool twoLineTitle=false;
        bool columnMode=false,columnGain=false;
        QSpacerItem *columnSpacer=nullptr;
        int originalColumnMin=0,appliedColumnMin=0;
        int columnButtonsStretch=0;
        Qt::Alignment columnButtonsAlignment;
        QMargins columnButtonMargins;
        QList<Qt::Alignment> columnButtonAlignments;
        QPointer<QBoxLayout> rotaryButtons;
        int rotaryButtonSpacing=0;
        QMargins rotaryButtonMargins;
        Qt::Alignment rotaryButtonAlignment;
        QList<Qt::Alignment> rotaryButtonItemAlignments;
        bool rotaryEdges=false;
        QPointer<QGridLayout> rotaryGrid;
        int rotaryButtonsStretch=0;

    };
    QObject *owner=nullptr;
    QString file;
    Query query;
    SetPan pan;
    SetOffset offset;
    std::function<void()> persist;
    QTimer saveDebounce;
    QPointer<QWidget> native;
    QPointer<QToolBar> footer;
    QPointer<QToolButton> menuButton;
    QPointer<QPushButton> hiddenButton,optionsButton;
    QPointer<QToolButton> layoutButton,advancedButton;
    QPointer<QMenu> nativeOptions;
    QString nativeOptionsTitle;
    bool footerWasHidden=false;
    QList<Row> rows;
    QStringList channelOrder;
    bool orderingUnlocked=false;
    QPointer<QWidget> dragHandle;
    QPoint dragStart;
    bool dragging=false;
    struct LayoutBaseline {
        QPointer<QBoxLayout> layout;
        QList<QPointer<QWidget>> order;
    };
    QList<LayoutBaseline> baselines;
    bool autoGrid=true;
    QPointer<QBoxLayout> gridBox;
    QPointer<QGridLayout> grid;
    struct GridSlot {QPointer<QWidget> widget;int stretch;Qt::Alignment alignment;};
    QList<GridSlot> gridSlots;
    int gridColumns=0;
    QTimer gridDebounce;

    int gridIndex()const{
        if(!grid||!gridBox)return -1;
        for(int i=0;i<gridBox->count();++i)if(gridBox->itemAt(i)->layout()==grid)return i;
        return -1;
    }
    void restoreGrid(){
        if(grid&&gridBox){
            const int index=gridIndex();
            // Native OBS can move widgets out of our nested layout. Only
            // restore items still owned by it; don't duplicate native moves.
            QList<GridSlot> remaining;
            for(const auto &slot:gridSlots)if(slot.widget&&grid->indexOf(slot.widget)>=0)
                remaining.append(slot);
            while(grid->count())delete grid->takeAt(0);
            gridBox->removeItem(grid);delete grid.data();
            int position=index<0?0:index;
            for(const auto &slot:remaining)
                gridBox->insertWidget(position++,slot.widget,slot.stretch,slot.alignment);
        }
        grid.clear();gridBox.clear();gridSlots.clear();gridColumns=0;
    }
    QBoxLayout *activeMixerBox()const{
        if(!native)return nullptr;
        for(bool columns:{false,true}){
            auto *scroll=native->findChild<QScrollArea*>(columns?"vMixerScrollArea":"hMixerScrollArea");
            if(!scroll||!scroll->isVisible()||!scroll->widget()||
               scroll->widget()->objectName()!=(columns?"vVolumeWidgets":"hVolumeWidgets"))continue;
            auto *box=qobject_cast<QBoxLayout*>(scroll->widget()->layout());
            if(box&&box->direction()==(columns?QBoxLayout::LeftToRight:QBoxLayout::TopToBottom)&&
               box->parentWidget()==scroll->widget())return box;
        }
        return nullptr;
    }
    bool gridIntact()const{
        if(!grid||!gridBox||gridIndex()<0)return false;
        if(grid->count()!=gridSlots.size())return false;
        for(const auto &slot:gridSlots)if(!slot.widget||grid->indexOf(slot.widget)<0)return false;
        for(int i=0;i<gridBox->count();++i){
            auto *item=gridBox->itemAt(i);
            if(item->layout()!=grid&&!item->spacerItem())return false;
        }
        return true;
    }
    int columnsFor(QBoxLayout *box,const QList<QWidget*> &widgets)const{
        if(!autoGrid||!box||widgets.size()<2||!box->parentWidget()->isVisible())return 1;
        auto *host=box->parentWidget();
        QScrollArea *scroll=nullptr;
        for(auto *p=host->parentWidget();p;p=p->parentWidget())
            if((scroll=qobject_cast<QScrollArea*>(p)))break;
        if(!scroll||scroll->widget()!=host)return 1;
        const auto margins=box->contentsMargins();
        const int width=scroll->viewport()->width()-margins.left()-margins.right();
        const bool strips=box->direction()==QBoxLayout::LeftToRight;
        // Compact side-control strips are shorter than the former vertically
        // stacked control group. Use their measured size hints, not the old
        // 180px row floor, while still requiring every row to fit.
        const bool compactRows=!strips&&std::all_of(widgets.cbegin(),widgets.cend(),[this](QWidget *w){
            const auto r=std::find_if(rows.cbegin(),rows.cend(),[w](const Row &r){return r.volume==w;});
            return r!=rows.cend()&&r->rotaryGrid;
        });
        int minimum=strips?80:compactRows?156:240,minimumHeight=0;
        for(auto *w:widgets){
            if(w->isHidden()||w->property("aftermarket-foreign").toBool())return 1;
            minimum=qMax(minimum,qMax(w->minimumWidth(),w->minimumSizeHint().width()));
            if(strips){
                // Native column host scrolls horizontally only. Wrap
                // solely compact integrated rotary strips when every
                // resulting row fits the viewport without clipping.
                const auto r=std::find_if(rows.cbegin(),rows.cend(),[w](const Row &r){return r.volume==w;});
                if(r==rows.cend()||!r->columnGain||w->maximumWidth()>150)return 1;
                minimum=qMax(minimum,w->maximumWidth());
                minimumHeight=qMax(minimumHeight,qMax(w->minimumSizeHint().height(),w->sizeHint().height()));
            }
        }
        constexpr int spacing=6;
        if(strips){
            const int cols=qMax(1,(width+spacing)/(minimum+spacing));
            if(cols<2||cols>=widgets.size())return 1;
            const int height=scroll->viewport()->height()-margins.top()-margins.bottom();
            const int count=(widgets.size()+cols-1)/cols;
            return count*minimumHeight+(count-1)*spacing<=height?cols:1;
        }
        return qBound(1,(width+spacing)/(minimum+spacing),qMin(4,int(widgets.size())));
    }
    void refreshGrid(){
        auto *box=activeMixerBox();
        if(grid&&(!gridIntact()||gridBox!=box))restoreGrid();
        if(!box||!autoGrid){restoreGrid();return;}
        auto widgets=orderedWidgets(box);
        const int cols=columnsFor(box,widgets);
        if(grid&&cols==gridColumns)return;
        if(grid)restoreGrid();
        if(cols<2)return;
        // Limit the adapter to a contiguous native channel run, optionally
        // followed by spacers. Leave unknown extensions/layouts untouched.
        int first=-1;
        QList<GridSlot> captured;
        for(int i=0;i<box->count();++i){
            auto *item=box->itemAt(i);
            if(item->spacerItem())continue;
            auto *w=item->widget();
            if(!w||!volumeControl(w)||w->property("aftermarket-foreign").toBool())return;
            if(first<0)first=i;
            if(i!=first+captured.size())return;
            captured.append({w,box->stretch(i),item->alignment()});
        }
        if(captured.size()!=widgets.size()||first<0)return;
        for(int i=0;i<captured.size();++i)delete box->takeAt(first);
        auto *layout=new QGridLayout;
        layout->setObjectName("aftermarket-mixer-grid");
        layout->setContentsMargins(0,0,0,0);
        layout->setSpacing(6);
        const bool strips=box->direction()==QBoxLayout::LeftToRight;
        for(int i=0;i<captured.size();++i)
            layout->addWidget(captured[i].widget,i/cols,i%cols,strips?Qt::AlignTop:Qt::Alignment{});
        if(strips)layout->setAlignment(Qt::AlignLeft|Qt::AlignTop);
        else for(int i=0;i<cols;++i)layout->setColumnStretch(i,1);
        box->insertLayout(first,layout);
        gridBox=box;grid=layout;gridSlots=captured;gridColumns=cols;
    }


    static QBoxLayout *containingBox(QLayout *layout,QWidget *widget){
        if(!layout||!widget)return nullptr;
        for(int i=0;i<layout->count();++i){
            auto *item=layout->itemAt(i);
            if(item->widget()==widget){
                if(layout->objectName()=="aftermarket-mixer-grid")
                    return qobject_cast<QBoxLayout*>(layout->parent());
                return qobject_cast<QBoxLayout*>(layout);
            }
            if(auto *nested=containingBox(item->layout(),widget))return nested;
        }
        return nullptr;
    }
    static QBoxLayout *channelBox(QWidget *widget){
        if(!widget||!widget->parentWidget())return nullptr;
        return containingBox(widget->parentWidget()->layout(),widget);
    }
    QString idFor(QWidget *widget)const{
        for(const auto &row:rows)if(row.volume==widget)return row.id;
        return {};
    }
    QList<QWidget*> orderedWidgets(QBoxLayout *box)const{
        QList<QWidget*> result;
        if(!box)return result;
        for(int i=0;i<box->count();++i){
            auto *widget=box->itemAt(i)->widget();
            if(widget&&volumeControl(widget)&&!widget->property("aftermarket-foreign").toBool())
                result.append(widget);
            else if(grid&&box==gridBox&&box->itemAt(i)->layout()==grid)
                for(const auto &slot:gridSlots)if(slot.widget&&grid->indexOf(slot.widget)>=0)result.append(slot.widget);
        }
        return result;
    }
    void arrange(QBoxLayout *box,const QList<QWidget*> &desired){
        if(!box||desired.size()<2)return;
        if(grid&&gridBox==box){
            QList<QWidget*> current;
            for(auto *w:orderedWidgets(box))if(desired.contains(w))current.append(w);
            if(current==desired)return;
            restoreGrid();
        }
        // Move only channel items. Preserve every native widget, spacer,
        // layout slot, stretch and alignment (including unrelated toolbar).
        QList<int> positions;
        QList<QWidget*> before;
        for(int i=0;i<box->count();++i){
            auto *w=box->itemAt(i)->widget();
            if(desired.contains(w)){positions.append(i);before.append(w);}
        }
        if(before==desired||before.size()!=desired.size())return;
        struct Item { QWidget *widget; int stretch; Qt::Alignment alignment; };
        QList<Item> items;
        for(int i:positions)items.append({box->itemAt(i)->widget(),box->stretch(i),box->itemAt(i)->alignment()});
        for(int i=positions.size()-1;i>=0;--i)delete box->takeAt(positions[i]);
        for(int i=0;i<desired.size();++i)
            for(const auto &entry:items)if(entry.widget==desired[i]){
                box->insertWidget(positions[i],entry.widget,entry.stretch,entry.alignment);break;
            }
    }
    void rememberLayout(QBoxLayout *box){
        if(!box)return;
        LayoutBaseline *record=nullptr;
        for(auto &entry:baselines)if(entry.layout==box){record=&entry;break;}
        if(!record){baselines.append(LayoutBaseline{box,{}});record=&baselines.last();}
        for(auto *widget:orderedWidgets(box))
            if(!record->order.contains(widget))record->order.append(widget);
    }
    void applyOrder(){
        for(int i=baselines.size()-1;i>=0;--i)if(!baselines[i].layout)baselines.removeAt(i);
        QSet<QBoxLayout*> boxes;
        for(const auto &row:rows)if(auto *box=channelBox(row.volume))boxes.insert(box);
        for(auto *box:boxes){
            rememberLayout(box);
            if(channelOrder.isEmpty())continue;
            auto widgets=orderedWidgets(box);
            // Unknown or ambiguous native rows stay in their original slots.
            widgets.erase(std::remove_if(widgets.begin(),widgets.end(),[this](QWidget *w){return idFor(w).isEmpty();}),widgets.end());
            std::stable_sort(widgets.begin(),widgets.end(),[this](QWidget *a,QWidget *b){
                const int ai=channelOrder.indexOf(idFor(a)),bi=channelOrder.indexOf(idFor(b));
                return (ai<0?channelOrder.size():ai)<(bi<0?channelOrder.size():bi);
            });
            arrange(box,widgets);
        }
    }
    void restoreOrder(){
        for(auto &entry:baselines){
            if(!entry.layout)continue;
            const auto current=orderedWidgets(entry.layout);
            QList<QWidget*> desired;
            for(auto &widget:entry.order)if(widget&&current.contains(widget))desired.append(widget);
            for(auto *widget:current)if(!desired.contains(widget))desired.append(widget);
            arrange(entry.layout,desired);
        }
        baselines.clear();
    }
    void updateHandles(){
        for(auto &row:rows)if(row.nameHandle){
            if(orderingUnlocked&&channelBox(row.volume))row.nameHandle->setCursor(Qt::OpenHandCursor);
            else if(row.hadCursor)row.nameHandle->setCursor(row.originalCursor);
            else row.nameHandle->unsetCursor();
        }
    }
    bool moveChannel(QWidget *from,QWidget *to){
        auto *box=channelBox(from);
        if(!box||box!=channelBox(to)||from==to)return false;
        QStringList visible;
        for(auto *widget:orderedWidgets(box)){
            const auto id=idFor(widget);
            if(!id.isEmpty()&&!visible.contains(id))visible.append(id);
        }
        const auto first=idFor(from),second=idFor(to);
        const int a=visible.indexOf(first),b=visible.indexOf(second);
        if(a<0||b<0||a==b)return false;
        const auto previous=channelOrder;
        for(const auto &id:visible)if(!channelOrder.contains(id))channelOrder.append(id);
        visible.move(a,b);
        // Retain slots of hidden / inactive sources.
        int index=0;
        for(auto &id:channelOrder)if(visible.contains(id))id=visible[index++];
        if(!save()){channelOrder=previous;return false;}
        applyOrder();refreshGrid();
        return true;
    }

    struct NamePolicy {
        QPointer<QWidget> label;
        QSizePolicy original;
        int originalMinimumWidth=0;
        int appliedMinimumWidth=0;
    };
    QList<NamePolicy> namePolicies;
    void normalizeSourceNameWidths(){
        if(!native)return;
        for(auto *volume:native->findChildren<QWidget*>()){
            if(!volumeControl(volume)||volume->property("aftermarket-foreign").toBool())
                continue;
            auto *label=sourceNameLabel(volume);
            if(!label)continue;
            const auto already=std::find_if(namePolicies.cbegin(),namePolicies.cend(),
                [label](const NamePolicy &entry){return entry.label==label;});
            if(already!=namePolicies.cend())continue;
            // OBS vertical VolControl already has a 110px max width. Use
            // that native channel budget for its name label rather than
            // inventing a narrower 68px limit (which needlessly shortens
            // ordinary source names). Ignored horizontal sizeHint prevents
            // *name length* from influencing channel width; the minimum
            // avoids OBS's centered vertical name layout collapsing to 0.
            // Uncapped horizontal / third-party layouts can still expand to
            // available width and retain a compact fallback.
            constexpr int uncappedFallbackWidth=68;
            int nameWidth=uncappedFallbackWidth;
            if(volume->maximumWidth()<QWIDGETSIZE_MAX){
                const auto margins=volume->layout()?volume->layout()->contentsMargins():QMargins{};
                nameWidth=qMax(1,volume->maximumWidth()-margins.left()-margins.right());
            }
            const auto original=label->sizePolicy();
            const int previousMinimum=label->minimumWidth();
            const int minimum=qMax(previousMinimum,nameWidth);
            namePolicies.append({label,original,previousMinimum,minimum});
            auto policy=original;
            policy.setHorizontalPolicy(QSizePolicy::Ignored);
            label->setSizePolicy(policy);
            if(minimum!=previousMinimum)label->setMinimumWidth(minimum);
        }
    }
    void restoreSourceNameWidths(){
        for(const auto &entry:namePolicies){
            if(!entry.label)continue;
            auto current=entry.label->sizePolicy();
            // Do not clobber unrelated external changes to vertical policy.
            if(current.horizontalPolicy()==QSizePolicy::Ignored){
                current.setHorizontalPolicy(entry.original.horizontalPolicy());
                entry.label->setSizePolicy(current);
            }
            if(entry.label->minimumWidth()==entry.appliedMinimumWidth)
                entry.label->setMinimumWidth(entry.originalMinimumWidth);
        }
        namePolicies.clear();
    }
    QTimer poll;
    bool useKnobs=false;
    bool busy=false;
    bool load(){
        QFile f(file);
        if(!f.exists())return true;
        if(!f.open(QIODevice::ReadOnly)||f.size()>65536)return false;
        const auto document=QJsonDocument::fromJson(f.readAll());
        if(!document.isObject())return false;
        const auto object=document.object();
        if(object.value("version").toInt()!=1||!object.value("knobs").isBool())return false;
        useKnobs=object.value("knobs").toBool();
        autoGrid=object.value("autoGrid").toBool(true);
        for(const auto &value:object.value("channelOrder").toArray()){
            const auto id=value.toString();
            if(!id.isEmpty()&&!channelOrder.contains(id))channelOrder.append(id);
        }
        return true;
    }
    bool save(){
        QSaveFile f(file);
        if(!f.open(QIODevice::WriteOnly))return false;
        QJsonArray order;
        for(const auto &id:channelOrder)order.append(id);
        const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"knobs",useKnobs},{"autoGrid",autoGrid},{"channelOrder",order}})
            .toJson(QJsonDocument::Indented);
        return f.write(bytes)==bytes.size()&&f.commit();
    }
    void restoreStatus(Row &r){
        if(!r.statusDecorated)return;
        if(r.status&&!r.statusWasHidden)r.status->show();
        if(r.statusHeader){
            for(const auto &entry:r.statusSpacers){
                bool present=false;
                for(int i=0;i<r.statusHeader->count();++i)
                    present|=r.statusHeader->itemAt(i)->spacerItem()==entry.item;
                if(present)entry.item->changeSize(entry.size.width(),entry.size.height(),
                    entry.policy.horizontalPolicy(),entry.policy.verticalPolicy());
            }
            r.statusHeader->invalidate();
        }
        r.statusSpacers.clear();r.statusHeader.clear();
        if(r.nameHandle){r.nameHandle->setProperty("aftermarket-channel-state",QVariant());r.nameHandle->update();}
        if(r.nameText)r.nameText->update();
        r.statusDecorated=false;
    }
    void syncStatus(Row &r){
        if(!r.status||!r.nameText||statusKind(r.status->text())==ChannelStatus::Unknown){restoreStatus(r);return;}
        if(!r.statusDecorated){
            r.statusWasHidden=r.status->isHidden();
            r.statusDecorated=true;
            // The status label remains a live native object. Reclaim its
            // header space, including OBS's expanding status spacer.
            auto *box=containingBox(r.root,r.status);
            r.statusHeader=box;
            if(box)for(int i=0;i<box->count();++i)
                if(auto *spacer=box->itemAt(i)->spacerItem()){
                    r.statusSpacers.append({spacer,spacer->sizeHint(),spacer->sizePolicy()});
                    spacer->changeSize(0,0,QSizePolicy::Fixed,QSizePolicy::Fixed);
                }
            if(box)box->invalidate();
        }
        if(!r.status->isHidden())r.status->hide();
        if(r.nameHandle&&r.nameHandle->property("aftermarket-channel-state").toString()!=r.status->text()){
            r.nameHandle->setProperty("aftermarket-channel-state",r.status->text());
            r.nameHandle->update();r.nameText->update();
        }
    }
    void syncTitleRows(Row &r,bool two){
        if(!r.nameText||!r.nameHandle)return;
        if(two){
            if(!r.twoLineTitle){
                r.titleMin=r.nameHandle->minimumHeight();r.titleMax=r.nameHandle->maximumHeight();
                r.twoLineTitle=true;r.nameHandle->setProperty("aftermarket-two-line-title",true);
            }
            const int h=2*r.nameText->fontMetrics().lineSpacing()+4;
            if(r.nameHandle->minimumHeight()!=h||r.nameHandle->maximumHeight()!=h)r.nameHandle->setFixedHeight(h);
        }else if(r.twoLineTitle){
            r.nameHandle->setMinimumHeight(r.titleMin);r.nameHandle->setMaximumHeight(r.titleMax);
            r.nameHandle->setProperty("aftermarket-two-line-title",QVariant());r.twoLineTitle=false;
        }else return;
        r.nameHandle->update();r.nameText->update();
    }
    void restoreColumnGain(Row &r){
        if(!r.columnGain)return;
        if(r.root&&r.columnBody&&r.columnCenter&&r.columnButtons&&r.frame&&r.db&&r.controls){
            if(r.columnSpacer){
                for(int i=0;i<r.root->count();++i)if(r.root->itemAt(i)==r.columnSpacer){
                    delete r.root->takeAt(i);break;
                }
            }
            r.columnCenter->removeWidget(r.frame);
            r.columnCenter->removeWidget(r.db);
            r.columnBody->removeItem(r.columnButtons);
            r.columnBody->removeWidget(r.controls);
            r.root->removeItem(r.columnBody);
            delete r.columnBody.data();
            r.columnButtons->setDirection(QBoxLayout::LeftToRight);
            for(int i=0;i<r.columnButtonAlignments.size()&&i<r.columnButtons->count();++i)
                r.columnButtons->setAlignment(r.columnButtons->itemAt(i)->widget(),r.columnButtonAlignments[i]);
            r.columnButtons->setSpacing(r.originalButtonSpacing);
            r.appliedButtonSpacing=r.originalButtonSpacing;
            r.columnButtons->setContentsMargins(r.columnButtonMargins);
            r.columnButtons->setAlignment(r.columnButtonsAlignment);
            r.root->insertWidget(3,r.frame,r.frameStretch,r.frameAlignment);
            r.root->insertLayout(4,r.columnButtons,r.columnButtonsStretch);
            r.root->insertWidget(5,r.controls,0,r.controlsAlignment);
            r.header->insertWidget(r.dbIndex,r.db,r.dbStretch,r.dbAlignment);
            if(auto *adjust=qobject_cast<QBoxLayout*>(r.controls->layout())){
                adjust->setDirection(QBoxLayout::LeftToRight);
                if(r.panDial)adjust->setAlignment(r.panDial,Qt::Alignment{});
                if(r.offsetDial)adjust->setAlignment(r.offsetDial,Qt::Alignment{});
            }
            r.controls->setFixedWidth(2*compactDialSize+4);
            r.controls->setMinimumHeight(r.controlsMin.height());
            r.controls->setMaximumHeight(r.controlsMax.height());
            r.controls->setSizePolicy(r.controlsPolicy);
        }
        if(r.volume&&r.volume->minimumWidth()==r.appliedColumnMin)
            r.volume->setMinimumWidth(r.originalColumnMin);
        r.columnBody.clear();r.columnCenter.clear();r.columnSpacer=nullptr;r.columnGain=false;
    }
    void restoreColumn(Row &r){
        if(!r.columnMode)return;
        restoreColumnGain(r);
        syncTitleRows(r,false);
        if(r.columnButtons&&r.columnButtons->spacing()==r.appliedButtonSpacing)
            r.columnButtons->setSpacing(r.originalButtonSpacing);
        r.columnButtons.clear();
        restoreStatus(r);
        if(r.controls){
            if(r.root&&r.root->indexOf(r.controls)>=0)
                r.root->setAlignment(r.controls,r.controlsAlignment);
            r.controls->setMinimumSize(r.controlsMin);r.controls->setMaximumSize(r.controlsMax);
            r.controls->setSizePolicy(r.controlsPolicy);
        }
        r.columnMode=false;r.root.clear();r.header.clear();r.frame.clear();r.db.clear();r.status.clear();
    }
    bool syncColumn(Row &r){
        if(!r.volume||!r.controls||!useKnobs){
            restoreColumn(r);return false;
        }
        auto *root=qobject_cast<QBoxLayout*>(r.volume->layout());
        auto *frame=r.volume->findChild<QWidget*>("volMeterFrame",Qt::FindDirectChildrenOnly);
        QSlider *gain=nullptr;
        if(frame)for(auto *s:frame->findChildren<QSlider*>(QString(),Qt::FindDirectChildrenOnly)){
            if(gain){restoreColumn(r);return false;}gain=s;
        }
        if(!root||root->direction()!=QBoxLayout::TopToBottom||!gain||gain->orientation()!=Qt::Vertical){
            restoreColumn(r);return false;
        }
        if(r.columnMode&&(r.root!=root||r.frame!=frame||!r.db||!r.header))restoreColumn(r);
        if(!r.columnMode){
            // OBS side-by-side channel: separate status, source name, dB,
            // meter frame, mute/monitor row, and our adjustment strip.
            // Refuse unknown or extended layouts.
            if(root->count()!=6||root->itemAt(3)->widget()!=frame||root->itemAt(5)->widget()!=r.controls)return false;
            auto row=[root](int i){return qobject_cast<QBoxLayout*>(root->itemAt(i)->layout());};
            auto *statusRow=row(0),*nameRow=row(1),*dbRow=row(2),*buttons=row(4);
            auto *db=r.volume->findChild<QLabel*>("volLabel",Qt::FindDirectChildrenOnly);
            if(!statusRow||statusRow->count()!=1||!nameRow||nameRow->count()!=1||
               nameRow->itemAt(0)->widget()!=r.nameHandle||!dbRow||!db||dbRow->indexOf(db)<0||
               !buttons||buttons->direction()!=QBoxLayout::LeftToRight||buttons->count()!=2||
               !qobject_cast<QAbstractButton*>(buttons->itemAt(0)->widget())||
               !qobject_cast<QAbstractButton*>(buttons->itemAt(1)->widget()))return false;
            auto *status=qobject_cast<QLabel*>(statusRow->itemAt(0)->widget());
            if(!status)return false;
            r.columnMode=true;r.root=root;r.header=dbRow;r.frame=frame;r.db=db;r.status=status;
            r.frameStretch=root->stretch(3);r.frameAlignment=root->itemAt(3)->alignment();
            r.dbIndex=dbRow->indexOf(db);r.dbStretch=dbRow->stretch(r.dbIndex);r.dbAlignment=dbRow->itemAt(r.dbIndex)->alignment();
            r.controlsMin=r.controls->minimumSize();r.controlsMax=r.controls->maximumSize();
            r.controlsPolicy=r.controls->sizePolicy();r.controlsAlignment=root->itemAt(5)->alignment();
            // Keep the native buttons centered over their respective
            // encoder. In compact themes their own width may be only 18px.
            r.columnButtons=buttons;
            r.originalButtonSpacing=buttons->spacing();
            r.appliedButtonSpacing=r.originalButtonSpacing;
            r.controls->setFixedWidth(2*compactDialSize+4);root->setAlignment(r.controls,Qt::AlignHCenter);
        }
        if(!r.columnGain&&r.columnButtons&&r.columnButtons->count()==2){
            auto *left=r.columnButtons->itemAt(0)->widget(),*right=r.columnButtons->itemAt(1)->widget();
            if(left&&right&&r.columnButtons->spacing()==r.appliedButtonSpacing){
                const int spacing=qMax(r.originalButtonSpacing,compactDialSize+2-(left->width()+right->width())/2);
                if(spacing!=r.appliedButtonSpacing){r.columnButtons->setSpacing(spacing);r.appliedButtonSpacing=spacing;}
            }
        }
        syncTitleRows(r,true);
        syncStatus(r);
        const bool integrated=gain->property("_obsUiAugmentKnob").toBool()&&gain->property("_obsUiAugmentIntegratedMeter").toBool();
        if(!integrated){restoreColumnGain(r);return true;}
        if(!r.columnGain){
            const int frameIndex=root->indexOf(frame);
            auto *buttons=r.columnButtons.data();
            auto *adjust=qobject_cast<QBoxLayout*>(r.controls->layout());
            if(frameIndex!=3||!buttons||!adjust||r.header->indexOf(r.db)<0||
               root->indexOf(r.controls)!=5)return true;
            // Compact side-by-side channels: buttons above the gain face,
            // adjustment dials below it. Preserve native strip width.
            r.originalColumnMin=r.volume->minimumWidth();
            r.columnButtonsStretch=root->stretch(4);
            r.columnButtonsAlignment=buttons->alignment();
            r.columnButtonMargins=buttons->contentsMargins();
            r.columnButtonAlignments.clear();
            for(int i=0;i<buttons->count();++i)r.columnButtonAlignments.append(buttons->itemAt(i)->alignment());
            root->removeWidget(frame);root->removeItem(buttons);
            root->removeWidget(r.controls);r.header->removeWidget(r.db);
            auto *body=new QVBoxLayout;
            body->setObjectName("aftermarket-column-side-controls");
            body->setContentsMargins(0,0,0,0);body->setSpacing(2);
            auto *middle=new QVBoxLayout;
            middle->setContentsMargins(0,0,0,0);middle->setSpacing(0);
            middle->addWidget(frame,0,Qt::AlignHCenter);
            middle->addWidget(r.db,0,Qt::AlignHCenter);
            buttons->setDirection(QBoxLayout::LeftToRight);
            auto buttonWidth=[buttons](int index){
                auto *w=buttons->itemAt(index)->widget();
                return qBound(w->minimumWidth(),w->sizeHint().width(),w->maximumWidth());
            };
            constexpr int faceDiameter=62; // integrated 68px knob minus 6px inset
            buttons->setSpacing(qMax(2,faceDiameter-buttonWidth(0)-buttonWidth(1)));
            buttons->setContentsMargins(0,0,0,0);
            buttons->setAlignment(Qt::AlignHCenter);
            for(int i=0;i<buttons->count();++i)buttons->setAlignment(buttons->itemAt(i)->widget(),Qt::AlignVCenter);
            adjust->setDirection(QBoxLayout::LeftToRight);
            r.controls->setFixedSize(2*compactDialSize+4,compactDialSize+2);
            r.controls->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
            adjust->setAlignment(r.panDial,Qt::AlignVCenter);
            adjust->setAlignment(r.offsetDial,Qt::AlignVCenter);
            body->addLayout(buttons);
            body->addLayout(middle);
            body->addWidget(r.controls,0,Qt::AlignHCenter);
            root->insertLayout(frameIndex,body);
            root->addStretch(1);r.columnSpacer=root->itemAt(root->count()-1)->spacerItem();
            r.columnBody=body;r.columnCenter=middle;r.columnGain=true;
            // Native Maximum size policy can allocate a short-name strip
            // below the rotary footprint. Measure required content instead
            // of using current width as an eligibility test (which leaves
            // an already-integrated gain floating in the full-height row).
            const auto margins=root->contentsMargins();
            const int minWidth=qMax(80,qMax(frame->minimumSizeHint().width(),
                r.controls->width())+margins.left()+margins.right());
            r.appliedColumnMin=qMax(r.originalColumnMin,qMin(r.volume->maximumWidth(),minWidth));
            r.volume->setMinimumWidth(r.appliedColumnMin);
            r.volume->updateGeometry();
        }
        return true;
    }
    void restoreRotaryEdges(Row &r){
        if(!r.rotaryEdges)return;
        if(r.rotaryButtons){
            r.rotaryButtons->setSpacing(r.rotaryButtonSpacing);
            r.rotaryButtons->setContentsMargins(r.rotaryButtonMargins);
            r.rotaryButtons->setAlignment(r.rotaryButtonAlignment);
            for(int i=0;i<r.rotaryButtonItemAlignments.size()&&i<r.rotaryButtons->count();++i)
                if(auto *w=r.rotaryButtons->itemAt(i)->widget())
                    r.rotaryButtons->setAlignment(w,r.rotaryButtonItemAlignments[i]);
        }
        r.rotaryEdges=false;
    }
    void syncRotaryPairSpacing(Row &r){
        auto *buttons=r.rotaryButtons.data();
        if(!r.rotaryGrid||!buttons||buttons->count()!=2)return;
        auto *first=buttons->itemAt(0)->widget(),*second=buttons->itemAt(1)->widget();
        if(!first||!second)return;
        auto height=[](QWidget *w){return qBound(w->minimumHeight(),w->sizeHint().height(),w->maximumHeight());};
        constexpr int diameter=62;
        const int gap=qMax(2,diameter-height(first)-height(second));
        if(buttons->spacing()!=gap)buttons->setSpacing(gap);
    }
    void restoreRotary(Row &r){
        restoreColumn(r);
        restoreStatus(r);
        if(!r.center)return;
        restoreRotaryEdges(r);
        if(r.root&&r.body&&r.header&&r.frame&&r.db&&r.controls){
            r.center->removeWidget(r.frame);
            if(r.rotaryGrid){
                r.rotaryGrid->removeWidget(r.controls);
                r.rotaryGrid->removeWidget(r.db);
                r.rotaryGrid->removeItem(r.center);
                r.rotaryGrid->removeItem(r.rotaryButtons);
                r.body->removeItem(r.rotaryGrid);
                delete r.rotaryGrid.data();
                if(r.rotaryButtons)r.body->insertLayout(0,r.rotaryButtons,r.rotaryButtonsStretch);
                delete r.center.data();
            }else{
                r.body->removeWidget(r.controls);
                r.center->removeWidget(r.db);
                r.body->removeItem(r.center);delete r.center.data();
            }
            r.body->insertWidget(r.frameIndex,r.frame,r.frameStretch,r.frameAlignment);
            r.header->insertWidget(r.dbIndex,r.db,r.dbStretch,r.dbAlignment);
            r.root->insertWidget(r.controlsIndex,r.controls,r.controlsStretch,r.controlsAlignment);
            r.controls->setMinimumSize(r.controlsMin);r.controls->setMaximumSize(r.controlsMax);
            r.controls->setSizePolicy(r.controlsPolicy);
            if(auto *box=qobject_cast<QBoxLayout*>(r.controls->layout())){
                box->setDirection(QBoxLayout::LeftToRight);
                if(r.panDial)box->setAlignment(r.panDial,Qt::Alignment{});
                if(r.offsetDial)box->setAlignment(r.offsetDial,Qt::Alignment{});
            }
        }
        if(r.nameRow&&r.nameHandle&&r.nameIndex>=0&&r.nameRow->indexOf(r.nameHandle)==r.nameIndex)
            r.nameRow->setStretch(r.nameIndex,r.nameStretch);
        r.nameRow.clear();r.nameIndex=-1;
        r.rotaryButtons.clear();r.rotaryGrid.clear();
        r.center.clear();r.root.clear();r.body.clear();r.header.clear();r.frame.clear();r.db.clear();r.status.clear();
    }
    bool syncRotary(Row &r){
        if(!r.volume||!r.controls||!useKnobs||r.volume->width()<145){restoreRotary(r);return false;}
        auto *frame=r.volume->findChild<QWidget*>("volMeterFrame",Qt::FindDirectChildrenOnly);
        QSlider *gain=nullptr;
        if(frame)for(auto *s:frame->findChildren<QSlider*>(QString(),Qt::FindDirectChildrenOnly)){
            if(gain){restoreRotary(r);return false;}gain=s;
        }
        if(!gain||gain->orientation()!=Qt::Horizontal||!gain->property("_obsUiAugmentKnob").toBool()){
            restoreRotary(r);return false;
        }
        const bool integrated=gain->property("_obsUiAugmentIntegratedMeter").toBool();
        if(r.center&&bool(r.rotaryGrid)!=integrated)restoreRotary(r);
        if(r.center){syncStatus(r);syncRotaryPairSpacing(r);return true;}
        auto *root=qobject_cast<QBoxLayout*>(r.volume->layout());
        auto *db=r.volume->findChild<QLabel*>("volLabel",Qt::FindDirectChildrenOnly);
        auto *body=containingBox(root,frame),*header=containingBox(root,db);
        if(!root||!db||!body||!header||root->direction()!=QBoxLayout::TopToBottom||
           body==header||body->direction()!=QBoxLayout::LeftToRight||
           header->direction()!=QBoxLayout::LeftToRight||body->count()!=2||
           body->itemAt(1)->widget()!=frame||root->indexOf(r.controls)<0)return false;
        auto *left=qobject_cast<QBoxLayout*>(body->itemAt(0)->layout());
        if(!left||left->direction()!=QBoxLayout::TopToBottom||left->count()!=2||
           !qobject_cast<QAbstractButton*>(left->itemAt(0)->widget())||
           !qobject_cast<QAbstractButton*>(left->itemAt(1)->widget()))return false;
        QLabel *status=nullptr;
        for(int i=0;i<header->count();++i)
            if(auto *label=qobject_cast<QLabel*>(header->itemAt(i)->widget());label&&label!=db){
                if(status)return false;status=label;
            }
        auto *adjust=qobject_cast<QBoxLayout*>(r.controls->layout());
        if(!adjust)return false;
        r.root=root;r.body=body;r.header=header;r.frame=frame;r.db=db;r.status=status;
        r.rotaryButtons=left;
        r.frameIndex=body->indexOf(frame);r.frameStretch=body->stretch(r.frameIndex);r.frameAlignment=body->itemAt(r.frameIndex)->alignment();
        r.dbIndex=header->indexOf(db);r.dbStretch=header->stretch(r.dbIndex);r.dbAlignment=header->itemAt(r.dbIndex)->alignment();
        r.controlsIndex=root->indexOf(r.controls);r.controlsStretch=root->stretch(r.controlsIndex);r.controlsAlignment=root->itemAt(r.controlsIndex)->alignment();
        r.controlsMin=r.controls->minimumSize();r.controlsMax=r.controls->maximumSize();r.controlsPolicy=r.controls->sizePolicy();
        // Allocate the reclaimed header width to the existing source name.
        // Its Ignored policy still keeps long names from widening channels.
        if(auto *names=containingBox(root,r.nameHandle);names&&names!=header&&
            names->direction()==QBoxLayout::LeftToRight){
            const int index=names->indexOf(r.nameHandle);
            bool sibling=false;
            for(int i=0;i<names->count();++i)if(names->itemAt(i)->layout()==header)sibling=true;
            if(index>=0&&sibling){
                r.nameRow=names;r.nameIndex=index;r.nameStretch=names->stretch(index);
                names->setStretch(index,1);
            }
        }
        header->removeWidget(db);body->removeWidget(frame);root->removeWidget(r.controls);
        auto *center=new QVBoxLayout;center->setObjectName("aftermarket-rotary-center");
        center->setContentsMargins(0,0,0,0);center->setSpacing(0);
        center->addWidget(frame,1);
        r.center=center;
        adjust->setDirection(QBoxLayout::TopToBottom);
        adjust->setAlignment(r.panDial,Qt::AlignHCenter);
        adjust->setAlignment(r.offsetDial,Qt::AlignHCenter);
        if(integrated){
            // One real layout row for the gain face and both side pairs.
            // The readout has its own row, so it cannot skew alignment.
            r.rotaryButtonSpacing=left->spacing();
            r.rotaryButtonMargins=left->contentsMargins();
            r.rotaryButtonAlignment=left->alignment();
            r.rotaryButtonItemAlignments={left->itemAt(0)->alignment(),left->itemAt(1)->alignment()};
            r.rotaryButtonsStretch=body->stretch(0);r.rotaryEdges=true;
            body->removeItem(left);
            left->setContentsMargins(0,0,0,0);
            left->setAlignment(Qt::AlignCenter);
            for(int i=0;i<left->count();++i)left->setAlignment(left->itemAt(i)->widget(),Qt::AlignCenter);
            auto *grid=new QGridLayout;
            grid->setObjectName("aftermarket-rotary-pairs");
            grid->setContentsMargins(0,0,0,0);grid->setHorizontalSpacing(2);grid->setVerticalSpacing(0);
            grid->addLayout(left,0,0,Qt::AlignCenter);
            grid->addLayout(center,0,1);
            r.controls->setFixedSize(compactDialSize+4,2*compactDialSize+4);
            r.controls->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
            grid->addWidget(r.controls,0,2,Qt::AlignCenter);
            grid->addWidget(db,1,1,Qt::AlignHCenter);
            grid->setColumnStretch(1,1);
            body->addLayout(grid,1);r.rotaryGrid=grid;
            syncRotaryPairSpacing(r);
        }else{
            center->addWidget(db,0,Qt::AlignHCenter);
            body->insertLayout(r.frameIndex,center,1);
            r.controls->setMinimumSize(compactDialSize+4,0);
            r.controls->setMaximumSize(compactDialSize+4,QWIDGETSIZE_MAX);
            r.controls->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Expanding);
            body->addWidget(r.controls);
        }
        syncStatus(r);
        return true;
    }
    void remove(Row &r){
        restoreRotary(r);
        syncTitleRows(r,false);
        if(r.nameText)r.nameText->removeEventFilter(owner);
        if(r.nameHandle){
            r.nameHandle->update();
            if(r.hadCursor)r.nameHandle->setCursor(r.originalCursor);
            else r.nameHandle->unsetCursor();
        }
        if(r.controls)delete r.controls.data();
    }
    void findAndHideFooter(){
        if(!native)return;
        // Match the actual OBS AudioMixer toolbar, never arbitrary dock
        // toolbars. Unknown third-party buttons leave the native row intact.
        for(auto *candidate:native->findChildren<QToolBar*>(
                QString(),Qt::FindDirectChildrenOnly)){
            QPushButton *hidden=nullptr,*options=nullptr;
            QToolButton *orientation=nullptr,*advanced=nullptr;
            int recognized=0,unknown=0;
            for(auto *child:candidate->findChildren<QAbstractButton*>(
                    QString(),Qt::FindDirectChildrenOnly)){
                if(child->objectName()=="qt_toolbar_ext_button")continue;
                if(auto *push=qobject_cast<QPushButton*>(child)){
                    if(push->text().contains("hidden",Qt::CaseInsensitive)){
                        hidden=push;++recognized;
                    }else if(push->text()=="Options"){options=push;++recognized;}
                    else ++unknown;
                }else if(auto *tool=qobject_cast<QToolButton*>(child)){
                    if(tool->text().contains("Advanced Audio Properties",
                                           Qt::CaseInsensitive)){
                        advanced=tool;++recognized;
                    }else if(!orientation){orientation=tool;++recognized;}
                    else ++unknown;
                }else ++unknown;
            }
            if(recognized!=4||unknown||!hidden||!options||
               !orientation||!advanced)continue;
            footer=candidate;
            hiddenButton=hidden;optionsButton=options;
            layoutButton=orientation;advancedButton=advanced;
            footerWasHidden=footer->isHidden();
            // OBS owns the QMenu and checkboxes; reuse them as a submenu
            // rather than cloning native actions or modifying preferences.
            for(auto *menu:native->findChildren<QMenu*>(
                    QString(),Qt::FindDirectChildrenOnly)){
                bool isMixerOptions=false;
                for(auto *action:menu->actions())
                    if(action->text().contains("Show hidden sources",
                                                Qt::CaseInsensitive))
                        isMixerOptions=true;
                if(!isMixerOptions)
                    for(auto *check:menu->findChildren<QAbstractButton*>())
                        if(check->text().contains("Show hidden sources",
                                                   Qt::CaseInsensitive))
                            isMixerOptions=true;
                if(isMixerOptions){
                    nativeOptions=menu;
                    nativeOptionsTitle=menu->title();
                    break;
                }
            }
            // Keep the original visible until the native title menu has
            // actually been placed. Floating/overfull titles use the footer.
            break;
        }
    }
    void syncFooterVisibility(){
        if(!footer)return;
        if(footerWasHidden||(menuButton&&menuButton->isVisible()))
            footer->hide();
        else footer->show();
    }
    void restoreFooter(QObject *owner){
        if(menuButton)menuButton->removeEventFilter(owner);
        menuButton.clear();
        if(nativeOptions){
            nativeOptions->setTitle(nativeOptionsTitle);
            nativeOptions.clear();
        }
        if(footer&&!footerWasHidden)footer->show();
        footer.clear();
        hiddenButton.clear();optionsButton.clear();
        layoutButton.clear();advancedButton.clear();
    }
    void syncLayout(Row &r){
        if(!r.controls)return;
        const bool compact=syncColumn(r)||syncRotary(r);
        if(compact){
            if(r.panLabel)r.panLabel->hide();
            if(r.offsetLabel)r.offsetLabel->hide();
            if(r.panSlider)r.panSlider->hide();
            if(r.panDial)r.panDial->show();
            if(r.offsetDial)r.offsetDial->show();
            if(r.offsetSpin)r.offsetSpin->hide();
            return;
        }
        const bool narrow=r.volume&&r.volume->width()<145;
        if(r.panLabel)r.panLabel->setVisible(!narrow);
        if(r.offsetLabel)r.offsetLabel->setVisible(!narrow);
        if(r.panSlider)r.panSlider->setVisible(!useKnobs&&!narrow);
        if(r.panDial)r.panDial->setVisible(useKnobs||narrow);
        if(r.offsetDial)r.offsetDial->setVisible(useKnobs&&!narrow);
        if(r.offsetSpin){
            r.offsetSpin->show();
            r.offsetSpin->setMinimumWidth(narrow?46:54);
            r.offsetSpin->setMaximumWidth(narrow?58:78);
        }
    }
    void apply(Row &r,const AudioPropertyState &source){
        if(!r.controls)return;
        const int panValue=qBound(0,qRound(source.balance*100),100);
        if(r.panSlider&&!r.panSlider->isSliderDown()){
            const QSignalBlocker guard(r.panSlider);
            r.panSlider->setValue(panValue);
        }
        if(r.panDial&&!r.panDial->isSliderDown()){
            const QSignalBlocker guard(r.panDial);
            r.panDial->setValue(panValue);
        }
        if(r.offsetSpin&&!r.offsetSpin->hasFocus()){
            const QSignalBlocker guard(r.offsetSpin);
            r.offsetSpin->setValue(qBound(-maxOffsetMs,source.offsetMs,maxOffsetMs));
        }
        if(r.offsetDial&&!r.offsetDial->isSliderDown()){
            const QSignalBlocker guard(r.offsetDial);
            r.offsetDial->setValue(qBound(-maxOffsetMs,source.offsetMs,maxOffsetMs));
        }
        if(r.panSlider)r.panSlider->setEnabled(source.stereo);
        if(r.panDial)r.panDial->setEnabled(source.stereo);
        if(r.panDial){
            const auto tip=QString("Stereo pan: %1% (50% center)").arg(panValue);
            if(r.panDial->toolTip()!=tip+" · Double-click: reset · Right-click: options")r.panDial->setToolTip(tip+" · Double-click: reset · Right-click: options");
        }
        if(r.offsetDial){
            const auto tip=QString("Sync offset: %1 ms · drag up/down or scroll · Shift: fine · right-click: exact value").arg(source.offsetMs);
            if(r.offsetDial->toolTip()!=tip+" · Double-click: reset to 0 ms")r.offsetDial->setToolTip(tip+" · Double-click: reset to 0 ms");
        }
        syncLayout(r);
    }
};
FrontAudioControls::FrontAudioControls(QString settingsFile,Query q,SetPan p,
                                       SetOffset o,std::function<void()> persist,QObject *parent)
    :QObject(parent),state(new State){
    state->owner=this;
    state->file=std::move(settingsFile);
    state->query=std::move(q);
    state->pan=std::move(p);
    state->offset=std::move(o);
    state->persist=std::move(persist);
    state->saveDebounce.setInterval(320);
    state->saveDebounce.setSingleShot(true);
    connect(&state->saveDebounce,&QTimer::timeout,this,[this]{
        if(state->persist)state->persist();
    });
    state->load();
    state->gridDebounce.setInterval(65);
    state->gridDebounce.setSingleShot(true);
    connect(&state->gridDebounce,&QTimer::timeout,this,[this]{
        if(state->native&&!state->busy&&!state->dragHandle)state->refreshGrid();
    });
    state->poll.setInterval(900);
    connect(&state->poll,&QTimer::timeout,this,[this]{
        if(state->native&&state->native->isVisible())refresh();
    });
}
FrontAudioControls::~FrontAudioControls(){detach();}
bool FrontAudioControls::attach(QWidget *nativeMixer){
    if(state->native||!nativeMixer)return false;
    state->native=nativeMixer;
    nativeMixer->installEventFilter(this);
    state->findAndHideFooter();
    refresh();
    state->poll.start();
    return true;
}
void FrontAudioControls::detach(){
    state->poll.stop();
    state->gridDebounce.stop();
    if(state->native)state->native->removeEventFilter(this);
    state->restoreGrid();
    state->orderingUnlocked=false;
    state->dragHandle.clear();
    state->restoreOrder();
    state->restoreSourceNameWidths();
    state->restoreFooter(this);
    for(auto &row:state->rows){
        if(row.nameHandle)row.nameHandle->removeEventFilter(this);
        state->remove(row);
    }
    state->rows.clear();
    state->native.clear();
}
bool FrontAudioControls::knobs() const{return state->useKnobs;}
bool FrontAudioControls::setKnobs(bool enabled){
    if(state->useKnobs==enabled)return false;
    state->useKnobs=enabled;
    for(auto &row:state->rows)state->syncLayout(row);
    state->refreshGrid();
    state->save();
    return true;
}
int FrontAudioControls::rowCount()const{return state->rows.size();}
QStringList FrontAudioControls::channelOrder()const{return state->channelOrder;}
bool FrontAudioControls::setMenuButton(QToolButton *button){
    if(!state->native||!state->footer||!button)return false;
    if(state->menuButton)state->menuButton->removeEventFilter(this);
    state->menuButton=button;
    button->installEventFilter(this);
    state->syncFooterVisibility();
    return true;
}
bool FrontAudioControls::eventFilter(QObject *watched,QEvent *event){
    if(event->type()==QEvent::Paint){
        for(const auto &row:state->rows)
            if(row.nameHandle==watched&&paintVolumeNameMenu(qobject_cast<QAbstractButton*>(watched)))return true;
        for(const auto &row:state->rows)
            if(row.nameText==watched&&paintChannelTitle(row.nameText,qobject_cast<QAbstractButton*>(row.nameHandle)))return true;
    }
    if(watched==state->native&&(event->type()==QEvent::Resize||event->type()==QEvent::Show))
        state->gridDebounce.start();
    if(watched==state->menuButton&&
       (event->type()==QEvent::Show||event->type()==QEvent::Hide)){
        QTimer::singleShot(0,this,[this]{state->syncFooterVisibility();});
    }
    if(state->orderingUnlocked){
        QWidget *volume=nullptr;
        for(const auto &row:state->rows)if(row.nameHandle==watched&&row.volume){
            volume=row.volume;break;
        }
        if(volume&&State::channelBox(volume)){
            auto *handle=static_cast<QWidget*>(watched);
            if(event->type()==QEvent::MouseButtonPress){
                auto *mouse=static_cast<QMouseEvent*>(event);
                if(mouse->button()==Qt::LeftButton){
                    state->dragHandle=handle;
                    state->dragStart=mouse->globalPosition().toPoint();
                    state->dragging=false;
                    return true;
                }
            }
            if(state->dragHandle==handle&&event->type()==QEvent::MouseMove){
                auto *mouse=static_cast<QMouseEvent*>(event);
                if((mouse->globalPosition().toPoint()-state->dragStart).manhattanLength()>=QApplication::startDragDistance()){
                    state->dragging=true;handle->setCursor(Qt::ClosedHandCursor);
                }
                return true;
            }
            if(state->dragHandle==handle&&event->type()==QEvent::MouseButtonRelease){
                auto *mouse=static_cast<QMouseEvent*>(event);
                if(mouse->button()==Qt::LeftButton){
                    const bool moved=state->dragging;
                    state->dragHandle.clear();state->dragging=false;
                    handle->setCursor(Qt::OpenHandCursor);
                    if(moved){
                        const auto point=mouse->globalPosition().toPoint();
                        for(const auto &row:state->rows){
                            auto *target=row.volume.data();
                            if(target&&target!=volume&&target->isVisible()&&
                               target->rect().contains(target->mapFromGlobal(point))){
                                state->moveChannel(volume,target);break;
                            }
                        }
                    }
                    return true;
                }
            }
        }
    }
    return QObject::eventFilter(watched,event);
}
void FrontAudioControls::populateSettingsMenu(QMenu *menu){
    if(!menu||!state->query)return;
    if(state->footer){
        auto addProxy=[menu](const QString &label,QAbstractButton *button){
            QPointer<QAbstractButton> safe=button;
            auto *action=menu->addAction(label);
            action->setEnabled(bool(safe));
            QObject::connect(action,&QAction::triggered,menu,
                             [safe]{if(safe)safe->click();});
            return action;
        };
        addProxy(state->hiddenButton?
                 state->hiddenButton->text():QStringLiteral("Hidden sources"),
                 state->hiddenButton);
        addProxy("Switch mixer layout",state->layoutButton);
        addProxy("Advanced Audio Properties",state->advancedButton);
        if(state->nativeOptions){
            // QMenu::menuAction is OBS-owned and not deleted by our popup.
            if(state->nativeOptions->title().isEmpty())
                state->nativeOptions->setTitle("Mixer options");
            menu->addMenu(state->nativeOptions);
        }else addProxy("Mixer options",state->optionsButton);
        menu->addSeparator();
    }
    auto *autoGrid=menu->addAction("Auto mixer grid");
    autoGrid->setCheckable(true);autoGrid->setChecked(state->autoGrid);
    autoGrid->setToolTip("Wrap mixer channels into a grid when the dock has room; full-height faders remain in their native row.");
    connect(autoGrid,&QAction::triggered,this,[this,autoGrid](bool enabled){
        const bool previous=state->autoGrid;
        state->autoGrid=enabled;
        if(!state->save()){state->autoGrid=previous;autoGrid->setChecked(previous);return;}
        state->refreshGrid();
    });
    auto *unlock=menu->addAction("Unlock channel ordering");
    unlock->setCheckable(true);
    unlock->setChecked(state->orderingUnlocked);
    unlock->setToolTip("Drag a channel name onto another channel; lock again to prevent changes.");
    connect(unlock,&QAction::triggered,this,[this](bool enabled){
        state->orderingUnlocked=enabled;
        state->dragHandle.clear();state->dragging=false;
        state->updateHandles();
    });
    auto *reset=menu->addAction("Reset channel order");
    reset->setEnabled(!state->channelOrder.isEmpty());
    connect(reset,&QAction::triggered,this,[this]{
        const auto before=state->channelOrder;
        state->channelOrder.clear();
        if(state->save()){state->restoreOrder();state->refreshGrid();}
        else state->channelOrder=before;
    });
    auto *channels=menu->addMenu("Channel settings");
    channels->setObjectName("aftermarket-channel-settings");
    const auto sources=state->query();
    if(sources.isEmpty()){
        channels->setEnabled(false);
        return;
    }
    for(const auto &source:sources){
        if(source.id.isEmpty()||source.name.isEmpty())continue;
        auto *sub=channels->addMenu(source.name);
        sub->setObjectName("aftermarket-channel-"+source.id);
        auto *row=new QWidget(sub);
        auto *layout=new QHBoxLayout(row);
        layout->setContentsMargins(7,4,7,4);
        layout->setSpacing(6);
        layout->addWidget(new QLabel("Pan",row));
        auto *pan=new QSpinBox(row);
        pan->setObjectName("aftermarket-menu-pan");
        pan->setRange(0,100);
        pan->setSuffix("%");
        pan->setValue(qBound(0,qRound(source.balance*100),100));
        pan->setEnabled(source.stereo);
        pan->setToolTip(source.stereo?"Native OBS balance: 0 left, 50 center, 100 right":
                                      "Pan unavailable for this mono source");
        layout->addWidget(pan);
        layout->addWidget(new QLabel("Sync",row));
        auto *offset=new QSpinBox(row);
        offset->setObjectName("aftermarket-menu-offset");
        offset->setRange(-maxOffsetMs,maxOffsetMs);
        offset->setSingleStep(10);
        offset->setSuffix("ms");
        offset->setValue(source.offsetMs);
        layout->addWidget(offset);
        auto *action=new QWidgetAction(sub);
        action->setDefaultWidget(row);
        sub->addAction(action);
        const QString uuid=source.id;
        connect(pan,&QSpinBox::valueChanged,this,[this,uuid](int v){
            if(state->pan&&state->pan(uuid,v/100.0)){
                state->saveDebounce.start();
                refresh();
            }
        });
        connect(offset,&QSpinBox::valueChanged,this,[this,uuid](int v){
            if(state->offset&&state->offset(uuid,v)){
                state->saveDebounce.start();
                refresh();
            }
        });
    }
}
void FrontAudioControls::refresh(){
    if(!state->native||state->busy)return;
    state->normalizeSourceNameWidths();
    if(!state->query)return;
    state->busy=true;
    const auto available=state->query();
    QMap<QString,AudioPropertyState> byName;
    QSet<QString> ambiguous;
    for(const auto &src:available){
        if(src.id.isEmpty()||src.name.isEmpty())continue;
        if(byName.contains(src.name))ambiguous.insert(src.name);
        byName.insert(src.name,src);
    }
    for(const auto &name:ambiguous)byName.remove(name);
    QList<QWidget*> candidates;
    for(auto *widget:state->native->findChildren<QWidget*>())
        if(volumeControl(widget)&&!widget->property("aftermarket-foreign").toBool())
            candidates.append(widget);
    QSet<QWidget*> seen;
    for(auto *widget:candidates){
        const QString name=sourceName(widget);
        if(!byName.contains(name))continue;
        const auto src=byName.value(name);
        if(!widget->layout())continue;
        State::Row *existing=nullptr;
        for(auto &r:state->rows)if(r.volume==widget){
            existing=&r;
            break;
        }
        if(existing&&existing->id!=src.id){
            if(existing->nameHandle)existing->nameHandle->removeEventFilter(this);
            state->remove(*existing);
            existing->controls.clear();
        }
        if(!existing||!existing->controls){
            auto *controls=new QWidget(widget);
            controls->setObjectName("aftermarket-source-adjustments");
            controls->setProperty("aftermarket-source-uuid",src.id);
            // One horizontal strip avoids stacking two additional control
            // rows under each OBS rotary/fader, including narrow vertical docks.
            controls->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
            controls->setMinimumHeight(34);
            controls->setMaximumHeight(36);
            auto *outer=new QHBoxLayout(controls);
            outer->setContentsMargins(1,1,1,1);
            outer->setSpacing(2);
            auto *panLabel=new QLabel("Pan",controls);
            QFont panFont=panLabel->font();
            panFont.setWeight(QFont::Normal);
            if(panFont.pointSizeF()>0)
                panFont.setPointSizeF(qMax(7.0,panFont.pointSizeF()-0.75));
            panLabel->setFont(panFont);
            panLabel->setMaximumWidth(25);
            outer->addWidget(panLabel);
            auto *pan=new QSlider(Qt::Horizontal,controls);
            pan->setObjectName("aftermarket-pan-slider");
            pan->setRange(0,100);
            pan->setMinimumWidth(22);
            pan->setFixedHeight(20);
            pan->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
            pan->setToolTip("Stereo pan · L 0 · Center 50 · R 100");
            outer->addWidget(pan,1);
            auto *panDial=new CompactRotary(controls);
            panDial->setObjectName("aftermarket-pan-dial");
            panDial->setRange(0,100);
            panDial->setNotchesVisible(false);
            panDial->setFixedSize(compactDialSize,compactDialSize);
            panDial->setToolTip(pan->toolTip());
            outer->addWidget(panDial);
            auto *offsetLabel=new QLabel("Δ",controls);
            offsetLabel->setFixedWidth(12);
            offsetLabel->setToolTip("Native OBS sync offset (milliseconds)");
            outer->addWidget(offsetLabel);
            auto *offsetDial=new SyncEncoder(controls);
            offsetDial->setObjectName("aftermarket-offset-dial");
            offsetDial->setFixedSize(compactDialSize,compactDialSize);
            offsetDial->setToolTip("Relative sync encoder · drag or scroll · Shift: fine");
            outer->addWidget(offsetDial);
            auto *offset=new QSpinBox(controls);
            offset->setObjectName("aftermarket-offset-ms");
            offset->setRange(-maxOffsetMs,maxOffsetMs);
            offset->setSingleStep(10);
            offset->setFixedHeight(23);
            offset->setSuffix("ms");
            offset->setMinimumWidth(46);
            offset->setMaximumWidth(78);
            offset->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
            offset->setToolTip("Precise OBS audio sync offset in milliseconds");
            outer->addWidget(offset);
            panDial->setAccessibleName("Stereo pan");
            offsetDial->setAccessibleName("Audio sync offset");
            // Precise editing stays reachable in floating docks whose
            // native title cannot host Aftermarket's overflow menu.
            panDial->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(panDial,&QWidget::customContextMenuRequested,panDial,[panDial](QPoint point){
                auto *menu=new QMenu(panDial);
                menu->setObjectName("aftermarket-pan-menu");
                auto *reset=menu->addAction("Reset pan to center (50%)");
                reset->setObjectName("aftermarket-reset-pan");
                reset->setEnabled(panDial->isEnabled()&&panDial->value()!=50);
                QObject::connect(reset,&QAction::triggered,panDial,[panDial]{if(panDial->isEnabled())panDial->setValue(50);});
                QObject::connect(menu,&QMenu::aboutToHide,menu,&QObject::deleteLater);
                menu->popup(panDial->mapToGlobal(point));
            });
            offsetDial->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(offsetDial,&QWidget::customContextMenuRequested,offsetDial,[offsetDial,safe=QPointer<QSpinBox>(offset)](QPoint point){
                if(!safe)return;
                auto *menu=new QMenu(offsetDial);
                menu->setObjectName("aftermarket-precise-sync-menu");
                auto *editor=new QSpinBox(menu);
                editor->setObjectName("aftermarket-precise-sync-value");
                editor->setRange(-maxOffsetMs,maxOffsetMs);editor->setSingleStep(10);
                editor->setSuffix(" ms");editor->setValue(safe->value());
                editor->setAccessibleName("Precise audio sync offset");
                auto *action=new QWidgetAction(menu);action->setDefaultWidget(editor);menu->addAction(action);
                QObject::connect(editor,&QSpinBox::valueChanged,menu,[safe](int value){if(safe)safe->setValue(value);});
                auto *reset=menu->addAction("Reset sync to default (0 ms)");
                reset->setObjectName("aftermarket-reset-sync");
                reset->setEnabled(offsetDial->isEnabled()&&offsetDial->value()!=0);
                QObject::connect(reset,&QAction::triggered,offsetDial,[offsetDial]{if(offsetDial->isEnabled())offsetDial->setValue(0);});
                QObject::connect(editor,&QSpinBox::valueChanged,reset,[reset,offsetDial](int value){reset->setEnabled(offsetDial->isEnabled()&&value!=0);});
                QObject::connect(menu,&QMenu::aboutToHide,menu,&QObject::deleteLater);
                menu->popup(offsetDial->mapToGlobal(point));editor->setFocus();
            });

            bool inserted=false;
            if(auto *box=qobject_cast<QBoxLayout*>(widget->layout())){
                // Native OBS uses either stacked or side-by-side VolumeControl
                // layouts; preserve its direction and existing child widgets.
                box->addWidget(controls);
                inserted=true;
            }else if(auto *grid=qobject_cast<QGridLayout*>(widget->layout())){
                grid->addWidget(controls,grid->rowCount(),0,1,qMax(1,grid->columnCount()));
                inserted=true;
            }
            if(!inserted){delete controls;continue;}
            controls->show();
            const QString id=src.id;
            connect(pan,&QSlider::valueChanged,this,[this,id](int v){
                if(!state->pan)return;
                if(state->pan(id,v/100.0))state->saveDebounce.start();
                refresh();
            });
            connect(panDial,&QDial::valueChanged,this,[this,id](int v){
                if(!state->pan)return;
                if(state->pan(id,v/100.0))state->saveDebounce.start();
                refresh();
            });
            connect(offset,&QSpinBox::valueChanged,this,[this,id](int ms){
                if(!state->offset)return;
                if(state->offset(id,ms))state->saveDebounce.start();
                refresh();
            });
            connect(offsetDial,&QDial::valueChanged,this,[this,id](int ms){
                if(!state->offset)return;
                if(state->offset(id,ms))state->saveDebounce.start();
                refresh();
            });
            connect(offsetDial,&QDial::sliderReleased,this,[this]{refresh();},Qt::QueuedConnection);
            State::Row row;
            row.volume=widget;row.controls=controls;row.id=id;
            row.nameHandle=sourceNameLabel(widget);
            if(row.nameHandle){
                row.originalCursor=row.nameHandle->cursor();
                row.hadCursor=row.nameHandle->testAttribute(Qt::WA_SetCursor);
                row.nameHandle->installEventFilter(this);
                if(row.nameHandle->inherits("VolumeName")){
                    row.nameText=row.nameHandle->findChild<QLabel*>(QString(),Qt::FindDirectChildrenOnly);
                    if(row.nameText)row.nameText->installEventFilter(this);
                }
                row.nameHandle->update();
            }
            row.panSlider=pan;row.panDial=panDial;row.offsetSpin=offset;
            row.offsetDial=offsetDial;row.panLabel=panLabel;row.offsetLabel=offsetLabel;
            if(existing)*existing=row;else state->rows.append(row);
            existing=nullptr;
            for(auto &record:state->rows)if(record.volume==widget)existing=&record;
        }
        if(existing)state->apply(*existing,src);
        seen.insert(widget);
    }
    for(int i=state->rows.size()-1;i>=0;--i)
        if(!state->rows[i].volume||!seen.contains(state->rows[i].volume)){
            if(state->rows[i].nameHandle)state->rows[i].nameHandle->removeEventFilter(this);
            state->remove(state->rows[i]);
            state->rows.removeAt(i);
        }
    if(state->grid&&!state->gridIntact())state->restoreGrid();
    state->applyOrder();
    if(!state->dragHandle)state->refreshGrid();
    if(!state->dragHandle)state->updateHandles();
    state->busy=false;
}
}
