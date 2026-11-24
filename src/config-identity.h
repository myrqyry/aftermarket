#pragma once
#include <QString>
namespace augment {
// Move the complete previous configuration, including unknown files and
// the safe-mode marker, before constructing any persistent UI services.
bool prepareConfigIdentity(const QString &folder,QString *error);
}
