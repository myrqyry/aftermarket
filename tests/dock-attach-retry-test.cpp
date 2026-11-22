#include "dock-attach-retry.h"
#include <QApplication>
#include <QTimer>
#include <iostream>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(int argc,char **argv){
 QApplication app(argc,argv);
 int called=0;
 bool exists=false;
 augment::DockAttachRetry retry([&]{++called;return exists;},4,10000);
 REQUIRE(!retry.start()&&retry.pending()&&retry.attempts()==1);
 REQUIRE(!retry.attemptNow()&&retry.pending()&&retry.attempts()==2);
 exists=true;
 REQUIRE(retry.attemptNow());
 REQUIRE(!retry.pending()&&retry.attempts()==3);
 REQUIRE(!retry.attemptNow()&&called==3);
 exists=false;
 REQUIRE(!retry.start()&&retry.pending());
 retry.cancel();REQUIRE(!retry.pending());
 REQUIRE(!retry.attemptNow());
 augment::DockAttachRetry exhausted([&]{++called;return false;},2,10000);
 REQUIRE(!exhausted.start()&&exhausted.pending());
 REQUIRE(!exhausted.attemptNow()&&!exhausted.pending());
 REQUIRE(exhausted.attempts()==2);
 std::cout<<"late dock retry lifecycle verified\n";
 return 0;
}
