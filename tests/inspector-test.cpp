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
#include <QLineEdit>
#include <QListWidget>
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
        augment::Inspector inspector(c);
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
        auto *gradient=inspector.findChild<QPushButton*>("augment-gradient-settings");
        REQUIRE(gradient);gradient->click();
        auto *dialog=inspector.findChild<QDialog*>("obs-ui-augment-gradient-settings");
        REQUIRE(dialog);
        auto *enable=dialog->findChild<QCheckBox*>("augment-gradient-enabled");
        auto *colors=dialog->findChild<QLineEdit*>("augment-gradient-colors");
        auto *period=dialog->findChild<QSpinBox*>("augment-gradient-period");
        auto *apply=dialog->findChild<QPushButton*>("augment-gradient-apply");
        REQUIRE(enable&&colors&&period&&apply);
        auto *reactive=dialog->findChild<QCheckBox*>("augment-gradient-reactive");
        auto *meters=dialog->findChild<QCheckBox*>("augment-gradient-meters");
        REQUIRE(reactive&&meters);
        auto *knobs=dialog->findChild<QCheckBox*>("augment-gradient-knobs");
        auto *knobSources=dialog->findChild<QLineEdit*>("augment-knob-sources");
        REQUIRE(knobs&&knobSources);knobs->setChecked(true);knobSources->setText("Mic; Music");
        auto *knobMeters=dialog->findChild<QCheckBox*>("augment-knob-meters");
        REQUIRE(knobMeters&&knobMeters->isChecked());knobMeters->setChecked(false);
        reactive->setChecked(true);meters->setChecked(true);
        enable->setChecked(true);colors->setText("#31748f, #c4a7e7, #9ccfd8");period->setValue(2300);
        apply->click();
        REQUIRE(c.gradientSettings().value("enabled").toBool());
        REQUIRE(c.gradientSettings().value("colors").toArray().size()==3);
        REQUIRE(c.gradientSettings().value("period").toInt()==2300);
        REQUIRE(c.gradientSettings().value("audioReactive").toBool());
        REQUIRE(c.gradientSettings().value("meters").toBool());
        REQUIRE(c.gradientSettings().value("knobs").toBool());
        REQUIRE(c.gradientSettings().contains("knobMeters")&&!c.gradientSettings().value("knobMeters").toBool());
        REQUIRE(c.gradientSettings().value("knobSources").toArray()==QJsonArray({"Mic","Music"}));
        colors->setText("invalid");apply->click();
        REQUIRE(c.gradientSettings().value("colors").toArray().size()==3);
        dialog->close();
        list->setCurrentRow(3); REQUIRE(!save->isEnabled());
        QWidget widget;widget.setObjectName("something");
        QPushButton child("Sample",&widget);child.setObjectName("target");
        inspector.capture(augment::inspectWidget(&child,QPoint()));
        REQUIRE(!save->isEnabled());
        REQUIRE(augment::inspectWidget(&child,QPoint()).ancestry.contains("something"));
    }
    std::cout << "snapshot lifetime, selector scoping, atomic rule edits, real rendering and inspector controls passed\n";
}
