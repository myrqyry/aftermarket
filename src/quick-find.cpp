#include "quick-find.h"
#include "context-notes.h"
#include "plugin-theme-icons.h"
#include <QHash>
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
#include <algorithm>

namespace augment {
namespace {
int score(const QuickFindEntry &entry,const QStringList &terms) {
    const QString name=entry.name.toCaseFolded();
    const QString type=entry.type.toCaseFolded();
    const QString location=entry.location.toCaseFolded();
    const QString detail=entry.detail.toCaseFolded();
    int total=0;
    for(const auto &term:terms){
        if(name==term)total+=100;
        else if(name.startsWith(term))total+=70;
        else if(name.contains(term))total+=50;
        else if(type.contains(term))total+=25;
        else if(location.contains(term))total+=15;
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
                                      const QString &query,int limit){
    if(limit<=0)return {};
    const QStringList terms=query.simplified().toCaseFolded().split(' ',Qt::SkipEmptyParts);
    struct Ranked {int score;int index;};
    QList<Ranked> ranked;
    for(int i=0;i<all.size();++i){
        const int match=terms.isEmpty()?0:score(all[i],terms);
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
    results_=new QListWidget(this);
    results_->setObjectName("aftermarket-quick-find-results");
    results_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(results_,1);
    auto *hint=new QLabel("Enter: open result  ·  Choose an action for properties, filters or scene",this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *actions=new QHBoxLayout;
    properties_=new QPushButton("Properties",this);
    filters_=new QPushButton("Filters",this);
    jump_=new QPushButton("Go to scene",this);
    pin_=new QPushButton("Pin to workspace",this);
    pin_->setObjectName("aftermarket-quick-find-pin");
    pin_->setVisible(pinAvailable);
    pin_->setEnabled(false);
    auto *refresh=new QPushButton("Refresh",this);
    actions->addWidget(properties_);actions->addWidget(filters_);
    actions->addWidget(jump_);actions->addWidget(pin_);actions->addStretch();actions->addWidget(refresh);
    layout->addLayout(actions);
    connect(input_,&QLineEdit::textChanged,this,[this]{updateResults();});
    connect(results_,&QListWidget::itemSelectionChanged,this,[this]{
        auto *item=results_->currentItem();
        const int index=item?item->data(Qt::UserRole).toInt():-1;
        const bool found=index>=0&&index<cached_.size();
        const auto kind=found?cached_[index].kind:QuickFindEntry::Source;
        pin_->setEnabled(found&&kind!=QuickFindEntry::Filter);
        properties_->setEnabled(found&&kind==QuickFindEntry::Source);
        filters_->setEnabled(found&&kind!=QuickFindEntry::Scene);
        jump_->setEnabled(found&&(kind==QuickFindEntry::Scene||
                                  (kind==QuickFindEntry::Source&&!cached_[index].sceneId.isEmpty())));
    });
    connect(results_,&QListWidget::itemActivated,this,[this]{activate();});
    connect(properties_,&QPushButton::clicked,this,[this]{activate(Properties);});
    connect(filters_,&QPushButton::clicked,this,[this]{activate(Filters);});
    connect(jump_,&QPushButton::clicked,this,[this]{activate(SceneJump);});
    connect(pin_,&QPushButton::clicked,this,[this]{activate(PinSource);});
    connect(refresh,&QPushButton::clicked,this,[this]{
        cached_=provider_?provider_():QList<QuickFindEntry>{};
        updateResults();
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
        const auto &entry=cached_[index];
        const bool canPin=!pin_->isHidden()&&entry.kind!=QuickFindEntry::Filter;
        const bool canConfigure=entry.kind==QuickFindEntry::Source&&entry.performanceAvailable;
        if(!canPin&&!canConfigure&&!notesAvailable)return;
        results_->setCurrentItem(item);
        QMenu menu(this);
        if(notesAvailable)menu.addAction(entry.note.isEmpty()?"Add note…":"Edit note…",this,[this]{activate(Note);});
        if(canPin)menu.addAction("Pin to workspace",this,[this]{activate(PinSource);});
        if(canConfigure){
            auto *action=menu.addAction("Off-air frame limit…",this,[this]{activate(Performance);});
            action->setObjectName("aftermarket-source-performance");
        }
        menu.exec(results_->viewport()->mapToGlobal(pos));
    });
}

void QuickFind::open() {
    cached_=provider_?provider_():QList<QuickFindEntry>{};
    input_->clear();
    updateResults();
    show();raise();activateWindow();
    input_->setFocus();
    input_->selectAll();
}

void QuickFind::updateResults(){
    const auto filtered=search(cached_,input_->text());
    const PluginThemeIcons pluginIcons;
    QHash<QString,QIcon> iconCache;
    results_->clear();
    for(int i=0;i<filtered.size();++i){
        const auto &entry=filtered[i];
        const QString label=entry.name+"   ·   "+kindName(entry.kind)+
            (entry.location.isEmpty()?"":"   ·   "+entry.location);
        const QString secondary=entry.detail.isEmpty()?entry.type:entry.detail;
        auto *item=new QListWidgetItem(label,results_);
        if(entry.kind!=QuickFindEntry::Scene){
            const bool filter=entry.kind==QuickFindEntry::Filter;
            const auto key=(filter?QStringLiteral("filter:"):QStringLiteral("source:"))+entry.type;
            if(!iconCache.contains(key)){
                const auto glyph=filter?pluginIcons.filter(entry.type):pluginIcons.source(entry.type);
                iconCache.insert(key,glyph.itemIcon(results_->palette()));
            }
            if(!iconCache.value(key).isNull())item->setIcon(iconCache.value(key));
        }
        item->setToolTip(secondary);
        item->setData(NoteTextRole,entry.note);
        // Rows refer to the current catalog by their stable identities.
        for(int j=0;j<cached_.size();++j)
            if(cached_[j].kind==entry.kind && cached_[j].id==entry.id &&
               cached_[j].ownerId==entry.ownerId && cached_[j].sceneId==entry.sceneId){
                item->setData(Qt::UserRole,j);
                break;
            }
    }
    if(results_->count())results_->setCurrentRow(0);
    else {pin_->setEnabled(false);properties_->setEnabled(false);filters_->setEnabled(false);jump_->setEnabled(false);}
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
