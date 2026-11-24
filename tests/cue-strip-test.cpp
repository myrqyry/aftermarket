#include "action-strip.h"
#include "cue-deck.h"
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QToolButton>
#include <QTemporaryDir>
#include <iostream>
#define REQUIRE(e) do{if(!(e)){std::cerr<<"FAIL "<<__LINE__<<" "<<#e<<"\n";return 1;}}while(false)
int main(int argc,char**argv){
 QApplication app(argc,argv);
 QTemporaryDir temp;REQUIRE(temp.isValid());
 const QString path=temp.filePath("cue-deck.json"),stripFile=temp.filePath("strip.json");
 QList<QString> executed;
 augment::CueDeck cues(path,
     []{return QList<augment::CueChoice>{{"layout-id","Work","Workspace"}};},
     [&](const QString &id){executed<<"layout:"+id;return true;},
     []{return QList<augment::CueChoice>{{"hotkey:v1:move1","Move Source","Exeldro"}};},
     [&](const QString &id){executed<<"key:"+id;return true;});
 const auto id=cues.addCue("Stream intro");REQUIRE(!id.isEmpty());
 REQUIRE(cues.updateCue(id,"Stream intro",{{"layout","layout-id",0},{"hotkey","hotkey:v1:move1",0}}));
 const auto choices=[&]{
     QList<augment::CueChoiceStrip> items;
     for(const auto &c:cues.cues())items.append({c.id,c.name});
     return items;
 };
 auto invoke=[&](const QString &cueId){return cues.run(cueId);};
 QMainWindow main;main.resize(800,400);main.menuBar()->addMenu("File");main.show();
 augment::ActionStrip strip(stripFile,{},{},{},nullptr,{},{},{},choices,invoke);
 REQUIRE(strip.addItem({"cue-button","cue",id,""}));
 REQUIRE(!strip.addItem({"wrong","cue","invalid",""}));
 REQUIRE(strip.setEnabled(main.menuBar(),true));
 auto *button=strip.widget()->findChild<QToolButton*>("aftermarket-strip-cue-button");
 REQUIRE(button&&button->isEnabled());
 REQUIRE(button->text()=="Stream intro");
 button->click();
 REQUIRE(executed==QStringList({"layout:layout-id","key:hotkey:v1:move1"}));
 REQUIRE(cues.updateCue(id,"Stream outro",{{"layout","layout-id",0}}));
 strip.refresh();REQUIRE(button->text()=="Stream outro");
 REQUIRE(cues.removeCue(id));
 strip.refresh();REQUIRE(!button->isEnabled());
 REQUIRE(!strip.trigger("cue-button"));
 strip.setEnabled(main.menuBar(),false);
 augment::ActionStrip reloaded(stripFile,{},{},{},nullptr,{},{},{},choices,invoke);
 REQUIRE(reloaded.items().size()==1);
 REQUIRE(reloaded.items().first().target==id);
 std::cout<<"AFTERMARKET_CUE_ACTION_STRIP_PASS=1\n";
}
