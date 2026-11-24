#include "action-strip.h"
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QToolButton>
#include <QTemporaryDir>
#include <iostream>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAILED "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(int argc,char **argv){
 QApplication app(argc,argv);QTemporaryDir temp;REQUIRE(temp.isValid());
 QMainWindow main;main.resize(850,300);main.menuBar()->addMenu("File");main.show();
 int opens=0;const QString file=temp.filePath("strip.json");
 augment::ActionStrip strip(file,{},{},{},nullptr,{},{},[&]{++opens;});
 REQUIRE(strip.addItem({"search","search","quick-find",""}));
 REQUIRE(!strip.addItem({"bad","search","arbitrary-command",""}));
 REQUIRE(strip.setEnabled(main.menuBar(),true));
 auto *button=strip.widget()->findChild<QToolButton*>("aftermarket-strip-search");
 REQUIRE(button&&button->text()=="Find sources");
 REQUIRE(button->isEnabled());
 REQUIRE(strip.trigger("search")&&opens==1);
 button->click();REQUIRE(opens==2);
 REQUIRE(strip.setEnabled(main.menuBar(),false));
 augment::ActionStrip restored(file,{},{},{},nullptr,{},{},[&]{++opens;});
 REQUIRE(restored.items().size()==1);
 REQUIRE(restored.trigger("search")&&opens==3);
 augment::ActionStrip unavailable(file);
 REQUIRE(!unavailable.trigger("search"));
 std::cout<<"AFTERMARKET_QUICK_FIND_ACTION_STRIP_PASS=1\n";
}
