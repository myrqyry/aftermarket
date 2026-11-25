#include "front-audio-controls.h"
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
constexpr int dialOffsetMs=2000; // coarse ±2s; numeric spin edits all offsets
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
    };
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
    QBoxLayout *horizontalBox()const{
        if(!native)return nullptr;
        auto *scroll=native->findChild<QScrollArea*>("hMixerScrollArea");
        if(!scroll||!scroll->widget()||scroll->widget()->objectName()!="hVolumeWidgets")return nullptr;
        auto *box=qobject_cast<QBoxLayout*>(scroll->widget()->layout());
        if(!box||box->direction()!=QBoxLayout::TopToBottom||
           box->parentWidget()!=scroll->widget())return nullptr;
        return box;
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
        int minimum=240;
        for(auto *w:widgets){
            if(w->isHidden()||w->property("aftermarket-foreign").toBool())return 1;
            minimum=qMax(minimum,qMax(w->minimumWidth(),w->minimumSizeHint().width()));
        }
        constexpr int spacing=6;
        return qBound(1,(width+spacing)/(minimum+spacing),qMin(4,int(widgets.size())));
    }
    void refreshGrid(){
        auto *box=horizontalBox();
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
        for(int i=0;i<captured.size();++i)layout->addWidget(captured[i].widget,i/cols,i%cols);
        for(int i=0;i<cols;++i)layout->setColumnStretch(i,1);
        box->insertLayout(first,layout);
        gridBox=box;grid=layout;gridSlots=captured;gridColumns=cols;
    }


    static QBoxLayout *containingBox(QLayout *layout,QWidget *widget){
        if(!layout)return nullptr;
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
    void remove(Row &r){
        if(r.nameHandle){
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
        const bool narrow=r.volume&&r.volume->width()<145;
        if(r.panLabel)r.panLabel->setVisible(!narrow);
        if(r.offsetLabel)r.offsetLabel->setVisible(!narrow);
        if(r.panSlider)r.panSlider->setVisible(!useKnobs&&!narrow);
        if(r.panDial)r.panDial->setVisible(useKnobs||narrow);
        if(r.offsetDial)r.offsetDial->setVisible(useKnobs&&!narrow);
        if(r.offsetSpin){
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
            r.offsetDial->setValue(qBound(-dialOffsetMs,source.offsetMs,dialOffsetMs));
        }
        if(r.panSlider)r.panSlider->setEnabled(source.stereo);
        if(r.panDial)r.panDial->setEnabled(source.stereo);
        syncLayout(r);
    }
};
FrontAudioControls::FrontAudioControls(QString settingsFile,Query q,SetPan p,
                                       SetOffset o,std::function<void()> persist,QObject *parent)
    :QObject(parent),state(new State){
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
    autoGrid->setToolTip("Wrap horizontal channels into columns when the dock has room.");
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
            controls->setMinimumHeight(30);
            controls->setMaximumHeight(34);
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
            auto *panDial=new QDial(controls);
            panDial->setObjectName("aftermarket-pan-dial");
            panDial->setRange(0,100);
            panDial->setNotchesVisible(false);
            panDial->setFixedSize(24,24);
            panDial->setToolTip(pan->toolTip());
            outer->addWidget(panDial);
            auto *offsetLabel=new QLabel("Δ",controls);
            offsetLabel->setFixedWidth(12);
            offsetLabel->setToolTip("Native OBS sync offset (milliseconds)");
            outer->addWidget(offsetLabel);
            auto *offsetDial=new QDial(controls);
            offsetDial->setObjectName("aftermarket-offset-dial");
            offsetDial->setRange(-dialOffsetMs,dialOffsetMs);
            offsetDial->setFixedSize(24,24);
            offsetDial->setToolTip("Coarse sync offset · ±2000 ms · precise value at right");
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
            State::Row row;
            row.volume=widget;row.controls=controls;row.id=id;
            row.nameHandle=sourceNameLabel(widget);
            if(row.nameHandle){
                row.originalCursor=row.nameHandle->cursor();
                row.hadCursor=row.nameHandle->testAttribute(Qt::WA_SetCursor);
                row.nameHandle->installEventFilter(this);
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
