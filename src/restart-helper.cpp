#include <QCoreApplication>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/types.h>
#include <signal.h>
#include <cerrno>
#include <limits>
#endif
// A separate process waits for the closing OBS instance, never kills it.
// If it cannot establish that OBS exited, it does not launch a duplicate.
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    auto args=app.arguments();args.removeFirst();
    if(args.size()<3)return 2;
    bool ok=false;const auto parent=args.takeFirst().toLongLong(&ok);
    const auto executable=args.takeFirst(),cwd=args.takeFirst();
    if(!ok||parent<=1||parent==app.applicationPid()||!QFileInfo(executable).isAbsolute()||
       !QFileInfo(executable).isExecutable()||!QDir(cwd).exists())return 2;
#ifdef Q_OS_WIN
    if(parent>0xffffffffLL)return 2;
    HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,DWORD(parent));
    if(!process){
        if(GetLastError()!=ERROR_INVALID_PARAMETER)return 3;
    }else{
        const auto result=WaitForSingleObject(process,120000);
        CloseHandle(process);
        if(result!=WAIT_OBJECT_0)return 3;
    }
#else
    if(parent>std::numeric_limits<pid_t>::max())return 2;
    QElapsedTimer timeout;timeout.start();
    for(;;){
        if(kill(pid_t(parent),0)!=0){
            if(errno==ESRCH)break;
            if(errno!=EPERM)return 3;
        }
        if(timeout.elapsed()>=120000)return 3;
        QThread::msleep(100);
    }
#endif
    return QProcess::startDetached(executable,args,cwd)?0:4;
}
