#include "tooltip-width.h"
#include "obs-settings-page.h"
#include "augment.h"
#include "dock-title-icons.h"
#include "gradient.h"
#include "rotary-paint.h"
#include <QGroupBox>
#include <QColorDialog>
#include <QPainter>
#include <QTabBar>
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
#include <QSlider>
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
namespace {
// Paint the same palette as the mixer, without a stylesheet, fixed-size
// pixmap, animation clock, or mutation of the live OBS preview.
class PalettePreview final : public QWidget {
public:
    explicit PalettePreview(QWidget *parent):QWidget(parent){
        setObjectName("aftermarket-settings-palette-preview");
        setAccessibleName("Gradient palette preview");
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        setMinimumHeight(22);setMaximumHeight(28);
    }
    void setColors(const QString &text){
        QJsonArray array;
        if(!text.trimmed().isEmpty())
            for(const auto &part:text.split(','))array.append(part.trimmed());
        GradientSpec spec;
        valid_=GradientSpec::parse(QJsonObject{{"colors",array}},spec);
        colors_=spec.colors;
        setProperty("validPalette",valid_);update();
    }
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const auto rect=QRectF(this->rect()).adjusted(.5,.5,-.5,-.5);
        p.setPen(palette().color(QPalette::Mid));
        if(!valid_){
            p.setBrush(palette().color(QPalette::Base));p.drawRoundedRect(rect,5,5);
            p.setPen(palette().color(QPalette::Text));
            p.drawText(rect,Qt::AlignCenter,"Invalid palette");return;
        }
        auto colors=colors_.isEmpty()?rotaryThemeColors(palette()):colors_;
        if(colors.first()!=colors.last())colors.append(colors.first());
        QLinearGradient brush(rect.topLeft(),rect.topRight());
        for(int i=0;i<colors.size();++i)brush.setColorAt(double(i)/(colors.size()-1),colors.at(i));
        p.setBrush(brush);p.drawRoundedRect(rect,5,5);
    }
private:
    bool valid_=true;
    QVector<QColor> colors_;
};
}
struct ObsSettingsPage::Entry {
    QPointer<QDialog> dialog;
    QPointer<QListWidget> navigation;
    QPointer<QStackedWidget> pages;
    QPointer<QWidget> page;
    QPointer<QLabel> message;
    QPointer<QComboBox> ellipsis,propertyColumns,gradientPattern,tooltipWidth;
    int columnsBaseline=0,tooltipBaseline=360;
    QPointer<QPushButton> applyButton;
    QHash<QString,QPointer<QCheckBox>> flags;
    QHash<QString,bool> baseline;
    QPointer<QLineEdit> gradientColors,knobSources;
    QPointer<QSpinBox> gradientPeriod;
    QPointer<QSlider> stripeStrength;
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
    root->setContentsMargins(4,4,4,4);
    root->setSpacing(6);
    auto *heading=new QLabel("Aftermarket",page);
    QFont title=heading->font();
    title.setWeight(QFont::DemiBold);
    if(title.pointSizeF()>0)title.setPointSizeF(title.pointSizeF()+2);
    heading->setFont(title);
    heading->setObjectName("aftermarket-settings-heading");
    root->addWidget(heading);
    auto *tabs=new QTabWidget(page);
    tabs->setObjectName("aftermarket-settings-tabs");
    tabs->setUsesScrollButtons(true);
    tabs->tabBar()->setExpanding(false);
    root->addWidget(tabs,1);
    auto makeTab=[tabs](const QString &name){
        auto *scroller=new QScrollArea(tabs);
        scroller->setFrameShape(QFrame::NoFrame);
        scroller->setWidgetResizable(true);
        auto *contents=new QWidget(scroller);
        auto *layout=new QVBoxLayout(contents);
        layout->setContentsMargins(6,5,6,6);
        layout->setSpacing(6);
        scroller->setWidget(contents);
        tabs->addTab(scroller,name);
        return qMakePair(contents,layout);
    };
    auto section=[](const auto &tab,const QString &title){
        auto *group=new QGroupBox(title,tab.first);
        group->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
        auto *layout=new QVBoxLayout(group);
        layout->setContentsMargins(6,4,6,4);layout->setSpacing(3);
        tab.second->addWidget(group);
        return qMakePair(static_cast<QWidget*>(group),layout);
    };
    auto formLayout=[]{
        auto *form=new QFormLayout;
        form->setContentsMargins(0,0,0,0);
        form->setHorizontalSpacing(8);form->setVerticalSpacing(4);
        form->setLabelAlignment(Qt::AlignRight|Qt::AlignVCenter);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        return form;
    };
    auto describe=[](const auto &group,const QString &text){
        auto *label=new QLabel(text,group.first);label->setWordWrap(true);
        group.second->addWidget(label);
        return label;
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
    auto textGroup=section(appearance,"Text and preview");
    auto *form=formLayout();
    entry->ellipsis=new QComboBox(appearance.first);
    entry->ellipsis->setObjectName("augment-global-ellipsis");
    entry->ellipsis->setAccessibleName("Ellipsis placement throughout OBS");
    entry->ellipsis->addItem("At end",QStringLiteral("end"));
    entry->ellipsis->addItem("In middle",QStringLiteral("middle"));
    entry->existingMode=titles_->ellipsisMode()==Qt::ElideMiddle?"middle":"end";
    entry->ellipsis->setCurrentIndex(entry->existingMode=="middle"?1:0);
    form->addRow("Long text",entry->ellipsis);
    textGroup.second->addLayout(form);
    checkbox("transform","Theme-colored transform frame and handles",
             textGroup.second,textGroup.first,controller_->modernTransformFrame());
    auto listGroup=section(appearance,"Scene and source lists");
    const auto stripes=controller_->listRowSettings();
    entry->listRowsBaseline=stripes;
    checkbox("scene-stripes","Alternate Scenes rows",
             listGroup.second,listGroup.first,stripes.value("scenes").toBool());
    checkbox("source-stripes","Alternate Sources rows",
             listGroup.second,listGroup.first,stripes.value("sources").toBool());
    auto *stripeRow=new QHBoxLayout;
    stripeRow->addWidget(new QLabel("Strength",listGroup.first));
    entry->stripeStrength=new QSlider(Qt::Horizontal,listGroup.first);
    entry->stripeStrength->setObjectName("aftermarket-setting-stripe-strength");
    entry->stripeStrength->setAccessibleName("Alternating row background strength");
    entry->stripeStrength->setToolTip("0% = no alternating contrast, 100% = original colors, 200% = twice the contrast.");
    entry->stripeStrength->setRange(0,200);
    entry->stripeStrength->setSingleStep(5);
    entry->stripeStrength->setPageStep(10);
    entry->stripeStrength->setValue(stripes.value("strength").toInt(100));
    stripeRow->addWidget(entry->stripeStrength,1);
    auto *stripeValue=new QLabel(QString::number(entry->stripeStrength->value())+"%",listGroup.first);
    stripeValue->setMinimumWidth(stripeValue->fontMetrics().horizontalAdvance("200%")+6);
    stripeRow->addWidget(stripeValue);
    QObject::connect(entry->stripeStrength,&QSlider::valueChanged,page,
                     [stripeValue](int amount){stripeValue->setText(QString::number(amount)+"%");});
    listGroup.second->addLayout(stripeRow);
    appearance.second->addStretch(1);

    auto menus=makeTab("Menus");
    auto menuGroup=section(menus,"Menu presentation");
    checkbox("compact-menus","Compact menus",menuGroup.second,menuGroup.first,
             controller_->compactMenusEnabled());
    checkbox("hamburger","Compact top menu bar",menuGroup.second,menuGroup.first,
             controller_->hamburgerMenuBarEnabled());
    checkbox("selections","Show selected submenu choices",menuGroup.second,menuGroup.first,
             controller_->showSubmenuSelections());
    auto interaction=section(menus,"Motion and tooltips");
    checkbox("motion","Interface animations",interaction.second,interaction.first,
             controller_->motionEnabled())->setToolTip("Menus, docks, AudioBus and gradient animation.");
    auto *tooltipRow=formLayout();
    entry->tooltipWidth=new QComboBox(menus.first);
    entry->tooltipWidth->setObjectName("aftermarket-setting-tooltip-width");
    entry->tooltipWidth->setAccessibleName("Maximum tooltip width");
    entry->tooltipBaseline=controller_->tooltipWidth();
    populateTooltipWidths(entry->tooltipWidth,entry->tooltipBaseline);
    tooltipRow->addRow("Maximum tooltip width",entry->tooltipWidth);
    interaction.second->addLayout(tooltipRow);
    menus.second->addStretch(1);

    auto mixer=makeTab("Mixer");
    auto routingGroup=section(mixer,"Routing");
    checkbox("mixer-back","AudioBus alternate routing view",routingGroup.second,routingGroup.first,
             controller_->mixerBacksideEnabled());
    describe(routingGroup,"Switch between the native mixer and AudioBus using the dock footer. Native routing and gain stay intact.");
    const auto gradient=controller_->gradientSettings();
    entry->gradientBaseline=gradient;
    GradientSpec spec;
    GradientSpec::parse(gradient,spec);
    auto gainGroup=section(mixer,"Gain controls");
    auto *knobs=checkbox("knobs","Use gain knobs",gainGroup.second,gainGroup.first,spec.knobs);
    auto *knobMeters=checkbox("knob-meters","Peak meters integrated into knobs",gainGroup.second,gainGroup.first,spec.knobMeters);
    auto *gainForm=formLayout();
    entry->knobSources=new QLineEdit(gainGroup.first);
    entry->knobSources->setObjectName("aftermarket-setting-knob-sources");
    entry->knobSources->setText(spec.knobSources.join(", "));
    entry->knobSources->setPlaceholderText("All sources");
    entry->knobSources->setToolTip("Comma-separated exact source names; leave empty for all sources.");
    gainForm->addRow("Knob sources",entry->knobSources);
    gainGroup.second->addLayout(gainForm);
    auto updateGain=[entry,knobs,knobMeters]{
        knobMeters->setEnabled(knobs->isChecked());
        if(entry->knobSources)entry->knobSources->setEnabled(knobs->isChecked());
    };
    QObject::connect(knobs,&QCheckBox::toggled,page,updateGain);updateGain();
    auto meterGroup=section(mixer,"Audio effects");
    checkbox("reactive","Audio-reactive mixer gradients",meterGroup.second,meterGroup.first,spec.audioReactive);
    checkbox("meter-pulse","Pulse native meter colors",meterGroup.second,meterGroup.first,spec.meters);
    describe(meterGroup,"Reactive effects use the gradient and Motion preferences.");
    mixer.second->addStretch(1);

    auto gradients=makeTab("Gradients");
    auto surfaceGroup=section(gradients,"Surfaces");
    checkbox("gradients","Enable gradient tracks",surfaceGroup.second,surfaceGroup.first,spec.enabled);
    checkbox("slider-gradients","Sliders and rotary rings",surfaceGroup.second,surfaceGroup.first,spec.sliders);
    checkbox("progress-gradients","Progress bars",surfaceGroup.second,surfaceGroup.first,spec.progress);
    checkbox("title-gradients","Dock titlebar accents",surfaceGroup.second,surfaceGroup.first,spec.titlebars);
    checkbox("titlebar-fill","Gradient dock headers",surfaceGroup.second,surfaceGroup.first,spec.titlebarFill)
        ->setToolTip("Tint the entire dock header; takes precedence over the underline.");
    auto paletteGroup=section(gradients,"Palette and motion");
    auto *mixerForm=formLayout();
    entry->gradientPattern=new QComboBox(paletteGroup.first);
    entry->gradientPattern->setObjectName("aftermarket-setting-gradient-pattern");
    for(const auto &id:GradientSpec::patterns())entry->gradientPattern->addItem(GradientSpec::patternLabel(id),id);
    entry->gradientPattern->setCurrentIndex(entry->gradientPattern->findData(spec.pattern));
    mixerForm->addRow("Animation",entry->gradientPattern);
    entry->gradientPeriod=new QSpinBox(paletteGroup.first);
    entry->gradientPeriod->setObjectName("aftermarket-setting-gradient-period");
    entry->gradientPeriod->setRange(400,12000);
    entry->gradientPeriod->setSuffix(" ms");
    entry->gradientPeriod->setSingleStep(100);
    entry->gradientPeriod->setValue(spec.period);
    mixerForm->addRow("Loop duration",entry->gradientPeriod);
    entry->gradientColors=new QLineEdit(paletteGroup.first);
    entry->gradientColors->setObjectName("aftermarket-setting-gradient-colors");
    entry->gradientColors->setPlaceholderText("Theme palette");
    entry->gradientColors->setToolTip("2–6 opaque colors, comma-separated; empty uses the theme.");
    QStringList colors;
    for(const auto &color:spec.colors)colors.append(color.name());
    entry->gradientColors->setText(colors.join(", "));
    mixerForm->addRow("Colors",entry->gradientColors);
    paletteGroup.second->addLayout(mixerForm);
    auto *preview=new PalettePreview(paletteGroup.first);
    paletteGroup.second->addWidget(preview);
    auto *actions=new QHBoxLayout;actions->setContentsMargins(0,0,0,0);actions->setSpacing(4);
    auto *pick=new QPushButton("Add color…",paletteGroup.first);
    pick->setObjectName("aftermarket-settings-add-color");
    auto *theme=new QPushButton("Use theme",paletteGroup.first);
    theme->setObjectName("aftermarket-settings-use-theme");
    actions->addStretch();actions->addWidget(pick);actions->addWidget(theme);
    paletteGroup.second->addLayout(actions);
    auto *paletteHint=describe(paletteGroup,"");
    auto updatePalette=[entry,preview,pick,theme,paletteHint]{
        const auto text=entry->gradientColors->text().trimmed();
        preview->setColors(text);
        const auto valid=preview->property("validPalette").toBool();
        const auto count=text.isEmpty()?0:text.split(',').size();
        pick->setEnabled(count<6);
        theme->setEnabled(!text.isEmpty());
        paletteHint->setText(text.isEmpty()?"Using active theme colors.":valid?
            "Custom palette. Motion controls animation.":"Enter 2–6 valid opaque colors, or use the theme.");
    };
    QObject::connect(entry->gradientColors,&QLineEdit::textChanged,page,updatePalette);
    QObject::connect(theme,&QPushButton::clicked,entry->gradientColors,&QLineEdit::clear);
    QObject::connect(pick,&QPushButton::clicked,page,[entry,page]{
        const auto chosen=QColorDialog::getColor(page->palette().color(QPalette::Highlight),page,"Add gradient color");
        if(!chosen.isValid()||!entry->gradientColors)return;
        const auto current=entry->gradientColors->text().trimmed();
        // A single color is not a valid custom palette. Start with the
        // chosen color and the theme accent (also editable afterwards).
        entry->gradientColors->setText(current.isEmpty()?
            chosen.name()+", "+page->palette().color(QPalette::Highlight).name():
            current+", "+chosen.name());
    });
    updatePalette();
    gradients.second->addStretch(1);

    auto panels=makeTab("Panels");
    auto propertyGroup=section(panels,"Source and filter properties");
    if(services_.compactSourcePickerEnabled&&services_.setCompactSourcePicker){
        auto *embedded=checkbox("source-picker","Embed Add Source picker in OBS",
            propertyGroup.second,propertyGroup.first,services_.compactSourcePickerEnabled());
        embedded->setToolTip("Keep OBS's native source picker and source creation, "
                            "but display it as a compact panel inside the Sources area. "
                            "Takes effect the next time Add Source opens.");
    }
    checkbox("sections","Collapsible property and filter groups",propertyGroup.second,propertyGroup.first,
             controller_->collapsiblePropertyGroups());
    auto *propertyForm=formLayout();
    entry->propertyColumns=new QComboBox(panels.first);
    entry->propertyColumns->setObjectName("aftermarket-setting-property-columns");
    entry->propertyColumns->addItem("Auto",0);
    entry->propertyColumns->addItem("1 column (native)",1);
    entry->propertyColumns->addItem("Up to 2 columns",2);
    entry->propertyColumns->addItem("Up to 3 columns",3);
    entry->columnsBaseline=controller_->propertyColumns();
    entry->propertyColumns->setCurrentIndex(entry->propertyColumns->findData(entry->columnsBaseline));
    propertyForm->addRow("Source / filter settings",entry->propertyColumns);
    propertyGroup.second->addLayout(propertyForm);

    auto dockGroup=section(panels,"Dock controls");
    auto *panelsHint=new QLabel("Each dock's three-dot menu provides Collapse / Expand. "
                                 "These actions are local to the dock.",panels.first);
    panelsHint->setWordWrap(true);
    dockGroup.second->addWidget(panelsHint);
    panels.second->addStretch(1);

    auto actionStrip=makeTab("Action strip");
    auto stripGroup=section(actionStrip,"Menu bar shortcuts");
    checkbox("action-strip","Show OBS action strip",stripGroup.second,stripGroup.first,
             services_.actionStripEnabled?services_.actionStripEnabled():false);
    if(services_.editActionStrip){
        auto *edit=new QPushButton("Configure action-strip buttons…",actionStrip.first);
        edit->setObjectName("aftermarket-action-strip-edit");
        stripGroup.second->addWidget(edit);
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
    entry->message->setAccessibleName("Aftermarket settings status");
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
        if(entry->gradientPattern&&entry->gradientPattern->currentData().toString()!=spec.pattern)changed=true;
        if(entry->gradientPeriod&&entry->gradientPeriod->value()!=spec.period)
            changed=true;
        if(entry->gradientColors&&entry->gradientColors->text()!=originalColors.join(", "))
            changed=true;
        if(entry->knobSources&&entry->knobSources->text()!=spec.knobSources.join(", "))
            changed=true;
        if(entry->tooltipWidth&&entry->tooltipWidth->currentData().toInt()!=entry->tooltipBaseline)changed=true;
        if(entry->propertyColumns&&entry->propertyColumns->currentData().toInt()!=entry->columnsBaseline)
            changed=true;
        if(entry->stripeStrength&&entry->stripeStrength->value()!=
           entry->listRowsBaseline.value("strength").toInt(100))
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
    QObject::connect(entry->tooltipWidth,&QComboBox::currentIndexChanged,page,[updateDirty](int){updateDirty();});
    QObject::connect(entry->propertyColumns,qOverload<int>(&QComboBox::currentIndexChanged),page,[updateDirty](int){updateDirty();});
    QObject::connect(entry->ellipsis,qOverload<int>(&QComboBox::currentIndexChanged),
                     page,[updateDirty](int){updateDirty();});
    for(const auto &checkbox:entry->flags)
        if(checkbox)QObject::connect(checkbox,&QCheckBox::toggled,page,
                                     [updateDirty](bool){updateDirty();});
    QObject::connect(entry->stripeStrength,&QSlider::valueChanged,page,
                     [updateDirty](int){updateDirty();});
    QObject::connect(entry->gradientPattern,&QComboBox::currentIndexChanged,page,[updateDirty](int){updateDirty();});
    QObject::connect(entry->gradientPeriod,qOverload<int>(&QSpinBox::valueChanged),
                     page,[updateDirty](int){updateDirty();});
    QObject::connect(entry->gradientColors,&QLineEdit::textChanged,
                     page,[updateDirty](const QString &){updateDirty();});
    QObject::connect(entry->knobSources,&QLineEdit::textChanged,
                     page,[updateDirty](const QString &){updateDirty();});

    stack->addWidget(page);
    auto *item=new QListWidgetItem("Aftermarket",nav);
    item->setData(Qt::UserRole,QStringLiteral("aftermarket"));
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
    const int previousTooltip=entry->tooltipBaseline;
    const int tooltip=entry->tooltipWidth->currentData().toInt();
    if(tooltip!=previousTooltip)
        changes.append({[c,tooltip](QString &e){return c&&c->setTooltipWidth(tooltip,&e);},
                        [c,previousTooltip]{QString ignored;if(c)c->setTooltipWidth(previousTooltip,&ignored);}});
    const int previousColumns=entry->columnsBaseline;
    const int columns=entry->propertyColumns->currentData().toInt();
    if(columns!=previousColumns){
        auto write=[this,c](int mode,QString &e){
            if(!c||!c->setPropertyColumns(mode,&e))return false;
            if(services_.setPropertyColumns)services_.setPropertyColumns(mode);
            return true;
        };
        changes.append({[write,columns](QString &e){return write(columns,e);},
                        [write,previousColumns]{QString ignored;write(previousColumns,ignored);}});
    }

    addFlag("transform",[c](bool b,QString &e){return c&&c->setModernTransformFrame(b,&e);});
    if(services_.setCompactSourcePicker)
        addFlag("source-picker",[this](bool b,QString &e){
            if(services_.setCompactSourcePicker(b))return true;
            e="Unable to save the Add Source picker setting.";
            return false;
        });
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
    if(entry->stripeStrength&&entry->stripeStrength->value()!=
       entry->listRowsBaseline.value("strength").toInt(100))
        newRows["strength"]=entry->stripeStrength->value();
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
        {"meter-pulse","meters"},{"title-gradients","titlebars"},{"titlebar-fill","titlebarFill"}
    };
    for(const auto &field:fields){
        const bool before=entry->baseline.value(field.first,false);
        const bool now=entry->flags.value(field.first)->isChecked();
        if(before!=now)updatedGradient[field.second]=now;
    }
    GradientSpec originalSpec;
    GradientSpec::parse(entry->gradientBaseline,originalSpec);
    if(entry->gradientPattern->currentData().toString()!=originalSpec.pattern)updatedGradient["pattern"]=entry->gradientPattern->currentData().toString();
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
        for(const auto &token:entry->gradientColors->text().trimmed().isEmpty()?QStringList{}:entry->gradientColors->text().split(',')){
            QColor color(token.trimmed());
            if(!color.isValid()||color.alpha()!=255){
                entry->message->setText("Invalid gradient color: "+token.trimmed());
                if(entry->applyButton)entry->applyButton->setEnabled(true);
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
            if(entry->applyButton)entry->applyButton->setEnabled(true);
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
    entry->columnsBaseline=controller_->propertyColumns();
    entry->tooltipBaseline=controller_->tooltipWidth();
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
                   entry->navigation->item(idx)->data(Qt::UserRole).toString()=="aftermarket")
                    delete entry->navigation->takeItem(idx);
                entry->pages->removeWidget(entry->page);
            }
        }
        delete entry->page.data();
    }
    entries_.clear();
}
}
