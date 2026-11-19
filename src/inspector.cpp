#include "inspector.h"
#include "augment.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QSpinBox>
#include "motion.h"
#include "gradient.h"
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
    if (widget->objectName()=="obs-ui-augment-overlay") widget=widget->parentWidget();
    if (!widget) return s;
    s.type=QString::fromLatin1(widget->metaObject()->className());
    s.objectName=widget->objectName();
    s.accessibility=widget->accessibleName();
    const auto pos=widget->mapToGlobal(QPoint());
    s.geometry=QString("%1 × %2 · screen %3, %4").arg(widget->width()).arg(widget->height()).arg(pos.x()).arg(pos.y());
    QStringList parents;
    for (auto *w=widget;w;w=w->parentWidget()) parents.prepend(identity(w));
    s.ancestry=parents.join("  ›  ");
    QMenu *menu=nullptr;
    for (auto *w=widget;w;w=w->parentWidget())
        if ((menu=qobject_cast<QMenu*>(w))) break;
    if (!menu) return s;
    s.isMenu=true;
    s.menuObjectName=menu->objectName();
    const auto *hit=menu->actionAt(menu->mapFromGlobal(globalPosition));
    for (auto *action : menu->actions()) {
        if (action->isSeparator() || !action->isVisible()) continue;
        const bool reservation=action->property("_obsUiAugmentReservation").toBool();
        ActionSnapshot item{action->text(),action->objectName(),action->isCheckable(),
            qobject_cast<QWidgetAction*>(action)!=nullptr,action->isVisible(),action->isEnabled(),
            reservation ? action->property("_obsUiAugmentOriginalIcon").toBool() : !action->icon().isNull()};
        if (action==hit) s.selected=s.actions.size();
        s.actions.push_back(item);
        s.menuLabels.append(action->text());
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
void openGradientSettings(Controller &controller,QWidget *parent) {
    if(auto *existing=parent->findChild<QDialog*>("obs-ui-augment-gradient-settings")) {
        existing->show();existing->raise();existing->activateWindow();return;
    }
    auto *dlg=new QDialog(parent);
    dlg->setObjectName("obs-ui-augment-gradient-settings");
    dlg->setWindowTitle("UI Augment · Gradient tracks");
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModal(false);dlg->resize(520,345);
    const auto raw=controller.gradientSettings();
    GradientSpec spec;GradientSpec::parse(raw,spec);
    auto *layout=new QVBoxLayout(dlg);
    layout->setContentsMargins(12,12,12,12);layout->setSpacing(8);
    auto *enable=new QCheckBox("Enable gradient tracks",dlg);
    enable->setObjectName("augment-gradient-enabled");enable->setChecked(spec.enabled);
    layout->addWidget(enable);
    auto *targets=new QHBoxLayout;
    auto *sliders=new QCheckBox("Sliders",dlg);sliders->setChecked(spec.sliders);
    auto *progress=new QCheckBox("Progress bars",dlg);progress->setChecked(spec.progress);
    targets->addWidget(sliders);targets->addWidget(progress);targets->addStretch();
    layout->addLayout(targets);
    auto *reactive=new QCheckBox("Audio-reactive mixer gradients",dlg);
    reactive->setObjectName("augment-gradient-reactive");reactive->setChecked(spec.audioReactive);
    auto *meters=new QCheckBox("Pulse native meter colors",dlg);
    meters->setObjectName("augment-gradient-meters");meters->setChecked(spec.meters);
    meters->setEnabled(spec.audioReactive);
    QObject::connect(reactive,&QCheckBox::toggled,meters,&QWidget::setEnabled);
    layout->addWidget(reactive);layout->addWidget(meters);
    auto *knobs=new QCheckBox("Replace mixer faders with knobs",dlg);
    knobs->setObjectName("augment-gradient-knobs");knobs->setChecked(spec.knobs);
    layout->addWidget(knobs);
    auto *form=new QFormLayout;
    auto *colors=new QLineEdit(dlg);colors->setObjectName("augment-gradient-colors");
    colors->setPlaceholderText("Theme palette (or #hex, #hex, …)");
    QStringList hex;for(const auto &c:spec.colors)hex.append(c.name());
    colors->setText(hex.join(", "));
    form->addRow("Colors",colors);
    auto *knobSources=new QLineEdit(dlg);knobSources->setObjectName("augment-knob-sources");
    knobSources->setPlaceholderText("All mixer sources (or exact names; separated by semicolons)");
    knobSources->setText(spec.knobSources.join("; "));
    knobSources->setEnabled(spec.knobs);
    QObject::connect(knobs,&QCheckBox::toggled,knobSources,&QWidget::setEnabled);
    form->addRow("Knob sources",knobSources);
    auto *period=new QSpinBox(dlg);period->setObjectName("augment-gradient-period");
    period->setRange(400,12000);period->setSingleStep(100);period->setSuffix(" ms");period->setValue(spec.period);
    form->addRow("Loop duration",period);
    layout->addLayout(form);
    auto *note=new QLabel(controller.motionEnabled() ?
        "Motion is on. Audio reactivity follows each mixer source independently." :
        "Motion is off: gradients are static. Enable Motion in the inspector to animate.",dlg);
    note->setWordWrap(true);layout->addWidget(note);
    auto *buttons=new QHBoxLayout;
    auto *apply=new QPushButton("Apply",dlg);apply->setObjectName("augment-gradient-apply");
    auto *close=new QPushButton("Close",dlg);
    buttons->addStretch();buttons->addWidget(apply);buttons->addWidget(close);layout->addLayout(buttons);
    QObject::connect(close,&QPushButton::clicked,dlg,&QDialog::close);
    QObject::connect(apply,&QPushButton::clicked,dlg,[=,&controller]{
        auto next=controller.gradientSettings();
        QJsonArray palette;
        if(!colors->text().trimmed().isEmpty())
            for(const auto &c:colors->text().split(','))palette.append(c.trimmed());
        next["enabled"]=enable->isChecked();next["sliders"]=sliders->isChecked();
        QJsonArray names;
        for(const auto &name:knobSources->text().split(';'))if(!name.trimmed().isEmpty())names.append(name.trimmed());
        next["knobs"]=knobs->isChecked();next["knobSources"]=names;
        next["progress"]=progress->isChecked();next["audioReactive"]=reactive->isChecked();next["meters"]=meters->isChecked();next["colors"]=palette;next["period"]=period->value();
        QString error;
        if(controller.setGradientSettings(next,&error))
            note->setText(enable->isChecked() ? (controller.motionEnabled()?
                "Applied. Visible tracks are now animated.":"Applied. Enable Motion to animate.") : (knobs->isChecked()?"Applied. Mixer knobs use the theme palette.":"Gradient tracks and knobs disabled."));
        else note->setText("Not saved: use 2–6 opaque colors, or leave blank for the theme palette. "+error);
    });
    dlg->show();
}
}
struct Inspector::Ui {
    Controller &controller;
    WidgetSnapshot snapshot;
    QTimer *timer=nullptr;
    QLabel *type=nullptr,*geometry=nullptr,*status=nullptr,*sample=nullptr,*scopeHint=nullptr;
    QPlainTextEdit *ancestry=nullptr,*selector=nullptr;
    QListWidget *actions=nullptr;
    QComboBox *icons=nullptr,*preset=nullptr;
    QSpinBox *duration=nullptr;
    QLineEdit *color=nullptr;
    QCheckBox *scope=nullptr,*replace=nullptr,*motion=nullptr;
    QPushButton *save=nullptr,*remove=nullptr,*pick=nullptr,*copy=nullptr,*choose=nullptr;
    explicit Ui(Controller &c):controller(c){}
};

Inspector::Inspector(Controller &controller,QWidget *parent):QDialog(parent),ui(new Ui(controller)) {
    setWindowTitle("UI Augment · Inspector");
    setObjectName("obs-ui-augment-inspector");
    setModal(false);
    resize(800,580);
    setMinimumSize(570,420);
    auto *root=new QVBoxLayout(this);
    root->setContentsMargins(11,11,11,11);root->setSpacing(7);
    auto *toolbar=new QHBoxLayout;
    ui->pick=new QPushButton("Pick target · 4s",this);
    ui->pick->setObjectName("augment-pick");
    ui->status=new QLabel("Pick any widget or open a context menu and hover an action.",this);
    ui->status->setWordWrap(true);
    toolbar->addWidget(ui->pick);toolbar->addWidget(ui->status,1);
    ui->motion=new QCheckBox("Motion",this);ui->motion->setObjectName("augment-motion");
    ui->motion->setToolTip("Enable motion globally. Uncheck for reduced motion. Menu motion applies on next popup; gradient motion updates immediately.");
    ui->motion->setChecked(controller.motionEnabled());toolbar->addWidget(ui->motion);
    auto *gradients=new QPushButton("Gradients…",this);gradients->setObjectName("augment-gradient-settings");
    toolbar->addWidget(gradients);
    connect(gradients,&QPushButton::clicked,this,[this]{openGradientSettings(ui->controller,this);});
    root->addLayout(toolbar);
    ui->type=new QLabel("No target captured",this);ui->type->setTextFormat(Qt::PlainText);ui->type->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont bold=ui->type->font();bold.setBold(true);ui->type->setFont(bold);
    root->addWidget(ui->type);
    ui->geometry=new QLabel(this);ui->geometry->setTextFormat(Qt::PlainText);ui->geometry->setWordWrap(true);root->addWidget(ui->geometry);
    ui->ancestry=new QPlainTextEdit(this);ui->ancestry->setReadOnly(true);ui->ancestry->setMaximumHeight(64);
    ui->ancestry->setPlaceholderText("Widget ancestry");root->addWidget(ui->ancestry);
    auto *split=new QSplitter(Qt::Horizontal,this);split->setChildrenCollapsible(false);root->addWidget(split,1);
    auto *left=new QWidget(split);auto *ll=new QVBoxLayout(left);ll->setContentsMargins(0,0,5,0);ll->setSpacing(4);
    ll->addWidget(new QLabel("Menu actions",left));
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
    ui->scopeHint=new QLabel(right);ui->scopeHint->setWordWrap(true);rl->addWidget(ui->scopeHint);
    rl->addWidget(new QLabel("Generated rule",right));
    ui->selector=new QPlainTextEdit(right);ui->selector->setReadOnly(true);ui->selector->setObjectName("augment-selector");
    ui->selector->setMinimumHeight(95);rl->addWidget(ui->selector,1);
    auto *buttons=new QHBoxLayout;
    ui->copy=new QPushButton("Copy JSON",right);
    ui->remove=new QPushButton("Remove exact rule",right);
    ui->save=new QPushButton("Save rule",right);ui->save->setObjectName("augment-save");
    buttons->addWidget(ui->copy);buttons->addWidget(ui->remove);buttons->addWidget(ui->save);
    rl->addLayout(buttons);split->addWidget(left);split->addWidget(right);split->setSizes({310,420});
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
    hide();ui->timer->start(4000);
}
void Inspector::capture(const WidgetSnapshot &snapshot) {
    ui->snapshot=snapshot;
    {const QSignalBlocker blocker(ui->motion);ui->motion->setChecked(ui->controller.motionEnabled());}
    {
        QSignalBlocker blocker(ui->actions);
        ui->actions->clear();
        for (const auto &a:snapshot.actions) {
            QString label=a.text;
            if (!a.objectName.isEmpty()) label+="\n#"+a.objectName;
            auto *item=new QListWidgetItem(label,ui->actions);
            if (!a.eligible()) item->setToolTip("Not supported by the menu painter.");
            else if (a.nativeIcon) item->setToolTip("Has a native icon; use Replace existing to override.");
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
}
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
    const auto text=ui->color->text().trimmed();
    QColor color(text);
    const bool validColor=text.isEmpty()||color.isValid();
    if(text.isEmpty())color=palette().color(QPalette::Text);
    ui->sample->setPixmap(previewIcon(ui->icons->currentText(),color).pixmap(24,24));
    for(auto *w:QList<QWidget*>{ui->icons,ui->color,ui->choose,ui->scope,ui->replace,ui->copy,ui->remove,ui->preset,ui->duration})w->setEnabled(eligible);
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
    if(ui->controller.saveRule(rule,&error))
        ui->status->setText("Saved. Close and reopen the target menu to see your icon.");
    else ui->status->setText(error);
}
}
