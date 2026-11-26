#include "native-plugin-icons.h"
#include "plugin-theme-icons.h"
#include <QApplication>
#include <QListView>
#include <QListWidget>
#include <QLabel>
#include <QBoxLayout>
#include <QTimer>
#include <QPointer>
#include <QEvent>
#include <QHash>
#include <QSet>
#include <QCheckBox>
#include <QItemSelectionModel>
#include <QAbstractItemModel>
namespace augment {
namespace {
bool sourceView(const QWidget *w){return w&&w->objectName()=="sources"&&w->inherits("SourceTree");}
bool filterView(const QWidget *w){return w&&(w->objectName()=="asyncFilters"||w->objectName()=="effectFilters")&&w->window()->inherits("OBSBasicFilters");}
QLabel *sourceName(QWidget *row){
    QLabel *result=nullptr;
    for(auto *l:row->findChildren<QLabel*>(QString{},Qt::FindDirectChildrenOnly)){
        if(!l->inherits("OBSSourceLabel"))continue;
        if(result)return nullptr;
        result=l;
    }
    return result;
}
QColor foreground(QAbstractItemView *view,const QModelIndex &index){
    const auto group=view->isEnabled()? (view->isActiveWindow()?QPalette::Active:QPalette::Inactive):QPalette::Disabled;
    return view->palette().color(group,view->selectionModel()&&view->selectionModel()->isSelected(index)?QPalette::HighlightedText:QPalette::Text);
}
}
struct NativePluginIcons::State {
    struct Entry {QPointer<QLabel> label;QPixmap original;qint64 applied=0;QString key;bool owned=false;};
    QHash<QLabel*,Entry> entries;
    QSet<QObject*> watched;
    SourceType sourceType;FilterType filterType;
    bool queued=false,refreshing=false;
};
NativePluginIcons::NativePluginIcons(SourceType s,FilterType f,QObject *parent):QObject(parent),state(new State){
    state->sourceType=std::move(s);state->filterType=std::move(f);
    qApp->installEventFilter(this);schedule();
}
NativePluginIcons::~NativePluginIcons(){
    qApp->removeEventFilter(this);
    state->refreshing=true;
    for(const auto &e:state->entries){
        if(!e.label)continue;
        if(e.owned)delete e.label.data();
        else if(e.label->pixmap().cacheKey()==e.applied)e.label->setPixmap(e.original);
    }
}
void NativePluginIcons::schedule(){
    if(state->queued||state->refreshing)return;
    state->queued=true;
    QTimer::singleShot(0,this,[this]{state->queued=false;refresh();});
}
bool NativePluginIcons::eventFilter(QObject *o,QEvent *e){
    if(state->refreshing)return false;
    const auto t=e->type();
    if(t!=QEvent::Show&&t!=QEvent::Hide&&t!=QEvent::ChildAdded&&t!=QEvent::ChildRemoved&&t!=QEvent::LayoutRequest&&t!=QEvent::PaletteChange&&t!=QEvent::StyleChange&&t!=QEvent::WindowActivate&&t!=QEvent::WindowDeactivate&&t!=QEvent::EnabledChange&&t!=QEvent::ScreenChangeInternal)return false;
    auto *w=qobject_cast<QWidget*>(o);
    for(auto *p=w;p;p=p->parentWidget()){
        if(sourceView(p)||filterView(p)||p->inherits("OBSBasicFilters")){schedule();break;}
    }
    return false;
}
void NativePluginIcons::refresh(){
    if(state->refreshing)return;
    state->refreshing=true;
    PluginThemeIcons registry;
    QSet<QLabel*> seen;
    QHash<QString,PluginThemeIcons::Glyph> glyphs;
    auto getGlyph=[&](const QString &id,bool filter){
        const QString key=(filter?"f:":"s:")+id;
        if(!glyphs.contains(key)){
            auto g=filter?registry.filter(id):registry.source(id);
            glyphs.insert(key,g.valid()?g:PluginThemeIcons::Glyph{});
        }
        return glyphs.value(key);
    };
    auto watch=[&](QAbstractItemView *view){
        auto track=[&](QObject *object){
            if(!object||state->watched.contains(object))return false;
            state->watched.insert(object);
            connect(object,&QObject::destroyed,this,[this,object]{state->watched.remove(object);});
            return true;
        };
        if(auto *m=view->model();track(m)){
            connect(m,&QAbstractItemModel::rowsInserted,this,[this]{schedule();});
            connect(m,&QAbstractItemModel::rowsRemoved,this,[this]{schedule();});
            connect(m,&QAbstractItemModel::rowsMoved,this,[this]{schedule();});
            connect(m,&QAbstractItemModel::modelReset,this,[this]{schedule();});
            connect(m,&QAbstractItemModel::dataChanged,this,[this]{schedule();});
            connect(m,&QAbstractItemModel::layoutChanged,this,[this]{schedule();});
        }
        if(auto *sel=view->selectionModel();track(sel))
            connect(sel,&QItemSelectionModel::selectionChanged,this,[this]{schedule();});
    };
    auto apply=[&](QLabel *label,const PluginThemeIcons::Glyph &g,const QColor &fg,bool owned){
        if(!label)return;
        auto it=state->entries.find(label);
        if(it!=state->entries.end()&&!it->label){state->entries.erase(it);it=state->entries.end();}
        if(it==state->entries.end()){
            State::Entry e;e.label=label;e.original=label->pixmap();e.owned=owned;
            it=state->entries.insert(label,e);
        }
        auto &entry=it.value();
        if(!owned&&label->pixmap().cacheKey()!=entry.applied)entry.original=label->pixmap();
        const auto dpr=label->devicePixelRatioF();
        const QString key=g.light+fg.name(QColor::HexArgb)+QString::number(dpr);
        if(entry.key!=key||label->pixmap().cacheKey()!=entry.applied){
            const auto pix=g.tinted(fg,QSize(16,16),dpr);
            if(pix.isNull())return;
            label->setPixmap(pix);entry.applied=label->pixmap().cacheKey();entry.key=key;
        }
        seen.insert(label);
    };
    for(auto *widget:QApplication::allWidgets()){
        auto *view=qobject_cast<QListView*>(widget);
        if(!view||!view->isVisible()||!view->model())continue;
        const bool source=sourceView(view),filter=filterView(view);
        if(!source&&!filter)continue;
        watch(view);
        for(int n=0;n<view->model()->rowCount();++n){
            const auto idx=view->model()->index(n,0);
            auto *row=view->indexWidget(idx);
            if(!row||!row->inherits(source?"SourceTreeItem":"VisibilityItemWidget"))continue;
            auto *name=sourceName(row);if(!name)continue;
            QString id;
            if(source){
                const auto actual=idx.data(Qt::AccessibleTextRole).toString();
                if(actual.isEmpty()||actual!=name->text())continue;
                id=state->sourceType(actual);
            }else id=state->filterType(idx.data(Qt::UserRole));
            if(id.isEmpty())continue;
            const auto glyph=getGlyph(id,filter);
            if(glyph.light.isEmpty())continue;
            const auto fg=foreground(view,idx);
            if(source){
                QLabel *icon=nullptr;
                for(auto *l:row->findChildren<QLabel*>(QString{},Qt::FindDirectChildrenOnly)){
                    if(l->property("class").toString()!="source-icon")continue;
                    if(icon){icon=nullptr;break;}icon=l;
                }
                if(icon&&!icon->pixmap().isNull())apply(icon,glyph,fg,false);
            }else{
                auto *layout=qobject_cast<QBoxLayout*>(row->layout());
                if(!layout||layout->indexOf(name)<0)continue;
                auto *icon=row->findChild<QLabel*>("aftermarket-native-filter-icon",Qt::FindDirectChildrenOnly);
                if(!icon){
                    icon=new QLabel(row);icon->setObjectName("aftermarket-native-filter-icon");
                    icon->setAttribute(Qt::WA_TransparentForMouseEvents);
                    icon->setFixedSize(16,16);
                    icon->setFocusPolicy(Qt::NoFocus);
                    layout->insertWidget(layout->indexOf(name),icon);icon->show();
                }
                apply(icon,glyph,fg,true);
            }
        }
    }
    // Remove only our additions. Preserve a pixmap replaced by native code.
    for(auto it=state->entries.begin();it!=state->entries.end();){
        if(it->label&&seen.contains(it.key())){++it;continue;}
        const auto e=it.value();it=state->entries.erase(it);
        if(!e.label)continue;
        if(e.owned)delete e.label.data();
        else if(e.label->pixmap().cacheKey()==e.applied)e.label->setPixmap(e.original);
    }
    state->refreshing=false;
}
}
