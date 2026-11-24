#include "cue-deck.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTimer>
#include <QPointer>
#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <iostream>

#define REQUIRE(cond) do { if(!(cond)){std::cerr<<"FAIL "<<__LINE__<<": "<<#cond<<"\n";return 1;} }while(false)
static void waitMs(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char **argv) {
 QApplication app(argc,argv);
 using namespace augment;
 QTemporaryDir temp;REQUIRE(temp.isValid());
 const QString path=temp.filePath("cue-deck.json");
 QList<QString> executed;
 QList<CueChoice> layouts{{"layout-a","Gaming","Workspaces"}};
 QList<CueChoice> hotkeys{{"hotkey:v1:move-A","Start Move Source","Sources · Game"},
                          {"hotkey:v1:studio","Switch Studio Mode","OBS"}};
 bool allow=true;
 auto listLayouts=[&]{return layouts;};
 auto listHotkeys=[&]{return hotkeys;};
 auto runLayout=[&](const QString &target){executed.append("layout:"+target);return allow;};
 auto runHotkey=[&](const QString &target){executed.append("key:"+target);return allow;};
 CueDeck deck(path,listLayouts,runLayout,listHotkeys,runHotkey);
 REQUIRE(deck.cues().isEmpty());
 REQUIRE(deck.addCue("  ") .isEmpty());
 REQUIRE(deck.addCue(QString(140,'x')).isEmpty());
 const auto id=deck.addCue("Going Live");
 REQUIRE(!id.isEmpty());
 REQUIRE(deck.addCue("going live").isEmpty());
 REQUIRE(deck.updateCue(id,"Broken",{{"layout","missing",0}}));
 REQUIRE(!deck.updateCue(id,"Broken",{{"hotkey","",0}}));
 REQUIRE(!deck.updateCue(id,"Broken",{{"wait","",30001}}));
 REQUIRE(!deck.updateCue(id,"Broken",{{"unknown","x",0}}));
 REQUIRE(!deck.updateCue(id,"Broken",{{"wait","x",20}}));
 REQUIRE(deck.updateCue(id,"Go Live",{{"layout","layout-a",0},
                                    {"wait","",20},{"hotkey","hotkey:v1:move-A",0}}));
 REQUIRE(deck.cues().size()==1&&deck.cues()[0].steps.size()==3);
 REQUIRE(deck.save());
 CueDeck restored(path,listLayouts,runLayout,listHotkeys,runHotkey);
 REQUIRE(restored.cues().size()==1);
 REQUIRE(restored.cues()[0].id==id&&restored.cues()[0].name=="Go Live");
 // Preflight prevents partially applying a cue when any step is unavailable.
 layouts.clear();
 REQUIRE(!restored.run(id));
 REQUIRE(executed.isEmpty());
 REQUIRE(restored.status().contains("unavailable",Qt::CaseInsensitive));
 layouts.append({"layout-a","Gaming","Workspaces"});
 hotkeys.clear();
 REQUIRE(!restored.run(id));REQUIRE(executed.isEmpty());
 hotkeys.append({"hotkey:v1:move-A","Start Move Source","Sources · Game"});
 REQUIRE(restored.run(id));
 REQUIRE(restored.running());
 REQUIRE(!restored.run(id));
 waitMs(85);
 REQUIRE(!restored.running());
 REQUIRE(executed==QStringList({"layout:layout-a","key:hotkey:v1:move-A"}));
 REQUIRE(restored.status().contains("dispatched",Qt::CaseInsensitive));
 executed.clear();
 REQUIRE(restored.run(id));
 REQUIRE(executed.size()==1);
 restored.cancel();
 waitMs(85);
 REQUIRE(executed.size()==1&&!restored.running());
 executed.clear();
 REQUIRE(restored.updateCue(id,"Failures",{{"layout","layout-a",0},{"hotkey","hotkey:v1:move-A",0}}));
 allow=false;
 REQUIRE(restored.run(id));
 waitMs(25);
 REQUIRE(executed.size()==1);
 REQUIRE(!restored.running()&&restored.status().contains("failed",Qt::CaseInsensitive));
 allow=true;
 REQUIRE(restored.removeCue(id));
 REQUIRE(!restored.run(id));
 REQUIRE(!restored.removeCue(id));
 auto *manager=restored.createManager();
 REQUIRE(manager);
 REQUIRE(manager->objectName()=="aftermarket-cue-deck-manager");
 REQUIRE(manager->findChild<QListWidget*>("aftermarket-cue-list"));
 REQUIRE(manager->findChild<QPushButton*>("aftermarket-cue-run"));
 delete manager;
 // Invalid disk JSON and oversized files may not replace already-loaded data.
 const auto savedId=restored.addCue("Saved");
 REQUIRE(restored.save());
 QFile file(path);
 REQUIRE(file.open(QIODevice::WriteOnly|QIODevice::Truncate));
 file.write("{ malformed");file.close();
 REQUIRE(!restored.reload());
 REQUIRE(restored.cues().size()==1&&restored.cues()[0].id==savedId);
 std::cout<<"AFTERMARKET_CUE_DECK_MODEL_PERSIST_RUN_PASS=1\n";
}
