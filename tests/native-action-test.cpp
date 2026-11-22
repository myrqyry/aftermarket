#include "native-action-bridge.h"
#include <QApplication>
#include <iostream>
#define REQUIRE(v) do{if(!(v)){std::cerr<<"FAILED "<<__LINE__<<": "<<#v<<"\n";return 1;}}while(false)
int main(int argc,char **argv){
 QApplication app(argc,argv);
 const auto src=augment::NativeActionBridge::binding("source","uuid-A","libobs.mute");
 const auto src2=augment::NativeActionBridge::binding("source","uuid-B","libobs.mute");
 const auto frontend=augment::NativeActionBridge::binding("frontend","","toggle_streaming");
 REQUIRE(src!=src2&&src.startsWith("hotkey:v1:"));
 REQUIRE(!augment::NativeActionBridge::binding("source","","libobs.mute").size());
 REQUIRE(!augment::NativeActionBridge::binding("frontend","","").size());
 const QList<augment::RegisteredHotkey> observed{
   {11,src},{22,src2},{99,frontend}
 };
 REQUIRE(augment::NativeActionBridge::resolveUnique(observed,src)==11);
 REQUIRE(augment::NativeActionBridge::resolveUnique(observed,src2)==22);
 REQUIRE(augment::NativeActionBridge::resolveUnique(observed,frontend)==99);
 REQUIRE(augment::NativeActionBridge::resolveUnique(observed,
     augment::NativeActionBridge::binding("source","missing","libobs.mute"))==-1);
 auto duplicates=observed;duplicates.append({101,src});
 REQUIRE(augment::NativeActionBridge::resolveUnique(duplicates,src)==-1);
 augment::StatusClock clock;
 REQUIRE(clock.seconds(1000)==-1);
 clock.start(1000);
 REQUIRE(clock.seconds(11000)==10);
 clock.pause(14000);
 REQUIRE(clock.seconds(55000)==13);
 clock.resume(55000);
 REQUIRE(clock.seconds(62000)==20);
 clock.stop();
 REQUIRE(clock.seconds(70000)==-1);
 std::cout<<"native hotkey identity and output elapsed clock verified\n";
 return 0;
}
