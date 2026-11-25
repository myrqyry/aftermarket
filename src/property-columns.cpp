#include "property-columns.h"
#include <QApplication>
#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QAbstractSpinBox>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QColor>
#include <QPointer>
#include <QTimer>
#include <QHash>
#include <algorithm>

namespace augment {
namespace {
using Role=QFormLayout::ItemRole;
enum class Kind { Wide, Compact, Group };
struct Row {
    bool label=false,spanning=false;
    Qt::Alignment labelAlignment,fieldAlignment;
};
struct Run {
    QPointer<QGridLayout> grid;
    QList<Row> rows;
};
struct Entry {
    QPointer<QFormLayout> form;
    QList<Run> runs;
    int columns=1,expectedRows=0;
};
bool compactField(QLayoutItem *item){
    if(!item)return false;
    if(auto *w=item->widget()){
        return qobject_cast<QAbstractSpinBox*>(w)||qobject_cast<QSlider*>(w)||
            qobject_cast<QCheckBox*>(w)||qobject_cast<QComboBox*>(w);
    }
    auto *layout=item->layout();
    if(!layout||layout->count()==0)return false;
    // Native AddColorInternal uses a framed hexadecimal swatch and button.
    // Recognize the structure, not the localized button label.
    if(layout->count()==2){
        auto *swatch=qobject_cast<QLabel*>(layout->itemAt(0)->widget());
        auto *button=qobject_cast<QPushButton*>(layout->itemAt(1)->widget());
        if(swatch&&button&&(swatch->autoFillBackground()||
           swatch->styleSheet().contains("background-color",Qt::CaseInsensitive))&&
           swatch->frameShape()==QFrame::Panel&&swatch->text().startsWith('#')&&
           (swatch->text().size()==7||swatch->text().size()==9)&&QColor(swatch->text()).isValid())
            return true;
    }

    // OBS numeric slider + spinbox layouts; do not split path/editable-list/
    // font/multiline controls or arbitrary custom nested layouts.
    for(int i=0;i<layout->count();++i){
        auto *child=layout->itemAt(i);
        if(!child->widget()||!compactField(child))return false;
    }
    return true;
}
Kind kind(QFormLayout *form,int row,int cellWidth){
    if(!form->isRowVisible(row))return Kind::Wide;
    auto *span=form->itemAt(row,Role::SpanningRole);
    auto *field=form->itemAt(row,Role::FieldRole);
    auto *label=form->itemAt(row,Role::LabelRole);
    if(span&&span->widget()&&qobject_cast<QGroupBox*>(span->widget())&&
       span->minimumSize().width()<=cellWidth)return Kind::Group;
    if(!field&&span)field=span;
    if(!compactField(field))return Kind::Wide;
    const int minWidth=field->minimumSize().width()+(label?label->minimumSize().width()+8:0);
    return minWidth<=cellWidth?Kind::Compact:Kind::Wide;
}
void addGridItem(QGridLayout *grid,QLayoutItem *item,int row,int col,int span,Qt::Alignment alignment={}){
    if(auto *widget=item->widget()){
        delete item;
        grid->addWidget(widget,row,col,1,span,alignment);
    }else if(auto *layout=item->layout()){
        layout->setParent(nullptr);
        grid->addLayout(layout,row,col,1,span,alignment);
    }
}
void restore(Entry &entry){
    if(!entry.form)return;
    auto *form=entry.form.data();
    for(auto &run:entry.runs){
        auto *grid=run.grid.data();
        if(!grid)continue;
        int at=-1;Role role;
        form->getLayoutPosition(grid,&at,&role);
        if(at<0)continue;
        auto taken=form->takeRow(grid);
        Q_UNUSED(taken);
        // QGridLayout preserves insertion order; widgets/layouts are the
        // same objects, no parenting change and no value/visibility writes.
        for(const auto &row:run.rows){
            QLayoutItem *label=row.label?grid->takeAt(0):nullptr;
            QLayoutItem *field=grid->takeAt(0);
            if(!field){delete label;continue;}
            QWidget *left=label?label->widget():nullptr;
            if(auto *w=field->widget()){
                if(row.spanning)form->insertRow(at,w);
                else form->insertRow(at,left,w);
                delete field;
            }else if(auto *layout=field->layout()){
                layout->setParent(nullptr);
                if(row.spanning)form->insertRow(at,layout);
                else form->insertRow(at,left,layout);
            }
            if(label)delete label;
            if(auto *item=form->itemAt(at,Role::LabelRole))item->setAlignment(row.labelAlignment);
            if(auto *item=form->itemAt(at,row.spanning?Role::SpanningRole:Role::FieldRole))item->setAlignment(row.fieldAlignment);
            ++at;
        }
        delete grid;
    }
    entry.runs.clear();entry.columns=1;entry.expectedRows=form->rowCount();
    form->invalidate();
}
void adapt(Entry &entry,int columns,int cellWidth){
    auto *form=entry.form.data();
    if(!form||columns<2)return;
    int i=0;
    while(i<form->rowCount()){
        const auto type=kind(form,i,cellWidth);
        if(type==Kind::Wide){++i;continue;}
        int end=i+1;
        while(end<form->rowCount()&&kind(form,end,cellWidth)==type)++end;
        const int count=end-i;
        if(count<2){++i;continue;}
        auto *grid=new QGridLayout;
        grid->setObjectName("aftermarket-property-columns");
        grid->setContentsMargins(0,0,0,0);
        grid->setHorizontalSpacing(qMax(8,form->horizontalSpacing()));
        grid->setVerticalSpacing(qMax(4,form->verticalSpacing()));
        Run run;run.grid=grid;
        for(int n=0;n<count;++n){
            const bool spanning=form->itemAt(i,Role::SpanningRole)!=nullptr;
            auto taken=form->takeRow(i);
            auto *left=taken.labelItem;
            auto *right=taken.fieldItem;
            if(!right){right=left;left=nullptr;}
            Row row{bool(left),spanning,left?left->alignment():Qt::Alignment{},right->alignment()};
            run.rows.append(row);
            const int r=n/columns,c=(n%columns)*2;
            if(left){
                addGridItem(grid,left,r,c,1,Qt::AlignRight|Qt::AlignVCenter);
                addGridItem(grid,right,r,c+1,1);
            }else addGridItem(grid,right,r,c,2);
        }
        for(int c=0;c<columns;++c)grid->setColumnStretch(c*2+1,1);
        form->insertRow(i,grid);
        entry.runs.append(run);
        ++i;
    }
    entry.columns=columns;entry.expectedRows=form->rowCount();
    form->invalidate();
}
QScrollArea *propertyView(QWidget *container){
    if(!container||container->objectName()!="PropertiesContainer")return nullptr;
    for(auto *p=container->parentWidget();p;p=p->parentWidget()){
        if(!p->inherits("OBSPropertiesView"))continue;
        auto *view=qobject_cast<QScrollArea*>(p);
        if(!view||view->widget()!=container)return nullptr;
        for(auto *owner=view->parentWidget();owner;owner=owner->parentWidget())
            if(owner->inherits("OBSBasicProperties")||owner->inherits("OBSBasicFilters"))return view;
        return nullptr;
    }
    return nullptr;
}
}
struct PropertyColumns::State{
    int mode=0;
    bool scheduled=false;
    QHash<QWidget*,Entry> entries;
};
PropertyColumns::PropertyColumns(QObject *parent):QObject(parent),state(new State){
    if(qApp)qApp->installEventFilter(this);
    schedule();
}
PropertyColumns::~PropertyColumns(){if(qApp)qApp->removeEventFilter(this);restoreAll();}
int PropertyColumns::mode()const{return state->mode;}
void PropertyColumns::setMode(int mode){
    if(mode<0||mode>3||mode==state->mode)return;
    restoreAll();state->mode=mode;schedule();
}
void PropertyColumns::restoreAll(){
    for(auto &entry:state->entries)restore(entry);
    state->entries.clear();
}
void PropertyColumns::schedule(){
    if(state->scheduled||state->mode==1)return;
    state->scheduled=true;
    QTimer::singleShot(0,this,[this]{state->scheduled=false;scan();});
}
void PropertyColumns::scan(){
    if(state->mode==1)return;
    for(auto it=state->entries.begin();it!=state->entries.end();)
        if(!it->form)it=state->entries.erase(it);else ++it;
    for(auto *w:QApplication::allWidgets()){
        if(w->objectName()!="PropertiesContainer"||!w->isVisible())continue;
        auto *view=propertyView(w);
        auto *form=qobject_cast<QFormLayout*>(w->layout());
        if(!view||!form)continue;
        const auto margins=form->contentsMargins();
        const int width=view->viewport()->width()-margins.left()-margins.right();
        const int maxCols=state->mode==0?3:state->mode;
        const int cols=qBound(1,width/340,maxCols);
        auto &entry=state->entries[w];
        if(entry.form!=form){entry=Entry{};entry.form=form;}
        if(entry.columns==cols&&entry.expectedRows==form->rowCount())continue;
        restore(entry);
        adapt(entry,cols,328);
    }
}
bool PropertyColumns::eventFilter(QObject *obj,QEvent *event){
    if(state->mode==1)return false;
    if(event->type()!=QEvent::Show&&event->type()!=QEvent::Resize)return false;
    auto *w=qobject_cast<QWidget*>(obj);
    if(w&&(w->objectName()=="PropertiesContainer"||w->inherits("OBSPropertiesView")||
        (w->parentWidget()&&w->parentWidget()->inherits("OBSPropertiesView"))))schedule();
    return false;
}
}
