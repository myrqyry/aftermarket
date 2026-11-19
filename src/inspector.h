#pragma once
#include <QDialog>
#include <QJsonObject>
#include <QPoint>
#include <QStringList>
#include <QVector>
#include <memory>
class QWidget;
class QAction;
namespace augment {
class Controller;
class Panels;
struct ActionSnapshot {
    QString text, objectName;
    bool checkable=false, custom=false, visible=false, enabled=false, nativeIcon=false;
    bool eligible() const;
};
struct WidgetSnapshot {
    QString type, objectName, ancestry, geometry, accessibility;
    QString menuObjectName, mixerSource;
    QStringList menuLabels;
    QVector<ActionSnapshot> actions;
    int selected=-1;
    bool isMenu=false;
    bool valid() const {return !type.isEmpty();}
};
WidgetSnapshot inspectWidget(QWidget *widget, const QPoint &globalPosition);
WidgetSnapshot inspectAt(const QPoint &globalPosition);
QJsonObject selectorFor(const WidgetSnapshot &, int row, bool scopeMenu);
class Inspector final : public QDialog {
public:
    explicit Inspector(Controller &controller, QWidget *parent=nullptr, Panels *panels=nullptr);
    ~Inspector() override;
    void openPanels();
    void capture(const WidgetSnapshot &snapshot);
protected:
    void reject() override;
private:
    struct Ui;
    std::unique_ptr<Ui> ui;
    void startPick();
    void selectRow(int row);
    void updatePreview();
    void save();
};
}
