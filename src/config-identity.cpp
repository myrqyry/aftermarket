#include "config-identity.h"
#include <QDir>
#include <QFileInfo>

namespace augment {
bool prepareConfigIdentity(const QString &folder,QString *error){
    if(error)error->clear();
    const auto fail=[error](const QString &message){if(error)*error=message;return false;};
    const QFileInfo dest(folder);
    if(dest.fileName()!="aftermarket")
        return fail("Unexpected Aftermarket config directory: "+folder);
    const QString legacy=dest.dir().filePath("obs-ui-augment");
    const QFileInfo old(legacy);
    if(old.isSymLink()||dest.isSymLink())
        return fail("Resolve configuration symlinks manually before migration.");
    if(!old.exists()){
        return QDir().mkpath(folder)||fail("Unable to create configuration directory: "+folder);
    }
    if(!old.isDir()||(dest.exists()&&!dest.isDir()))
        return fail("Invalid legacy or current configuration directory.");
    if(dest.exists()){
        if(!QDir(folder).entryList(QDir::AllEntries|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot).isEmpty())
            return fail("Both legacy and current configuration directories contain data. Preserve both and resolve manually.");
        if(!QDir().rmdir(folder))return fail("Cannot remove empty destination directory.");
    }
    if(!QDir().rename(legacy,folder))
        return fail("Unable to move legacy configuration; original preserved: "+legacy);
    return true;
}
}
