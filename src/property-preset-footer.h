#pragma once
#include <QObject>
#include <memory>
#include <QString>
namespace augment {
class PropertyPresetFooter final:public QObject {
public:
    explicit PropertyPresetFooter(QString storage,QObject *parent=nullptr);
    ~PropertyPresetFooter()override;
protected:
    bool eventFilter(QObject*,QEvent*)override;
private:
    struct State;
    std::unique_ptr<State> state;
    void schedule();
    void scan();
};
}
