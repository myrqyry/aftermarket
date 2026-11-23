#include "obs-settings-page.h"
#include "augment.h"
#include "dock-title-icons.h"
#include "gradient.h"
#include <QApplication>
#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QDialogButtonBox>
#include <QAbstractButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QTabWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QTimer>
#include <QEvent>
#include <QSettings>
#include <QSignalBlocker>
#include <QMenu>
#include <QFont>
#include <QJsonArray>
#include <QIcon>
#include <QPointer>
#include <QVector>
#include <QStringList>
#include <algorithm>

namespace augment {
struct ObsSettingsPage::Entry {
    QPointer<QDialog> dialog;
    QPointer<QListWidget> navigation;
    QPointer<QStackedWidget> pages;
    QPointer<QWidget> page;
    QPointer<QLabel> message;
    QPointer<QComboBox> ellipsis;
    QPointer<QPushButton> applyButton;
    QHash<QString,QPointer<QCheckBox>> flags;
    QHash<QString,bool> baseline;
    QPointer<QLineEdit> gradientColors,knobSources;
    QPointer<QSpinBox> gradientPeriod;
    QJsonObject gradientBaseline,listRowsBaseline;
    QString existingMode;
    bool editPending=false;
};
ObsSettingsPage::ObsSettingsPage(Controller &controller,DockTitleIcons &titles,
                                  Services services,QObject *parent)
    :QObject(parent),controller_(&controller),titles_(&titles),services_(std::move(services)){
    if(qApp)qApp->installEventFilter(this);
    scheduleScan();
}
ObsSettingsPage::~ObsSettingsPage(){
    if(qApp)qApp->removeEventFilter(this);
    detach();
}
bool ObsSettingsPage::eventFilter(QObject *object,QEvent *event){
    if(!object||!event)return false;
    if(event->type()==QEvent::Show||event->type()==QEvent::Polish){
        if(object->objectName()=="OBSBasicSettings")
            scheduleScan();
    }
    return false;
}
void ObsSettingsPage::scheduleScan(){
    if(scheduled_||!qApp)return;
    scheduled_=true;
    QTimer::singleShot(0,this,[this]{
        scheduled_=false;
        scan();
    });
}
void ObsSettingsPage::scan(){
    if(!qApp||!controller_||!titles_)return;
    for(auto *widget:QApplication::allWidgets()){
        auto *dialog=qobject_cast<QDialog*>(widget);
        if(!dialog||dialog->objectName()!="OBSBasicSettings"||
           entries_.contains(dialog))continue;
        attach(dialog);
    }
}
void ObsSettingsPage::attach(QDialog *dialog){
    if(!dialog||!controller_||!titles_)return;
    auto *nav=dialog->findChild<QListWidget*>("listWidget");
    auto *stack=dialog->findChild<QStackedWidget*>("settingsPages");
    auto *buttons=dialog->findChild<QDialogButtonBox*>("buttonBox");
    // OBS 32.x owns this pair. Abort if another theme/plugin replaced it
    // or its native index alignment is already inconsistent.
    if(!nav||!stack||!buttons||nav->count()<8||nav->count()!=stack->count())
        return;
    if(nav->findItems("Aftermarket",Qt::MatchExactly).size())return;
    auto entry=std::make_shared<Entry>();
    entry->dialog=dialog;
    entry->navigation=nav;
    entry->pages=stack;
    entry->page=new QWidget(stack);
    entry->page->setObjectName("aftermarket-native-settings");
    auto *page=entry->page.data();
    auto *root=new QVBoxLayout(page);
    root->setContentsMargins(8,6,8,6);
    root->setSpacing(9);
    auto *heading=new QLabel("Aftermarket",page);
    QFont title=heading->font();
    title.setWeight(QFont::DemiBold);
    if(title.pointSizeF()>0)title.setPointSizeF(title.pointSizeF()+2);
    heading->setFont(title);
    root->addWidget(heading);
    auto *tabs=new QTabWidget(page);
    tabs->setObjectName("aftermarket-settings-tabs");
    root->addWidget(tabs,1);
    auto makeTab=[tabs](const QString &name){
        auto *scroller=new QScrollArea(tabs);
        scroller->setFrameShape(QFrame::NoFrame);
        scroller->setWidgetResizable(true);
        auto *contents=new QWidget(scroller);
        auto *layout=new QVBoxLayout(contents);
        layout->setContentsMargins(12,12,12,12);
        layout->setSpacing(10);
        scroller->setWidget(contents);
        tabs->addTab(scroller,name);
        return qMakePair(contents,layout);
    };
    auto checkbox=[entry](const QString &id,const QString &text,QVBoxLayout *layout,QWidget *parent,bool value){
        auto *check=new QCheckBox(text,parent);
        check->setObjectName("aftermarket-setting-"+id);
        check->setChecked(value);
        layout->addWidget(check);
        entry->flags.insert(id,check);
        entry->baseline.insert(id,value);
        return check;
    };
    auto appearance=makeTab("Appearance");
    auto *hint=new QLabel("Interface-wide text and visual presentation. "
                          "OBS source names and saved scenes remain unchanged.",appearance.first);
    hint->setWordWrap(true);
    appearance.second->addWidget(hint);
    auto *form=new QFormLayout;
    entry->ellipsis=new QComboBox(appearance.first);
    entry->ellipsis->setObjectName("augment-global-ellipsis");
    entry->ellipsis->setAccessibleName("Ellipsis placement throughout OBS");
    entry->ellipsis->addItem("At end",QStringLiteral("end"));
    entry->ellipsis->addItem("In middle",QStringLiteral("middle"));
    entry->existingMode=titles_->ellipsisMode()==Qt::ElideMiddle?"middle":"end";
    entry->ellipsis->setCurrentIndex(entry->existingMode=="middle"?1:0);
    form->addRow("Long text",entry->ellipsis);
    appearance.second->addLayout(form);
    checkbox("transform","Theme-colored transform frame and handles",
             appearance.second,appearance.first,controller_->modernTransformFrame());
    const auto stripes=controller_->listRowSettings();
    entry->listRowsBaseline=stripes;
    checkbox("scene-stripes","Alternate Scenes row backgrounds",
             appearance.second,appearance.first,stripes.value("scenes").toBool());
    checkbox("source-stripes","Alternate Sources row backgrounds",
             appearance.second,appearance.first,stripes.value("sources").toBool());
    appearance.second->addStretch(1);

    auto menus=makeTab("Menus");
    checkbox("compact-menus","Compact menus",menus.second,menus.first,
             controller_->compactMenusEnabled());
    checkbox("hamburger","Compact top menu bar",menus.second,menus.first,
             controller_->hamburgerMenuBarEnabled());
    checkbox("motion","Interface animations (menus, docks, AudioBus)",menus.second,menus.first,
             controller_->motionEnabled());
    checkbox("selections","Show selected submenu choices",menus.second,menus.first,
             controller_->showSubmenuSelections());
    menus.second->addStretch(1);

    auto mixer=makeTab("Mixer");
    checkbox("mixer-back","AudioBus alternate routing view",mixer.second,mixer.first,
             controller_->mixerBacksideEnabled());
    auto *mixerHint=new QLabel("Native OBS routing and gain controls are retained. "
                               "The AudioBus switch remains at the mixer dock bottom.",mixer.first);
    mixerHint->setWordWrap(true);
    mixer.second->addWidget(mixerHint);
    const auto gradient=controller_->gradientSettings();
    entry->gradientBaseline=gradient;
    GradientSpec spec;
    GradientSpec::parse(gradient,spec);
    checkbox("knobs","Use gain knobs",mixer.second,mixer.first,spec.knobs);
    checkbox("knob-meters","Peak meters integrated into knobs",mixer.second,mixer.first,spec.knobMeters);
    checkbox("gradients","Enable gradient tracks",mixer.second,mixer.first,spec.enabled);
    checkbox("slider-gradients","Sliders and rotary rings",mixer.second,mixer.first,spec.sliders);
    checkbox("progress-gradients","Progress bars",mixer.second,mixer.first,spec.progress);
    checkbox("reactive","Audio-reactive mixer gradients",mixer.second,mixer.first,spec.audioReactive);
    checkbox("meter-pulse","Pulse native meter colors",mixer.second,mixer.first,spec.meters);
    auto *mixerForm=new QFormLayout;
    entry->gradientPeriod=new QSpinBox(mixer.first);
    entry->gradientPeriod->setObjectName("aftermarket-setting-gradient-period");
    entry->gradientPeriod->setRange(400,12000);
    entry->gradientPeriod->setSuffix(" ms");
    entry->gradientPeriod->setSingleStep(100);
    entry->gradientPeriod->setValue(spec.period);
    mixerForm->addRow("Gradient loop",entry->gradientPeriod);
    entry->gradientColors=new QLineEdit(mixer.first);
    entry->gradientColors->setObjectName("aftermarket-setting-gradient-colors");
    entry->gradientColors->setPlaceholderText("Theme colors, or comma-separated hex colors");
    QStringList colors;
    for(const auto &color:spec.colors)colors.append(color.name());
    entry->gradientColors->setText(colors.join(", "));
    mixerForm->addRow("Colors",entry->gradientColors);
    entry->knobSources=new QLineEdit(mixer.first);
    entry->knobSources->setObjectName("aftermarket-setting-knob-sources");
    entry->knobSources->setText(spec.knobSources.join(", "));
    entry->knobSources->setPlaceholderText("Empty = all sources");
    entry->knobSources->setToolTip("Comma-separated exact source names; leave empty for all sources.");
    mixerForm->addRow("Knob sources",entry->knobSources);
    mixer.second->addLayout(mixerForm);
    mixer.second->addStretch(1);

    auto panels=makeTab("Panels");
    checkbox("sections","Collapsible property and filter groups",panels.second,panels.first,
             controller_->collapsiblePropertyGroups());
    auto *panelsHint=new QLabel("Each dock's three-dot menu provides Collapse / Expand. "
                                 "These actions are local to the dock.",panels.first);
    panelsHint->setWordWrap(true);
    panels.second->addWidget(panelsHint);
    panels.second->addStretch(1);

    auto actionStrip=makeTab("Action strip");
    checkbox("action-strip","Show OBS action strip",actionStrip.second,actionStrip.first,
             services_.actionStripEnabled?services_.actionStripEnabled():false);
    if(services_.editActionStrip){
        auto *edit=new QPushButton("Configure action-strip buttons…",actionStrip.first);
        edit->setObjectName("aftermarket-action-strip-edit");
        actionStrip.second->addWidget(edit);
        QPointer<ObsSettingsPage> safe=this;
        QObject::connect(edit,&QPushButton::clicked,page,[safe,entry]{
            if(safe&&safe->services_.editActionStrip&&entry->dialog)
                safe->services_.editActionStrip(entry->dialog);
        });
    }
    actionStrip.second->addStretch(1);
    entry->message=new QLabel("Changes take effect when you choose Apply or OK.",page);
    entry->message->setObjectName("aftermarket-settings-status");
    entry->message->setWordWrap(true);
    root->addWidget(entry->message);
    entry->applyButton=buttons->button(QDialogButtonBox::Apply);
    auto updateDirty=[entry]{
        if(!entry->page)return;
        bool changed=entry->ellipsis &&
            entry->ellipsis->currentData().toString()!=entry->existingMode;
        for(auto it=entry->flags.cbegin();it!=entry->flags.cend();++it)
            if(it.value()&&it.value()->isChecked()!=entry->baseline.value(it.key(),false))
                changed=true;
        GradientSpec spec;
        GradientSpec::parse(entry->gradientBaseline,spec);
        QStringList originalColors;
        for(const auto &color:spec.colors)originalColors.append(color.name());
        if(entry->gradientPeriod&&entry->gradientPeriod->value()!=spec.period)
            changed=true;
        if(entry->gradientColors&&entry->gradientColors->text()!=originalColors.join(", "))
            changed=true;
        if(entry->knobSources&&entry->knobSources->text()!=spec.knobSources.join(", "))
            changed=true;
        if(changed){
            entry->editPending=true;
            // Native OBS only watches its own fields; enable the existing
            // Apply button for plugin-owned edits without faking an OBS edit.
            if(entry->applyButton)entry->applyButton->setEnabled(true);
            if(entry->message)entry->message->setText(
                "Unapplied Aftermarket changes. Choose Apply or OK to save.");
        }else{
            entry->editPending=false;
            if(entry->message)entry->message->setText(
                "Changes take effect when you choose Apply or OK.");
            // Do not disable native Apply: OBS may have OTHER unsaved fields.
        }
    };
    QObject::connect(entry->ellipsis,qOverload<int>(&QComboBox::currentIndexChanged),
                     page,[updateDirty](int){updateDirty();});
    for(const auto &checkbox:entry->flags)
        if(checkbox)QObject::connect(checkbox,&QCheckBox::toggled,page,
                                     [updateDirty](bool){updateDirty();});
    QObject::connect(entry->gradientPeriod,qOverload<int>(&QSpinBox::valueChanged),
                     page,[updateDirty](int){updateDirty();});
    QObject::connect(entry->gradientColors,&QLineEdit::textChanged,
                     page,[updateDirty](const QString &){updateDirty();});
    QObject::connect(entry->knobSources,&QLineEdit::textChanged,
                     page,[updateDirty](const QString &){updateDirty();});

    stack->addWidget(page);
    auto *item=new QListWidgetItem("Aftermarket",nav);
    item->setData(Qt::UserRole,QStringLiteral("obs-ui-augment"));
    item->setIcon(QIcon::fromTheme("preferences-system"));
    entries_.insert(dialog,entry);
    QObject::connect(dialog,&QObject::destroyed,this,[this,dialog]{
        entries_.remove(dialog);
    });
    // OBS owns the Apply/OK/Cancel action. We only stage changed controls
    // until those native buttons are activated, leaving Cancel untouched.
    QObject::connect(buttons,&QDialogButtonBox::clicked,this,
        [this,entry,buttons](QAbstractButton *button){
            const auto role=buttons->buttonRole(button);
            if(role==QDialogButtonBox::ApplyRole||role==QDialogButtonBox::AcceptRole)
                commit(entry);
        });
}
bool ObsSettingsPage::commit(const std::shared_ptr<Entry> &entry){
    if(!controller_||!titles_||!entry||!entry->page)return false;
    // Build from CURRENT config to avoid overwriting unrelated edits made
    // outside the settings dialog while it was open.
    QString error;
    struct Change {
        std::function<bool(QString&)> apply;
        std::function<void()> undo;
    };
    QVector<Change> changes;
    auto addFlag=[&](const QString &key,
                     std::function<bool(bool,QString&)> write){
        auto it=entry->flags.find(key);
        if(it==entry->flags.end()||!it.value())return;
        const bool previous=entry->baseline.value(key,false);
        const bool current=it.value()->isChecked();
        if(current==previous)return;
        changes.append({[write,current](QString &err){return write(current,err);},
                       [write,previous]{QString ignored;write(previous,ignored);}});
    };
    const auto priorMode=titles_->ellipsisMode();
    const auto mode=entry->ellipsis->currentData().toString()=="middle"?
        Qt::ElideMiddle:Qt::ElideRight;
    if(mode!=priorMode){
        QPointer<DockTitleIcons> target=titles_;
        changes.append({[target,mode](QString &err){
                            if(target&&target->setEllipsisMode(mode))return true;
                            err="Unable to save app-wide ellipsis placement.";return false;
                        },
                        [target,priorMode]{if(target)target->setEllipsisMode(priorMode);}});
    }
    QPointer<Controller> c=controller_;
    addFlag("transform",[c](bool b,QString &e){return c&&c->setModernTransformFrame(b,&e);});
    addFlag("compact-menus",[c](bool b,QString &e){return c&&c->setCompactMenus(b,&e);});
    addFlag("motion",[c](bool b,QString &e){return c&&c->setMotionEnabled(b,&e);});
    addFlag("selections",[c](bool b,QString &e){return c&&c->setShowSubmenuSelections(b,&e);});
    addFlag("sections",[this,c](bool b,QString &e){
        if(!c||!c->setCollapsiblePropertyGroups(b,&e))return false;
        if(services_.setPropertySections)services_.setPropertySections(b);
        return true;
    });
    addFlag("hamburger",[this,c](bool b,QString &e){
        if(services_.setHamburger&&!services_.setHamburger(b)){
            e="Cannot switch the native OBS menu bar in this layout.";return false;
        }
        if(c&&c->setHamburgerMenuBarEnabled(b,&e))return true;
        if(services_.setHamburger)services_.setHamburger(!b);
        return false;
    });
    addFlag("mixer-back",[this,c](bool b,QString &e){
        if(services_.setMixer&&!services_.setMixer(b)){
            e="Native OBS Audio Mixer is unavailable.";return false;
        }
        if(c&&c->setMixerBacksideEnabled(b,&e))return true;
        if(services_.setMixer)services_.setMixer(!b);
        return false;
    });
    addFlag("action-strip",[this](bool b,QString &e){
        if(services_.setActionStrip&&services_.setActionStrip(b))return true;
        e="Cannot update the OBS action strip in this layout.";return false;
    });
    auto currentRows=controller_->listRowSettings();
    auto newRows=currentRows;
    for(const auto &[key,field]:{qMakePair("scenes","scene-stripes"),
                                  qMakePair("sources","source-stripes")}){
        const bool old=entry->baseline.value(field,false);
        const bool next=entry->flags.value(field)->isChecked();
        if(old!=next)newRows[key]=next;
    }
    if(newRows!=currentRows){
        changes.append({[this,c,newRows](QString &e){
                            if(!c||!c->setListRowSettings(newRows,&e))return false;
                            if(services_.setRowStripes)services_.setRowStripes(newRows);
                            return true;
                        },
                        [this,c,currentRows]{
                            QString ignored;
                            if(c)c->setListRowSettings(currentRows,&ignored);
                            if(services_.setRowStripes)services_.setRowStripes(currentRows);
                        }});
    }
    const QJsonObject originalGradient=controller_->gradientSettings();
    auto updatedGradient=originalGradient;
    const auto fields=QVector<QPair<QString,QString>>{
        {"knobs","knobs"},{"knob-meters","knobMeters"},
        {"gradients","enabled"},{"slider-gradients","sliders"},
        {"progress-gradients","progress"},{"reactive","audioReactive"},
        {"meter-pulse","meters"}
    };
    for(const auto &field:fields){
        const bool before=entry->baseline.value(field.first,false);
        const bool now=entry->flags.value(field.first)->isChecked();
        if(before!=now)updatedGradient[field.second]=now;
    }
    GradientSpec originalSpec;
    GradientSpec::parse(entry->gradientBaseline,originalSpec);
    if(entry->gradientPeriod->value()!=originalSpec.period)
        updatedGradient["period"]=entry->gradientPeriod->value();
    const QStringList sources=entry->knobSources->text()
        .split(',',Qt::SkipEmptyParts);
    QStringList cleanSources;
    for(auto value:sources){
        value=value.trimmed();
        if(!value.isEmpty()&&!cleanSources.contains(value))cleanSources.append(value);
    }
    if(cleanSources!=originalSpec.knobSources)
        updatedGradient["knobSources"]=QJsonArray::fromStringList(cleanSources);
    QStringList existingColors;
    for(const auto &color:originalSpec.colors)existingColors.append(color.name());
    if(entry->gradientColors->text().trimmed()!=existingColors.join(", ")){
        QJsonArray arr;
        for(const auto &token:entry->gradientColors->text().split(',',Qt::SkipEmptyParts)){
            QColor color(token.trimmed());
            if(!color.isValid()){
                entry->message->setText("Invalid gradient color: "+token.trimmed());
                return false;
            }
            arr.append(color.name());
        }
        updatedGradient["colors"]=arr;
    }
    if(updatedGradient!=originalGradient){
        GradientSpec validated;
        if(!GradientSpec::parse(updatedGradient,validated)){
            entry->message->setText("Invalid mixer gradient settings. Nothing saved.");
            return false;
        }
        changes.append({[c,updatedGradient,originalGradient](QString &e){
                            return c&&c->setGradientSettings(updatedGradient,&e,&originalGradient);
                        },
                        [c,originalGradient]{
                            QString ignored;
                            if(c)c->setGradientSettings(originalGradient,&ignored);
                        }});
    }
    int applied=0;
    for(const auto &change:changes){
        if(!change.apply(error)){
            for(int i=applied-1;i>=0;--i)changes.at(i).undo();
            entry->message->setText("Not saved: "+(error.isEmpty()?
                QStringLiteral("A setting could not be applied."):error));
            return false;
        }
        ++applied;
    }
    // Current state is the new baseline for repeated Apply clicks.
    for(auto it=entry->flags.cbegin();it!=entry->flags.cend();++it)
        if(it.value())entry->baseline[it.key()]=it.value()->isChecked();
    entry->gradientBaseline=controller_->gradientSettings();
    entry->listRowsBaseline=controller_->listRowSettings();
    entry->existingMode=mode==Qt::ElideMiddle?"middle":"end";
    entry->editPending=false;
    entry->message->setText(applied?"Aftermarket settings applied and saved.":
                                "No Aftermarket settings changed.");
    return true;
}
void ObsSettingsPage::detach(){
    for(const auto &entry:entries_){
        if(!entry||!entry->page||!entry->dialog)continue;
        // Only tear down our OWN additions; never destroy OBS pages.
        if(entry->navigation&&entry->pages){
            const int idx=entry->pages->indexOf(entry->page);
            if(idx>=0){
                if(entry->navigation->currentRow()==idx)
                    entry->navigation->setCurrentRow(0);
                if(idx<entry->navigation->count() &&
                   entry->navigation->item(idx)->data(Qt::UserRole).toString()=="obs-ui-augment")
                    delete entry->navigation->takeItem(idx);
                entry->pages->removeWidget(entry->page);
            }
        }
        delete entry->page.data();
    }
    entries_.clear();
}
}
