#include "property-sections.h"
#include <QApplication>
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <iostream>

// Reproduce OBS's meta-object identity without linking its private frontend classes.
class OBSPropertiesView : public QScrollArea {
    Q_OBJECT
public:
    using QScrollArea::QScrollArea;
};
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(false)

int main(int argc,char **argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");
    OBSPropertiesView view;
    view.setWidgetResizable(true);
    auto *container=new QWidget;
    container->setObjectName("PropertiesContainer");
    auto *root=new QVBoxLayout(container);
    auto *group=new QGroupBox("Color Correction",container);
    auto *form=new QFormLayout(group);
    auto *exposure=new QSpinBox(group);exposure->setValue(42);
    auto *contrast=new QCheckBox("Enabled",group);contrast->setChecked(true);
    auto *hidden=new QLineEdit("Hidden original",group);
    form->addRow("Exposure",exposure);
    form->addRow(contrast);
    form->addRow("Special",hidden);
    form->setRowVisible(2,false);
    root->addWidget(group);
    auto *nativeCheckable=new QGroupBox("Filter enabled",container);
    nativeCheckable->setCheckable(true);
    nativeCheckable->setChecked(false);
    auto *nativeForm=new QFormLayout(nativeCheckable);
    nativeForm->addRow("Native",new QSpinBox(nativeCheckable));
    root->addWidget(nativeCheckable);
    view.setWidget(container);view.resize(500,400);view.show();app.processEvents();

    QGroupBox outside("Unrelated");outside.setLayout(new QFormLayout);
    outside.layout()->addWidget(new QSpinBox(&outside));outside.show();app.processEvents();
    augment::PropertySections sections;
    REQUIRE(!group->findChild<QToolButton*>("aftermarket-group-toggle"));
    sections.setEnabled(true);app.processEvents();
    auto *toggle=group->findChild<QToolButton*>("aftermarket-group-toggle");
    REQUIRE(toggle&&toggle->isVisible());
    REQUIRE(!outside.findChild<QToolButton*>("aftermarket-group-toggle"));
    REQUIRE(!nativeCheckable->findChild<QToolButton*>("aftermarket-group-toggle"));
    toggle->click();app.processEvents();
    REQUIRE(sections.collapsed(group));
    REQUIRE(!form->isRowVisible(0)&&!form->isRowVisible(1)&&!form->isRowVisible(2));
    REQUIRE(exposure->value()==42&&contrast->isChecked()&&!nativeCheckable->isChecked());
    toggle->click();app.processEvents();
    REQUIRE(!sections.collapsed(group));
    REQUIRE(form->isRowVisible(0)&&form->isRowVisible(1)&&!form->isRowVisible(2));
    REQUIRE(exposure->value()==42&&contrast->isChecked());

    auto *group2=new QGroupBox("New group",container);
    auto *newForm=new QFormLayout(group2);newForm->addRow("Name",new QLineEdit("Original",group2));
    root->addWidget(group2);group2->show();app.processEvents();
    REQUIRE(group2->findChild<QToolButton*>("aftermarket-group-toggle"));
    toggle->click();app.processEvents();
    REQUIRE(sections.collapsed(group));
    sections.setEnabled(false);app.processEvents();
    REQUIRE(!sections.collapsed(group));
    REQUIRE(!group->findChild<QToolButton*>("aftermarket-group-toggle"));
    REQUIRE(!group2->findChild<QToolButton*>("aftermarket-group-toggle"));
    REQUIRE(form->isRowVisible(0)&&form->isRowVisible(1)&&!form->isRowVisible(2));
    sections.setEnabled(true);app.processEvents();
    REQUIRE(group->findChild<QToolButton*>("aftermarket-group-toggle"));
    // Dropping native containers while enabled must not leave dangling group entries.
    view.takeWidget()->deleteLater();app.processEvents();app.sendPostedEvents(nullptr,QEvent::DeferredDelete);
    sections.setEnabled(false);
    std::cout<<"property group collapsing verified\n";
    return 0;
}
#include "property-sections-test.moc"
