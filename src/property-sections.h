#pragma once
#include <QObject>
#include <memory>
class QGroupBox;
namespace augment {
// Optional presentation adapter for OBS's own property-group QFormLayouts.
// Does not replace controls or alter their values, update callbacks or bindings.
class PropertySections final : public QObject {
public:
    explicit PropertySections(QObject *parent=nullptr);
    ~PropertySections() override;
    void setEnabled(bool enabled);
    bool enabled() const;
    bool collapsed(const QGroupBox *group) const;
protected:
    bool eventFilter(QObject *,QEvent *) override;
private:
    struct State;
    std::unique_ptr<State> state;
    void attach(QGroupBox *);
    void restore(QGroupBox *);
    void positionButton(QGroupBox *);
};
}