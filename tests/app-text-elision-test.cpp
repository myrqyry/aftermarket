#include "app-text-elision.h"
#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QTreeView>
#include <QListView>
#include <QTabWidget>
#include <QTabBar>
#include <QVBoxLayout>
#include <QStringListModel>
#include <QEventLoop>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)

int main(int argc,char **argv){
 QApplication app(argc,argv);
 QWidget window;
 auto *layout=new QVBoxLayout(&window);
 auto *label=new QLabel("The Longest Notable Audio Source Identifier In An OBS Panel",&window);
 label->setObjectName("panel-label");
 label->setTextFormat(Qt::PlainText);
 label->setFixedWidth(128);
 layout->addWidget(label);
 auto *wrapped=new QLabel("A wrapped multi-line help paragraph is not an ellipsis target.",&window);
 wrapped->setWordWrap(true);layout->addWidget(wrapped);
 auto *view=new QListView(&window);
 QStringListModel model({"Extremely Long Source Name In An OBS Project"},view);
 view->setModel(&model);
 view->setTextElideMode(Qt::ElideRight);
 layout->addWidget(view);
 auto *tabs=new QTabWidget(&window);
 tabs->addTab(new QLabel("First",&window),"Extremely Long Output Monitoring Controls");
 tabs->addTab(new QLabel("Second",&window),"Second Tab");
 tabs->tabBar()->setElideMode(Qt::ElideRight);
 layout->addWidget(tabs);
 auto *action=new QPushButton("Output Device Settings",&window);
 action->setObjectName("source-control");layout->addWidget(action);
 auto *combo=new QComboBox(&window);
 combo->setFixedWidth(120);
 combo->addItems({"Output Monitoring For A Very Long Source Name", "Another choice"});
 layout->addWidget(combo);
 auto *button=new QPushButton("A Very Long Button Label Describing An Action",&window);
 button->setFixedWidth(125);layout->addWidget(button);
 auto *check=new QCheckBox("A Very Long Setting Label On A Checkbox",&window);
 check->setFixedWidth(125);check->setChecked(true);layout->addWidget(check);
 window.resize(250,440);window.show();app.processEvents();
 const QString full=label->text();
 const QString buttonText=action->text();
 const QImage comboBefore=combo->grab().toImage();
 const QImage buttonBefore=button->grab().toImage();
 const QImage checkBefore=check->grab().toImage();
 {
  augment::AppTextElision global(&window);
  REQUIRE(global.mode()==Qt::ElideRight);
  REQUIRE(augment::AppTextElision::currentCustomMode()==Qt::ElideRight);
  REQUIRE(view->textElideMode()==Qt::ElideRight);
  REQUIRE(tabs->tabBar()->elideMode()==Qt::ElideRight);
  REQUIRE(label->text()==full); // default must leave OBS's existing UI alone
  REQUIRE(global.setMode(Qt::ElideMiddle));
  REQUIRE(augment::AppTextElision::currentCustomMode()==Qt::ElideMiddle);
  app.processEvents();
  REQUIRE(view->textElideMode()==Qt::ElideMiddle);
  REQUIRE(tabs->tabBar()->elideMode()==Qt::ElideMiddle);
  REQUIRE(label->text().startsWith("The"));
  REQUIRE(label->text().contains(QChar(0x2026)));
  REQUIRE(label->text().endsWith("Panel"));
  REQUIRE(wrapped->text()=="A wrapped multi-line help paragraph is not an ellipsis target.");
  REQUIRE(action->text()==buttonText); // native OBS source/button bindings untouched
  REQUIRE(global.visibleText(combo->currentText(),combo->fontMetrics(),100).contains(QChar(0x2026)));
  REQUIRE(global.visibleText(combo->currentText(),combo->fontMetrics(),100).endsWith("Name"));
  REQUIRE(combo->currentText()=="Output Monitoring For A Very Long Source Name");
  REQUIRE(button->text()=="A Very Long Button Label Describing An Action");
  REQUIRE(check->text()=="A Very Long Setting Label On A Checkbox");
  REQUIRE(check->isChecked());
  REQUIRE(combo->grab().toImage()!=comboBefore); // native paint result changes
  REQUIRE(button->grab().toImage()!=buttonBefore);
  REQUIRE(check->grab().toImage()!=checkBefore);
  REQUIRE(view->model()->index(0,0).data().toString()=="Extremely Long Source Name In An OBS Project");
  REQUIRE(tabs->tabText(0)=="Extremely Long Output Monitoring Controls");
  REQUIRE(global.originalLabel(label)==full);
  // New widgets and model views added after setup inherit the active choice.
  auto *late=new QListView(&window);
  late->setObjectName("late-item-view");layout->addWidget(late);
  late->show();app.processEvents();
  REQUIRE(late->textElideMode()==Qt::ElideMiddle);
  // New external label text must remain the authority; it must not be lost
  // because our earlier visible text had been shortened.
  label->setText("A New Very Long Selected Source With An Ending");
  app.processEvents();
  REQUIRE(global.originalLabel(label).endsWith("Ending"));
  REQUIRE(label->text().endsWith("Ending"));
  REQUIRE(global.setMode(Qt::ElideRight));
  REQUIRE(augment::AppTextElision::currentCustomMode()==Qt::ElideRight);
  app.processEvents();
  REQUIRE(augment::AppTextElision::currentCustomMode()==Qt::ElideRight);
 REQUIRE(label->text()=="A New Very Long Selected Source With An Ending");
  REQUIRE(view->textElideMode()==Qt::ElideRight);
  REQUIRE(tabs->tabBar()->elideMode()==Qt::ElideRight);
  // Saving/restoring is separate; this class only applies in-memory preference.
  REQUIRE(global.setMode(Qt::ElideMiddle));
  app.processEvents();
 }
 app.processEvents();
 REQUIRE(augment::AppTextElision::currentCustomMode()==Qt::ElideRight);
 REQUIRE(label->text()=="A New Very Long Selected Source With An Ending");
 REQUIRE(view->textElideMode()==Qt::ElideRight);
 REQUIRE(tabs->tabBar()->elideMode()==Qt::ElideRight);
 REQUIRE(action->text()==buttonText);
 std::cout<<"App-wide native item views, tabs and safe display labels restored\n";
 return 0;
}
