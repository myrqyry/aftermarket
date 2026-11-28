#include "restart-request.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QProcess>
#include <QThread>
#include <QElapsedTimer>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<" "<<#x<<"\n";return 1;}}while(0)
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);
 const auto args=app.arguments();
 if(args.size()==3&&args[1]=="--marker"){QFile f(args[2]);return f.open(QIODevice::WriteOnly)&&f.write("restarted")==9?0:1;}
 if(args.size()==2&&args[1]=="--wait"){QThread::msleep(1800);return 0;}
 using namespace augment;
 CHECK(restartArguments({"--portable","--startstreaming","--profile","old profile","--startrecording","--collection","old collection","--scene","old scene","--startreplaybuffer","--startvirtualcam","--minimize-to-tray","--verbose","--multi"})==QStringList({"--portable","--verbose","--multi"}));
 RestartRequest request;int launched=0;
 auto launch=[&]{++launched;return true;};
 CHECK(!request.exitAccepted(launch)&&launched==0);
 CHECK(!request.request([&]{return false;})&&launched==0);
 CHECK(!request.exitAccepted(launch)&&launched==0);
 CHECK(request.request([&]{if(request.request([]{return true;}))return false;return request.exitAccepted(launch);})&&launched==1);
 CHECK(!request.exitAccepted(launch)&&launched==1);
 CHECK(!request.request([&]{return request.exitAccepted([]{return false;});}));
 CHECK(!request.exitAccepted(launch)&&launched==1);
 QTemporaryDir tmp;CHECK(tmp.isValid());
 const auto marker=tmp.filePath("marker with spaces");
 QProcess parent;parent.start(app.applicationFilePath(),{"--wait"});CHECK(parent.waitForStarted());
 QProcess helper;helper.start(QString::fromUtf8(RESTART_HELPER_PATH),{QString::number(parent.processId()),app.applicationFilePath(),tmp.path(),"--marker",marker});
 CHECK(helper.waitForStarted());QThread::msleep(250);CHECK(!QFile::exists(marker));
 CHECK(parent.waitForFinished(4000));CHECK(helper.waitForFinished(6000)&&helper.exitCode()==0);
 QElapsedTimer clock;clock.start();while(!QFile::exists(marker)&&clock.elapsed()<3000)QThread::msleep(20);
 QFile file(marker);CHECK(file.open(QIODevice::ReadOnly)&&file.readAll()=="restarted");
 return 0;
}
