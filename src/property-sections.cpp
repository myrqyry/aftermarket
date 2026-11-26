#include "property-sections.h"
#include <QApplication>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHash>
#include <QPointer>
#include <QScrollArea>
#include <QStyle>
#include <QToolButton>
#include <QVector>

namespace augment {
struct PropertySections::State {
    struct Entry {
        QPointer<QFormLayout> form;
        QPointer<QToolButton> button;
        struct Visibility {QPointer<QWidget> widget;bool visible;};
        QVector<Visibility> visibility;
        bool collapsed=false;
    };
    QHash<QGroupBox*,Entry> groups;
    bool enabled=false;
};
PropertySections::PropertySections(QObject *parent):QObject(parent),state(new State) {
    qApp->installEventFilter(this);
}
PropertySections::~PropertySections() {
    if(qApp)qApp->removeEventFilter(this);
    const auto groups=state->groups.keys();
    for(auto *group:groups)restore(group);
}
bool PropertySections::enabled() const {return state->enabled;}
bool PropertySections::collapsed(const QGroupBox *group) const {
    auto it=state->groups.constFind(const_cast<QGroupBox*>(group));
    return it!=state->groups.cend()&&it->collapsed;
}
void PropertySections::setEnabled(bool value) {
    if(value==state->enabled)return;
    state->enabled=value;
    if(!value){
        const auto groups=state->groups.keys();
        for(auto *group:groups)restore(group);
        return;
    }
    // Attach to already-visible dialogs as well as subsequently created ones.
    for(auto *w:qApp->allWidgets())
        if(auto *group=qobject_cast<QGroupBox*>(w))
            if(group->isVisible())attach(group);
}
void PropertySections::restore(QGroupBox *group) {
    auto it=state->groups.find(group);
    if(it==state->groups.end())return;
    const auto entry=it.value();
    state->groups.erase(it);
    if(entry.collapsed)for(const auto &item:entry.visibility)
        if(item.widget)item.widget->setVisible(item.visible);
    group->setProperty("aftermarket-section-collapsed",QVariant());
    delete entry.button.data();
}
void PropertySections::positionButton(QGroupBox *group) {
    if(auto it=state->groups.find(group);it!=state->groups.end()&&it->button){
        auto *b=it->button.data();
        b->move(qMax(0,group->width()-b->width()-5),0);
        b->raise();
    }
}
void PropertySections::attach(QGroupBox *group) {
    if(!group||state->groups.contains(group)||group->isCheckable())return;
    auto *form=qobject_cast<QFormLayout*>(group->layout());
    if(!form||form->rowCount()==0)return;
    // Only OBS's property editors: avoid unrelated QGroupBox widgets or UI augment dialogs.
    bool withinContainer=false,withinPropertiesView=false;
    for(auto *p=group->parentWidget();p;p=p->parentWidget()){
        if(p->objectName()==QStringLiteral("PropertiesContainer"))withinContainer=true;
        if(withinContainer&&p->inherits("OBSPropertiesView")){withinPropertiesView=true;break;}
    }
    if(!withinPropertiesView)return;
    State::Entry entry;entry.form=form;

    auto *button=new QToolButton(group);
    button->setObjectName("aftermarket-group-toggle");
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::StrongFocus);
    button->setFixedSize(20,20);
    button->setArrowType(Qt::DownArrow);
    button->setToolTip("Collapse section");
    button->setAccessibleName("Collapse "+group->title());
    entry.button=button;
    state->groups.insert(group,entry);
    connect(button,&QToolButton::clicked,this,[this,group] {
        auto it=state->groups.find(group);
        if(it==state->groups.end()||!it->form)return;
        it->collapsed=!it->collapsed;
        if(it->collapsed){
            // Row indices change under responsive reflow. Capture actual
            // layout-owned widgets, including nested slider/spinbox pairs,
            // without touching the contents of child group widgets.
            it->visibility.clear();
            auto capture=[&](auto &&self,QLayout *layout)->void{
                for(int i=0;i<layout->count();++i){
                    auto *item=layout->itemAt(i);
                    if(auto *w=item->widget())it->visibility.append({w,!w->isHidden()});
                    else if(auto *child=item->layout())self(self,child);
                }
            };
            capture(capture,it->form);
            group->setProperty("aftermarket-section-collapsed",true);
            for(const auto &item:it->visibility)if(item.widget)item.widget->hide();
        }else{
            for(const auto &item:it->visibility)if(item.widget)item.widget->setVisible(item.visible);
            it->visibility.clear();
            group->setProperty("aftermarket-section-collapsed",false);
        }
        if(it->button){
            it->button->setArrowType(it->collapsed?Qt::RightArrow:Qt::DownArrow);
            it->button->setToolTip(it->collapsed?"Expand section":"Collapse section");
            it->button->setAccessibleName((it->collapsed?"Expand ":"Collapse ")+group->title());
        }
        group->updateGeometry();
    });
    connect(group,&QObject::destroyed,this,[this,group] {state->groups.remove(group);});
    positionButton(group);
    button->show();
}
bool PropertySections::eventFilter(QObject *o,QEvent *e) {
    if(!state->enabled)return false;
    if(auto *group=qobject_cast<QGroupBox*>(o)){
        if(e->type()==QEvent::Show)attach(group);
        if(e->type()==QEvent::Resize)positionButton(group);
    }
    return false;
}
}
