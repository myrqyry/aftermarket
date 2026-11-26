#include "augment.h"
#include "inspector.h"
#include <QApplication>
#include <QMenu>
#include <QPushButton>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QImage>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QLineEdit>
#include <QListWidget>
#include <QTabWidget>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTimer>
#include <QDialogButtonBox>
#include <QDialog>
#include <QMainWindow>
#include <QDockWidget>
#include "panels.h"
#include "property-sections.h"
#include "row-stripes.h"
#include "menu-bar-compact.h"
#include "action-strip.h"
#include "dock-layouts.h"
#include "cue-deck.h"
#include <QMenuBar>
#include <iostream>
#define REQUIRE(x) do { if (!(x)) { std::cerr << "FAILED line " << __LINE__ << ": " << #x << '\n'; return 1; } } while(false)
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QTemporaryDir temp;
    auto path=temp.filePath("rules.json");
    auto write=[&](QByteArray b){QFile f(path);return f.open(QIODevice::WriteOnly|QIODevice::Truncate)&&f.write(b)==b.size();};
    auto read=[&](){QFile f(path);f.open(QIODevice::ReadOnly);return f.readAll();};
    REQUIRE(write(R"({"enabled":true,"inspect":false,"extra":"keep","rules":[{"text":"Rename","icon":"pencil"},{"text":"Copy","icon":"copy"}]})"));
    augment::Controller c(path);
    REQUIRE(!c.modernTransformFrame());
    REQUIRE(!c.collapsiblePropertyGroups());
    REQUIRE(!c.hamburgerMenuBarEnabled());
    REQUIRE(c.setHamburgerMenuBarEnabled(true));
    REQUIRE(c.hamburgerMenuBarEnabled());
    REQUIRE(QJsonDocument::fromJson(read()).object().value("hamburgerMenuBar").toBool());
    REQUIRE(c.setHamburgerMenuBarEnabled(false));
    REQUIRE(!c.hamburgerMenuBarEnabled());
    REQUIRE(c.listRowSettings().isEmpty());
    const QJsonObject stripeSettings{{"scenes",true},{"sources",true},{"base","#223344"},{"alternate","#425466"}};
    REQUIRE(c.setListRowSettings(stripeSettings));
    REQUIRE(c.listRowSettings()==stripeSettings);
    REQUIRE(QJsonDocument::fromJson(read()).object().value("listRows").toObject()==stripeSettings);
    REQUIRE(!c.setListRowSettings({{"sources","not boolean"}}));
    REQUIRE(!c.setListRowSettings({{"alternate","invalid!"}}));
    REQUIRE(!c.setListRowSettings({{"strength",-1}}));
    REQUIRE(!c.setListRowSettings({{"strength",201}}));
    REQUIRE(!c.setListRowSettings({{"strength",55.5}}));
    REQUIRE(!c.setListRowSettings({{"strength","high"}}));
    REQUIRE(c.listRowSettings()==stripeSettings);
    auto strong=stripeSettings;strong["strength"]=175;
    REQUIRE(c.setListRowSettings(strong));
    REQUIRE(c.listRowSettings()==strong);
    REQUIRE(QJsonDocument::fromJson(read()).object().value("listRows").toObject()==strong);
    REQUIRE(c.setListRowSettings({}));
    REQUIRE(!c.mixerBacksideEnabled());
    REQUIRE(c.setMixerBacksideEnabled(true));
    REQUIRE(c.mixerBacksideEnabled());
    REQUIRE(QJsonDocument::fromJson(read()).object().value("mixerBackside").toBool());
    REQUIRE(c.setMixerBacksideEnabled(false));
    REQUIRE(!c.mixerBacksideEnabled());
    REQUIRE(c.setCollapsiblePropertyGroups(true));
    REQUIRE(c.collapsiblePropertyGroups());
    REQUIRE(QJsonDocument::fromJson(read()).object().value("collapsiblePropertyGroups").toBool());
    REQUIRE(c.setCollapsiblePropertyGroups(false));
    REQUIRE(!c.collapsiblePropertyGroups());
    REQUIRE(c.setModernTransformFrame(true));
    REQUIRE(c.modernTransformFrame());
    REQUIRE(c.setModernTransformFrame(false));
    REQUIRE(!c.modernTransformFrame());

    {
        QMainWindow main;
        augment::Panels panels;
        augment::PropertySections sections;
        augment::RowStripes stripes;
        augment::MenuBarCompact compactBar;
        augment::ActionStrip strip(temp.filePath("strip.json"));
        main.menuBar()->addMenu("&File");
        main.menuBar()->addMenu("&Tools");
        int flips=0;
        augment::Inspector inspector(c,&main,&panels,&sections,[&](bool){++flips;return true;},&stripes,&compactBar,&strip);
        const auto typeRules=inspector.styleSheet();
        REQUIRE(typeRules.contains("font-weight: normal"));
        REQUIRE(typeRules.contains("QLabel#augment-heading-mixer"));
        REQUIRE(typeRules.contains("QCheckBox"));
        REQUIRE(!typeRules.contains("color:")); // theme owns all foreground/background colors
        auto *mixerHeading=inspector.findChild<QLabel*>("augment-heading-mixer");
        auto *gradientHeading=inspector.findChild<QLabel*>("augment-heading-gradient");
        auto *targetTitle=inspector.findChild<QLabel*>("augment-target-title");
        REQUIRE(mixerHeading&&gradientHeading&&targetTitle);
        REQUIRE(mixerHeading->font().weight()==QFont::DemiBold);
        REQUIRE(gradientHeading->font().weight()==QFont::DemiBold);
        REQUIRE(targetTitle->font().weight()==QFont::DemiBold);
        REQUIRE(mixerHeading->font().family()==inspector.font().family());
        REQUIRE(mixerHeading->font().pointSizeF()<=inspector.font().pointSizeF()+1.1);
        auto *hamburgerToggle=inspector.findChild<QCheckBox*>("augment-hamburger-menu-bar");
        REQUIRE(hamburgerToggle&&!hamburgerToggle->isChecked());
        hamburgerToggle->click();
        REQUIRE(c.hamburgerMenuBarEnabled()&&compactBar.enabled());
        REQUIRE(main.menuBar()->actions().size()==1);
        hamburgerToggle->click();
        REQUIRE(!c.hamburgerMenuBarEnabled()&&!compactBar.enabled());
        REQUIRE(main.menuBar()->actions().size()==2);
        auto *sceneStripes=inspector.findChild<QCheckBox*>("augment-striped-scenes");
        auto *sourceStripes=inspector.findChild<QCheckBox*>("augment-striped-sources");
        REQUIRE(sceneStripes&&sourceStripes&&!sceneStripes->isChecked()&&!sourceStripes->isChecked());
        sceneStripes->click();
        REQUIRE(c.listRowSettings().value("scenes").toBool()&&stripes.settings().value("scenes").toBool());
        sourceStripes->click();
        REQUIRE(c.listRowSettings().value("sources").toBool()&&stripes.settings().value("sources").toBool());
        auto *stripeStrength=inspector.findChild<QSlider*>("augment-striped-strength");
        REQUIRE(stripeStrength&&stripeStrength->minimum()==0&&stripeStrength->maximum()==200);
        REQUIRE(stripeStrength->value()==100);
        stripeStrength->setValue(160);app.processEvents();
        REQUIRE(c.listRowSettings().value("strength").toInt()==160);
        REQUIRE(stripes.settings().value("strength").toInt()==160);
        // During a drag, the canvas previews the color immediately but
        // the persistent setting waits for slider release/valueChanged.
        stripeStrength->sliderMoved(35);app.processEvents();
        REQUIRE(stripes.settings().value("strength").toInt()==35);
        REQUIRE(c.listRowSettings().value("strength").toInt()==160);
        stripeStrength->setValue(175);app.processEvents();
        REQUIRE(stripes.settings().value("strength").toInt()==175);
        REQUIRE(c.listRowSettings().value("strength").toInt()==175);
        auto *resetAlternate=inspector.findChild<QPushButton*>("augment-striped-alternate-theme");
        REQUIRE(resetAlternate);
        REQUIRE(c.setListRowSettings({{"scenes",true},{"sources",true},{"alternate","#304050"}}));
        stripes.configure(c.listRowSettings());
        resetAlternate->click();
        REQUIRE(!c.listRowSettings().contains("alternate")&&stripes.settings()==c.listRowSettings());
        sceneStripes->click();sourceStripes->click();
        REQUIRE(!c.listRowSettings().value("sources").toBool()&&!c.listRowSettings().value("scenes").toBool());
        auto *mixerBack=inspector.findChild<QCheckBox*>("augment-mixer-backside");
        REQUIRE(mixerBack&&!mixerBack->isChecked());
        mixerBack->click();
        REQUIRE(c.mixerBacksideEnabled()&&flips==1);
        mixerBack->click();
        REQUIRE(!c.mixerBacksideEnabled()&&flips==2);
        auto *propertyGroups=inspector.findChild<QCheckBox*>("augment-property-sections");
        REQUIRE(propertyGroups&&!propertyGroups->isChecked());
        propertyGroups->click();
        REQUIRE(c.collapsiblePropertyGroups()&&sections.enabled());
        propertyGroups->click();
        REQUIRE(!c.collapsiblePropertyGroups()&&!sections.enabled());
        auto *compact=inspector.findChild<QCheckBox*>("augment-compact-menus");
        REQUIRE(compact&&compact->isChecked());
        compact->click();
        REQUIRE(!c.compactMenusEnabled());
        REQUIRE(QJsonDocument::fromJson(read()).object().value("extra").toString()=="keep");
        compact->click();
        REQUIRE(c.compactMenusEnabled());
    }
    augment::WidgetSnapshot snap;
    {
        QMenu menu;
        auto *rename=menu.addAction("&Rename...");
        rename->setObjectName("actionRenameSource");
        menu.addAction("Add Source");
        menu.addSeparator();
        menu.addAction("Filters");
        auto *check=menu.addAction("Check");check->setCheckable(true);
        menu.popup(QPoint(50,50));app.processEvents();
        snap=augment::inspectWidget(&menu,menu.mapToGlobal(menu.actionGeometry(rename).center()));
        REQUIRE(snap.isMenu && snap.selected==0 && snap.actions.size()==4);
        REQUIRE(snap.actions.at(0).eligible() && !snap.actions.at(3).eligible());
        REQUIRE(!snap.actions.at(0).nativeIcon); // don't confuse our transparent reservation for a native icon
        menu.hide();
    } // snapshot must survive destruction
    {
        augment::Inspector inspector(c);inspector.capture(snap);
        auto *more=inspector.findChild<QCheckBox*>("augment-overflow");
        REQUIRE(more&&more->isEnabled()&&!more->isChecked());
        const auto chosen=augment::selectorFor(snap,snap.selected,true);
        more->click();
        REQUIRE(c.hasOverflowRule(chosen));
        REQUIRE(c.effectiveRule(QJsonObject{{"text","Rename"}},{}).value("icon").toString()=="pencil");
        more->click();
        REQUIRE(!c.hasOverflowRule(chosen));
        auto *hide=inspector.findChild<QCheckBox*>("augment-hide-entry");
        REQUIRE(hide&&hide->isEnabled()&&!hide->isChecked());
        hide->click();
        REQUIRE(c.hasHiddenRule(chosen));
        REQUIRE(c.allHiddenRules().size()==1);
        auto *add=inspector.findChild<QPushButton*>("augment-add-entry");
        REQUIRE(add&&add->isEnabled());
        bool editorVisible=false;
        QTimer::singleShot(0,&inspector,[&]{
            auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(!dialog)return;
            auto *label=dialog->findChild<QLineEdit*>("augment-custom-label");
            auto *program=dialog->findChild<QLineEdit*>("augment-custom-program");
            auto *args=dialog->findChild<QLineEdit*>("augment-custom-args");
            if(!label||!program||!args)return;
            editorVisible=true;
            label->setText("Open audit log");
            program->setText("/usr/bin/touch");
            args->setText("\"/tmp/audit log\"");
            if(auto *buttons=dialog->findChild<QDialogButtonBox*>())buttons->button(QDialogButtonBox::Save)->click();
        });
        add->click();
        REQUIRE(editorVisible);
        const auto custom=c.customActionsForMenu(snap.menuObjectName,snap.menuLabels);
        REQUIRE(custom.size()==1);
        REQUIRE(custom.at(0).toObject().value("label").toString()=="Open audit log");
        REQUIRE(custom.at(0).toObject().value("args").toArray().size()==1);
        REQUIRE(custom.at(0).toObject().value("args").toArray().at(0).toString()=="/tmp/audit log");
        auto *manage=inspector.findChild<QPushButton*>("augment-manage-entries");
        REQUIRE(manage&&manage->isEnabled());
        int managedCount=0;
        QTimer::singleShot(0,&inspector,[&]{
            auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(!dialog)return;
            auto *list=dialog->findChild<QListWidget*>("augment-managed-entries");
            if(list) {
                managedCount=list->count();
                if(managedCount>0){
                    list->setCurrentRow(0);
                    for(auto *button:dialog->findChildren<QPushButton*>())
                        if(button->text()=="Restore / remove"){button->click();break;}
                }
            }
            dialog->accept();
        });
        manage->click();
        REQUIRE(managedCount==2);
        REQUIRE(!c.hasHiddenRule(chosen));
        REQUIRE(c.removeCustomAction(custom.at(0).toObject().value("id").toString()));
        REQUIRE(c.setHiddenRule(chosen,true));
        augment::Inspector standalone(c);
        auto *all=standalone.findChild<QPushButton*>("augment-manage-entries");
        REQUIRE(all&&all->isEnabled());
        bool globalRecovery=false;
        QTimer::singleShot(0,&standalone,[&]{
            auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(!dialog)return;
            auto *filter=dialog->findChild<QCheckBox*>("augment-show-all-menu-rules");
            auto *list=dialog->findChild<QListWidget*>("augment-managed-entries");
            globalRecovery=filter&&filter->isChecked()&&list&&list->count()==1;
            if(globalRecovery) {
                list->setCurrentRow(0);
                for(auto *button:dialog->findChildren<QPushButton*>())
                    if(button->text()=="Restore / remove"){button->click();break;}
            }
            dialog->accept();
        });
        all->click();
        REQUIRE(globalRecovery&&!c.hasHiddenRule(chosen));
    }
    {
        QMenu root;
        auto *scale=root.addMenu("Scale Filtering");
        scale->setObjectName("scale-options");
        auto *lanczos=scale->addAction("Lanczos");lanczos->setCheckable(true);lanczos->setChecked(true);
        scale->addAction("Bilinear");
        root.addAction("Rename");
        root.popup(QPoint(50,50));app.processEvents();
        const auto nested=augment::inspectWidget(&root,root.mapToGlobal(root.actionGeometry(scale->menuAction()).center()));
        root.hide();app.processEvents();
        augment::Inspector editor(c);
        editor.capture(nested);
        auto *list=editor.findChild<QListWidget*>("augment-actions");
        auto *include=editor.findChild<QCheckBox*>("augment-list-submenus");
        auto *selected=editor.findChild<QCheckBox*>("augment-show-submenu-selections");
        REQUIRE(list&&include&&selected);
        REQUIRE(list->count()==2);
        include->setChecked(true);
        REQUIRE(list->count()==4);
        REQUIRE(list->item(1)->text().contains("Lanczos"));
        list->setCurrentRow(1);
        auto *hint=editor.findChild<QPlainTextEdit*>("augment-selector");
        auto *scope=editor.findChild<QCheckBox*>("augment-scope");
        REQUIRE(hint&&scope);
        REQUIRE(QJsonDocument::fromJson(hint->toPlainText().toUtf8()).object().value("menuObjectName")=="scale-options");
        selected->click();
        REQUIRE(c.showSubmenuSelections());
        REQUIRE(QJsonDocument::fromJson(read()).object().value("extra").toString()=="keep");
        selected->click();
        REQUIRE(!c.showSubmenuSelections());
        include->setChecked(false);
        REQUIRE(list->count()==2);
    }
    auto selector=augment::selectorFor(snap,0,true);
    REQUIRE(selector.value("objectName").toString()=="actionRenameSource");
    REQUIRE(selector.value("menuContains").toArray().size()==2);
    auto target=selector;target["text"]=snap.actions.at(0).text;
    auto rule=selector;rule["icon"]="trash";rule["color"]="#ff00ff";
    QString error;
    REQUIRE(c.saveRule(rule,&error));
    auto json=QJsonDocument::fromJson(read()).object();
    REQUIRE(json.value("extra").toString()=="keep");
    REQUIRE(json.value("rules").toArray().size()==3);
    REQUIRE(c.effectiveRule(target,snap.menuLabels).value("icon").toString()=="trash");
    REQUIRE(c.effectiveRule(target,{"Unrelated"}).value("icon").toString()=="pencil");
    rule["icon"]="gear";REQUIRE(c.saveRule(rule,&error));
    REQUIRE(QJsonDocument::fromJson(read()).object().value("rules").toArray().size()==3);
    QMenu same;
    auto *a=same.addAction("Rename");a->setObjectName("actionRenameSource");
    same.addAction("Add Source");same.addAction("Filters");
    same.popup(QPoint(50,50));app.processEvents();
    REQUIRE(!a->icon().isNull());
    const auto im=same.grab().toImage();
    int magenta=0;for(int y=0;y<im.height();++y)for(int x=0;x<im.width();++x) {
        QColor p=im.pixelColor(x,y); if(p.red()>180&&p.blue()>180&&p.green()<90) ++magenta;
    }
    REQUIRE(magenta>8);
    same.hide();
    REQUIRE(c.removeRule(selector,&error));
    REQUIRE(c.effectiveRule(target,snap.menuLabels).value("icon").toString()=="pencil");
    {
        QMenu scoped;
        auto *targetAction=scoped.addAction("Another");
        scoped.addAction("Rock && Roll");
        scoped.addAction("&Open...");
        scoped.popup(QPoint(50,50));app.processEvents();
        auto context=augment::inspectWidget(&scoped,scoped.mapToGlobal(scoped.actionGeometry(targetAction).center()));
        auto scopedRule=augment::selectorFor(context,0,true);
        REQUIRE(scopedRule.value("menuContains").toArray().size()==2);
        scopedRule["icon"]="plus";
        scopedRule["color"]="#ff00ff";
        REQUIRE(c.saveRule(scopedRule,&error));
        scoped.hide();
        scoped.popup(QPoint(50,50));app.processEvents();
        REQUIRE(!targetAction->icon().isNull()); // literal escaped ampersand anchor still matches
        scoped.hide();
        QMenu other;
        auto *otherAction=other.addAction("Another");
        other.addAction("Rock && Roll");
        other.popup(QPoint(50,50));app.processEvents();
        REQUIRE(otherAction->icon().isNull()); // unrelated menu does not get scoped icon
        other.hide();
    }
    REQUIRE(write("{broken"));
    REQUIRE(!c.saveRule(rule,&error) && read()=="{broken");
    REQUIRE(!c.removeRule(selector,&error) && read()=="{broken");
    REQUIRE(write(R"({"enabled":true,"rules":[]})"));
    {
        QMainWindow main;
        auto *dock=new QDockWidget("Scenes",&main);dock->setWidget(new QWidget(dock));main.addDockWidget(Qt::LeftDockWidgetArea,dock);
        main.resize(800,600);main.show();app.processEvents();
        augment::Panels panels;
        augment::ActionStrip strip(temp.filePath("second-strip.json"));
        dock->setObjectName("test-scenes");
        augment::DockLayouts layouts(temp.filePath("workspace-layouts.json"),&main);
        const auto layoutId=layouts.capture("Editing");REQUIRE(!layoutId.isEmpty());
        augment::CueDeck cues(temp.filePath("workspace-cues.json"),
            [&]{return QList<augment::CueChoice>{{layoutId,"Editing","Layouts"}};},
            [&](const QString &id){return layouts.restore(id);},{},{});
        const auto cueId=cues.addCue("Editing cue");REQUIRE(!cueId.isEmpty());
        REQUIRE(cues.updateCue(cueId,"Editing cue",{{"layout",layoutId,0}}));
        int checkpoints=0;
        augment::Inspector inspector(c,&main,&panels,nullptr,{},nullptr,nullptr,&strip,
            &layouts,&cues,[&](QWidget*){++checkpoints;});

        auto *tabs=inspector.findChild<QTabWidget*>("augment-workspace");
        REQUIRE(tabs&&tabs->count()==7);
        REQUIRE(inspector.windowTitle()=="Aftermarket · Workspace");
        REQUIRE(tabs->tabText(5)=="Dock layouts"&&tabs->tabText(6)=="Cue Deck");
        auto *layoutList=tabs->widget(5)->findChild<QListWidget*>("aftermarket-layout-list");
        REQUIRE(layoutList&&layoutList->count()==1);
        auto *checkpoint=tabs->widget(5)->findChild<QPushButton*>("aftermarket-workspace-checkpoints");
        REQUIRE(checkpoint);checkpoint->click();REQUIRE(checkpoints==1);
        auto *cueList=tabs->widget(6)->findChild<QListWidget*>("aftermarket-cue-list");
        REQUIRE(cueList&&cueList->count()==1);
        auto *runCue=tabs->widget(6)->findChild<QPushButton*>("aftermarket-cue-run");
        REQUIRE(runCue&&runCue->isEnabled());runCue->click();app.processEvents();
        REQUIRE(cues.status().contains("completed",Qt::CaseInsensitive));
        tabs->setCurrentIndex(0);

        auto *stripToggle=inspector.findChild<QCheckBox*>("augment-action-strip-enable");
        auto *stripEditor=inspector.findChild<QPushButton*>("augment-action-strip-edit");
        REQUIRE(stripToggle&&stripEditor&&!stripToggle->isChecked());
        stripToggle->click();REQUIRE(strip.enabled()&&main.menuBar()->cornerWidget());
        stripToggle->click();REQUIRE(!strip.enabled()&&!main.menuBar()->cornerWidget());
        inspector.capture(snap);
        auto *list=inspector.findChild<QListWidget*>("augment-actions");
        auto *icons=inspector.findChild<QComboBox*>("augment-icon");
        auto *color=inspector.findChild<QLineEdit*>("augment-color");
        auto *save=inspector.findChild<QPushButton*>("augment-save");
        REQUIRE(list && icons && color && save && list->currentRow()==0);
        icons->setCurrentText("camera");color->setText("#ff00ff");
        auto *motion=inspector.findChild<QCheckBox*>("augment-motion");
        auto *preset=inspector.findChild<QComboBox*>("augment-motion-preset");
        auto *duration=inspector.findChild<QSpinBox*>("augment-motion-duration");
        REQUIRE(motion && preset && duration);
        motion->setChecked(true); REQUIRE(c.motionEnabled());
        preset->setCurrentText("spin");duration->setValue(260);
        REQUIRE(save->isEnabled());save->click();
        REQUIRE(c.effectiveRule(target,snap.menuLabels).value("icon").toString()=="camera");
        auto animation=c.effectiveRule(target,snap.menuLabels).value("animation").toObject();
        REQUIRE(animation.value("preset").toString()=="spin" && animation.value("duration").toInt()==260);
        motion->setChecked(false);REQUIRE(!c.motionEnabled());
        auto *dialog=&inspector;
        tabs->setCurrentIndex(1);
        auto *sourceList=dialog->findChild<QListWidget*>("augment-knob-source-list");
        auto *allSources=dialog->findChild<QCheckBox*>("augment-knob-all");
        REQUIRE(sourceList&&allSources&&allSources->isChecked());
        auto *pending=dialog->findChild<QLabel*>("augment-settings-state");
        REQUIRE(pending&&pending->text()=="Settings saved");
        auto *filter=dialog->findChild<QLineEdit*>("augment-menu-filter");
        REQUIRE(filter);filter->setText("Copy");REQUIRE(list->item(0)->isHidden());
        filter->clear();REQUIRE(!list->item(0)->isHidden());
        auto *enable=dialog->findChild<QCheckBox*>("augment-gradient-enabled");
        auto *colors=dialog->findChild<QLineEdit*>("augment-gradient-colors");
        auto *period=dialog->findChild<QSpinBox*>("augment-gradient-period");
        auto *apply=dialog->findChild<QPushButton*>("augment-gradient-apply");
        auto *titlebars=dialog->findChild<QCheckBox*>("augment-gradient-titlebars");
        auto *pattern=dialog->findChild<QComboBox*>("augment-gradient-pattern");
        REQUIRE(enable&&colors&&period&&apply&&titlebars&&pattern);
        auto *headerFill=dialog->findChild<QCheckBox*>("augment-gradient-titlebar-fill");
        REQUIRE(headerFill&&!headerFill->isChecked());headerFill->setChecked(true);
        titlebars->setChecked(true);pattern->setCurrentIndex(pattern->findData("bounce"));
        auto *reactive=dialog->findChild<QCheckBox*>("augment-gradient-reactive");
        auto *meters=dialog->findChild<QCheckBox*>("augment-gradient-meters");
        REQUIRE(reactive&&meters);
        auto *knobs=dialog->findChild<QCheckBox*>("augment-gradient-knobs");
        REQUIRE(knobs);knobs->setChecked(true);allSources->setChecked(false);
        auto *sourceName=dialog->findChild<QLineEdit*>("augment-knob-add-name");
        auto *addSource=dialog->findChild<QPushButton*>("augment-knob-add");
        REQUIRE(sourceName&&addSource);
        sourceName->setText("Mic; Music");addSource->click();
        sourceName->setText("Desktop");addSource->click();
        REQUIRE(sourceList->count()==2&&pending->text()=="Unapplied changes");
        auto *knobMeters=dialog->findChild<QCheckBox*>("augment-knob-meters");
        REQUIRE(knobMeters&&knobMeters->isChecked());knobMeters->setChecked(false);
        reactive->setChecked(true);meters->setChecked(true);
        enable->setChecked(true);colors->setText("#31748f, #c4a7e7, #9ccfd8");period->setValue(2300);
        apply->click();
        REQUIRE(c.gradientSettings().value("enabled").toBool());
        REQUIRE(c.gradientSettings().value("colors").toArray().size()==3);
        REQUIRE(c.gradientSettings().value("period").toInt()==2300);
        REQUIRE(c.gradientSettings().value("pattern").toString()=="bounce"&&c.gradientSettings().value("titlebars").toBool());
        REQUIRE(c.gradientSettings().value("titlebarFill").toBool());
        REQUIRE(c.gradientSettings().value("audioReactive").toBool());
        REQUIRE(c.gradientSettings().value("meters").toBool());
        REQUIRE(c.gradientSettings().value("knobs").toBool());
        REQUIRE(c.gradientSettings().contains("knobMeters")&&!c.gradientSettings().value("knobMeters").toBool());
        REQUIRE(c.gradientSettings().value("knobSources").toArray()==QJsonArray({"Desktop","Mic; Music"}));
        REQUIRE(pending->text()=="Settings saved"&&!apply->isEnabled());
        colors->setText("invalid");apply->click();
        REQUIRE(c.gradientSettings().value("colors").toArray().size()==3);
        auto *reload=dialog->findChild<QPushButton*>("augment-settings-reload");REQUIRE(reload);
        reload->click();REQUIRE(colors->text().contains("#31748f"));
        // Stale drafts must not silently overwrite a second editor.
        auto external=c.gradientSettings();external["period"]=3300;REQUIRE(c.setGradientSettings(external));
        period->setValue(1800);apply->click();REQUIRE(c.gradientSettings().value("period").toInt()==3300);
        reload->click();REQUIRE(period->value()==3300);
        for(int i=0;i<sourceList->count();++i)sourceList->item(i)->setCheckState(Qt::Unchecked);
        apply->click();REQUIRE(c.gradientSettings().value("knobSources").toArray().size()==2);
        reload->click();
        tabs->setCurrentIndex(3);
        auto *panelList=dialog->findChild<QListWidget*>("augment-panel-list");
        REQUIRE(panelList&&panelList->count()==1);
        auto *panelToggle=dialog->findChild<QPushButton*>("augment-panel-toggle");
        REQUIRE(panelToggle);panelToggle->click();REQUIRE(panels.collapsed(dock));
        panelToggle->click();REQUIRE(!panels.collapsed(dock));
        tabs->setCurrentIndex(0);
        list->setCurrentRow(3); REQUIRE(!save->isEnabled());
        QWidget widget;widget.setObjectName("something");
        QPushButton child("Sample",&widget);child.setObjectName("target");
        inspector.capture(augment::inspectWidget(&child,QPoint()));
        REQUIRE(!save->isEnabled());
        REQUIRE(augment::inspectWidget(&child,QPoint()).ancestry.contains("something"));
    }
    std::cout << "snapshot lifetime, selector scoping, atomic rule edits, real rendering and inspector controls passed\n";
}
