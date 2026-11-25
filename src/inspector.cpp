#include "inspector.h"
#include "augment.h"
#include "tooltip-width.h"
#include "menu-layout.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QProcess>
#include <QUuid>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include "motion.h"
#include "gradient.h"
#include "panels.h"
#include "property-sections.h"
#include "row-stripes.h"
#include "visual-polish.h"
#include "menu-bar-compact.h"
#include "action-strip.h"
#include "dock-layouts.h"
#include "cue-deck.h"
#include <QMenuBar>
#include <QGroupBox>
#include <QGridLayout>
#include <QMainWindow>
#include <QTabWidget>
#include <QAbstractButton>
#include <QPainter>
#include <QPointer>
#include <functional>
#include <QCursor>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSet>
#include <QMap>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QPushButton>
#include <algorithm>

namespace augment {
namespace {
QString identity(const QWidget *w) {
    const auto type=QString::fromLatin1(w->metaObject()->className());
    return w->objectName().isEmpty() ? type : type+" #"+w->objectName();
}
QJsonObject targetFor(const WidgetSnapshot &s,int row) {
    if (row<0 || row>=s.actions.size()) return {};
    const auto &a=s.actions.at(row);
    return {{"text",a.text},{"objectName",a.objectName},
            {"menuObjectName",a.scopeMenuName.isEmpty()?s.menuObjectName:a.scopeMenuName}};
}
QStringList actionLabels(const WidgetSnapshot &s,int row) {
    if(row>=0&&row<s.actions.size()&&!s.actions.at(row).scopeLabels.isEmpty())
        return s.actions.at(row).scopeLabels;
    return s.menuLabels;
}
QString originalText(const QAction *a) {
    return a->property("_aftermarketBaseText").isValid()?
        a->property("_aftermarketBaseText").toString():a->text();
}
ActionSnapshot captureAction(const QAction *action,const QMenu *parent,int depth) {
    const bool reservation=action->property("_obsUiAugmentReservation").toBool();
    QString original=originalText(action);
    if(auto *widgetAction=qobject_cast<const QWidgetAction*>(action);
       original.isEmpty()&&widgetAction&&widgetAction->defaultWidget()&&
       widgetAction->defaultWidget()->objectName()=="ColorSelect")
        original="Color presets";
    ActionSnapshot result{original,action->objectName(),action->isCheckable(),
        (action->property("_aftermarketCustomId").isValid()||qobject_cast<const QWidgetAction*>(action)!=nullptr),
        action->isVisible(),action->isEnabled(),
        reservation?action->property("_obsUiAugmentOriginalIcon").toBool():!action->icon().isNull()};
    result.scopeMenuName=logicalMenuName(parent);
    result.scopeLabels=logicalMenuLabels(parent);
    result.depth=depth;
    result.checked=action->isChecked();
    result.separator=action->isSeparator();
    result.hasSubmenu=action->menu()!=nullptr;
    if(action->menu())result.selectedChoice=submenuSelection(action->menu());
    return result;
}
void captureTree(const QMenu *menu,int depth,QSet<const QMenu*> &visited,QVector<ActionSnapshot> &out) {
    if(!menu||depth>7||visited.contains(menu)||out.size()>=500)return;
    visited.insert(menu);
    for(const auto *action:menu->actions()){
        if(overflowAction(action))continue;
        out.append(captureAction(action,menu,depth));
        if(const auto *widgetAction=qobject_cast<const QWidgetAction*>(action)) {
            const auto *widget=widgetAction->defaultWidget();
            if(widget&&widget->objectName()=="ColorSelect") {
                QMap<int,const QPushButton*> swatches;
                for(const auto *button:widget->findChildren<QPushButton*>()) {
                    const int index=button->property("bgColor").toInt();
                    if(button->objectName().startsWith("preset")&&!presetColorLabel(index).isEmpty())
                        swatches.insert(index,button);
                }
                for(auto i=swatches.cbegin();i!=swatches.cend();++i) {
                    const auto *button=i.value();
                    QString css=button->styleSheet().toLower();css.remove(QChar(' '));
                    ActionSnapshot preset;
                    preset.text=presetColorLabel(i.key());preset.objectName=button->objectName();
                    preset.custom=true;preset.visible=true;preset.enabled=button->isEnabled();
                    preset.checked=css.contains("border:2pxsolidblack");
                    preset.depth=depth+1;preset.scopeMenuName=logicalMenuName(menu);
                    preset.scopeLabels=logicalMenuLabels(menu);
                    out.append(preset);
                }
            }
        }
        if(action->menu())captureTree(action->menu(),depth+1,visited,out);
        if(out.size()>=500)break;
    }
}
}
bool ActionSnapshot::eligible() const {
    return visible && !separator && !checkable && !custom &&
        (!objectName.isEmpty() || !normalizeActionText(text).isEmpty());
}
WidgetSnapshot inspectWidget(QWidget *widget,const QPoint &globalPosition) {
    WidgetSnapshot s;
    if (!widget) return s;
    if (widget->objectName()=="aftermarket-overlay"||widget->objectName()=="aftermarket-gradient") widget=widget->parentWidget();
    if (!widget) return s;
    s.type=QString::fromLatin1(widget->metaObject()->className());
    s.objectName=widget->objectName();
    s.accessibility=widget->accessibleName();
    const auto pos=widget->mapToGlobal(QPoint());
    s.geometry=QString("%1 × %2 · screen %3, %4").arg(widget->width()).arg(widget->height()).arg(pos.x()).arg(pos.y());
    QStringList parents;
    for (auto *w=widget;w;w=w->parentWidget()) parents.prepend(identity(w));
    s.ancestry=parents.join("  ›  ");
    for(auto *w=widget;w;w=w->parentWidget())if(w->inherits("VolumeControl")) {
        for(auto *b:w->findChildren<QAbstractButton*>())if(b->inherits("VolumeName")){s.mixerSource=b->text();break;}
        break;
    }
    QMenu *menu=nullptr;
    for (auto *w=widget;w;w=w->parentWidget())
        if ((menu=qobject_cast<QMenu*>(w))) break;
    if (!menu) return s;
    s.isMenu=true;
    s.menuObjectName=logicalMenuName(menu);
    s.menuLabels=logicalMenuLabels(menu);
    const auto *hit=menu->actionAt(menu->mapFromGlobal(globalPosition));
    for (auto *action:menu->actions()) {
        if(overflowAction(action)||action->isSeparator()||!action->isVisible())continue;
        if(action==hit)s.selected=s.actions.size();
        s.actions.append(captureAction(action,menu,0));
    }
    QSet<const QMenu*> visited;
    captureTree(menu,0,visited,s.allActions);
    return s;
}
WidgetSnapshot inspectAt(const QPoint &globalPosition) {
    // Menus sometimes have a native popup window separate from the widget under it.
    if (auto *popup=qobject_cast<QMenu*>(QApplication::activePopupWidget()))
        if (popup->rect().contains(popup->mapFromGlobal(globalPosition)))
            return inspectWidget(popup,globalPosition);
    return inspectWidget(QApplication::widgetAt(globalPosition),globalPosition);
}
QJsonObject selectorFor(const WidgetSnapshot &s,int row,bool scopeMenu) {
    if (!s.isMenu || row<0 || row>=s.actions.size()) return {};
    const auto &a=s.actions.at(row);
    // A checkable child can be scoped and hidden even though the independent
    // icon painter does not decorate checkable rows.
    if(!a.visible||a.separator||a.custom||
       (a.objectName.isEmpty()&&normalizeActionText(a.text).isEmpty()))return {};
    QJsonObject selector;
    if (!a.objectName.isEmpty()) selector["objectName"]=a.objectName;
    else selector["text"]=a.text;
    if (!scopeMenu) return selector;
    const auto menuName=a.scopeMenuName.isEmpty()?s.menuObjectName:a.scopeMenuName;
    if (!menuName.isEmpty()) selector["menuObjectName"]=menuName;
    else {
        // Anonymous menus have no durable Qt ID. Anchor on two other observed labels.
        QStringList anchors, seen;
        const auto own=normalizeActionText(a.text);
        const auto siblingLabels=a.scopeLabels.isEmpty()?s.menuLabels:a.scopeLabels;
        for (const auto &label:siblingLabels) {
            const auto normalized=normalizeActionText(label);
            if (normalized.isEmpty() || normalized==own || seen.contains(normalized)) continue;
            seen.append(normalized);
            // Retain the original escaped label. The rule matcher normalizes once.
            anchors.append(label);
            if (anchors.size()==2) break;
        }
        if (!anchors.isEmpty()) selector["menuContains"]=QJsonArray::fromStringList(anchors);
    }
    return selector;
}

namespace {
QJsonObject menuScope(const WidgetSnapshot &snapshot) {
    if(!snapshot.isMenu)return {};
    if(!snapshot.menuObjectName.isEmpty())return {{"menuObjectName",snapshot.menuObjectName}};
    QJsonArray anchors;
    QStringList seen;
    for(const auto &label:snapshot.menuLabels) {
        const auto norm=normalizeActionText(label);
        if(norm.isEmpty()||seen.contains(norm))continue;
        seen.append(norm);anchors.append(label);
        if(anchors.size()==2)break;
    }
    if(anchors.isEmpty())return {};
    return {{"menuContains",anchors}};
}
QJsonObject openCustomEditor(QWidget *parent,const QJsonObject &original) {
    QDialog dialog(parent);
    dialog.setWindowTitle(original.isEmpty()?"Add custom menu action":"Edit custom menu action");
    dialog.setModal(true);dialog.setMinimumWidth(440);
    auto *root=new QVBoxLayout(&dialog);
    auto *form=new QFormLayout;root->addLayout(form);
    auto *label=new QLineEdit(original.value("label").toString(),&dialog);label->setObjectName("augment-custom-label");
    auto *program=new QLineEdit(original.value("program").toString(),&dialog);program->setObjectName("augment-custom-program");
    program->setPlaceholderText("xdg-open or /absolute/path/to/program");
    auto *args=new QLineEdit(&dialog);args->setObjectName("augment-custom-args");
    QStringList previous;
    for(const auto &value:original.value("args").toArray()) {
        QString arg=value.toString();
        if(arg.contains(QChar(' '))||arg.contains(QChar('"'))) {
            arg.replace(QChar(92),QString(QChar(92))+QChar(92));
            arg.replace(QChar(34),QString(QChar(92))+QChar(34));
            arg=QString(QChar(34))+arg+QChar(34);
        }
        previous.append(arg);
    }
    args->setText(previous.join(QChar(' ')));
    args->setPlaceholderText("Optional arguments; quote paths containing spaces");
    auto *icon=new QComboBox(&dialog);icon->setObjectName("augment-custom-icon");
    icon->addItem("No icon","");
    for(const auto &name:iconNames())icon->addItem(previewIcon(name,parent->palette().color(QPalette::Text)),name,name);
    const int selected=icon->findData(original.value("icon").toString());
    if(selected>=0)icon->setCurrentIndex(selected);
    form->addRow("Label",label);form->addRow("Executable",program);
    form->addRow("Arguments",args);form->addRow("Icon",icon);
    auto *hint=new QLabel("The executable is launched directly with these arguments. No implicit shell or interpolation. The action appears at the end of the captured menu.",&dialog);
    hint->setWordWrap(true);root->addWidget(hint);
    auto *error=new QLabel(&dialog);error->setWordWrap(true);error->setTextFormat(Qt::PlainText);root->addWidget(error);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);root->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        if(label->text().trimmed().isEmpty()||program->text().trimmed().isEmpty()){
            error->setText("Label and executable are required.");return;
        }
        dialog.accept();
    });
    if(dialog.exec()!=QDialog::Accepted)return {};
    QJsonObject next=original;
    next["label"]=label->text().trimmed();
    next["program"]=program->text().trimmed();
    next["args"]=QJsonArray::fromStringList(QProcess::splitCommand(args->text()));
    const QString iconName=icon->currentData().toString();
    if(iconName.isEmpty())next.remove("icon");
    else next["icon"]=iconName;
    return next;
}
QString mixerSource(const QWidget *widget) {
    for(auto *w=widget;w;w=w->parentWidget()) {
        if(!w->inherits("VolumeControl"))continue;
        for(auto *b:w->findChildren<QAbstractButton*>())
            if(b->inherits("VolumeName"))return b->text();
        break;
    }
    return {};
}
QStringList mixerSources(QWidget *host) {
    QStringList result;
    const auto widgets=host?host->findChildren<QWidget*>():QApplication::allWidgets();
    for(auto *w:widgets)if(w->inherits("VolumeControl")) {
        const auto name=mixerSource(w);
        if(!name.isEmpty()&&!result.contains(name))result.append(name);
    }
    result.sort();return result;
}
struct SettingsPages {
    QJsonObject baseline;
    std::function<void()> refreshSources;
    QPointer<QListWidget> sources;
};
std::shared_ptr<SettingsPages> buildSettings(Controller &controller,QTabWidget *tabs,QWidget *host,QWidget *footer,QLabel *status) {
    auto state=std::make_shared<SettingsPages>();
    auto *mixer=new QWidget(tabs);auto *ml=new QVBoxLayout(mixer);
    auto *gradient=new QWidget(tabs);auto *gl=new QVBoxLayout(gradient);
    ml->setContentsMargins(12,12,12,12);gl->setContentsMargins(12,12,12,12);
    ml->setSpacing(8);gl->setSpacing(8);
    auto heading=[](const QString &title,const QString &desc,QVBoxLayout *layout,QWidget *parent){
        auto *label=new QLabel(title,parent);
        label->setObjectName(title=="Mixer controls"?"augment-heading-mixer":"augment-heading-gradient");
        label->setFont(subtleHeadingFont(label->font()));
        layout->addWidget(label);
        auto *hint=new QLabel(desc,parent);hint->setWordWrap(true);layout->addWidget(hint);
    };
    auto check=[](const QString &label,const QString &id,QVBoxLayout *layout,QWidget *parent){
        auto *w=new QCheckBox(label,parent);w->setObjectName(id);layout->addWidget(w);return w;
    };
    heading("Mixer controls","Choose which channels use rotary controls. The original OBS gain and audio behavior stay intact.",ml,mixer);
    auto *knobs=check("Replace mixer faders with knobs","augment-gradient-knobs",ml,mixer);
    auto *knobMeters=check("Integrate peak meter into knob","augment-knob-meters",ml,mixer);
    knobMeters->setToolTip("Outer ring: maximum-channel peak. Inner ring: gain. Disable to keep detailed native meters.");
    auto *all=check("All mixer sources","augment-knob-all",ml,mixer);
    auto *selectionHint=new QLabel("Check the sources you want, or double-click a row to toggle it.",mixer);selectionHint->setWordWrap(true);ml->addWidget(selectionHint);
    auto *sources=new QListWidget(mixer);sources->setObjectName("augment-knob-source-list");ml->addWidget(sources,1);state->sources=sources;
    auto *sourceRow=new QHBoxLayout;
    auto *name=new QLineEdit(mixer);name->setObjectName("augment-knob-add-name");name->setPlaceholderText("Exact source name (including offline sources)");
    auto *add=new QPushButton("Add source",mixer);add->setObjectName("augment-knob-add");
    auto *refresh=new QPushButton("Refresh sources",mixer);refresh->setObjectName("augment-knob-refresh");
    sourceRow->addWidget(name,1);sourceRow->addWidget(add);sourceRow->addWidget(refresh);ml->addLayout(sourceRow);
    auto *reactive=check("Audio-reactive mixer gradients","augment-gradient-reactive",ml,mixer);
    auto *meters=check("Pulse native meter colors","augment-gradient-meters",ml,mixer);
    auto *audioHint=new QLabel("Reactive gradients need enabled gradient tracks, sliders and Motion. Integrated peak meters work even with Motion off.",mixer);
    audioHint->setWordWrap(true);ml->addWidget(audioHint);
    heading("Gradient tracks","Use the active theme palette or a custom color sequence. Motion controls decorative animation globally.",gl,gradient);
    auto *enable=check("Enable gradient tracks","augment-gradient-enabled",gl,gradient);
    auto *sliders=check("Sliders and rotary rings","augment-gradient-sliders",gl,gradient);
    auto *progress=check("Progress bars","augment-gradient-progress",gl,gradient);
    auto *titlebars=check("Dock titlebar accents","augment-gradient-titlebars",gl,gradient);
    auto *form=new QFormLayout;
    auto *pattern=new QComboBox(gradient);pattern->setObjectName("augment-gradient-pattern");
    for(const auto &id:GradientSpec::patterns())pattern->addItem(GradientSpec::patternLabel(id),id);
    form->addRow("Animation",pattern);
    auto *colors=new QLineEdit(gradient);colors->setObjectName("augment-gradient-colors");colors->setPlaceholderText("Theme palette (or 2–6 colors, comma-separated)");
    auto *colorRow=new QHBoxLayout;auto *pickColor=new QPushButton("Add color…",gradient);
    auto *theme=new QPushButton("Use theme",gradient);colorRow->addWidget(colors,1);colorRow->addWidget(pickColor);colorRow->addWidget(theme);form->addRow("Colors",colorRow);
    auto *period=new QSpinBox(gradient);period->setObjectName("augment-gradient-period");period->setRange(400,12000);period->setSingleStep(100);period->setSuffix(" ms");form->addRow("Loop duration",period);
    gl->addLayout(form);
    heading("Preview selection","Colorway-aware native transform outlines and round visual handles. Original OBS dragging and hit targets stay unchanged.",gl,gradient);
    auto *modernTransforms=check("Modern transform frame and handles","augment-modern-transform",gl,gradient);
    modernTransforms->setChecked(controller.modernTransformFrame());
    modernTransforms->setToolTip("Use the active theme accent for OBS's native selection, crop, and hover colors, with a visual-only rounded-handle layer. Off restores the previous OBS accessibility colors.");
    QObject::connect(modernTransforms,&QCheckBox::toggled,gradient,[&controller,modernTransforms,status](bool value){
        QString error;
        if(controller.setModernTransformFrame(value,&error))
            status->setText(value?"Transform styling enabled. Native handles retain OBS's original click radius.":"Transform styling disabled. Original OBS colors will be restored.");
        else {
            const QSignalBlocker blocker(modernTransforms);
            modernTransforms->setChecked(controller.modernTransformFrame());
            status->setText(error);
        }
    });
    auto *preview=new QLabel(gradient);preview->setObjectName("augment-gradient-preview");preview->setMinimumHeight(32);preview->setAlignment(Qt::AlignCenter);gl->addWidget(preview);
    auto *colorHint=new QLabel(gradient);colorHint->setWordWrap(true);gl->addWidget(colorHint);gl->addStretch();
    tabs->addTab(mixer,"Mixer");tabs->addTab(gradient,"Gradients");
    auto checkedNames=[=]{QStringList result;for(int i=0;i<sources->count();++i)if(sources->item(i)->checkState()==Qt::Checked)result.append(sources->item(i)->data(Qt::UserRole).toString());result.sort();return result;};
    auto populate=[=](QStringList chosen,QStringList extra={}){
        const QSignalBlocker block(sources);auto live=mixerSources(host);auto names=live;
        for(const auto &n:chosen+extra)if(!names.contains(n))names.append(n);
        names.sort();sources->clear();
        for(const auto &n:names){
            auto *item=new QListWidgetItem(n+(live.contains(n)?"":"  ·  Not in current mixer"),sources);
            item->setData(Qt::UserRole,n);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(chosen.contains(n)?Qt::Checked:Qt::Unchecked);
        }
    };
    state->refreshSources=[=]{populate(checkedNames());};
    auto enablement=[=]{
        all->setEnabled(knobs->isChecked());knobMeters->setEnabled(knobs->isChecked());
        const bool specific=knobs->isChecked()&&!all->isChecked();sources->setEnabled(specific);name->setEnabled(specific);add->setEnabled(specific);
        meters->setEnabled(reactive->isChecked());
        selectionHint->setText(all->isChecked()?"All current and future mixer sources are included. Uncheck All mixer sources to choose individually.":"Check the sources you want, or double-click a row to toggle it.");
    };
    auto updatePreview=[=]{
        QJsonArray arr;for(const auto &v:colors->text().split(',',Qt::SkipEmptyParts))arr.append(v.trimmed());
        GradientSpec spec;bool valid=GradientSpec::parse(QJsonObject{{"colors",arr}},spec);
        if(!colors->text().trimmed().isEmpty()&&arr.isEmpty())valid=false;
        if(!valid){preview->clear();colorHint->setText("Enter 2–6 valid opaque colors, or clear the field for the theme palette.");return;}
        auto palette=spec.colors;
        if(palette.isEmpty()){auto p=gradient->palette();palette={p.color(QPalette::Highlight),p.color(QPalette::Highlight).lighter(155),p.color(QPalette::Link),p.color(QPalette::Highlight)};}
        if(palette.first()!=palette.last())palette.append(palette.first());
        QPixmap image(320,24);image.fill(Qt::transparent);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient brush(0,0,image.width(),0);for(int i=0;i<palette.size();++i)brush.setColorAt(double(i)/(palette.size()-1),palette.at(i));
        p.setPen(Qt::NoPen);p.setBrush(brush);p.drawRoundedRect(image.rect(),12,12);p.end();preview->setPixmap(image);colorHint->setText(spec.colors.isEmpty()?"Using the active Qt theme palette.":"Custom palette preview. Timing is set by loop duration.");
    };
    auto *buttons=new QHBoxLayout(footer);buttons->setContentsMargins(0,3,0,3);footer->setMinimumHeight(40);footer->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    auto *pending=new QLabel("Settings saved",footer);pending->setObjectName("augment-settings-state");
    auto *reload=new QPushButton("Reload saved settings",footer);reload->setObjectName("augment-settings-reload");
    auto *apply=new QPushButton("Apply changes",footer);apply->setObjectName("augment-gradient-apply");
    reload->setMinimumHeight(30);apply->setMinimumHeight(30);
    buttons->addWidget(pending);buttons->addStretch();buttons->addWidget(reload);buttons->addWidget(apply);
    auto draft=[=]{
        auto next=state->baseline;
        QJsonArray palette;const auto text=colors->text().trimmed();if(!text.isEmpty())for(const auto &c:text.split(','))palette.append(c.trimmed());
        next["enabled"]=enable->isChecked();next["sliders"]=sliders->isChecked();next["progress"]=progress->isChecked();next["titlebars"]=titlebars->isChecked();next["pattern"]=pattern->currentData().toString();
        next["knobs"]=knobs->isChecked();next["knobSources"]=QJsonArray::fromStringList(all->isChecked()?QStringList{}:checkedNames());next["knobMeters"]=knobMeters->isChecked();
        next["audioReactive"]=reactive->isChecked();next["meters"]=meters->isChecked();next["colors"]=palette;next["period"]=period->value();
        return next;
    };
    auto normalized=std::make_shared<QJsonObject>();
    auto changed=[=]{
        const bool dirty=draft()!=*normalized;
        pending->setText(dirty?"Unapplied changes":"Settings saved");
        apply->setEnabled(dirty);
    };
    auto load=[=,&controller]{
        state->baseline=controller.gradientSettings();GradientSpec spec;GradientSpec::parse(state->baseline,spec);
        {const QSignalBlocker blocker(modernTransforms);modernTransforms->setChecked(controller.modernTransformFrame());}
        knobs->setChecked(spec.knobs);knobMeters->setChecked(spec.knobMeters);all->setChecked(spec.knobSources.isEmpty());populate(spec.knobSources);
        reactive->setChecked(spec.audioReactive);meters->setChecked(spec.meters);enable->setChecked(spec.enabled);sliders->setChecked(spec.sliders);progress->setChecked(spec.progress);titlebars->setChecked(spec.titlebars);pattern->setCurrentIndex(pattern->findData(spec.pattern));
        QStringList hex;for(const auto &c:spec.colors)hex.append(c.name());colors->setText(hex.join(", "));period->setValue(spec.period);
        enablement();updatePreview();*normalized=draft();changed();
    };
    for(auto *box:{knobs,knobMeters,all,reactive,meters,enable,sliders,progress,titlebars})
        QObject::connect(box,&QCheckBox::toggled,footer,changed);
    QObject::connect(sources,&QListWidget::itemChanged,footer,changed);
    QObject::connect(colors,&QLineEdit::textChanged,footer,changed);
    QObject::connect(period,&QSpinBox::valueChanged,footer,changed);
    QObject::connect(pattern,&QComboBox::currentIndexChanged,footer,changed);
    QObject::connect(knobs,&QCheckBox::toggled,mixer,enablement);QObject::connect(all,&QCheckBox::toggled,mixer,enablement);QObject::connect(reactive,&QCheckBox::toggled,mixer,enablement);
    QObject::connect(refresh,&QPushButton::clicked,mixer,[=]{state->refreshSources();changed();});
    QObject::connect(sources,&QListWidget::itemDoubleClicked,mixer,[](QListWidgetItem *item){if(item)item->setCheckState(item->checkState()==Qt::Checked?Qt::Unchecked:Qt::Checked);});
    auto addName=[=]{const auto n=name->text();if(n.isEmpty()||n.trimmed().isEmpty())return;auto chosen=checkedNames();if(!chosen.contains(n))chosen.append(n);populate(chosen);name->clear();changed();};
    QObject::connect(add,&QPushButton::clicked,mixer,addName);QObject::connect(name,&QLineEdit::returnPressed,mixer,addName);
    QObject::connect(colors,&QLineEdit::textChanged,gradient,updatePreview);
    QObject::connect(theme,&QPushButton::clicked,colors,&QLineEdit::clear);
    QObject::connect(pickColor,&QPushButton::clicked,gradient,[=]{
        const auto color=QColorDialog::getColor(gradient->palette().color(QPalette::Highlight),gradient,"Add gradient color");
        if(color.isValid())colors->setText(colors->text().trimmed().isEmpty()?color.name():colors->text()+", "+color.name());
    });
    QObject::connect(reload,&QPushButton::clicked,footer,[=]{load();status->setText("Reloaded saved mixer and gradient settings.");});
    QObject::connect(apply,&QPushButton::clicked,footer,[=,&controller]{
        if(knobs->isChecked()&&!all->isChecked()&&checkedNames().isEmpty()){status->setText("Choose at least one source, or enable All mixer sources.");return;}
        QString error;
        if(controller.setGradientSettings(draft(),&error,&state->baseline)){load();status->setText("Applied mixer and gradient settings to OBS.");}
        else status->setText("Not saved: "+error);
    });
    load();return state;
}
}
struct Inspector::Ui {
    Controller &controller;
    WidgetSnapshot snapshot,captured;
    QTimer *timer=nullptr;
    QTabWidget *tabs=nullptr;
    QLineEdit *filter=nullptr;
    QSpinBox *pickDelay=nullptr;
    QWidget *footer=nullptr,*details=nullptr;
    QCheckBox *advanced=nullptr;
    std::shared_ptr<SettingsPages> settings;
    QLabel *type=nullptr,*geometry=nullptr,*status=nullptr,*sample=nullptr,*scopeHint=nullptr;
    QPlainTextEdit *ancestry=nullptr,*selector=nullptr;
    QListWidget *actions=nullptr;
    QComboBox *icons=nullptr,*preset=nullptr,*tooltipWidth=nullptr;
    QSpinBox *duration=nullptr;
    QLineEdit *color=nullptr;
    QCheckBox *scope=nullptr,*replace=nullptr,*motion=nullptr,*compactMenus=nullptr,*overflow=nullptr,*hideEntry=nullptr,*includeSubmenus=nullptr,*showValues=nullptr;
    QPushButton *save=nullptr,*remove=nullptr,*pick=nullptr,*copy=nullptr,*choose=nullptr,*addEntry=nullptr,*manageEntries=nullptr;
    explicit Ui(Controller &c):controller(c){}
};

Inspector::Inspector(Controller &controller,QWidget *parent,Panels *panels,PropertySections *sections,std::function<bool(bool)> mixerToggle,RowStripes *rowStripes,MenuBarCompact *compactBar,ActionStrip *actionStrip,DockLayouts *layouts,CueDeck *cues,std::function<void(QWidget*)> checkpoints,std::function<void()> openCollectionManager):QDialog(parent),ui(new Ui(controller)) {
    setWindowTitle("Aftermarket · Workspace");
    setObjectName("aftermarket-inspector");
    // Colorway and other OBS themes may make every control bold. Apply only
    // font weights inside this plugin-owned dialog: inherit the theme's
    // actual font family, font size, palette, borders and control styling.
    setStyleSheet(QStringLiteral(
        "QLabel, QCheckBox, QComboBox, QLineEdit, QListWidget, "
        "QPlainTextEdit, QSpinBox, QPushButton, QToolButton { font-weight: normal; }"
        "QTabBar::tab { font-weight: normal; }"
        "QTabBar::tab:selected { font-weight: 500; }"
        "QLabel#augment-heading-mixer, QLabel#augment-heading-gradient, "
        "QLabel#augment-target-title { font-weight: 600; }"));
    setModal(false);
    resize(820,590);
    setMinimumSize(570,420);
    auto *root=new QVBoxLayout(this);
    root->setContentsMargins(11,11,11,11);root->setSpacing(7);
    auto *toolbar=new QHBoxLayout;
    ui->pick=new QPushButton("Pick target",this);
    ui->pick->setObjectName("augment-pick");
    ui->status=new QLabel("Click Pick target, then open a context menu and hover an action before the timer expires.",this);
    ui->status->setWordWrap(true);ui->status->setTextFormat(Qt::PlainText);
    toolbar->addWidget(ui->pick);toolbar->addStretch();
    ui->motion=new QCheckBox("Motion",this);ui->motion->setObjectName("augment-motion");
    ui->motion->setToolTip("Enable menu artwork, dock collapse and AudioBus transitions. Turn off for immediate UI changes and reduced motion; gradient animation updates immediately.");
    ui->motion->setChecked(controller.motionEnabled());
    ui->compactMenus=new QCheckBox("Compact menus",this);ui->compactMenus->setObjectName("augment-compact-menus");
    ui->compactMenus->setToolTip("Tighten long menus into visible columns. Only explicitly selected actions enter More actions. Reopen a menu to apply.");
    ui->compactMenus->setChecked(controller.compactMenusEnabled());toolbar->addWidget(ui->compactMenus);
    if(compactBar){
        auto *main=qobject_cast<QMainWindow*>(parent);
        auto *bar=main?main->menuBar():nullptr;
        if(bar){
            auto *toggle=new QCheckBox(QString::fromUtf8("☰ Menu bar"),this);
            toggle->setObjectName("augment-hamburger-menu-bar");
            toggle->setToolTip("Replace the native top-level OBS menu labels with one hamburger menu. Alt+M opens the original menus. Reversible.");
            toggle->setAccessibleName("Compact hamburger menu bar");
            toggle->setChecked(controller.hamburgerMenuBarEnabled());
            toolbar->addWidget(toggle);
            QPointer<MenuBarCompact> safe(compactBar);
            QPointer<QMenuBar> safeBar(bar);
            connect(toggle,&QCheckBox::toggled,this,
                [this,&controller,toggle,safe,safeBar](bool enabled){
                    if(!safe||!safeBar||!safe->setEnabled(safeBar,enabled)){
                        const QSignalBlocker blocker(toggle);
                        toggle->setChecked(controller.hamburgerMenuBarEnabled());
                        ui->status->setText("Cannot change the OBS menu bar in this layout.");
                        return;
                    }
                    QString error;
                    if(controller.setHamburgerMenuBarEnabled(enabled,&error))
                        ui->status->setText(enabled?"Hamburger menu bar enabled. Alt+M opens OBS menus.":"Original OBS menu bar restored.");
                    else{
                        safe->setEnabled(safeBar,!enabled);
                        const QSignalBlocker blocker(toggle);
                        toggle->setChecked(controller.hamburgerMenuBarEnabled());
                        ui->status->setText(error);
                    }
                });
        }
    }
    toolbar->addWidget(ui->motion);
    connect(ui->compactMenus,&QCheckBox::toggled,this,[this](bool enabled){
        QString error;
        if(ui->controller.setCompactMenus(enabled,&error))
            ui->status->setText(enabled?"Compact menus enabled. Reopen a menu.":"Native menu layout restored on next popup.");
        else {
            const QSignalBlocker blocker(ui->compactMenus);
            ui->compactMenus->setChecked(ui->controller.compactMenusEnabled());ui->status->setText(error);
        }
    });
    ui->pickDelay=new QSpinBox(this);ui->pickDelay->setRange(2,10);ui->pickDelay->setValue(4);ui->pickDelay->setSuffix(" s");ui->pickDelay->setToolTip("Time to open a menu or hover a widget");toolbar->insertWidget(1,ui->pickDelay);
    root->addLayout(toolbar);
    ui->tabs=new QTabWidget(this);ui->tabs->setObjectName("augment-workspace");root->addWidget(ui->tabs,1);
    auto *menus=new QWidget(ui->tabs);auto *menuLayout=new QVBoxLayout(menus);menuLayout->setContentsMargins(8,8,8,8);menuLayout->setSpacing(6);
    ui->tabs->addTab(menus,"Menus & widgets");
    auto *tooltipRow=new QHBoxLayout;
    tooltipRow->addWidget(new QLabel("Maximum tooltip width",menus));
    ui->tooltipWidth=new QComboBox(menus);ui->tooltipWidth->setObjectName("augment-tooltip-width");
    ui->tooltipWidth->setAccessibleName("Maximum tooltip width");
    populateTooltipWidths(ui->tooltipWidth,controller.tooltipWidth());
    tooltipRow->addWidget(ui->tooltipWidth);tooltipRow->addStretch();menuLayout->addLayout(tooltipRow);
    connect(ui->tooltipWidth,&QComboBox::currentIndexChanged,this,[this](int){
        QString error;
        if(!ui->controller.setTooltipWidth(ui->tooltipWidth->currentData().toInt(),&error)){
            populateTooltipWidths(ui->tooltipWidth,ui->controller.tooltipWidth());
            ui->status->setText(error);
        }else ui->status->setText("Tooltip width saved.");
    });
    ui->advanced=new QCheckBox("Show technical details",menus);
    ui->details=new QWidget(menus);auto *detailsLayout=new QVBoxLayout(ui->details);detailsLayout->setContentsMargins(0,0,0,0);
    ui->type=new QLabel("Choose a menu action or inspect a native widget",this);ui->type->setTextFormat(Qt::PlainText);ui->type->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->type->setObjectName("augment-target-title");
    ui->type->setFont(subtleHeadingFont(ui->type->font(),0));
    menuLayout->addWidget(ui->type);
    ui->geometry=new QLabel(this);ui->geometry->setTextFormat(Qt::PlainText);ui->geometry->setWordWrap(true);detailsLayout->addWidget(ui->geometry);
    ui->ancestry=new QPlainTextEdit(this);ui->ancestry->setReadOnly(true);ui->ancestry->setMaximumHeight(64);
    ui->ancestry->setPlaceholderText("Widget ancestry");detailsLayout->addWidget(ui->ancestry);
    auto *split=new QSplitter(Qt::Horizontal,this);split->setChildrenCollapsible(false);menuLayout->addWidget(split,1);
    auto *left=new QWidget(split);auto *ll=new QVBoxLayout(left);ll->setContentsMargins(0,0,5,0);ll->setSpacing(4);
    auto *listHeader=new QHBoxLayout;
    listHeader->addWidget(new QLabel("Menu actions",left));
    listHeader->addStretch();
    ui->includeSubmenus=new QCheckBox("List submenu items",left);
    ui->includeSubmenus->setObjectName("augment-list-submenus");
    ui->includeSubmenus->setToolTip("Include nested actions, check states, disabled rows and separators available in the captured Qt menu. Dynamically built submenus are shown only after OBS populates them.");
    listHeader->addWidget(ui->includeSubmenus);
    ll->addLayout(listHeader);
    ui->filter=new QLineEdit(left);ui->filter->setObjectName("augment-menu-filter");ui->filter->setPlaceholderText("Filter actions…");ll->addWidget(ui->filter);
    connect(ui->filter,&QLineEdit::textChanged,this,[this](const QString &text){
        for(int i=0;i<ui->actions->count();++i)ui->actions->item(i)->setHidden(!ui->actions->item(i)->text().contains(text,Qt::CaseInsensitive));
        if(auto *item=ui->actions->currentItem();item&&item->isHidden())ui->actions->setCurrentRow(-1);
        if(ui->actions->currentRow()<0)for(int i=0;i<ui->actions->count();++i)if(!ui->actions->item(i)->isHidden()){ui->actions->setCurrentRow(i);break;}
    });
    ui->actions=new QListWidget(left);ui->actions->setObjectName("augment-actions");ll->addWidget(ui->actions,1);
    auto *right=new QWidget(split);auto *rl=new QVBoxLayout(right);rl->setContentsMargins(5,0,0,0);rl->setSpacing(5);
    auto *iconRow=new QHBoxLayout;
    ui->sample=new QLabel(right);ui->sample->setFixedSize(32,32);ui->sample->setAlignment(Qt::AlignCenter);
    ui->icons=new QComboBox(right);ui->icons->setObjectName("augment-icon");ui->icons->setIconSize(QSize(18,18));
    for (auto name:iconNames()) ui->icons->addItem(previewIcon(name,palette().color(QPalette::Text)),name);
    iconRow->addWidget(ui->sample);iconRow->addWidget(ui->icons,1);rl->addLayout(iconRow);
    auto *colorRow=new QHBoxLayout;
    auto *cl=new QLabel("Color",right);ui->color=new QLineEdit(right);ui->color->setObjectName("augment-color");
    ui->color->setPlaceholderText("Theme text color");
    ui->choose=new QPushButton("Pick…",right);
    colorRow->addWidget(cl);colorRow->addWidget(ui->color,1);colorRow->addWidget(ui->choose);
    rl->addLayout(colorRow);
    auto *motionRow=new QHBoxLayout;
    motionRow->addWidget(new QLabel("Animation",right));
    ui->preset=new QComboBox(right);ui->preset->setObjectName("augment-motion-preset");
    ui->preset->addItems({"soft","pop","spin","fade","lift","none"});
    ui->preset->setToolTip("soft: fade/slide and gentle hover growth; pop: stronger scale; spin: rotation; fade: opacity only; lift: vertical rise; none: static.");
    motionRow->addWidget(ui->preset,1);
    ui->duration=new QSpinBox(right);ui->duration->setObjectName("augment-motion-duration");
    ui->duration->setRange(60,600);ui->duration->setSingleStep(20);ui->duration->setSuffix(" ms");ui->duration->setValue(160);
    motionRow->addWidget(ui->duration);rl->addLayout(motionRow);
    ui->scope=new QCheckBox("Scope to observed menu",right);ui->scope->setObjectName("augment-scope");ui->scope->setChecked(true);
    ui->replace=new QCheckBox("Replace existing native icon",right);
    rl->addWidget(ui->scope);rl->addWidget(ui->replace);
    ui->overflow=new QCheckBox("Move to More actions",right);ui->overflow->setObjectName("augment-overflow");
    ui->overflow->setToolTip("Save this action's placement independently of its icon. Uncheck to restore it. No actions are hidden automatically.");
    rl->addWidget(ui->overflow);
    ui->hideEntry=new QCheckBox("Hide this default entry",right);
    ui->hideEntry->setObjectName("augment-hide-entry");
    ui->hideEntry->setToolTip("Only hides the selected item from matching popup menus. OBS's QAction remains untouched; use Manage entries to restore.");
    rl->addWidget(ui->hideEntry);
    ui->showValues=new QCheckBox("Show selected submenu choices",right);
    ui->showValues->setObjectName("augment-show-submenu-selections");
    ui->showValues->setToolTip("Draw the currently checked choice in parentheses at the trailing edge, using the active theme’s link color. Native labels and submenu arrows stay intact. Applied when menus reopen.");
    ui->showValues->setChecked(controller.showSubmenuSelections());
    rl->addWidget(ui->showValues);
    connect(ui->showValues,&QCheckBox::toggled,this,[this](bool enabled){
        QString error;
        if(ui->controller.setShowSubmenuSelections(enabled,&error))
            ui->status->setText(enabled?"Selected choices will appear right-aligned in parentheses when menus reopen.":"Selected choice annotations disabled for the next popup.");
        else {
            const QSignalBlocker blocker(ui->showValues);
            ui->showValues->setChecked(ui->controller.showSubmenuSelections());
            ui->status->setText(error);
        }
    });
    connect(ui->hideEntry,&QCheckBox::clicked,this,[this](bool hidden){
        const int row=ui->actions->currentRow();
        auto snapshot=ui->snapshot;
        if(row<0||row>=snapshot.actions.size())return;
        snapshot.actions[row].checkable=false;
        const auto selector=selectorFor(snapshot,row,ui->scope->isChecked());
        QString error;
        if(ui->controller.setHiddenRule(selector,hidden,&error))
            ui->status->setText(hidden?"Default entry hidden on next popup. Use Manage entries to restore.":"Entry restored on next popup.");
        else ui->status->setText(error);
        updatePreview();
    });
    auto *entryButtons=new QHBoxLayout;
    ui->addEntry=new QPushButton("Add custom…",right);ui->addEntry->setObjectName("augment-add-entry");
    ui->manageEntries=new QPushButton("Manage entries…",right);ui->manageEntries->setObjectName("augment-manage-entries");
    entryButtons->addWidget(ui->addEntry);entryButtons->addWidget(ui->manageEntries);
    rl->addLayout(entryButtons);
    connect(ui->addEntry,&QPushButton::clicked,this,[this]{
        if(!ui->snapshot.isMenu){ui->status->setText("Capture the target menu first.");return;}
        auto entry=openCustomEditor(this,{});
        if(entry.isEmpty())return;
        const auto scope=menuScope(ui->snapshot);
        if(scope.isEmpty()){ui->status->setText("Cannot scope this menu; capture a menu with identifiable actions.");return;}
        for(auto it=scope.begin();it!=scope.end();++it)entry[it.key()]=it.value();
        entry["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);
        QString error;
        if(ui->controller.setCustomAction(entry,&error))
            ui->status->setText("Custom action saved. Reopen the menu to see it.");
        else ui->status->setText(error);
    });
    connect(ui->manageEntries,&QPushButton::clicked,this,[this]{
        QDialog dialog(this);
        dialog.setWindowTitle("Manage menu entries");
        dialog.setMinimumSize(475,340);
        auto *layout=new QVBoxLayout(&dialog);
        auto *hint=new QLabel("Hidden defaults are reversible. Custom actions can be edited or removed here.",&dialog);
        hint->setWordWrap(true);layout->addWidget(hint);
        auto *showAll=new QCheckBox("Show all menu changes",&dialog);
        showAll->setObjectName("augment-show-all-menu-rules");
        showAll->setChecked(!ui->snapshot.isMenu);
        layout->addWidget(showAll);
        auto *list=new QListWidget(&dialog);list->setObjectName("augment-managed-entries");layout->addWidget(list,1);
        auto *buttons=new QHBoxLayout;
        auto *edit=new QPushButton("Edit custom…",&dialog);
        auto *remove=new QPushButton("Restore / remove",&dialog);
        buttons->addWidget(edit);buttons->addWidget(remove);layout->addLayout(buttons);
        auto *close=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);layout->addWidget(close);
        connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::accept);
        auto refresh=[&]{
            list->clear();
            const auto hidden=showAll->isChecked()?ui->controller.allHiddenRules():
                ui->controller.hiddenRulesForMenu(ui->snapshot.menuObjectName,ui->snapshot.menuLabels);
            for(const auto &v:hidden) {
                const auto rule=v.toObject();
                const auto scope=rule.value("menuObjectName").toString();
                const auto anchors=rule.value("menuContains").toArray();
                const auto context=!scope.isEmpty()?scope:
                    anchors.isEmpty()?QStringLiteral("All matching menus"):
                    QStringLiteral("Menus containing: ")+anchors.at(0).toString();
                auto *item=new QListWidgetItem("Hidden default · "+
                    rule.value("text").toString(rule.value("objectName").toString())+"  ·  "+context,list);
                item->setData(Qt::UserRole,rule);
                item->setData(Qt::UserRole+1,"hidden");
            }
            const auto custom=showAll->isChecked()?ui->controller.allCustomActions():
                ui->controller.customActionsForMenu(ui->snapshot.menuObjectName,ui->snapshot.menuLabels);
            for(const auto &v:custom) {
                const auto rule=v.toObject();
                const auto scope=rule.value("menuObjectName").toString();
                const auto anchors=rule.value("menuContains").toArray();
                const auto context=!scope.isEmpty()?scope:
                    anchors.isEmpty()?QStringLiteral("Unscoped"):
                    QStringLiteral("Menus containing: ")+anchors.at(0).toString();
                auto *item=new QListWidgetItem("Custom · "+rule.value("label").toString()+"  ·  "+context,list);
                item->setData(Qt::UserRole,rule);
                item->setData(Qt::UserRole+1,"custom");
            }
            remove->setEnabled(list->currentItem()!=nullptr);
            edit->setEnabled(false);
        };
        connect(showAll,&QCheckBox::toggled,&dialog,[&](bool){refresh();});
        connect(list,&QListWidget::currentItemChanged,&dialog,[&](QListWidgetItem *current){
            remove->setEnabled(current);
            edit->setEnabled(current&&current->data(Qt::UserRole+1).toString()=="custom");
        });
        connect(remove,&QPushButton::clicked,&dialog,[&]{
            auto *item=list->currentItem();if(!item)return;
            const auto rule=item->data(Qt::UserRole).toJsonObject();
            QString error;
            const bool ok=item->data(Qt::UserRole+1).toString()=="hidden"?
                ui->controller.setHiddenRule(rule,false,&error):
                ui->controller.removeCustomAction(rule.value("id").toString(),&error);
            if(ok){refresh();ui->status->setText("Entry rule removed. Reopen the menu.");}
            else QMessageBox::warning(&dialog,"Cannot change menu entry",error);
        });
        connect(edit,&QPushButton::clicked,&dialog,[&]{
            auto *item=list->currentItem();if(!item||item->data(Qt::UserRole+1).toString()!="custom")return;
            auto updated=openCustomEditor(&dialog,item->data(Qt::UserRole).toJsonObject());
            if(updated.isEmpty())return;
            QString error;
            if(ui->controller.setCustomAction(updated,&error)){
                refresh();ui->status->setText("Custom entry updated. Reopen the menu.");
            } else QMessageBox::warning(&dialog,"Cannot update menu entry",error);
        });
        refresh();
        dialog.exec();
    });
    connect(ui->overflow,&QCheckBox::clicked,this,[this](bool enabled){
        auto snapshot=ui->snapshot;
        const int row=ui->actions->currentRow();
        if(row<0||row>=snapshot.actions.size())return;
        snapshot.actions[row].checkable=false;
        const auto selector=selectorFor(snapshot,row,ui->scope->isChecked());
        QString error;
        if(ui->controller.setOverflowRule(selector,enabled,&error))
            ui->status->setText(enabled?"Placement saved. Reopen the menu to find this action in More actions.":"Placement restored. Reopen the menu.");
        else ui->status->setText(error);
        updatePreview();
    });
    ui->scopeHint=new QLabel(right);ui->scopeHint->setWordWrap(true);rl->addWidget(ui->scopeHint);
    rl->addWidget(ui->advanced);
    ui->selector=new QPlainTextEdit(right);ui->selector->setReadOnly(true);ui->selector->setObjectName("augment-selector");
    ui->selector->setMinimumHeight(95);detailsLayout->addWidget(ui->selector,1);
    menuLayout->addWidget(ui->details);ui->details->hide();
    auto *buttons=new QHBoxLayout;
    ui->copy=new QPushButton("Copy JSON",right);
    ui->remove=new QPushButton("Remove exact rule",right);
    ui->save=new QPushButton("Save rule",right);ui->save->setObjectName("augment-save");
    buttons->addWidget(ui->copy);buttons->addWidget(ui->remove);buttons->addWidget(ui->save);
    rl->addStretch();rl->addLayout(buttons);split->addWidget(left);split->addWidget(right);split->setSizes({310,420});
    ui->footer=new QWidget(this);root->addWidget(ui->footer);root->addWidget(ui->status);
    ui->settings=buildSettings(controller,ui->tabs,parent,ui->footer,ui->status);
    if(panels){
        auto *page=panels->createPage(qobject_cast<QMainWindow*>(parent),ui->tabs);
        if(mixerToggle){
            auto *toggle=new QCheckBox("Audio Mixer backside (native routing)",page);
            toggle->setObjectName("augment-mixer-backside");
            toggle->setToolTip("Flip the original OBS mixer to a graphical router for native tracks 1–6 and monitoring. No external audio backend.");
            toggle->setChecked(controller.mixerBacksideEnabled());
            if(auto *layout=qobject_cast<QVBoxLayout*>(page->layout()))layout->insertWidget(1,toggle);
            connect(toggle,&QCheckBox::toggled,page,[this,&controller,toggle,mixerToggle](bool value){
                if(!mixerToggle(value)){
                    const QSignalBlocker blocker(toggle);
                    toggle->setChecked(controller.mixerBacksideEnabled());
                    ui->status->setText("The native OBS Audio Mixer dock is unavailable in this layout.");
                    return;
                }
                QString error;
                if(controller.setMixerBacksideEnabled(value,&error))
                    ui->status->setText(value?"Audio Mixer backside enabled. Use the flip icon in the mixer dock.":"Original native Audio Mixer restored.");
                else{
                    mixerToggle(!value);
                    const QSignalBlocker blocker(toggle);
                    toggle->setChecked(controller.mixerBacksideEnabled());
                    ui->status->setText(error);
                }
            });
        }
        if(sections){
            auto *toggle=new QCheckBox("Collapsible source / filter property groups",page);
            toggle->setObjectName("augment-property-sections");
            toggle->setToolTip("Add a separate collapse arrow to ordinary OBS property groups. Does not alter filter settings or checkable enable/disable groups.");
            toggle->setChecked(controller.collapsiblePropertyGroups());
            if(auto *layout=qobject_cast<QVBoxLayout*>(page->layout()))layout->insertWidget(1,toggle);
            QPointer<PropertySections> safe(sections);
            connect(toggle,&QCheckBox::toggled,page,[&controller,toggle,safe, this](bool value){
                QString error;
                if(controller.setCollapsiblePropertyGroups(value,&error)) {
                    if(safe)safe->setEnabled(value);
                    ui->status->setText(value?"Property group collapse arrows enabled.":"Original property group layouts restored.");
                } else {
                    const QSignalBlocker blocker(toggle);
                    toggle->setChecked(controller.collapsiblePropertyGroups());
                    ui->status->setText(error);
                }
            });
        }
        if(rowStripes){
            auto *section=new QGroupBox("Alternating list backgrounds",page);
            section->setObjectName("augment-striped-list-settings");
            auto *contents=new QVBoxLayout(section);
            auto *hint=new QLabel("Use native row striping in OBS's Scenes and Sources lists. Source-specific colors remain native.",section);
            hint->setWordWrap(true);contents->addWidget(hint);
            QPointer<RowStripes> safe(rowStripes);
            auto commit=[this,&controller,safe](const QJsonObject &next){
                QString error;
                if(!controller.setListRowSettings(next,&error)){
                    ui->status->setText(error);
                    return false;
                }
                if(safe)safe->configure(next);
                ui->status->setText("List background settings saved.");
                return true;
            };
            auto *scopeRow=new QHBoxLayout;
            for(const auto &target:{qMakePair(QStringLiteral("scenes"),QStringLiteral("Scenes")),
                                    qMakePair(QStringLiteral("sources"),QStringLiteral("Sources"))}){
                auto *toggle=new QCheckBox(target.second,section);
                toggle->setObjectName("augment-striped-"+target.first);
                toggle->setChecked(controller.listRowSettings().value(target.first).toBool(false));
                scopeRow->addWidget(toggle);
                connect(toggle,&QCheckBox::toggled,section,[&controller,commit,toggle,key=target.first](bool enabled){
                    auto next=controller.listRowSettings();
                    next[key]=enabled;
                    if(!commit(next)){
                        const QSignalBlocker blocker(toggle);
                        toggle->setChecked(controller.listRowSettings().value(key).toBool(false));
                    }
                });
            }
            scopeRow->addStretch();contents->addLayout(scopeRow);
            auto *strengthRow=new QHBoxLayout;
            strengthRow->addWidget(new QLabel("Strength",section));
            auto *strength=new QSlider(Qt::Horizontal,section);
            strength->setObjectName("augment-striped-strength");
            strength->setAccessibleName("Alternating row background strength");
            strength->setToolTip("0% = no alternating contrast, 100% = original colors, 200% = twice the contrast.");
            strength->setRange(0,200);
            strength->setSingleStep(5);
            strength->setPageStep(10);
            strength->setTracking(false); // preview while dragging, save on release
            strength->setValue(controller.listRowSettings().value("strength").toInt(100));
            strengthRow->addWidget(strength,1);
            auto *strengthValue=new QLabel(QString::number(strength->value())+"%",section);
            strengthValue->setMinimumWidth(strengthValue->fontMetrics().horizontalAdvance("200%")+6);
            strengthRow->addWidget(strengthValue);
            contents->addLayout(strengthRow);
            connect(strength,&QSlider::sliderMoved,section,
                    [&controller,safe,strengthValue](int amount){
                strengthValue->setText(QString::number(amount)+"%");
                if(safe){
                    auto preview=controller.listRowSettings();
                    preview["strength"]=amount;
                    safe->configure(preview);
                }
            });
            connect(strength,&QSlider::valueChanged,section,
                    [&controller,commit,strength,strengthValue,safe](int amount){
                strengthValue->setText(QString::number(amount)+"%");
                auto next=controller.listRowSettings();
                next["strength"]=amount;
                if(!commit(next)){
                    const auto saved=controller.listRowSettings();
                    const QSignalBlocker blocker(strength);
                    strength->setValue(saved.value("strength").toInt(100));
                    strengthValue->setText(QString::number(strength->value())+"%");
                    if(safe)safe->configure(saved);
                }
            });
            auto *colorGrid=new QGridLayout;
            int colorRow=0;
            for(const auto &field:{qMakePair(QStringLiteral("base"),QStringLiteral("Normal rows")),
                                   qMakePair(QStringLiteral("alternate"),QStringLiteral("Alternating rows"))}){
                const QString key=field.first;
                auto *label=new QLabel(field.second,section);
                auto *choose=new QPushButton(section);
                choose->setObjectName("augment-striped-"+key+"-pick");
                auto *theme=new QPushButton("Use theme",section);
                theme->setObjectName("augment-striped-"+key+"-theme");
                choose->setAccessibleName("Choose "+field.second+" background color");
                theme->setAccessibleName("Restore "+field.second+" to OBS theme color");
                const auto updateLabel=[choose,key,&controller]{
                    const QString color=controller.listRowSettings().value(key).toString();
                    choose->setText(color.isEmpty()?"Choose color…":color);
                    choose->setToolTip(color.isEmpty()?"Default OBS theme color":color);
                    if(color.isEmpty())choose->setIcon(QIcon());
                    else {
                        QPixmap swatch(14,14);
                        swatch.fill(QColor(color));
                        choose->setIcon(QIcon(swatch));
                    }
                };
                updateLabel();
                colorGrid->addWidget(label,colorRow,0);
                colorGrid->addWidget(choose,colorRow,1);
                colorGrid->addWidget(theme,colorRow,2);
                connect(choose,&QPushButton::clicked,section,[=,&controller]{
                    QColor initial(controller.listRowSettings().value(key).toString());
                    if(!initial.isValid())initial=section->palette().color(key=="base"?QPalette::Base:QPalette::AlternateBase);
                    const auto color=QColorDialog::getColor(initial,section,field.second);
                    if(!color.isValid())return;
                    auto next=controller.listRowSettings();
                    next[key]=color.name();
                    if(commit(next))updateLabel();
                });
                connect(theme,&QPushButton::clicked,section,[=,&controller]{
                    auto next=controller.listRowSettings();
                    next.remove(key);
                    if(commit(next))updateLabel();
                });
                ++colorRow;
            }
            contents->addLayout(colorGrid);
            if(auto *layout=qobject_cast<QVBoxLayout*>(page->layout()))
                layout->insertWidget(1,section);
        }
        ui->tabs->addTab(page,"Panels");
    }
    if(actionStrip){
        auto *page=new QWidget(ui->tabs);
        auto *layout=new QVBoxLayout(page);
        layout->setContentsMargins(10,10,10,10);
        auto *heading=new QLabel("Action strip",page);
        heading->setFont(subtleHeadingFont(heading->font()));
        layout->addWidget(heading);
        auto *description=new QLabel(
            "Place live OBS status and existing hotkeys/dock toggles beside the menu bar. "
            "Drag buttons to rearrange them; right-click a button to rename or remove it. "
            "Uses the active theme and preserves OBS actions.",page);
        description->setWordWrap(true);
        layout->addWidget(description);
        auto *main=qobject_cast<QMainWindow*>(parent);
        auto *bar=main?main->menuBar():nullptr;
        auto *visible=new QCheckBox("Show action strip",page);
        visible->setObjectName("augment-action-strip-enable");
        visible->setChecked(actionStrip->enabled());
        layout->addWidget(visible);
        auto *edit=new QPushButton("Edit buttons and status readouts…",page);
        edit->setObjectName("augment-action-strip-edit");
        layout->addWidget(edit);
        auto *contents=new QLabel(page);
        contents->setObjectName("augment-action-strip-summary");
        contents->setWordWrap(true);
        layout->addWidget(contents);
        layout->addStretch();
        QPointer<ActionStrip> safeStrip(actionStrip);
        QPointer<QMenuBar> safeBar(bar);
        auto refresh=[safeStrip,contents,visible]{
            if(!safeStrip)return;
            QStringList labels;
            for(const auto &item:safeStrip->items())
                labels.append(item.label.isEmpty()?item.target:item.label);
            contents->setText(labels.isEmpty()?"No buttons configured yet. Select Edit to add them.":
                "Current items: "+labels.join(" · "));
            const QSignalBlocker block(visible);
            visible->setChecked(safeStrip->enabled());
        };
        refresh();
        connect(visible,&QCheckBox::toggled,this,[safeStrip,safeBar,visible,refresh](bool on){
            if(!safeStrip||!safeBar||!safeStrip->setEnabled(safeBar,on)){
                if(safeStrip){const QSignalBlocker block(visible);visible->setChecked(safeStrip->enabled());}
            }
            refresh();
        });
        connect(edit,&QPushButton::clicked,this,[safeStrip,safeBar,this,refresh]{
            if(safeStrip){safeStrip->showEditor(this,safeBar);refresh();}
        });
        ui->tabs->addTab(page,"Action strip");
    }
    QPointer<ActionStrip> safeStrip(actionStrip);
    QPointer<CueDeck> safeCues(cues);
    if(layouts)if(auto *page=layouts->createPage(ui->tabs,std::move(checkpoints),std::move(openCollectionManager),
        [safeStrip,safeCues]{
            if(safeStrip)safeStrip->refresh();
            if(safeCues)safeCues->refreshPage();
        }))
        ui->tabs->addTab(page,"Dock layouts");
    if(cues)if(auto *page=cues->createPage(ui->tabs))
        ui->tabs->addTab(page,"Cue Deck");
    ui->footer->hide();
    connect(ui->tabs,&QTabWidget::currentChanged,this,[this](int index){
        ui->footer->setVisible(index==1||index==2);
        if(index==1)ui->settings->refreshSources();
        if(index==3)if(auto *refresh=ui->tabs->widget(index)->findChild<QPushButton*>("augment-panel-refresh"))refresh->click();
    });
    connect(ui->advanced,&QCheckBox::toggled,ui->details,&QWidget::setVisible);
    ui->timer=new QTimer(this);ui->timer->setSingleShot(true);
    connect(ui->pick,&QPushButton::clicked,this,[this]{startPick();});
    connect(ui->timer,&QTimer::timeout,this,[this]{
        auto *under=QApplication::widgetAt(QCursor::pos());
        const auto snapshot=(under && (under==this || isAncestorOf(under))) ? WidgetSnapshot{} : inspectAt(QCursor::pos());
        capture(snapshot);
        // Capture is complete before showing; a window system may dismiss the popup.
        setAttribute(Qt::WA_ShowWithoutActivating,true);
        show();
        setAttribute(Qt::WA_ShowWithoutActivating,false);
    });
    connect(ui->includeSubmenus,&QCheckBox::toggled,this,[this](bool){
        const auto original=ui->captured;
        if(original.isMenu)capture(original);
    });
    connect(ui->actions,&QListWidget::currentRowChanged,this,[this](int row){selectRow(row);});
    connect(ui->icons,&QComboBox::currentTextChanged,this,[this]{updatePreview();});
    connect(ui->preset,&QComboBox::currentTextChanged,this,[this]{updatePreview();});
    connect(ui->duration,&QSpinBox::valueChanged,this,[this]{updatePreview();});
    connect(ui->motion,&QCheckBox::toggled,this,[this](bool enabled){
        QString error;
        if(ui->controller.setMotionEnabled(enabled,&error))
            ui->status->setText(enabled?"Motion enabled for menus, dock collapse and AudioBus.":"Motion disabled: UI changes are immediate.");
        else {
            const QSignalBlocker blocker(ui->motion);
            ui->motion->setChecked(ui->controller.motionEnabled());
            ui->status->setText(error);
        }
        updatePreview();
    });
    connect(ui->color,&QLineEdit::textChanged,this,[this]{updatePreview();});
    connect(ui->scope,&QCheckBox::toggled,this,[this]{updatePreview();});
    connect(ui->replace,&QCheckBox::toggled,this,[this]{updatePreview();});
    connect(ui->choose,&QPushButton::clicked,this,[this]{
        QColor initial(ui->color->text());if(!initial.isValid())initial=palette().color(QPalette::Text);
        const auto color=QColorDialog::getColor(initial,this,"Icon color");
        if(color.isValid())ui->color->setText(color.name());
    });
    connect(ui->copy,&QPushButton::clicked,this,[this]{QApplication::clipboard()->setText(ui->selector->toPlainText());});
    connect(ui->save,&QPushButton::clicked,this,[this]{save();});
    connect(ui->remove,&QPushButton::clicked,this,[this]{
        const auto selector=selectorFor(ui->snapshot,ui->actions->currentRow(),ui->scope->isChecked());
        QString error;
        if(ui->controller.removeRule(selector,&error)) {
            ui->status->setText("Exact rule removed. Reopen the target menu.");
            selectRow(ui->actions->currentRow());
            if(auto *item=ui->actions->currentItem()) {
                const auto rule=ui->controller.effectiveRule(targetFor(ui->snapshot,ui->actions->currentRow()),actionLabels(ui->snapshot,ui->actions->currentRow()));
                item->setIcon(rule.isEmpty()?QIcon{}:previewIcon(rule.value("icon").toString(),palette().color(QPalette::Text)));
            }
        } else ui->status->setText(error);
    });
    selectRow(-1);
}
Inspector::~Inspector() =default;
void Inspector::reject() {
    ui->timer->stop();
    QDialog::reject();
}
void Inspector::startPick() {
    if (ui->timer->isActive()) return;
    hide();ui->timer->start(ui->pickDelay->value()*1000);
}
void Inspector::capture(const WidgetSnapshot &snapshot) {
    ui->captured=snapshot;
    auto visible=snapshot;
    if(snapshot.isMenu&&ui->includeSubmenus->isChecked())visible.actions=snapshot.allActions;
    ui->snapshot=visible;
    ui->tabs->setCurrentIndex(visible.mixerSource.isEmpty()?0:1);
    if(snapshot.valid()&&!snapshot.isMenu&&snapshot.mixerSource.isEmpty())ui->advanced->setChecked(true);
    ui->filter->clear();
    {const QSignalBlocker blocker(ui->motion);ui->motion->setChecked(ui->controller.motionEnabled());}
    {const QSignalBlocker blocker(ui->compactMenus);ui->compactMenus->setChecked(ui->controller.compactMenusEnabled());}
    populateTooltipWidths(ui->tooltipWidth,ui->controller.tooltipWidth());
    {const QSignalBlocker blocker(ui->showValues);ui->showValues->setChecked(ui->controller.showSubmenuSelections());}
    {
        QSignalBlocker blocker(ui->actions);
        ui->actions->clear();
        for (const auto &a:visible.actions) {
            QString label=a.text.section(QChar(9),0,0);
            label.replace("&&",QString(QChar(1)));label.remove('&');label.replace(QChar(1),'&');label=label.trimmed();
            if(a.separator)label="────────";
            else if(label.isEmpty())label=a.objectName.isEmpty()?"(unnamed action)":a.objectName;
            if(a.checked)label="✓ "+label;
            if(a.hasSubmenu)label+="  ▸"+(a.selectedChoice.isEmpty()?QString{}:"  "+a.selectedChoice);
            if(a.depth>0)label=QString("  ").repeated(qMin(a.depth,8))+"↳ "+label;
            if(!a.visible)label+="  (hidden)";
            else if(!a.enabled)label+="  (disabled)";
            auto *item=new QListWidgetItem(label,ui->actions);
            if (a.separator) {
                item->setFlags(Qt::NoItemFlags);
                item->setToolTip("Native separator");
            } else if (!a.eligible()) item->setToolTip("Available for inspection; icon painting is limited to ordinary non-checkable actions.");
            else if (a.nativeIcon) item->setToolTip("Has a native icon; use Replace existing to override.");
            if(!a.objectName.isEmpty())item->setToolTip(item->toolTip()+"\n#"+a.objectName);
            const int idx=ui->actions->count()-1;
            const auto rule=ui->controller.effectiveRule(targetFor(visible,idx),actionLabels(visible,idx));
            if(!rule.isEmpty())item->setIcon(previewIcon(rule.value("icon").toString(),palette().color(QPalette::Text)));
        }
        int row=visible.selected>=0 ? visible.selected : (visible.actions.isEmpty()?-1:0);
        if(ui->includeSubmenus->isChecked()&&snapshot.selected>=0&&snapshot.selected<snapshot.actions.size()) {
            row=-1;
            const auto &chosen=snapshot.actions.at(snapshot.selected);
            for(int i=0;i<visible.actions.size();++i)
                if(visible.actions.at(i).depth==0&&visible.actions.at(i).text==chosen.text&&
                   visible.actions.at(i).objectName==chosen.objectName){row=i;break;}
        }
        if(row<0&&!visible.actions.isEmpty())row=0;
        if(row>=0&&row<ui->actions->count()&&ui->actions->item(row)->flags()!=Qt::NoItemFlags)
            ui->actions->setCurrentRow(row);
        else for(int i=0;i<ui->actions->count();++i)
            if(ui->actions->item(i)->flags()!=Qt::NoItemFlags){ui->actions->setCurrentRow(i);break;}
    }
    ui->type->setText(snapshot.valid() ? snapshot.type+(snapshot.objectName.isEmpty()?"":"  #"+snapshot.objectName) : "No OBS widget under the pointer");
    ui->geometry->setText(snapshot.geometry+(snapshot.accessibility.isEmpty()?"":"  ·  "+snapshot.accessibility));
    ui->ancestry->setPlainText(snapshot.ancestry);
    ui->status->setText(!snapshot.valid()?"Pick again with the pointer over OBS.":
        snapshot.isMenu?"Captured native menu. Pick an action to author an override.":
        "Widget inspected. This slice can edit menu actions; other widgets are read-only.");
    selectRow(ui->actions->currentRow());
    if(!snapshot.mixerSource.isEmpty()) {
        ui->settings->refreshSources();
        if(auto *sources=ui->settings->sources.data())for(int i=0;i<sources->count();++i)
            if(sources->item(i)->data(Qt::UserRole).toString()==snapshot.mixerSource)sources->setCurrentRow(i);
        ui->status->setText("Captured mixer source: "+snapshot.mixerSource+". Select sources and apply changes.");
    }
}
void Inspector::selectRow(int row) {
    const auto target=targetFor(ui->snapshot,row);
    const auto rule=target.isEmpty()?QJsonObject{}:ui->controller.effectiveRule(target,actionLabels(ui->snapshot,row));
    const QSignalBlocker a(ui->icons),b(ui->color),c(ui->replace),d(ui->preset),e(ui->duration);
    ui->icons->setCurrentText(rule.value("icon").toString("plus"));
    ui->color->setText(rule.value("color").toString());
    ui->replace->setChecked(rule.value("replaceExisting").toBool(false));
    MotionSpec spec;MotionSpec::parse(rule.value("animation"),spec);
    ui->preset->setCurrentText(spec.preset);ui->duration->setValue(spec.duration);
    updatePreview();
}
void Inspector::updatePreview() {
    auto selector=selectorFor(ui->snapshot,ui->actions->currentRow(),ui->scope->isChecked());
    const int selectedRow=ui->actions->currentRow();
    const bool eligible=!selector.isEmpty()&&selectedRow>=0&&
        selectedRow<ui->snapshot.actions.size()&&ui->snapshot.actions.at(selectedRow).eligible();
    auto overflowSnapshot=ui->snapshot;
    const int overflowRow=ui->actions->currentRow();
    if(overflowRow>=0&&overflowRow<overflowSnapshot.actions.size())overflowSnapshot.actions[overflowRow].checkable=false;
    const auto overflowSelector=selectorFor(overflowSnapshot,overflowRow,ui->scope->isChecked());
    {const QSignalBlocker blocked(ui->overflow);
        ui->overflow->setEnabled(!overflowSelector.isEmpty());
        ui->overflow->setChecked(!overflowSelector.isEmpty()&&ui->controller.hasOverflowRule(overflowSelector));}
    {const QSignalBlocker blocked(ui->hideEntry);
        const bool isDefault=overflowRow>=0&&overflowRow<ui->snapshot.actions.size()&&!ui->snapshot.actions.at(overflowRow).custom;
        ui->hideEntry->setEnabled(isDefault&&!overflowSelector.isEmpty());
        ui->hideEntry->setChecked(isDefault&&!overflowSelector.isEmpty()&&ui->controller.hasHiddenRule(overflowSelector));}
    ui->addEntry->setEnabled(ui->snapshot.isMenu);
    ui->manageEntries->setEnabled(true);
    const auto text=ui->color->text().trimmed();
    QColor color(text);
    const bool validColor=text.isEmpty()||color.isValid();
    if(text.isEmpty())color=palette().color(QPalette::Text);
    ui->sample->setPixmap(previewIcon(ui->icons->currentText(),color).pixmap(24,24));
    for(auto *w:QList<QWidget*>{ui->icons,ui->color,ui->choose,ui->scope,ui->replace,ui->copy,ui->remove,ui->preset,ui->duration})w->setEnabled(eligible);
    ui->scope->setEnabled(eligible||!overflowSelector.isEmpty());
    ui->duration->setEnabled(eligible && ui->preset->currentText()!="none");
    bool native=false;
    if(eligible)native=ui->snapshot.actions.at(ui->actions->currentRow()).nativeIcon;
    ui->save->setEnabled(eligible && validColor && (!native || ui->replace->isChecked()));
    ui->copy->setEnabled(eligible && validColor);
    if(!eligible) {
        ui->selector->setPlainText(selector.isEmpty()?QString{}:
            QString::fromUtf8(QJsonDocument(selector).toJson(QJsonDocument::Indented)));
        ui->scopeHint->setText(selector.isEmpty()?
            "Choose an action to inspect or manage.":
            "This native entry can be scoped or hidden. Its checkable or custom control is not eligible for icon painting.");
        return;
    }
    QString hint=selector.contains("menuObjectName")?"Exact menu object name.":
        selector.contains("menuContains")?"Anonymous menu: matched by two observed labels (when available).":
        selector.contains("objectName")?"Matches this action ID across menus.":"Matches this label across menus.";
    if(native&&!ui->replace->isChecked())hint+="  Native icon present: enable replacement to save.";
    if(!validColor)hint+="  Invalid color.";
    if(!ui->motion->isChecked())hint+="  Global motion is off.";
    ui->scopeHint->setText(hint);
    selector["icon"]=ui->icons->currentText();
    selector["animation"]=QJsonObject{{"preset",ui->preset->currentText()},{"duration",ui->duration->value()}};
    if(!text.isEmpty())selector["color"]=text;
    if(ui->replace->isChecked())selector["replaceExisting"]=true;
    ui->selector->setPlainText(QString::fromUtf8(QJsonDocument(selector).toJson(QJsonDocument::Indented)));
}
void Inspector::save() {
    if(!ui->save->isEnabled())return;
    const auto rule=QJsonDocument::fromJson(ui->selector->toPlainText().toUtf8()).object();
    QString error;
    if(ui->controller.saveRule(rule,&error)) {
        if(auto *item=ui->actions->currentItem())item->setIcon(previewIcon(rule.value("icon").toString(),palette().color(QPalette::Text)));
        ui->status->setText("Saved. Close and reopen the target menu to see your icon.");
    }
    else ui->status->setText(error);
}
}
