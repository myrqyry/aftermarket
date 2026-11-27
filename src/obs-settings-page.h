#pragma once
#include <QObject>
#include <QPointer>
#include <QHash>
#include <QJsonObject>
#include <functional>
#include <memory>
class QDialog;
class QWidget;
class QEvent;
namespace augment {
class Controller;
class DockTitleIcons;
// Attaches a plugin-owned settings page to an existing OBSBasicSettings
// sidebar+page stack. The native OBS dialog and all its original pages stay.
class ObsSettingsPage final : public QObject {
public:
    struct Services {
        std::function<bool(bool)> setMixer;
        std::function<bool(bool)> setHamburger;
        std::function<bool(bool)> setActionStrip;
        std::function<bool()> actionStripEnabled;
        std::function<void(QWidget*)> editActionStrip;
        std::function<void(bool)> setPropertySections;
        std::function<void(int)> setPropertyColumns;
        std::function<void(const QJsonObject&)> setRowStripes;
        std::function<bool()> compactSourcePickerEnabled;
        std::function<bool(bool)> setCompactSourcePicker;
        std::function<bool()> healthStatsEnabled;
        std::function<bool(bool)> setHealthStats;
        std::function<int()> healthPlacement;
        std::function<bool(int)> setHealthPlacement;
    };
    explicit ObsSettingsPage(Controller &controller,DockTitleIcons &titles,
                             Services services={},QObject *parent=nullptr);
    ~ObsSettingsPage() override;
    // Explicit scan also supports recreation after OBS closes its dialog.
    void scan();
private:
    struct Entry;
    bool eventFilter(QObject *,QEvent *) override;
    void scheduleScan();
    void attach(QDialog *);
    bool commit(const std::shared_ptr<Entry> &);
    void detach();
    QPointer<Controller> controller_;
    QPointer<DockTitleIcons> titles_;
    Services services_;
    QHash<QDialog*,std::shared_ptr<Entry>> entries_;
    bool scheduled_=false;
};
}
