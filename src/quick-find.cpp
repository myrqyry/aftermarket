#include "quick-find.h"
#include "context-notes.h"
#include "plugin-theme-icons.h"
#include <QHash>
#include <QSet>
#include <QComboBox>
#include <QSignalBlocker>
#include <QMap>
#include <QHideEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QKeyEvent>
#include <QPointer>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <algorithm>

namespace augment {
namespace {
int score(const QuickFindEntry &entry,const QStringList &terms) {
    const QString name=entry.name.toCaseFolded();
    const QString type=(entry.type+" "+entry.typeName).toCaseFolded();
    const QString location=(entry.location+" "+entry.canvasNames.join(" ")).toCaseFolded();
    const QString detail=entry.detail.toCaseFolded();
    int total=0;
    for(const auto &term:terms){
        if(name==term)total+=100;
        else if(name.startsWith(term))total+=70;
        else if(name.contains(term))total+=50;
        else if(type.contains(term))total+=25;
        else if(location.contains(term)||std::any_of(entry.placements.cbegin(),entry.placements.cend(),
             [&](const auto &p){return p.path.contains(term,Qt::CaseInsensitive);}))total+=15;
        else if(detail.contains(term))total+=10;
        else if(entry.note.contains(term,Qt::CaseInsensitive))total+=8;
        else return -1;
    }
    return total;
}
QString kindName(QuickFindEntry::Kind kind){
    if(kind==QuickFindEntry::Scene)return "Scene";
    if(kind==QuickFindEntry::Filter)return "Filter";
    return "Source";
}
}

QList<QuickFindEntry> QuickFind::search(const QList<QuickFindEntry> &all,
                                      const QString &query,int limit,const QuickFindFilter &filter){
    if(limit<=0)return {};
    const QStringList terms=query.simplified().toCaseFolded().split(' ',Qt::SkipEmptyParts);
    struct Ranked {int score;int index;};
    QList<Ranked> ranked;
    for(int i=0;i<all.size();++i){
        const auto &e=all[i];
        if((filter.scope==QuickFindFilter::Sources&&e.kind!=QuickFindEntry::Source)||
           (filter.scope==QuickFindFilter::Filters&&e.kind!=QuickFindEntry::Filter)||
           (filter.scope==QuickFindFilter::Scenes&&e.kind!=QuickFindEntry::Scene)||
           (filter.scope==QuickFindFilter::Unplaced&&!e.unplaced)||
           (!filter.type.isEmpty()&&e.type!=filter.type)||
           (!filter.canvas.isEmpty()&&!e.canvasIds.contains(filter.canvas)))continue;
        const int match=terms.isEmpty()?0:score(e,terms);
        if(match>=0)ranked.append({match,i});
    }
    std::sort(ranked.begin(),ranked.end(),[&](const Ranked &a,const Ranked &b){
        if(a.score!=b.score)return a.score>b.score;
        const int cmp=QString::compare(all[a.index].name,all[b.index].name,Qt::CaseInsensitive);
        if(cmp)return cmp<0;
        return a.index<b.index;
    });
    QList<QuickFindEntry> matches;
    for(int i=0;i<ranked.size()&&i<limit;++i)matches.append(all[ranked[i].index]);
    return matches;
}

QuickFind::QuickFind(QWidget *parent,Provider provider,Performer perform,bool pinAvailable,bool notesAvailable)
    :QDialog(parent),provider_(std::move(provider)),perform_(std::move(perform)) {
    setObjectName("aftermarket-quick-find");
    setWindowTitle("Aftermarket · Find sources");
    // Native popup stack handles outside clicks and nested context menus.
    setWindowFlags(Qt::Popup|Qt::FramelessWindowHint);
    setWindowModality(Qt::NonModal);
    resize(600,480);
    auto *layout=new QVBoxLayout(this);
    input_=new QLineEdit(this);
    input_->setObjectName("aftermarket-quick-find-input");
    input_->setPlaceholderText("Search sources, scenes, filters, paths or URLs…");
    input_->setClearButtonEnabled(true);
    layout->addWidget(input_);
    auto *facets=new QHBoxLayout;
    scope_=new QComboBox(this);scope_->setObjectName("aftermarket-quick-find-scope");
    for(const auto &label:{"Everything","Sources","Filters","Scenes","No scene placements"})scope_->addItem(label);
    type_=new QComboBox(this);type_->setObjectName("aftermarket-quick-find-type");
    canvas_=new QComboBox(this);canvas_->setObjectName("aftermarket-quick-find-canvas");
    for(auto *combo:{scope_,type_,canvas_}){combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);combo->setMinimumContentsLength(8);facets->addWidget(combo,1);}
    layout->addLayout(facets);
    connect(scope_,&QComboBox::currentIndexChanged,this,[this]{updateFacets();updateResults();});
    connect(type_,&QComboBox::currentIndexChanged,this,[this]{updateResults();});
    connect(canvas_,&QComboBox::currentIndexChanged,this,[this]{updateResults();});
    count_=new QLabel(this);count_->setObjectName("aftermarket-quick-find-count");
    refreshTimer_.setInterval(300);
    connect(&refreshTimer_,&QTimer::timeout,this,[this]{refreshIfChanged();});
    results_=new QListWidget(this);
    results_->setObjectName("aftermarket-quick-find-results");
    results_->setSelectionMode(QAbstractItemView::SingleSelection);
    results_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    results_->setTextElideMode(Qt::ElideRight);
    layout->addWidget(results_,1);
    auto *hint=new QLabel("Enter: open result  ·  Choose an action for properties, filters or scene",this);
    hint->setWordWrap(true);
    layout->addWidget(count_);
    layout->addWidget(hint);
    auto *actions=new QHBoxLayout;
    properties_=new QPushButton("Properties",this);
    filters_=new QPushButton("Filters",this);
    jump_=new QPushButton("Go to scene",this);
    pin_=new QPushButton("Pin to workspace",this);
    pin_->setObjectName("aftermarket-quick-find-pin");
    pin_->setVisible(pinAvailable);
    pin_->setEnabled(false);
    used_=new QPushButton("Used in…",this);used_->setObjectName("aftermarket-quick-find-used");used_->setEnabled(false);
    auto *refresh=new QPushButton("Refresh",this);
    actions->addWidget(properties_);actions->addWidget(filters_);
    actions->addWidget(jump_);actions->addWidget(used_);actions->addWidget(pin_);actions->addStretch();actions->addWidget(refresh);
    layout->addLayout(actions);
    connect(input_,&QLineEdit::textChanged,this,[this]{updateResults();});
    connect(results_,&QListWidget::itemSelectionChanged,this,[this]{
        auto *item=results_->currentItem();
        const int index=item?item->data(Qt::UserRole).toInt():-1;
        const bool found=index>=0&&index<cached_.size();
        const auto kind=found?cached_[index].kind:QuickFindEntry::Source;
        pin_->setEnabled(found&&kind!=QuickFindEntry::Filter);
        properties_->setEnabled(found&&kind==QuickFindEntry::Source);
        filters_->setEnabled(found);
        used_->setEnabled(found&&!cached_[index].placements.isEmpty());
        jump_->setEnabled(found&&((kind==QuickFindEntry::Scene&&cached_[index].mainCanvas)||
                                  (kind==QuickFindEntry::Source&&!cached_[index].sceneId.isEmpty())));
    });
    connect(results_,&QListWidget::itemActivated,this,[this]{activate();});
    connect(properties_,&QPushButton::clicked,this,[this]{activate(Properties);});
    connect(filters_,&QPushButton::clicked,this,[this]{activate(Filters);});
    connect(jump_,&QPushButton::clicked,this,[this]{
        auto *item=results_->currentItem();if(!item)return;
        const int i=item->data(Qt::UserRole).toInt();if(i<0||i>=cached_.size())return;
        const auto &entry=cached_[i];
        if(entry.kind==QuickFindEntry::Source&&entry.placements.size()>1){used_->click();return;}
        activate(SceneJump);
    });
    connect(pin_,&QPushButton::clicked,this,[this]{activate(PinSource);});
    connect(refresh,&QPushButton::clicked,this,[this]{refreshCatalog();});
    connect(used_,&QPushButton::clicked,this,[this]{
        auto *item=results_->currentItem();if(!item)return;
        const int i=item->data(Qt::UserRole).toInt();if(i<0||i>=cached_.size())return;
        QMenu menu(this);appendUsage(menu,cached_[i]);menu.exec(used_->mapToGlobal(QPoint(0,used_->height())));
    });
    connect(input_,&QLineEdit::returnPressed,this,[this]{activate();});
    auto *down=new QShortcut(QKeySequence(Qt::Key_Down),input_);
    connect(down,&QShortcut::activated,this,[this]{
        if(results_->count()){
            results_->setFocus();
            results_->setCurrentRow(qMin(results_->count()-1,results_->currentRow()+1));
        }
    });
    auto *escape=new QShortcut(QKeySequence(Qt::Key_Escape),this);
    connect(escape,&QShortcut::activated,this,&QDialog::reject);
    if(notesAvailable)attachNoteRows(results_,[this](QListWidgetItem *row){results_->setCurrentItem(row);activate(Note);});
    results_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(results_,&QListWidget::customContextMenuRequested,this,[this,notesAvailable](const QPoint &pos){
        auto *item=results_->itemAt(pos);if(!item)return;
        const int index=item->data(Qt::UserRole).toInt();
        if(index<0||index>=cached_.size())return;
        const auto entry=cached_[index];
        const bool canPin=!pin_->isHidden()&&entry.kind!=QuickFindEntry::Filter;
        const bool canConfigure=entry.kind==QuickFindEntry::Source&&entry.performanceAvailable;

        results_->setCurrentItem(item);
        QMenu menu(this);
        if(entry.kind==QuickFindEntry::Source)menu.addAction("Properties",this,[this,entry]{performEntry(entry,Properties);});
        menu.addAction("Filters",this,[this,entry]{performEntry(entry,Filters);});
        if(!entry.placements.isEmpty()){auto *usage=menu.addMenu("Used in…");appendUsage(*usage,entry);}
        if(notesAvailable)menu.addAction(entry.note.isEmpty()?"Add note…":"Edit note…",this,[this]{activate(Note);});
        if(canPin)menu.addAction("Pin to workspace",this,[this]{activate(PinSource);});
        if(canConfigure){
            auto *action=menu.addAction("Off-air frame limit…",this,[this]{activate(Performance);});
            action->setObjectName("aftermarket-source-performance");
        }
        menu.exec(results_->viewport()->mapToGlobal(pos));
    });
}

QuickFindFilter QuickFind::filter()const{
    return {static_cast<QuickFindFilter::Scope>(scope_->currentIndex()),type_->currentData().toString(),canvas_->currentData().toString()};
}
void QuickFind::updateFacets(){
    auto f=filter();f.type.clear();f.canvas.clear();
    const auto entries=search(cached_,{},cached_.size(),f);
    QMap<QString,QString> types,canvases;
    for(const auto &entry:entries){
        if(!entry.type.isEmpty())types.insert(entry.type,entry.typeName.isEmpty()?entry.type:entry.typeName);
        for(int i=0;i<entry.canvasIds.size();++i)canvases.insert(entry.canvasIds[i],entry.canvasNames.value(i,entry.canvasIds[i]));
    }
    auto fill=[](QComboBox *combo,const QMap<QString,QString> &values,const QString &all){
        QSignalBlocker block(combo);const auto previous=combo->currentData();combo->clear();combo->addItem(all,QString{});
        QList<QPair<QString,QString>> ordered;
        for(auto it=values.cbegin();it!=values.cend();++it)ordered.append({it.value(),it.key()});
        std::sort(ordered.begin(),ordered.end(),[](const auto&a,const auto&b){return QString::localeAwareCompare(a.first,b.first)<0;});
        for(const auto &v:ordered)combo->addItem(v.first,v.second);
        combo->setCurrentIndex(qMax(0,combo->findData(previous)));
    };
    fill(type_,types,"All types");fill(canvas_,canvases,"All canvases");
}
void QuickFind::refreshCatalog(){
    generation_=changeProvider_?changeProvider_():0;
    cached_=provider_?provider_():QList<QuickFindEntry>{};
    updateFacets();updateResults();
}
void QuickFind::setChangeProvider(std::function<quint64()> provider,std::function<void(bool)> watch){
    changeProvider_=std::move(provider);watch_=std::move(watch);
}
void QuickFind::hideEvent(QHideEvent *event){
    refreshTimer_.stop();if(watch_)watch_(false);QDialog::hideEvent(event);
}
void QuickFind::refreshIfChanged(){
    if(!isVisible()||!changeProvider_||QApplication::activePopupWidget()!=this)return;
    if(generation_!=changeProvider_())refreshCatalog();
}
void QuickFind::invalidateCollection(){hide();cached_.clear();results_->clear();}
void QuickFind::open() {
    if(watch_)watch_(true);
    if(changeProvider_)refreshTimer_.start();
    {QSignalBlocker block(input_);input_->clear();}
    refreshCatalog();
    show();raise();activateWindow();input_->setFocus();input_->selectAll();
}
void QuickFind::performEntry(const QuickFindEntry &entry,Action action){
    if(perform_&&perform_(entry,action))hide();
}
void QuickFind::appendUsage(QMenu &menu,const QuickFindEntry &entry){
    for(const auto &p:entry.placements){
        QString label=p.canvasName.isEmpty()?p.path:p.canvasName+" · "+p.path;
        // Identical names can refer to distinct placements.
        label+="  (#"+QString::number(p.itemId)+")";
        auto hit=entry;hit.sceneId=p.rootId;hit.revealKey=p.key;
        auto *branch=menu.addMenu(label);
        if(p.mainCanvas&&entry.kind!=QuickFindEntry::Filter)
            branch->addAction("Go to scene",this,[this,hit]{performEntry(hit,SceneJump);});
        if(p.mainCanvas&&p.revealable&&revealAvailable_)
            branch->addAction("Reveal in Unscene (no scene activation)",this,[this,hit]{performEntry(hit,RevealPlacement);});
        branch->addAction("Copy placement path",this,[path=p.path]{QApplication::clipboard()->setText(path);});
    }
}
void QuickFind::updateResults(){
    const auto filtered=search(cached_,input_->text(),cached_.size(),filter());
    QString previous;
    if(auto *item=results_->currentItem())previous=item->data(Qt::UserRole+10).toString();
    count_->setText(QString("%1 result(s)%2").arg(filtered.size()).arg(filtered.size()>80?" · Showing first 80":""));
    const PluginThemeIcons pluginIcons;
    QHash<QString,PluginThemeIcons::Glyph> iconCache;
    results_->clear();
    int selected=0;
    for(int i=0;i<qMin(qsizetype(80),filtered.size());++i){
        const auto &entry=filtered[i];
        QSet<QString> uniquePlacements,rootScenes;
        for(const auto &p:entry.placements){
            uniquePlacements.insert(p.parentId+"/"+QString::number(p.itemId));
            rootScenes.insert(p.rootId);
        }
        QString usageSummary;
        if(!uniquePlacements.isEmpty()){
            usageSummary=QString(" · %1 placement(s) / %2 scene(s)").arg(uniquePlacements.size()).arg(rootScenes.size());
            if(entry.placements.size()>=256)usageSummary+=" (partial)";
        }
        const QString label=entry.name+"   ·   "+(entry.typeName.isEmpty()?kindName(entry.kind):entry.typeName)+
            (entry.kind==QuickFindEntry::Filter?"   ·   on "+entry.location.section(" · ",0,0):"")+
            (entry.canvasNames.isEmpty()?"":"   ·   "+entry.canvasNames.join(", "))+
            (entry.unplaced?"   ·   No scene placements":"")+usageSummary;
        const QString secondary=entry.detail.isEmpty()?entry.type:entry.detail;
        auto *item=new QListWidgetItem(label,results_);
        if(entry.kind!=QuickFindEntry::Scene){
            const bool filter=entry.kind==QuickFindEntry::Filter;
            const auto key=(filter?QStringLiteral("filter:"):QStringLiteral("source:"))+entry.type;
            if(!iconCache.contains(key)){
                const auto glyph=filter?pluginIcons.filter(entry.type):pluginIcons.source(entry.type);
                iconCache.insert(key,glyph);
            }
            PalettePluginIcons::bind(results_,results_->model()->index(results_->count()-1,0),iconCache.value(key));
        }
        QStringList usage;
        for(const auto &p:entry.placements)usage.append(p.canvasName+" · "+p.path);
        item->setToolTip(entry.name+" · "+secondary+(usage.isEmpty()?(entry.location.isEmpty()?"":"\n"+entry.location):"\nUsed in "+QString::number(usage.size())+" path(s):\n"+usage.join("\n")));
        const auto identity=QString::number(entry.kind)+":"+entry.ownerId+":"+entry.id;
        item->setData(Qt::UserRole+10,identity);if(identity==previous)selected=i;
        item->setData(NoteTextRole,entry.note);
        // Rows refer to the current catalog by their stable identities.
        for(int j=0;j<cached_.size();++j)
            if(cached_[j].kind==entry.kind && cached_[j].id==entry.id &&
               cached_[j].ownerId==entry.ownerId && cached_[j].sceneId==entry.sceneId){
                item->setData(Qt::UserRole,j);
                break;
            }
    }
    if(results_->count())results_->setCurrentRow(selected);
    else {used_->setEnabled(false);pin_->setEnabled(false);properties_->setEnabled(false);filters_->setEnabled(false);jump_->setEnabled(false);}
}

bool QuickFind::activate(Action action){
    auto *selected=results_->currentItem();
    if(!selected||!perform_)return false;
    const int index=selected->data(Qt::UserRole).toInt();
    if(index<0||index>=cached_.size())return false;
    const QuickFindEntry entry=cached_[index];
    if(action==PinSource&&(pin_->isHidden()||entry.kind==QuickFindEntry::Filter))return false;
    if(action==Performance&&(entry.kind!=QuickFindEntry::Source||!entry.performanceAvailable))return false;
    if(!perform_(entry,action))return false;
    hide();
    return true;
}
} // namespace augment
