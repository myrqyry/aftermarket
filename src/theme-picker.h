#pragma once
class QObject;
class QEvent;
namespace augment {
// Only the OBS Appearance theme and variant combos, with long flat models.
bool handleThemePicker(QObject *object,QEvent *event,bool enabled);
void closeThemePickers();
}
