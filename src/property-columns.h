#pragma once
#include <QObject>
#include <memory>
namespace augment {
// Only changes presentation of native source/filter property rows.
class PropertyColumns final:public QObject {
public:
    explicit PropertyColumns(QObject *parent=nullptr);
    ~PropertyColumns() override;
    void setMode(int mode); // 0 auto, 1 native, 2/3 preferred maximum
    int mode()const;
protected:
    bool eventFilter(QObject*,QEvent*) override;
private:
    struct State;
    std::unique_ptr<State> state;
    void schedule();
    void scan();
    void restoreAll();
};
}
