#include "property-columns.h"
#include "property-sections.h"
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QApplication>
#include <QDialog>
#include <QScrollArea>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QSlider>
#include <QCheckBox>
#include <QLineEdit>
#include <QGroupBox>
#include <iostream>
class OBSPropertiesView:public QScrollArea{
 Q_OBJECT
 public:using QScrollArea::QScrollArea;
};
class OBSBasicProperties:public QDialog{
 Q_OBJECT
 public:using QDialog::QDialog;
};
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setStyle("Fusion");
 OBSBasicProperties dialog;
 auto *outer=new QVBoxLayout(&dialog);
 auto *view=new OBSPropertiesView(&dialog);outer->addWidget(view);view->setWidgetResizable(true);
 auto *container=new QWidget;container->setObjectName("PropertiesContainer");
 auto *form=new QFormLayout(container);
 QList<QSpinBox*> spins;
 for(int i=0;i<6;++i){
  auto *spin=new QSpinBox;spin->setValue(i+11);spins.append(spin);
  auto *pair=new QHBoxLayout;pair->addWidget(new QSlider(Qt::Horizontal));pair->addWidget(spin);
  form->addRow(QString("Param %1").arg(i),pair);
 }
 auto *path=new QLineEdit("unchanged");form->addRow("File",path);
 auto *hidden=new QSpinBox;form->addRow("Hidden",hidden);form->setRowVisible(7,false);
 view->setWidget(container);
 augment::PropertyColumns columns;
 auto settle=[&]{for(int i=0;i<8;++i)app.processEvents();};
 dialog.resize(470,480);dialog.show();settle();
 REQUIRE(form->rowCount()==8);
 dialog.resize(1050,480);settle();
 REQUIRE(spins[0]->y()==spins[1]->y()&&spins[0]->x()<spins[1]->x());
 REQUIRE(path->y()>spins[5]->y()&&hidden->isHidden());
 REQUIRE(spins[0]->value()==11&&spins[5]->value()==16);
 int changes=0;QObject::connect(spins[0],&QSpinBox::valueChanged,[&]{++changes;});spins[0]->setValue(42);
 REQUIRE(changes==1);
 columns.setMode(1);settle();
 REQUIRE(form->rowCount()==8&&spins[0]->y()<spins[1]->y()&&spins[0]->value()==42);
 REQUIRE(hidden->isHidden());
 columns.setMode(2);settle();REQUIRE(spins[0]->y()==spins[1]->y());
 dialog.resize(450,480);settle();REQUIRE(form->rowCount()==8);
 dialog.resize(1050,480);settle();REQUIRE(spins[0]->y()==spins[1]->y());
 auto *replacement=new QWidget;replacement->setObjectName("PropertiesContainer");
 auto *next=new QFormLayout(replacement);
 auto *a=new QSpinBox;auto *b=new QSpinBox;next->addRow("A",a);next->addRow("B",b);
 view->setWidget(replacement);settle();REQUIRE(a->y()==b->y());
 columns.setMode(1);settle();REQUIRE(next->rowCount()==2&&a->y()<b->y());
 auto *colors=new QWidget;colors->setObjectName("PropertiesContainer");
 auto *colorForm=new QFormLayout(colors);
 QList<QPushButton*> buttons;
 for(int i=0;i<2;++i){
  auto *layout=new QHBoxLayout;
  auto *swatch=new QLabel("#ff0055");swatch->setFrameStyle(QFrame::Sunken|QFrame::Panel);swatch->setStyleSheet("background-color :#ff0055; color: #ffffff;");
  auto *button=new QPushButton("Select color");buttons.append(button);
  layout->addWidget(swatch);layout->addWidget(button);
  colorForm->addRow("Tint",layout);
 }
 auto *firstGroup=new QGroupBox("Group A");
 auto *secondGroup=new QGroupBox("Group B");
 for(auto *group:{firstGroup,secondGroup}){
  auto *native=new QFormLayout(group);native->addRow("Amount",new QSpinBox);
  colorForm->addRow(group);
 }
 augment::PropertySections sections;sections.setEnabled(true);
 view->setWidget(colors);columns.setMode(2);settle();
 REQUIRE(buttons[0]->y()==buttons[1]->y()&&buttons[0]->x()<buttons[1]->x());
 REQUIRE(firstGroup->y()==secondGroup->y()&&firstGroup->x()<secondGroup->x());
 auto *toggle=firstGroup->findChild<QToolButton*>("aftermarket-group-toggle");REQUIRE(toggle);
 toggle->click();settle();REQUIRE(sections.collapsed(firstGroup));
 columns.setMode(1);settle();REQUIRE(colorForm->rowCount()==4&&sections.collapsed(firstGroup));
 REQUIRE(firstGroup->y()<secondGroup->y());
 toggle->click();settle();REQUIRE(!sections.collapsed(firstGroup));

 // A full-width section must get its own responsive layout; use
 // available group width, not the outer viewport for nested sections.
 auto *groupContainer=new QWidget;groupContainer->setObjectName("PropertiesContainer");
 auto *groupOuter=new QFormLayout(groupContainer);
 auto *camera=new QGroupBox("Camera Controls");
 auto *cameraForm=new QFormLayout(camera);
 QList<QSpinBox*> cameraSpins;
 for(int i=0;i<6;++i){
  auto *pair=new QHBoxLayout;pair->addWidget(new QSlider(Qt::Horizontal));
  auto *spin=new QSpinBox;spin->setValue(i+20);cameraSpins.append(spin);pair->addWidget(spin);
  cameraForm->addRow(QString("Camera %1").arg(i),pair);
 }
 auto *secret=new QSpinBox;cameraForm->addRow("Hidden",secret);cameraForm->setRowVisible(6,false);
 groupOuter->addRow(camera);
 view->setWidget(groupContainer);columns.setMode(2);dialog.resize(1000,560);settle();
 REQUIRE(cameraSpins[0]->y()==cameraSpins[1]->y()&&cameraSpins[0]->x()<cameraSpins[1]->x());
 REQUIRE(secret->isHidden());
 auto *cameraToggle=camera->findChild<QToolButton*>("aftermarket-group-toggle");REQUIRE(cameraToggle);
 cameraToggle->click();settle();
 for(auto *spin:cameraSpins)REQUIRE(spin->isHidden());
 columns.setMode(1);settle();
 REQUIRE(cameraForm->rowCount()==7);
 for(auto *spin:cameraSpins)REQUIRE(spin->isHidden());
 cameraToggle->click();settle();
 for(int i=0;i<6;++i)REQUIRE(cameraSpins[i]->isVisible()&&cameraSpins[i]->value()==i+20);
 REQUIRE(secret->isHidden());
 columns.setMode(2);settle();
 REQUIRE(cameraSpins[0]->y()==cameraSpins[1]->y());
 dialog.resize(460,560);settle();
 REQUIRE(cameraForm->rowCount()==7&&cameraSpins[0]->y()<cameraSpins[1]->y());
 REQUIRE(secret->isHidden());
 dialog.resize(1000,560);settle();
 REQUIRE(cameraSpins[0]->y()==cameraSpins[1]->y());
 cameraToggle->click();settle();sections.setEnabled(false);settle();
 for(auto *spin:cameraSpins)REQUIRE(spin->isVisible());
 REQUIRE(secret->isHidden());

 std::cout<<"PROPERTY_COLUMNS_PASS\n";
}
#include "property-columns-test.moc"
