#include <QSpinBox>
#include "obs-settings-page.h"
#include "augment.h"
#include "dock-title-icons.h"
#include <QApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QSlider>
#include <QMainWindow>
#include <QDockWidget>
#include <QTemporaryDir>
#include <QFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QGroupBox>
#include <QScrollArea>
#include <QScrollBar>
#include <iostream>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAILED line "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
static QDialog *openSettings(){
    auto *dlg=new QDialog;
    dlg->setObjectName("OBSBasicSettings");
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    auto *row=new QHBoxLayout(dlg);
    auto *list=new QListWidget(dlg);list->setObjectName("listWidget");
    auto *stack=new QStackedWidget(dlg);stack->setObjectName("settingsPages");
    for(auto name:{"General","Appearance","Stream","Output","Audio",
                   "Video","Hotkeys","Accessibility","Advanced"}){
        list->addItem(name);
        stack->addWidget(new QWidget(stack));
    }
    list->setCurrentRow(0);
    QObject::connect(list,&QListWidget::currentRowChanged,stack,&QStackedWidget::setCurrentIndex);
    auto *box=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel|
                                   QDialogButtonBox::Apply,dlg);
    box->setObjectName("buttonBox");
    box->button(QDialogButtonBox::Apply)->setEnabled(false); // native OBS starts with Apply disabled
    QObject::connect(box,&QDialogButtonBox::clicked,dlg,[dlg,box](QAbstractButton *button){
        if(box->buttonRole(button)==QDialogButtonBox::RejectRole)
            dlg->close();
        else if(box->buttonRole(button)==QDialogButtonBox::AcceptRole)
            dlg->close();
    });
    auto *column=new QVBoxLayout;
    column->addWidget(stack);
    column->addWidget(box);
    row->addWidget(list);
    row->addLayout(column);
    dlg->resize(930,670);
    dlg->show();
    QApplication::processEvents();
    return dlg;
}
int main(int argc,char **argv){
    QApplication app(argc,argv);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    auto rules=dir.filePath("rules.json");
    QFile file(rules);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write("{\"enabled\":true,\"rules\":[]}")>0);
    file.close();
    augment::Controller controller(rules);
    QMainWindow main;
    main.setObjectName("OBSBasic");
    main.resize(900,500);
    main.show();
    augment::DockTitleIcons titles(&main,nullptr,dir.filePath("dock-title-settings.ini"));
    int mixerCalls=0,sectionUpdates=0,stripeUpdates=0,stripCalls=0;
    bool stripEnabled=false;
    augment::ObsSettingsPage::Services services;
    services.setMixer=[&](bool){++mixerCalls;return true;};
    services.setActionStrip=[&](bool b){stripEnabled=b;++stripCalls;return true;};
    services.actionStripEnabled=[&]{return stripEnabled;};
    services.setPropertySections=[&](bool){++sectionUpdates;};
    int columnUpdates=0;
    services.setPropertyColumns=[&](int mode){columnUpdates=mode;};
    services.setRowStripes=[&](const QJsonObject &){++stripeUpdates;};
    bool embeddedPickerEnabled=true;
    int pickerUpdates=0;
    services.compactSourcePickerEnabled=[&]{return embeddedPickerEnabled;};
    services.setCompactSourcePicker=[&](bool value){
        embeddedPickerEnabled=value;++pickerUpdates;return true;
    };
    bool healthEnabled=true;int healthUpdates=0,healthPlacement=0;
    services.healthPlacement=[&]{return healthPlacement;};
    services.setHealthPlacement=[&](int value){healthPlacement=value;return true;};
    services.healthStatsEnabled=[&]{return healthEnabled;};
    services.setHealthStats=[&](bool value){healthEnabled=value;++healthUpdates;return true;};
    auto bridge=std::make_unique<augment::ObsSettingsPage>(controller,titles,services);

    {
        auto *dialog=openSettings();
        bridge->scan();
        auto *nav=dialog->findChild<QListWidget*>("listWidget");
        auto *pages=dialog->findChild<QStackedWidget*>("settingsPages");
        REQUIRE(nav&&pages&&nav->count()==10&&pages->count()==10);
        REQUIRE(nav->item(9)->text()=="Aftermarket");
        bridge->scan();
        REQUIRE(nav->count()==10&&pages->count()==10);
        nav->setCurrentRow(9);
        REQUIRE(pages->currentWidget()->objectName()=="aftermarket-native-settings");
        auto *tabs=pages->currentWidget()->findChild<QTabWidget*>("aftermarket-settings-tabs");
        REQUIRE(tabs&&tabs->count()==6);
        REQUIRE(tabs->tabText(2)=="Mixer"&&tabs->tabText(3)=="Gradients");
        auto *gradientPage=tabs->widget(3);
        auto *mixerPage=tabs->widget(2);
        REQUIRE(gradientPage->findChild<QCheckBox*>("aftermarket-setting-titlebar-fill"));
        REQUIRE(!mixerPage->findChild<QCheckBox*>("aftermarket-setting-titlebar-fill"));
        REQUIRE(mixerPage->findChild<QLineEdit*>("aftermarket-setting-knob-sources"));
        auto *preview=gradientPage->findChild<QWidget*>("aftermarket-settings-palette-preview");
        auto *colors=gradientPage->findChild<QLineEdit*>("aftermarket-setting-gradient-colors");
        auto *theme=gradientPage->findChild<QPushButton*>("aftermarket-settings-use-theme");
        REQUIRE(preview&&colors&&theme&&gradientPage->findChild<QPushButton*>("aftermarket-settings-add-color"));
        REQUIRE(gradientPage->findChildren<QGroupBox*>().size()>=2);
        tabs->setCurrentIndex(3);app.processEvents();
        const auto original=preview->grab().toImage();
        colors->setText("#ff0000, #0000ff");app.processEvents();
        REQUIRE(preview->grab().toImage()!=original);
        REQUIRE(preview->property("validPalette").toBool());
        colors->setText("invalid");app.processEvents();
        REQUIRE(!preview->property("validPalette").toBool());
        theme->click();app.processEvents();
        REQUIRE(colors->text().isEmpty()&&preview->property("validPalette").toBool());
        REQUIRE(controller.gradientSettings().isEmpty());
        // Keep the native Apply state check below independent of preview edits.
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->setEnabled(false);
        tabs->setCurrentIndex(0);
        for(int width:{930,780}){
            dialog->resize(width,670);
            for(int i=0;i<tabs->count();++i){
                tabs->setCurrentIndex(i);app.processEvents();
                auto *area=qobject_cast<QScrollArea*>(tabs->currentWidget());
                REQUIRE(area&&area->horizontalScrollBar()->maximum()==0);
            }
        }
        tabs->setCurrentIndex(0);app.processEvents();
        REQUIRE(tabs->tabText(0)=="Appearance");
        auto *ellipsis=pages->currentWidget()->findChild<QComboBox*>("augment-global-ellipsis");
        REQUIRE(ellipsis&&ellipsis->currentData().toString()=="end");
        auto *stripeStrength=dialog->findChild<QSlider*>("aftermarket-setting-stripe-strength");
        REQUIRE(stripeStrength&&stripeStrength->minimum()==0&&stripeStrength->maximum()==200);
        REQUIRE(stripeStrength->value()==100);
        auto *picker=dialog->findChild<QCheckBox*>("aftermarket-setting-source-picker");
        REQUIRE(picker&&picker->isChecked());
        auto*health=dialog->findChild<QCheckBox*>("aftermarket-setting-health-stats");
        REQUIRE(health&&health->isChecked());
        auto*placement=dialog->findChild<QComboBox*>("aftermarket-setting-health-placement");REQUIRE(placement&&placement->currentData().toInt()==0);
        REQUIRE(!dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->isEnabled());
        auto *tooltip=dialog->findChild<QComboBox*>("aftermarket-setting-tooltip-width");
        REQUIRE(tooltip&&tooltip->currentData().toInt()==360);
        tooltip->setCurrentIndex(tooltip->findData(240));
        REQUIRE(controller.tooltipWidth()==360);
        picker->setChecked(false);
        REQUIRE(embeddedPickerEnabled&&pickerUpdates==0);
        REQUIRE(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->isEnabled());
        picker->setChecked(true); // OBS owns Apply; it need not disable on undo
        REQUIRE(embeddedPickerEnabled&&pickerUpdates==0);
        ellipsis->setCurrentIndex(ellipsis->findData("middle"));
        QApplication::processEvents();
        REQUIRE(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->isEnabled());
        REQUIRE(titles.ellipsisMode()==Qt::ElideRight); // native Cancel must truly discard
        auto *mixer=pages->currentWidget()->findChild<QCheckBox*>("aftermarket-setting-mixer-back");
        REQUIRE(mixer);
        mixer->setChecked(true);
        REQUIRE(!controller.mixerBacksideEnabled()&&mixerCalls==0);
        dialog->findChild<QSlider*>("aftermarket-setting-stripe-strength")->setValue(35);
        QApplication::processEvents();
        REQUIRE(controller.listRowSettings().isEmpty());
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel)->click();
        QApplication::processEvents();
        REQUIRE(titles.ellipsisMode()==Qt::ElideRight);
        REQUIRE(!controller.mixerBacksideEnabled()&&mixerCalls==0);
        REQUIRE(controller.propertyColumns()==0&&columnUpdates==0&&controller.tooltipWidth()==360);
    }
    {
        auto *dialog=openSettings();
        bridge->scan();
        auto *nav=dialog->findChild<QListWidget*>("listWidget");
        nav->setCurrentRow(9);
        auto *ellipsis=dialog->findChild<QComboBox*>("augment-global-ellipsis");
        auto *mixer=dialog->findChild<QCheckBox*>("aftermarket-setting-mixer-back");
        REQUIRE(ellipsis&&mixer&&ellipsis->currentData().toString()=="end");
        ellipsis->setCurrentIndex(ellipsis->findData("middle"));
        mixer->setChecked(true);
        dialog->findChild<QCheckBox*>("aftermarket-setting-scene-stripes")->setChecked(true);
        dialog->findChild<QSlider*>("aftermarket-setting-stripe-strength")->setValue(175);
        dialog->findChild<QCheckBox*>("aftermarket-setting-sections")->setChecked(true);
        auto *columns=dialog->findChild<QComboBox*>("aftermarket-setting-property-columns");
        REQUIRE(columns&&columns->currentData().toInt()==0);
        columns->setCurrentIndex(columns->findData(2));
        auto *tooltip=dialog->findChild<QComboBox*>("aftermarket-setting-tooltip-width");
        REQUIRE(tooltip&&tooltip->currentData().toInt()==360);
        tooltip->setCurrentIndex(tooltip->findData(240));
        dialog->findChild<QCheckBox*>("aftermarket-setting-source-picker")->setChecked(false);
        dialog->findChild<QCheckBox*>("aftermarket-setting-health-stats")->setChecked(false);
        dialog->findChild<QComboBox*>("aftermarket-setting-health-placement")->setCurrentIndex(1);REQUIRE(healthPlacement==0);
        dialog->findChild<QCheckBox*>("aftermarket-setting-action-strip")->setChecked(true);
        dialog->findChild<QCheckBox*>("aftermarket-setting-knobs")->setChecked(true);
        auto *titleGrad=dialog->findChild<QCheckBox*>("aftermarket-setting-title-gradients");
        auto *pattern=dialog->findChild<QComboBox*>("aftermarket-setting-gradient-pattern");
        REQUIRE(titleGrad&&pattern&&pattern->currentData().toString()=="flow");
        auto *headerFill=dialog->findChild<QCheckBox*>("aftermarket-setting-titlebar-fill");
        REQUIRE(headerFill&&!headerFill->isChecked());headerFill->setChecked(true);
        titleGrad->setChecked(true);pattern->setCurrentIndex(pattern->findData("nebula"));
        REQUIRE(pattern->currentData().toString()=="nebula");
        auto *period=dialog->findChild<QSpinBox*>("aftermarket-setting-gradient-period");REQUIRE(period&&period->maximum()==60000);period->setValue(24000);
        auto *scale=dialog->findChild<QSpinBox*>("aftermarket-setting-gradient-scale");
        REQUIRE(scale&&scale->value()==100&&scale->minimum()==50&&scale->maximum()==400);scale->setValue(275);
        REQUIRE(!controller.mixerBacksideEnabled()&&titles.ellipsisMode()==Qt::ElideRight);
        REQUIRE(controller.listRowSettings().value("scenes").toBool()==false);
        REQUIRE(!controller.collapsiblePropertyGroups()&&!stripEnabled);
        QApplication::processEvents();
        REQUIRE(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->isEnabled());
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        QApplication::processEvents();
        REQUIRE(controller.mixerBacksideEnabled()&&mixerCalls==1);
        REQUIRE(controller.listRowSettings().value("scenes").toBool()&&stripeUpdates==1);
        REQUIRE(controller.listRowSettings().value("strength").toInt()==175);
        REQUIRE(!embeddedPickerEnabled&&pickerUpdates==1);
        REQUIRE(controller.collapsiblePropertyGroups()&&sectionUpdates==1);
        REQUIRE(controller.propertyColumns()==2&&columnUpdates==2&&controller.tooltipWidth()==240);
        REQUIRE(!controller.setPropertyColumns(4));
        REQUIRE(stripEnabled&&stripCalls==1);REQUIRE(!healthEnabled&&healthUpdates==1&&healthPlacement==1);
        REQUIRE(controller.gradientSettings().value("knobs").toBool());
        REQUIRE(controller.gradientSettings().value("titlebars").toBool());
        REQUIRE(controller.gradientSettings().value("titlebarFill").toBool());
        REQUIRE(controller.gradientSettings().value("pattern").toString()=="nebula"&&controller.gradientSettings().value("period").toInt()==24000&&controller.gradientSettings().value("scale").toInt()==275);
        REQUIRE(titles.ellipsisMode()==Qt::ElideMiddle);
        REQUIRE(QSettings(dir.filePath("dock-title-settings.ini"),QSettings::IniFormat)
                    .value("title/ellipsis").toString()=="middle");
        dialog->close();
        QApplication::processEvents();
    }
    {
        auto *dialog=openSettings();
        bridge->scan();
        auto *nav=dialog->findChild<QListWidget*>("listWidget");
        nav->setCurrentRow(9);
        REQUIRE(dialog->findChild<QComboBox*>("aftermarket-setting-health-placement")->currentData().toInt()==1);
        REQUIRE(dialog->findChild<QComboBox*>("aftermarket-setting-property-columns")->currentData().toInt()==2);
        REQUIRE(dialog->findChild<QComboBox*>("aftermarket-setting-tooltip-width")->currentData().toInt()==240);
        REQUIRE(dialog->findChild<QComboBox*>("augment-global-ellipsis")
                       ->currentData().toString()=="middle");
        REQUIRE(dialog->findChild<QCheckBox*>("aftermarket-setting-mixer-back")->isChecked());
        REQUIRE(dialog->findChild<QSlider*>("aftermarket-setting-stripe-strength")->value()==175);
        REQUIRE(dialog->findChild<QCheckBox*>("aftermarket-setting-knobs")->isChecked());
        REQUIRE(dialog->findChild<QCheckBox*>("aftermarket-setting-title-gradients")->isChecked());
        REQUIRE(dialog->findChild<QComboBox*>("aftermarket-setting-gradient-pattern")->currentData().toString()=="nebula");
        REQUIRE(dialog->findChild<QSpinBox*>("aftermarket-setting-gradient-scale")->value()==275);
        REQUIRE(dialog->findChild<QCheckBox*>("aftermarket-setting-action-strip")->isChecked());
        // Do not shift / override the original OBS native settings categories.
        for(int i=0;i<9;++i)REQUIRE(nav->item(i)->text()!="Aftermarket");
        dialog->close();
    }
    // OK is handled by OBS's own slot before the extension receives its
    // clicked notification. It must still commit staged edits.
    {
        auto *dialog=openSettings();
        bridge->scan();
        dialog->findChild<QListWidget*>("listWidget")->setCurrentRow(9);
        auto *ellipsis=dialog->findChild<QComboBox*>("augment-global-ellipsis");
        REQUIRE(ellipsis&&ellipsis->currentData().toString()=="middle");
        ellipsis->setCurrentIndex(ellipsis->findData("end"));
        QApplication::processEvents();
        REQUIRE(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->isEnabled());
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        QApplication::processEvents();
        REQUIRE(titles.ellipsisMode()==Qt::ElideRight);
        REQUIRE(QSettings(dir.filePath("dock-title-settings.ini"),QSettings::IniFormat)
                    .value("title/ellipsis").toString()=="end");
    }
    {
        auto *dialog=openSettings();
        bridge->scan();
        auto *nav=dialog->findChild<QListWidget*>("listWidget");
        nav->setCurrentRow(9);
        auto *colors=dialog->findChild<QLineEdit*>("aftermarket-setting-gradient-colors");
        REQUIRE(colors);
        colors->setText("not a valid color");
        REQUIRE(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->isEnabled());
        const QJsonObject previousGradient=controller.gradientSettings();
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        QApplication::processEvents();
        REQUIRE(controller.gradientSettings()==previousGradient);
        REQUIRE(dialog->findChild<QLabel*>("aftermarket-settings-status")->text()
                    .contains("Invalid gradient color"));
        colors->setText("#80ff0000, #00ff00");
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        REQUIRE(controller.gradientSettings()==previousGradient);
        REQUIRE(!dialog->findChild<QWidget*>("aftermarket-settings-palette-preview")->property("validPalette").toBool());
        REQUIRE(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->isEnabled());

        dialog->close();QApplication::processEvents();
    }
    {
        auto *dialog=openSettings();
        bridge->scan();
        auto *nav=dialog->findChild<QListWidget*>("listWidget");
        auto *pages=dialog->findChild<QStackedWidget*>("settingsPages");
        REQUIRE(nav->count()==10&&pages->count()==10);
        nav->setCurrentRow(9);
        bridge.reset();QApplication::processEvents();
        REQUIRE(nav->count()==9&&pages->count()==9);
        REQUIRE(nav->currentRow()==0);
        REQUIRE(!dialog->findChild<QWidget*>("aftermarket-native-settings"));
        dialog->close();QApplication::processEvents();
    }
    std::cout<<"OBS native Settings Apply/Cancel/OK, rollback and teardown passed\n";
    return 0;
}
