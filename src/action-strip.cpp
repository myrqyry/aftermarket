#include "action-strip.h"
#include "visual-polish.h"
#include <QApplication>
#include <QMenuBar>
#include <QMainWindow>
#include <QFrame>
#include <QCheckBox>
#include <QMap>
#include <QSignalBlocker>
#include <QScopedValueRollback>
#include <QMenu>
#include <QAction>
#include <QWidget>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QListWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTimer>
#include <QDateTime>
#include <QFile>
#include <QSaveFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QPointer>
#include <QMimeData>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QStyle>
#include <QUuid>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace augment {
namespace {
constexpr auto dragFormat="application/x-aftermarket-action-strip";
class StripHost final : public QWidget {
public:
    std::function<void(const QString &,int)> dropped;
    std::function<QStringList()> order;
    explicit StripHost(QWidget *parent=nullptr):QWidget(parent){setAcceptDrops(true);}
    void dragEnterEvent(QDragEnterEvent *e) override {
        if(e->mimeData()->hasFormat(dragFormat))e->acceptProposedAction();
        else e->ignore();
    }
    void dragMoveEvent(QDragMoveEvent *e) override {
        if(e->mimeData()->hasFormat(dragFormat))e->acceptProposedAction();
        else e->ignore();
    }
    void dropEvent(QDropEvent *e) override {
        if(!e->mimeData()->hasFormat(dragFormat)){e->ignore();return;}
        const QString id=QString::fromUtf8(e->mimeData()->data(dragFormat));
        // Compare against actual button midpoints, not the gaps/spacers.
        int index=0;
        const auto ordered=order?order():QStringList();
        for(const QString &other:ordered){
            if(auto *button=findChild<QToolButton*>("aftermarket-strip-"+other)){
                const auto centre=button->mapTo(this,QPoint(button->width()/2,0)).x();
                if(e->position().x()>centre)++index;
            }
        }
        if(dropped)dropped(id,index);
        e->acceptProposedAction();
    }
};
QString printDuration(qint64 seconds){
    if(seconds<0)return "--:--";
    const qint64 hours=seconds/3600,minutes=(seconds/60)%60,remaining=seconds%60;
    return QString("%1:%2:%3").arg(hours,2,10,QChar('0'))
        .arg(minutes,2,10,QChar('0')).arg(remaining,2,10,QChar('0'));
}
QString printStatus(const StripItem &item,const StatusSnapshot &s) {
    if(item.target=="recording")return s.recording?QStringLiteral("● REC"):QStringLiteral("REC");
    if(item.target=="streaming")return s.streaming?QStringLiteral("● LIVE"):QStringLiteral("LIVE");
    if(item.target=="fps")return QString::number(s.fps,'f',1)+" FPS";
    if(item.target=="cpu")return QString::number(s.cpu,'f',1)+"% CPU";
    if(item.target=="record-time")return "REC "+printDuration(s.recordSeconds);
    if(item.target=="stream-time")return "LIVE "+printDuration(s.streamSeconds);
    if(item.target=="lagged")return QString::number(s.lagged)+" render missed";
    if(item.target=="dropped")return QString::number(s.dropped)+" stream dropped";
    if(item.target=="stream-bitrate")return QString::number(s.streamKbps,'f',0)+" kbps↑";
    if(item.target=="record-bitrate")return QString::number(s.recordKbps,'f',0)+" kbps●";
    return item.target;
}
QString typeLabel(const StripItem &item) {
    return item.label.isEmpty()?
        (item.type=="search"?QStringLiteral("Find sources"):
         item.type=="dock"?item.target:item.type=="hotkey"?QStringLiteral("Hotkey"):item.target):item.label;
}
QDockWidget *dockFor(QMenuBar *bar,const StripItem &item){
    auto *main=qobject_cast<QMainWindow*>(bar?bar->parentWidget():nullptr);
    return main?main->findChild<QDockWidget*>(item.target):nullptr;
}
QString dockLabel(const StripItem &item,QMenuBar *bar){
    if(!item.label.trimmed().isEmpty())return item.label.trimmed();
    if(auto *dock=dockFor(bar,item)){
        // Aftermarket may elide/pad an on-screen dock title to fit a narrow
        // header. The native Docks-menu action retains its full original
        // title; use that rather than a technical ID while the dock exists.
        if(auto *action=dock->toggleViewAction()){
            const auto name=action->text().simplified();
            if(!name.isEmpty())return name;
        }
        const auto name=dock->windowTitle().simplified();
        if(!name.isEmpty())return name;
    }
    // Keep missing targets diagnosable without guessing replacement docks.
    return item.target;
}
}
bool ActionStrip::validItem(const StripItem &i) {
    if(i.id.isEmpty()||i.id.size()>128||i.label.size()>160||i.target.size()>2048)return false;
    if(i.type=="spacer"||i.type=="separator")return i.target.isEmpty();
    if(i.type=="dock")return !i.target.isEmpty();
    if(i.type=="search")return i.target=="quick-find";
    if(i.type=="cue")return QRegularExpression(
        "^[0-9a-fA-F]{8}-(?:[0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}$").match(i.target).hasMatch();
    if(i.type=="layout")return QRegularExpression(
        "^[0-9a-fA-F]{8}-(?:[0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}$").match(i.target).hasMatch();
    if(i.type=="hotkey")return i.target.startsWith("hotkey:v1:")&&i.target.size()>11;
    if(i.type=="status")return QStringList{"recording","streaming","fps","cpu","dropped",
        "lagged","record-time","stream-time","stream-bitrate","record-bitrate"}.contains(i.target);
    return false;
}
ActionStrip::ActionStrip(QString file,StatusProvider status,HotkeyList hotkeys,HotkeyTrigger trigger,
                         QObject *parent,LayoutList layouts,LayoutApply applyLayout,
                         SearchOpen search,CueList cues,CueApply applyCue)
    :QObject(parent),path_(std::move(file)),status_(std::move(status)),hotkeys_(std::move(hotkeys)),
     trigger_(std::move(trigger)),layouts_(std::move(layouts)),applyLayout_(std::move(applyLayout)),
     search_(std::move(search)),cues_(std::move(cues)),applyCue_(std::move(applyCue)) {load();}
ActionStrip::~ActionStrip(){
    closeEditor();
    shutdown_=true;
    if(bar_)setEnabled(bar_,false);
}
bool ActionStrip::load(){
    QFile f(path_);
    if(!f.exists())return true;
    if(!f.open(QIODevice::ReadOnly)||f.size()>128*1024)return false;
    QJsonParseError error;
    const auto doc=QJsonDocument::fromJson(f.readAll(),&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject())return false;
    const auto obj=doc.object();
    if(obj.value("version").toInt()!=1||!obj.value("enabled").isBool()||
       !obj.value("items").isArray()||obj.value("items").toArray().size()>32)return false;
    QList<StripItem> candidate;
    QSet<QString> ids;
    for(const auto &v:obj.value("items").toArray()){
        if(!v.isObject())return false;
        const auto row=v.toObject();
        for(auto field:{"id","type","target","label"})
            if(!row.value(field).isString())return false;
        StripItem item{row.value("id").toString(),row.value("type").toString(),
                       row.value("target").toString(),row.value("label").toString()};
        if(!validItem(item)||ids.contains(item.id))return false;
        ids.insert(item.id);candidate.append(item);
    }
    if(enabled_)return false; // never silently replace a live toolbar
    desiredEnabled_=obj.value("enabled").toBool();
    items_=candidate;
    return true;
}
bool ActionStrip::save() const{
    QJsonArray array;
    for(const auto &i:items_)
        array.append(QJsonObject{{"id",i.id},{"type",i.type},{"target",i.target},{"label",i.label}});
    const QJsonObject root{{"version",1},{"enabled",desiredEnabled_},{"items",array}};
    QSaveFile file(path_);
    if(!file.open(QIODevice::WriteOnly))return false;
    const auto bytes=QJsonDocument(root).toJson(QJsonDocument::Indented);
    return file.write(bytes)==bytes.size()&&file.commit();
}
bool ActionStrip::enabled()const{return enabled_;}
bool ActionStrip::desiredEnabled()const{return desiredEnabled_;}
QList<StripItem> ActionStrip::items()const{return items_;}
QWidget *ActionStrip::widget()const{return content_;}
bool ActionStrip::setEnabled(QMenuBar *bar,bool enabled){
    if(enabled){
        if(enabled_||!bar||bar->isNativeMenuBar())return false;
        bar_=bar;
        bar_->installEventFilter(this);
        originalCorner_=bar->cornerWidget(Qt::TopRightCorner);
        host_=new QWidget(bar);
        host_->setObjectName("aftermarket-action-strip-host");
        auto *layout=new QHBoxLayout(host_);
        layout->setContentsMargins(0,0,0,0);layout->setSpacing(5);
        const bool settingsCorner=originalCorner_&&originalCorner_->objectName()=="aftermarket-menu-settings-host";
        if(originalCorner_){
            originalCorner_->setParent(host_);
            if(!settingsCorner)layout->addWidget(originalCorner_);
        }
        auto *content=new StripHost(host_);
        content_=content;
        content->setObjectName("aftermarket-action-strip");
        auto *inside=new QHBoxLayout(content);
        inside->setContentsMargins(1,0,1,0);
        inside->setSpacing(3);
        layout->addWidget(content);
        if(settingsCorner)layout->addWidget(originalCorner_);
        content->dropped=[this](const QString &id,int position){
            // QDrag::exec still owns the source widget while Drop is dispatched.
            // Rebuilding synchronously would delete that widget mid-drag.
            QTimer::singleShot(0,this,[this,id,position]{
                int old=-1;
                for(int i=0;i<items_.size();++i)if(items_[i].id==id){old=i;break;}
                if(old>=0)moveItem(id,position>old?position-1:position);
            });
        };
        content->order=[this]{
            QStringList result;for(const auto &item:items_)result.append(item.id);return result;
        };
        bar->setCornerWidget(host_,Qt::TopRightCorner);
        // When Qt replaces a previously removed corner widget (notably
        // after emergency recovery), it may leave the new corner hidden and
        // at its default 100px geometry. Establish its styled size explicitly.
        content_->show();
        host_->adjustSize();
        host_->show();
        bar->updateGeometry();
        bar->update();
        enabled_=true;
        desiredEnabled_=true;
        rebuild();
        timer_=new QTimer(this);
        timer_->setInterval(1000);
        connect(timer_,&QTimer::timeout,this,[this]{if(enabled_&&host_&&host_->isVisible())refresh();});
        timer_->start();
        refresh();
        save();
        return true;
    }
    if(!enabled_||bar!=bar_)return false;
    timer_->stop();delete timer_.data();
    enabled_=false;
    if(bar_)bar_->removeEventFilter(this);
    overflowIds_.clear();
    overflow_.clear();
    if(!shutdown_)desiredEnabled_=false;
    if(bar_){
        // Transfer OBS's original corner to its original owner before the
        // accessory wrapper is discarded; never delete the OBS widget.
        if(originalCorner_){
            originalCorner_->setParent(bar_);
            bar_->setCornerWidget(originalCorner_,Qt::TopRightCorner);
            if(originalCorner_->objectName()=="aftermarket-menu-settings-host"){
                originalCorner_->adjustSize();
                originalCorner_->show();
                originalCorner_->move(qMax(0,bar_->width()-originalCorner_->width()),0);
                bar_->updateGeometry();bar_->update();
            }
        }else bar_->setCornerWidget(nullptr,Qt::TopRightCorner);
    }
    delete host_.data();
    content_.clear();host_.clear();
    originalCorner_.clear();bar_.clear();
    if(!shutdown_)save();
    return true;
}
bool ActionStrip::addItem(StripItem i){
    if(!validItem(i)||items_.size()>=32)return false;
    for(const auto &existing:items_)if(existing.id==i.id)return false;
    items_.append(std::move(i));rebuild();save();return true;
}
bool ActionStrip::replaceItem(const QString &id,StripItem i){
    if(!validItem(i)||i.id!=id)return false;
    for(auto &item:items_)if(item.id==id){item=i;rebuild();save();return true;}
    return false;
}
bool ActionStrip::removeItem(const QString &id){
    for(int k=0;k<items_.size();++k)if(items_[k].id==id){
        items_.removeAt(k);rebuild();save();return true;
    }
    return false;
}
bool ActionStrip::moveItem(const QString &id,int index){
    int old=-1;for(int i=0;i<items_.size();++i)if(items_[i].id==id){old=i;break;}
    if(old<0)return false;
    const auto item=items_.takeAt(old);
    items_.insert(qBound(0,index,items_.size()),item);
    rebuild();save();return true;
}
bool ActionStrip::trigger(const QString &id){
    for(const auto &item:items_)if(item.id==id){
        if(item.type=="search"){
            if(!search_)return false;
            search_();return true;
        }
        if(item.type=="cue"){
            if(!cues_||!applyCue_)return false;
            const auto items=cues_();
            if(!std::any_of(items.begin(),items.end(),
                [&](const CueChoiceStrip &cue){return cue.id==item.target;}))return false;
            return applyCue_(item.target);
        }
        if(item.type=="hotkey")return trigger_&&trigger_(item.target);
        if(item.type=="layout"){
            if(!applyLayout_||!layouts_)return false;
            const auto choices=layouts_();
            if(!std::any_of(choices.begin(),choices.end(),
                [&](const auto &c){return c.id==item.target;}))return false;
            const bool ok=applyLayout_(item.target);
            refresh();
            return ok;
        }
        if(item.type=="dock"&&bar_){
            auto *main=qobject_cast<QMainWindow*>(bar_->parentWidget());
            if(!main)return false;
            auto *dock=main->findChild<QDockWidget*>(item.target);
            if(!dock)return false;
            dock->setVisible(!dock->isVisible());
            refresh();return true;
        }
        return false;
    }
    return false;
}
QString ActionStrip::itemText(const QString &id) const {
    for(const auto &i:items_)if(i.id==id){
        if(i.type=="status")return printStatus(i,latest_);
        if(i.type=="dock")return dockLabel(i,bar_);
        if(i.type=="cue"&&i.label.trimmed().isEmpty()){
            if(cues_)for(const auto &c:cues_())if(c.id==i.target)return c.name;
            return "Cue unavailable";
        }
        if(i.type=="layout"&&i.label.trimmed().isEmpty()&&layouts_)
            for(const auto &c:layouts_())if(c.id==i.target)return c.name;
        return typeLabel(i);
    }
    return {};
}
void ActionStrip::refresh(){
    if(status_)latest_=status_();
    if(!content_)return;
    const auto now=QDateTime::currentMSecsSinceEpoch();
    if(hotkeys_&&std::any_of(items_.begin(),items_.end(),[](const auto &i){return i.type=="hotkey";})&&
       (!lastHotkeyPoll_||now-lastHotkeyPoll_>=5000)){
        cachedHotkeys_=hotkeys_();
        lastHotkeyPoll_=now;
    }
    for(const auto &item:items_){
        auto *b=content_->findChild<QToolButton*>("aftermarket-strip-"+item.id);
        if(!b)continue;
        const QString label=itemText(item.id);
        b->setText(label);
        if(item.type=="status"){
            b->setEnabled(true);
            b->setToolTip(item.target+" · native OBS status");
            b->setProperty("aftermarket-on-air",
                (item.target=="recording"&&latest_.recording)||
                (item.target=="streaming"&&latest_.streaming));
        }
        if(item.type=="search"){
            b->setEnabled(bool(search_));
            b->setToolTip(search_?"Find OBS sources, scenes and filters":
                "Source search unavailable");
        }
        if(item.type=="dock"){
            auto *dock=dockFor(bar_,item);
            const bool showing=dock&&dock->isVisible();
            b->setEnabled(dock);
            b->setChecked(showing);
            const QString action=dock?(showing?"Hide ":"Show ")+label:
                QStringLiteral("Dock unavailable: ")+item.target;
            b->setToolTip(action);
            b->setAccessibleName(action);
        }
        if(item.type=="cue"){
            bool found=false;
            if(cues_)for(const auto &c:cues_())if(c.id==item.target){found=true;break;}
            b->setEnabled(found&&bool(applyCue_));
            b->setToolTip(found?"Run cue: "+label:"Saved cue missing: "+item.target);
            b->setAccessibleName(b->toolTip());
        }
        if(item.type=="layout"){
            bool found=false;
            if(layouts_)for(const auto &c:layouts_())if(c.id==item.target){found=true;break;}
            b->setEnabled(found);
            b->setToolTip(found?"Switch to dock layout: "+label:
                "Saved dock layout missing: "+item.target);
            b->setAccessibleName(b->toolTip());
        }
        if(item.type=="hotkey"){
            bool found=false;
            for(const auto &h:cachedHotkeys_)if(h.binding==item.target){found=true;break;}
            b->setEnabled(found);
            b->setToolTip(found?item.target:"Missing OBS hotkey: "+item.target);
        }
    }
    updateOverflow();
}
void ActionStrip::setMotionProvider(std::function<bool()> provider){
    motionProvider_=std::move(provider);
    if(content_)for(auto *button:content_->findChildren<QToolButton*>())
        if(auto *owned=dynamic_cast<MotionToolButton*>(button))owned->setMotionProvider(motionProvider_);
}
void ActionStrip::rebuild(){
    if(!content_)return;
    auto *layout=qobject_cast<QHBoxLayout*>(content_->layout());
    overflow_.clear();
    overflowIds_.clear();
    while(auto *child=layout->takeAt(0)){
        if(child->widget())delete child->widget();
        delete child;
    }
    lastHotkeyPoll_=0; // refresh native choices after changes to the strip
    for(const auto &item:items_){
        if(item.type=="spacer"){layout->addStretch(1);continue;}
        if(item.type=="separator"){
            auto *separator=new QFrame(content_);
            separator->setObjectName("aftermarket-strip-"+item.id);
            separator->setFrameShape(QFrame::VLine);
            separator->setFrameShadow(QFrame::Plain);
            layout->addWidget(separator);continue;
        }
        QToolButton *button=item.type=="dock"?
            static_cast<QToolButton*>(new FineStateToolButton(content_)):
            static_cast<QToolButton*>(new QToolButton(content_));
        if(auto *owned=dynamic_cast<MotionToolButton*>(button))owned->setMotionProvider(motionProvider_);
        button->setObjectName("aftermarket-strip-"+item.id);
        button->setCheckable(item.type=="dock");
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Preferred);
        button->setMinimumHeight(qMax(22,button->fontMetrics().height()+8));
        button->setAccessibleName(typeLabel(item));
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        button->installEventFilter(this);
        connect(button,&QToolButton::clicked,this,[this,id=item.id]{trigger(id);});
        connect(button,&QWidget::customContextMenuRequested,this,
            [this,button,id=item.id](const QPoint &local){
                openButtonMenu(id,button,button->mapToGlobal(local));
            });
        layout->addWidget(button);
    }
    auto *more=new QToolButton(content_);
    more->setObjectName("aftermarket-strip-overflow");
    more->setText(QString::fromUtf8("⋯"));
    more->setAccessibleName("More Aftermarket actions");
    more->setToolTip("More action-strip items");
    more->setAutoRaise(true);
    more->setPopupMode(QToolButton::InstantPopup);
    more->setMenu(new QMenu(more));
    layout->addWidget(more);
    overflow_=more;
    more->hide();
    refresh();
    updateOverflow();
}
void ActionStrip::scheduleOverflow(){
    if(overflowScheduled_)return;
    overflowScheduled_=true;
    QTimer::singleShot(0,this,[this]{
        overflowScheduled_=false;
        updateOverflow();
    });
}
void ActionStrip::updateOverflow(){
    if(!enabled_||!bar_||!content_||!host_||!overflow_||overflowBusy_)return;
    QScopedValueRollback<bool> guard(overflowBusy_,true);
    int usedByMenus=0;
    for(auto *action:bar_->actions())
        if(action->isVisible())usedByMenus=qMax(usedByMenus,bar_->actionGeometry(action).right()+10);
    const int oldCornerWidth=originalCorner_?qMax(originalCorner_->minimumWidth(),
        originalCorner_->sizeHint().width())+6:0;
    const int available=qMax(25,bar_->width()-usedByMenus-oldCornerWidth-8);
    const int spacing=3;
    int needed=0;
    QList<QWidget*> widgets;
    QVector<int> widths;
    for(const auto &item:items_){
        QWidget *w=content_->findChild<QWidget*>("aftermarket-strip-"+item.id);
        widgets.append(w);
        int width=0;
        if(w){
            width=qMax(1,w->sizeHint().width());
            if(auto *button=qobject_cast<QToolButton*>(w)){
                // Explicit styled minimum prevents Qt themes squeezing the
                // text to a sliver when an additional item is introduced.
                width=qMax(width,button->fontMetrics().horizontalAdvance(button->text())+20);
                button->setMinimumWidth(width);
            }
            width+=spacing;
            needed+=width;
        }
        widths.append(width);
    }
    const int moreWidth=qMax(28,overflow_->sizeHint().width())+spacing;
    QStringList overflow;
    if(needed>available){
        for(int i=items_.size()-1;i>=0&&needed+moreWidth>available;--i){
            if(!widgets[i])continue;
            overflow.prepend(items_[i].id);
            needed-=widths[i];
        }
    }
    const bool showMore=!overflow.isEmpty();
    for(int i=0;i<items_.size();++i)
        if(widgets[i])widgets[i]->setVisible(!overflow.contains(items_[i].id));
    if(showMore&&overflow!=overflowIds_){
        auto *menu=overflow_->menu();
        menu->clear();
        for(const QString &id:overflow){
            const auto it=std::find_if(items_.begin(),items_.end(),
                [&id](const StripItem &item){return item.id==id;});
            if(it==items_.end()||it->type=="spacer")continue;
            if(it->type=="separator"){menu->addSeparator();continue;}
            auto *action=menu->addAction(itemText(id));
            if(it->type=="status")action->setEnabled(false);
            else connect(action,&QAction::triggered,this,[this,id]{trigger(id);});
            if(it->type=="dock")action->setCheckable(true);
            action->setData(id);
        }
    }
    overflowIds_=overflow;
    if(showMore){
        for(auto *action:overflow_->menu()->actions()){
            const QString id=action->data().toString();
            action->setText(itemText(id));
            const auto it=std::find_if(items_.begin(),items_.end(),
                [&id](const StripItem &item){return item.id==id;});
            if(it!=items_.end()&&it->type=="search"){
                action->setEnabled(bool(search_));
                action->setToolTip(search_?"Find OBS sources, scenes and filters":
                    "Source search unavailable");
            }
            if(it!=items_.end()&&it->type=="cue"){
                bool found=false;
                if(cues_)for(const auto &c:cues_())if(c.id==it->target){found=true;break;}
                action->setEnabled(found&&bool(applyCue_));
                action->setToolTip(found?"Run cue: "+itemText(id):
                    "Saved cue missing: "+it->target);
            }
            if(it!=items_.end()&&it->type=="layout"){
                bool found=false;
                if(layouts_)for(const auto &c:layouts_())if(c.id==it->target){found=true;break;}
                action->setEnabled(found);
                action->setToolTip(found?"Switch to dock layout: "+itemText(id):
                    "Saved dock layout missing: "+it->target);
            }
            if(it!=items_.end()&&it->type=="dock"){
                auto *dock=dockFor(bar_,*it);
                action->setEnabled(dock);
                action->setCheckable(true);
                action->setChecked(dock&&dock->isVisible());
                action->setToolTip(dock?(dock->isVisible()?"Hide ":"Show ")+action->text():
                    QStringLiteral("Dock unavailable: ")+it->target);
            }
        }
    }
    overflow_->setVisible(showMore);
    content_->layout()->activate();
    host_->layout()->activate();
    host_->adjustSize();
    // QMenuBar does not always reposition its corner widget when children
    // are inserted after the bar was laid out. Place it at the actual right
    // edge instead of leaving the enlarged strip beyond the window bounds.
    host_->move(qMax(usedByMenus,bar_->width()-host_->width()),0);
    bar_->updateGeometry();
    bar_->update();
}
bool ActionStrip::eventFilter(QObject *watched,QEvent *event){
    if(watched==bar_){
        switch(event->type()){
            case QEvent::Resize:
            case QEvent::Show:
            case QEvent::StyleChange:
            case QEvent::FontChange:
                scheduleOverflow();break;
            default:break;
        }
        return false;
    }
    auto *button=qobject_cast<QToolButton*>(watched);
    if(!button||!content_)return false;
    if(event->type()==QEvent::MouseButtonPress){
        auto *mouse=static_cast<QMouseEvent*>(event);
        if(mouse->button()==Qt::LeftButton){
            dragStart_=mouse->position().toPoint();
            dragId_=button->objectName().mid(QString("aftermarket-strip-").size());
        }
    }else if(event->type()==QEvent::MouseMove&& !dragId_.isEmpty()){
        auto *mouse=static_cast<QMouseEvent*>(event);
        if(mouse->buttons()&Qt::LeftButton&&
           (mouse->position().toPoint()-dragStart_).manhattanLength()>=QApplication::startDragDistance()){
            const QString id=dragId_;dragId_.clear();
            QDrag drag(button);
            auto *mime=new QMimeData;
            mime->setData(dragFormat,id.toUtf8());
            drag.setMimeData(mime);
            drag.exec(Qt::MoveAction);
            return true;
        }
    }else if(event->type()==QEvent::MouseButtonRelease)dragId_.clear();
    return false;
}
void ActionStrip::openButtonMenu(const QString &id,QWidget *owner,const QPoint &global){
    QMenu menu(owner);
    auto *left=menu.addAction("Move left");
    auto *right=menu.addAction("Move right");
    auto *rename=menu.addAction("Rename…");
    menu.addSeparator();
    auto *remove=menu.addAction("Remove");
    auto *editor=menu.addAction("Configure strip…");
    auto *picked=menu.exec(global);
    if(!picked)return;
    int index=-1;for(int i=0;i<items_.size();++i)if(items_[i].id==id){index=i;break;}
    if(index<0)return;
    if(picked==left)moveItem(id,qMax(0,index-1));
    else if(picked==right)moveItem(id,index+1);
    else if(picked==remove)removeItem(id);
    else if(picked==rename){
        bool ok=false;
        const auto label=QInputDialog::getText(owner,"Rename action","Label:",
            QLineEdit::Normal,items_[index].label,&ok);
        if(ok){auto change=items_[index];change.label=label;replaceItem(id,change);}
    }else if(picked==editor)showEditor(owner,bar_);
}
QString ActionStrip::chooseHotkey(QWidget *parent)const{
    QDialog dialog(parent);dialog.setWindowTitle("Add OBS hotkey");
    dialog.resize(470,390);
    auto *layout=new QVBoxLayout(&dialog);
    auto *search=new QLineEdit(&dialog);
    search->setPlaceholderText("Search native OBS and plugin hotkeys…");
    layout->addWidget(search);
    auto *tree=new QTreeWidget(&dialog);
    tree->setHeaderHidden(true);
    layout->addWidget(tree,1);
    const auto choices=hotkeys_?hotkeys_():QList<HotkeyChoice>();
    QMap<QString,QTreeWidgetItem*> groups;
    for(const auto &hotkey:choices){
        QTreeWidgetItem *group=groups.value(hotkey.group);
        if(!group){group=new QTreeWidgetItem(tree,QStringList{hotkey.group});groups[hotkey.group]=group;}
        auto *item=new QTreeWidgetItem(group,QStringList{hotkey.label});
        item->setData(0,Qt::UserRole,hotkey.binding);
    }
    tree->expandAll();
    connect(search,&QLineEdit::textChanged,&dialog,[tree](const QString &text){
        for(int a=0;a<tree->topLevelItemCount();++a){
            auto *group=tree->topLevelItem(a);bool any=false;
            for(int i=0;i<group->childCount();++i){
                auto *item=group->child(i);
                const bool matches=(item->text(0)+" "+group->text(0)).contains(text,Qt::CaseInsensitive);
                item->setHidden(!matches);any|=matches;
            }
            group->setHidden(!any);
        }
    });
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    layout->addWidget(buttons);
    if(dialog.exec()!=QDialog::Accepted||!tree->currentItem())return {};
    return tree->currentItem()->data(0,Qt::UserRole).toString();
}
QString ActionStrip::chooseDock(QWidget *parent)const{
    const auto *main=qobject_cast<QMainWindow*>(bar_?bar_->parentWidget():nullptr);
    if(!main)return {};
    QStringList labels;QMap<QString,QString> identities;
    for(auto *dock:main->findChildren<QDockWidget*>()){
        if(dock->objectName().isEmpty())continue;
        const StripItem option{"","dock",dock->objectName(),{}};
        const auto display=dockLabel(option,bar_)+" ["+dock->objectName()+"]";
        labels.append(display);identities[display]=dock->objectName();
    }
    bool accepted=false;
    const auto chosen=QInputDialog::getItem(parent,"Add dock toggle",
        "Native OBS dock",labels,0,false,&accepted);
    return accepted?identities.value(chosen):QString{};
}
QString ActionStrip::chooseLayout(QWidget *parent)const{
    if(!layouts_)return {};
    QStringList choices;QMap<QString,QString> ids;
    for(const auto &layout:layouts_()){
        const QString label=layout.name+" ["+layout.id+"]";
        choices.append(label);ids.insert(label,layout.id);
    }
    if(choices.isEmpty())return {};
    bool accepted=false;
    const auto chosen=QInputDialog::getItem(parent,"Add dock layout macro",
        "Saved dock layout",choices,0,false,&accepted);
    return accepted?ids.value(chosen):QString{};
}
QString ActionStrip::chooseCue(QWidget *parent) const{
    if(!cues_)return {};
    QStringList names;QMap<QString,QString> byName;
    for(const auto &cue:cues_()){
        const auto label=cue.name+" ["+cue.id+"]";
        names.append(label);byName[label]=cue.id;
    }
    if(names.isEmpty())return {};
    bool accepted=false;
    const auto selected=QInputDialog::getItem(parent,"Add Cue Deck action",
        "Saved cue",names,0,false,&accepted);
    return accepted?byName.value(selected):QString{};
}
bool ActionStrip::addFromEditor(const QString &type,QWidget *parent){
    StripItem item;item.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.type=type;
    if(type=="status"){
        const QStringList values{"recording","streaming","record-time","stream-time",
            "fps","cpu","dropped","lagged","stream-bitrate","record-bitrate"};
        bool accepted=false;
        item.target=QInputDialog::getItem(parent,"Add status readout",
            "OBS metric",values,0,false,&accepted);
        if(!accepted)return false;
    }else if(type=="hotkey"){
        item.target=chooseHotkey(parent);
        if(item.target.isEmpty())return false;
        if(hotkeys_)for(const auto &h:hotkeys_())
            if(h.binding==item.target){item.label=h.label;break;}
    }else if(type=="dock"){
        item.target=chooseDock(parent);
        if(item.target.isEmpty())return false;
    }else if(type=="layout"){
        item.target=chooseLayout(parent);
        if(item.target.isEmpty())return false;
    }else if(type=="search"){
        item.target="quick-find";
    }else if(type=="cue"){
        item.target=chooseCue(parent);
        if(item.target.isEmpty())return false;
    }
    return addItem(item);
}
void ActionStrip::closeEditor(){if(editor_)editor_->reject();}
void ActionStrip::showEditor(QWidget *parent,QMenuBar *menuBar){
    if(editor_){editor_->raise();editor_->activateWindow();return;}
    QDialog dialog(parent);
    editor_=&dialog;dialog.setWindowTitle("Aftermarket · Action strip");
    dialog.resize(490,440);
    auto *root=new QVBoxLayout(&dialog);
    auto *enabled=new QCheckBox("Show action strip in OBS menu bar",&dialog);
    enabled->setChecked(enabled_);
    root->addWidget(enabled);
    auto *list=new QListWidget(&dialog);
    list->setObjectName("aftermarket-strip-editor-list");
    root->addWidget(list,1);
    auto rebuildList=[this,list](){
        list->clear();
        for(const auto &item:items_){
            const auto text=itemText(item.id);
            auto *row=new QListWidgetItem(text+"    ("+item.type+")",list);
            row->setData(Qt::UserRole,item.id);
        }
    };
    rebuildList();
    auto *tools=new QHBoxLayout;
    auto *add=new QPushButton("Add…",&dialog);
    auto *left=new QPushButton("←",&dialog);
    auto *right=new QPushButton("→",&dialog);
    auto *remove=new QPushButton("Remove",&dialog);
    tools->addWidget(add);tools->addWidget(left);tools->addWidget(right);tools->addWidget(remove);
    root->addLayout(tools);
    auto *hint=new QLabel("Drag buttons on the menu bar to reorder. Right-click to rename or remove.",&dialog);
    hint->setWordWrap(true);root->addWidget(hint);
    auto *close=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);
    connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::accept);
    root->addWidget(close);
    connect(enabled,&QCheckBox::toggled,&dialog,[this,enabled,menuBar](bool value){
        if(!menuBar||!setEnabled(menuBar,value)){
            const QSignalBlocker blocker(enabled);
            enabled->setChecked(enabled_);
        }
    });
    connect(add,&QPushButton::clicked,&dialog,[this,add,&dialog,rebuildList](){
        QMenu types(add);
        QMap<QAction*,QString> choices;
        for(const auto &name:{"status","hotkey","dock","layout","cue","search","separator","spacer"})
            choices[types.addAction(QString::fromLatin1(name))]=QString::fromLatin1(name);
        if(auto *chosen=types.exec(add->mapToGlobal(QPoint(0,add->height())))){
            if(addFromEditor(choices.value(chosen),&dialog))rebuildList();
        }
    });
    connect(remove,&QPushButton::clicked,&dialog,[this,list,rebuildList](){
        if(auto *item=list->currentItem()){removeItem(item->data(Qt::UserRole).toString());rebuildList();}
    });
    auto move=[this,list,rebuildList](int offset){
        if(auto *row=list->currentItem()){
            const QString id=row->data(Qt::UserRole).toString();
            moveItem(id,list->currentRow()+offset);
            rebuildList();
            for(int i=0;i<list->count();++i)
                if(list->item(i)->data(Qt::UserRole).toString()==id){list->setCurrentRow(i);break;}
        }
    };
    connect(left,&QPushButton::clicked,&dialog,[move]{move(-1);});
    connect(right,&QPushButton::clicked,&dialog,[move]{move(1);});
    QPointer<ActionStrip> surviving(this);
    dialog.exec();
    if(surviving)surviving->editor_.clear();
}
} // namespace augment
