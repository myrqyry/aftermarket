#pragma once
#include <QString>
class QDialog;
class QPushButton;
namespace augment {
struct SourceDockPinOptions {
    bool preview=false,audio=false,media=false,scene=false;
};
// The installed native manager owns the actual creation. Match the known
// form exactly; never click an arbitrary button or edit an existing row.
QPushButton *prepareSourceDockPin(QDialog *dialog,const QString &source,
    const QString &title,const QString &addLabel,SourceDockPinOptions options);
}
