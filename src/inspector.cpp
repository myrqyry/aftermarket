#include "inspector.h"
#include "augment.h"
#include "menu-layout.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QSpinBox>
#include "motion.h"
#include "gradient.h"
#include "panels.h"
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
#include <QTimer>
#include <QVBoxLayout>
#include <QWidgetAction>
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
    return {{"text",a.text},{"objectName",a.objectName},{"menuObjectName",s.menuObjectName}};
}
}
bool ActionSnapshot::eligible() const {
    return visible && !checkable && !custom &&
        (!objectName.isEmpty() || !normalizeActionText(text).isEmpty());
}
WidgetSnapshot inspectWidget(QWidget *widget,const QPoint &globalPosition) {
    WidgetSnapshot s;
    if (!widget) return s;
    if (widget->objectName()=="obs-ui-augment-overlay"||widget->objectName()=="obs-ui-augment-gradient") widget=widget->parentWidget();
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
    for (auto *action : menu->actions()) {
        if (overflowAction(action) || action->isSeparator() || !action->isVisible()) continue;
        const bool reservation=action->property("_obsUiAugmentReservation").toBool();
        ActionSnapshot item{action->text(),action->objectName(),action->isCheckable(),
            qobject_cast<QWidgetAction*>(action)!=nullptr,action->isVisible(),action->isEnabled(),
            reservation ? action->property("_obsUiAugmentOriginalIcon").toBool() : !action->icon().isNull()};
        if (action==hit) s.selected=s.actions.size();
        s.actions.push_back(item);
    }
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
    if (!s.isMenu || row<0 || row>=s.actions.size() || !s.actions.at(row).eligible()) return {};
    const auto &a=s.actions.at(row);
    QJsonObject selector;
    if (!a.objectName.isEmpty()) selector["objectName"]=a.objectName;
    else selector["text"]=a.text;
    if (!scopeMenu) return selector;
    if (!s.menuObjectName.isEmpty()) selector["menuObjectName"]=s.menuObjectName;
    else {
        // Anonymous menus have no durable Qt ID. Anchor on two other observed labels.
        QStringList anchors, seen;
        const auto own=normalizeActionText(a.text);
        for (const auto &label:s.menuLabels) {
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
        auto *label=new QLabel(title,parent);QFont f=label->font();f.setBold(true);f.setPointSizeF(f.pointSizeF()+2);label->setFont(f);layout->addWidget(label);
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
    auto *form=new QFormLayout;
    auto *colors=new QLineEdit(gradient);colors->setObjectName("augment-gradient-colors");colors->setPlaceholderText("Theme palette (or 2–6 colors, comma-separated)");
    auto *colorRow=new QHBoxLayout;auto *pickColor=new QPushButton("Add color…",gradient);
    auto *theme=new QPushButton("Use theme",gradient);colorRow->addWidget(colors,1);colorRow->addWidget(pickColor);colorRow->addWidget(theme);form->addRow("Colors",colorRow);
    auto *period=new QSpinBox(gradient);period->setObjectName("augment-gradient-period");period->setRange(400,12000);period->setSingleStep(100);period->setSuffix(" ms");form->addRow("Loop duration",period);
    gl->addLayout(form);
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
        next["enabled"]=enable->isChecked();next["sliders"]=sliders->isChecked();next["progress"]=progress->isChecked();
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
        knobs->setChecked(spec.knobs);knobMeters->setChecked(spec.knobMeters);all->setChecked(spec.knobSources.isEmpty());populate(spec.knobSources);
        reactive->setChecked(spec.audioReactive);meters->setChecked(spec.meters);enable->setChecked(spec.enabled);sliders->setChecked(spec.sliders);progress->setChecked(spec.progress);
        QStringList hex;for(const auto &c:spec.colors)hex.append(c.name());colors->setText(hex.join(", "));period->setValue(spec.period);
        enablement();updatePreview();*normalized=draft();changed();
    };
    for(auto *box:{knobs,knobMeters,all,reactive,meters,enable,sliders,progress})
        QObject::connect(box,&QCheckBox::toggled,footer,changed);
    QObject::connect(sources,&QListWidget::itemChanged,footer,changed);
    QObject::connect(colors,&QLineEdit::textChanged,footer,changed);
    QObject::connect(period,&QSpinBox::valueChanged,footer,changed);
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
    WidgetSnapshot snapshot;
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
    QComboBox *icons=nullptr,*preset=nullptr;
    QSpinBox *duration=nullptr;
    QLineEdit *color=nullptr;
    QCheckBox *scope=nullptr,*replace=nullptr,*motion=nullptr,*compactMenus=nullptr,*overflow=nullptr;
    QPushButton *save=nullptr,*remove=nullptr,*pick=nullptr,*copy=nullptr,*choose=nullptr;
    explicit Ui(Controller &c):controller(c){}
};

Inspector::Inspector(Controller &controller,QWidget *parent,Panels *panels):QDialog(parent),ui(new Ui(controller)) {
    setWindowTitle("UI Augment · Inspector");
    setObjectName("obs-ui-augment-inspector");
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
    ui->motion->setToolTip("Enable motion globally. Uncheck for reduced motion. Menu motion applies on next popup; gradient motion updates immediately.");
    ui->motion->setChecked(controller.motionEnabled());
    ui->compactMenus=new QCheckBox("Compact menus",this);ui->compactMenus->setObjectName("augment-compact-menus");
    ui->compactMenus->setToolTip("Tighten long menus into visible columns. Only explicitly selected actions enter More actions. Reopen a menu to apply.");
    ui->compactMenus->setChecked(controller.compactMenusEnabled());toolbar->addWidget(ui->compactMenus);toolbar->addWidget(ui->motion);
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
    ui->advanced=new QCheckBox("Show technical details",menus);
    ui->details=new QWidget(menus);auto *detailsLayout=new QVBoxLayout(ui->details);detailsLayout->setContentsMargins(0,0,0,0);
    ui->type=new QLabel("Choose a menu action or inspect a native widget",this);ui->type->setTextFormat(Qt::PlainText);ui->type->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont bold=ui->type->font();bold.setBold(true);ui->type->setFont(bold);
    menuLayout->addWidget(ui->type);
    ui->geometry=new QLabel(this);ui->geometry->setTextFormat(Qt::PlainText);ui->geometry->setWordWrap(true);detailsLayout->addWidget(ui->geometry);
    ui->ancestry=new QPlainTextEdit(this);ui->ancestry->setReadOnly(true);ui->ancestry->setMaximumHeight(64);
    ui->ancestry->setPlaceholderText("Widget ancestry");detailsLayout->addWidget(ui->ancestry);
    auto *split=new QSplitter(Qt::Horizontal,this);split->setChildrenCollapsible(false);menuLayout->addWidget(split,1);
    auto *left=new QWidget(split);auto *ll=new QVBoxLayout(left);ll->setContentsMargins(0,0,5,0);ll->setSpacing(4);
    ll->addWidget(new QLabel("Menu actions",left));
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
    ui->preset->addItems({"soft","pop","spin","none"});
    ui->preset->setToolTip("soft: fade/slide and gentle hover growth; pop: stronger scale; spin: rotation; none: static.");
    motionRow->addWidget(ui->preset,1);
    ui->duration=new QSpinBox(right);ui->duration->setObjectName("augment-motion-duration");
    ui->duration->setRange(60,600);ui->duration->setSingleStep(20);ui->duration->setSuffix(" ms");ui->duration->setValue(160);
    motionRow->addWidget(ui->duration);rl->addLayout(motionRow);
    ui->scope=new QCheckBox("Scope to observed menu",right);ui->scope->setChecked(true);
    ui->replace=new QCheckBox("Replace existing native icon",right);
    rl->addWidget(ui->scope);rl->addWidget(ui->replace);
    ui->overflow=new QCheckBox("Move to More actions",right);ui->overflow->setObjectName("augment-overflow");
    ui->overflow->setToolTip("Save this action's placement independently of its icon. Uncheck to restore it. No actions are hidden automatically.");
    rl->addWidget(ui->overflow);
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
    if(panels)ui->tabs->addTab(panels->createPage(qobject_cast<QMainWindow*>(parent),ui->tabs),"Panels");
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
    connect(ui->actions,&QListWidget::currentRowChanged,this,[this](int row){selectRow(row);});
    connect(ui->icons,&QComboBox::currentTextChanged,this,[this]{updatePreview();});
    connect(ui->preset,&QComboBox::currentTextChanged,this,[this]{updatePreview();});
    connect(ui->duration,&QSpinBox::valueChanged,this,[this]{updatePreview();});
    connect(ui->motion,&QCheckBox::toggled,this,[this](bool enabled){
        QString error;
        if(ui->controller.setMotionEnabled(enabled,&error))
            ui->status->setText(enabled?"Motion enabled. Reopen a menu to see it.":"Motion disabled. Reopen menus for static icons.");
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
                const auto rule=ui->controller.effectiveRule(targetFor(ui->snapshot,ui->actions->currentRow()),ui->snapshot.menuLabels);
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
    ui->snapshot=snapshot;
    ui->tabs->setCurrentIndex(snapshot.mixerSource.isEmpty()?0:1);
    if(snapshot.valid()&&!snapshot.isMenu&&snapshot.mixerSource.isEmpty())ui->advanced->setChecked(true);
    ui->filter->clear();
    {const QSignalBlocker blocker(ui->motion);ui->motion->setChecked(ui->controller.motionEnabled());}
    {const QSignalBlocker blocker(ui->compactMenus);ui->compactMenus->setChecked(ui->controller.compactMenusEnabled());}
    {
        QSignalBlocker blocker(ui->actions);
        ui->actions->clear();
        for (const auto &a:snapshot.actions) {
            QString label=a.text.section(QChar(9),0,0);
            label.replace("&&",QString(QChar(1)));label.remove('&');label.replace(QChar(1),'&');label=label.trimmed();
            if(label.isEmpty())label=a.objectName;
            auto *item=new QListWidgetItem(label,ui->actions);
            if (!a.eligible()) item->setToolTip("Not supported by the menu painter.");
            else if (a.nativeIcon) item->setToolTip("Has a native icon; use Replace existing to override.");
            if(!a.objectName.isEmpty())item->setToolTip(item->toolTip()+"\n#"+a.objectName);
            auto rule=ui->controller.effectiveRule(targetFor(snapshot,ui->actions->count()-1),snapshot.menuLabels);
            if(!rule.isEmpty())item->setIcon(previewIcon(rule.value("icon").toString(),palette().color(QPalette::Text)));
        }
        const int row=snapshot.selected>=0 ? snapshot.selected : (snapshot.actions.isEmpty()?-1:0);
        ui->actions->setCurrentRow(row);
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
void Inspector::openPanels(){if(ui->tabs->count()>3)ui->tabs->setCurrentIndex(3);}
void Inspector::selectRow(int row) {
    const auto target=targetFor(ui->snapshot,row);
    const auto rule=target.isEmpty()?QJsonObject{}:ui->controller.effectiveRule(target,ui->snapshot.menuLabels);
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
    const bool eligible=!selector.isEmpty();
    auto overflowSnapshot=ui->snapshot;
    const int overflowRow=ui->actions->currentRow();
    if(overflowRow>=0&&overflowRow<overflowSnapshot.actions.size())overflowSnapshot.actions[overflowRow].checkable=false;
    const auto overflowSelector=selectorFor(overflowSnapshot,overflowRow,ui->scope->isChecked());
    {const QSignalBlocker blocked(ui->overflow);
        ui->overflow->setEnabled(!overflowSelector.isEmpty());
        ui->overflow->setChecked(!overflowSelector.isEmpty()&&ui->controller.hasOverflowRule(overflowSelector));}
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
    if(!eligible) {ui->selector->clear();ui->scopeHint->setText("Choose an ordinary menu action to edit.");return;}
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
