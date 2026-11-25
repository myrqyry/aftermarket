#include "command-center.h"
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QLineEdit>
#include <QListWidget>
#include <QDialog>
#include <QMessageBox>
#include <QAbstractButton>
#include <QLabel>
#include <QKeyEvent>
#include <QTimer>
#include <QEventLoop>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"line "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
static void settle(){QEventLoop loop;QTimer::singleShot(80,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char **argv){
 QApplication app(argc,argv);
 QMainWindow main;main.menuBar()->addMenu("File");main.menuBar()->addMenu("Edit");
 auto *corner=new QLabel("Action strip");corner->setFixedWidth(180);main.menuBar()->setCornerWidget(corner);
 main.resize(1000,500);main.show();settle();
 int searches=0,commands=0,runs=0;QString invoked;
 augment::CommandCenter center(&main,[&]{++searches;return QList<augment::QuickFindEntry>{
  {augment::QuickFindEntry::Source,"mic-id",{},"Microphone","audio"},
  {augment::QuickFindEntry::Filter,"f-id","mic-id","Noise suppression","filter"}
 };},[&](const auto &entry){invoked=entry.id;++runs;return true;},
 [&]{++commands;return QList<augment::CommandChoice>{{"workspace","Workspace","Aftermarket",false},{"record","Start recording","OBS hotkey",true}};},
 [&](const QString &id){invoked=id;++runs;return true;});
 settle();
 auto *field=main.findChild<QLineEdit*>("aftermarket-command-center");CHECK(field&&field->isVisible());
 CHECK(field->geometry().left()>main.menuBar()->actionGeometry(main.menuBar()->actions().last()).right());
 CHECK(field->geometry().right()<corner->geometry().left());
 CHECK(searches==0&&commands==0);
 center.open();settle();
 auto *initialPopup=main.findChild<QDialog*>("aftermarket-command-popup");
 CHECK(initialPopup&&initialPopup->findChild<QListWidget*>()->count()==2&&searches==0&&commands==0);
 initialPopup->reject();
 center.open("@mic");settle();
 auto *popup=main.findChild<QDialog*>("aftermarket-command-popup");
 auto *input=popup?popup->findChild<QLineEdit*>():nullptr;
 auto *list=popup?popup->findChild<QListWidget*>():nullptr;
 CHECK(popup&&popup->isVisible()&&input&&list&&list->count()==1&&searches==1&&runs==0);
 input->setText("@noise");CHECK(list->count()==1&&searches==1);
 QMetaObject::invokeMethod(input,"returnPressed");settle();
 CHECK(invoked=="f-id"&&runs==1&&!popup->isVisible());
 center.open("/workspace");settle();CHECK(list->count()==1&&commands==1);
 QMetaObject::invokeMethod(input,"returnPressed");settle();CHECK(invoked=="workspace"&&runs==2);
 center.open("/record");settle();CHECK(list->count()==1);
 QMetaObject::invokeMethod(input,"returnPressed");settle();CHECK(runs==2);
 auto *confirm=main.findChild<QMessageBox*>("aftermarket-command-confirm");CHECK(confirm&&confirm->isVisible());
 confirm->button(QMessageBox::No)->click();settle();CHECK(runs==2);
 center.open("/record");settle();QMetaObject::invokeMethod(input,"returnPressed");settle();
 confirm=main.findChild<QMessageBox*>("aftermarket-command-confirm");CHECK(confirm);
 confirm->button(QMessageBox::Yes)->click();settle();CHECK(runs==3&&invoked=="record");
 center.open("/missing");settle();CHECK(list->count()==0);
 QMetaObject::invokeMethod(input,"returnPressed");settle();CHECK(runs==3);
 popup->reject();main.resize(300,500);settle();CHECK(!field->isVisible());
 center.open("/workspace");settle();CHECK(popup->isVisible()&&list->count()==1);popup->reject();
 main.resize(1000,500);settle();CHECK(field->isVisible());
 center.detach();CHECK(!main.findChild<QLineEdit*>("aftermarket-command-center")&&main.menuBar()->cornerWidget()==corner);
 return 0;
}
