#include "source-picker.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QDialog>
#include <QListWidget>
#include <QScrollArea>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTimer>
#include <QTemporaryDir>
#include <QFile>
#include <QDebug>
#include <QLabel>
#include <QToolButton>
#include <iostream>
#define CHECK(expr) do {if(!(expr)){std::cerr<<"FAILED "<<__LINE__<<": "<<#expr<<"\n";return 1;}}while(false)
static void fill(QDialog &dlg,bool complete){
  dlg.setObjectName("OBSBasicSourceSelect");
  auto *layout=new QVBoxLayout(&dlg);
  auto *list=new QListWidget(&dlg);
  list->setObjectName("sourceTypeList");
  list->setMinimumHeight(460);
  list->addItems({"Browser","Image","Media Source","Video Capture Device (V4L2)"});
  layout->addWidget(list);
  if(complete){
    auto *scroll=new QScrollArea(&dlg);
    scroll->setObjectName("existingScrollArea");
    scroll->setWidget(new QWidget);
    layout->addWidget(scroll);
    auto *visible=new QCheckBox("Make source visible",&dlg);
    visible->setObjectName("sourceVisible");
    layout->addWidget(visible);
    auto *add=new QPushButton("Add existing",&dlg);
    add->setObjectName("addExistingButton");
    layout->addWidget(add);
    QObject::connect(add,&QPushButton::clicked,&dlg,&QDialog::accept);
  }
}
int main(int argc,char **argv){
 QApplication app(argc,argv);
 QTemporaryDir temp;
 QMainWindow main;
 main.resize(1200,820);
 main.setCentralWidget(new QLabel("Native OBS main content"));
 auto *sources=new QDockWidget("Sources",&main);
 sources->setObjectName("sourcesDock");
 sources->setWidget(new QListWidget);
 main.addDockWidget(Qt::LeftDockWidgetArea,sources);
 main.show();app.processEvents();
 augment::SourcePicker picker(&main,temp.filePath("source-picker.ini"));
 CHECK(picker.enabled());
 {
  QDialog dialog(&main);fill(dialog,true);
  dialog.setWindowTitle("Add Source");
  bool inspected=false,valid=true;
  #define CHECK_CB(expr) do {if(!(expr)){std::cerr<<"FAILED CALLBACK "<<__LINE__<<": "<<#expr<<"\n";valid=false;}}while(false)
  QTimer::singleShot(30,[&]{
    inspected=true;
    auto *list=dialog.findChild<QListWidget*>("sourceTypeList");
    CHECK_CB(list->minimumHeight()<=280);
    CHECK_CB(dialog.parentWidget()==&main);
    CHECK_CB(!dialog.isWindow());
    CHECK_CB(dialog.width()<=820 && dialog.height()<=560);
    CHECK_CB(main.rect().contains(dialog.geometry()));
    CHECK_CB(dialog.isVisible());
    CHECK_CB(dialog.isModal());
    CHECK_CB(list->item(3)->toolTip()=="Video Capture Device (V4L2)");
    auto *heading=dialog.findChild<QWidget*>("aftermarket-source-picker-heading");
    auto *close=dialog.findChild<QToolButton*>("aftermarket-source-picker-close");
    CHECK_CB(heading&&close&&close->isVisible());
    CHECK_CB(close->accessibleName()=="Close Add Source");
    dialog.findChild<QPushButton*>("addExistingButton")->click();
  });
  QTimer::singleShot(900,&dialog,&QDialog::reject);
  CHECK(dialog.exec()==QDialog::Accepted);
  CHECK(inspected&&valid);
 }
 CHECK(picker.setEnabled(false));
 CHECK(!picker.enabled());
 {
  QDialog dialog(&main);fill(dialog,true);
  bool inspected=false,valid=true;
  QTimer::singleShot(25,[&]{
    inspected=true;
    CHECK_CB(dialog.isWindow());
    CHECK_CB(dialog.findChild<QListWidget*>("sourceTypeList")->minimumHeight()==460);
    dialog.reject();
  });
  CHECK(dialog.exec()==QDialog::Rejected);
  CHECK(inspected&&valid);
 }
 CHECK(picker.setEnabled(true));
 {
  // Setting survives a new hook instance and is opt-in reversible.
  augment::SourcePicker persisted(&main,temp.filePath("source-picker.ini"));
  CHECK(persisted.enabled());
  CHECK(persisted.setEnabled(false));
 }
 {
  augment::SourcePicker persisted(&main,temp.filePath("source-picker.ini"));
  CHECK(!persisted.enabled());
  CHECK(persisted.setEnabled(true));
 }
 {
  QDialog other(&main);fill(other,false);
  QTimer::singleShot(25,&other,&QDialog::reject);
  CHECK(other.exec()==QDialog::Rejected);
  CHECK(other.isWindow()); // incomplete or changed native picker must be left alone
 }
 std::cout<<"Native Add Source embedded lifecycle verified\n";
 return 0;
}
