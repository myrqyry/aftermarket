#pragma once
#include <QObject>
#include <QString>
#include <memory>
#include <functional>
class QMenu;
namespace augment {
using Logger = std::function<void(const QString &)>;
class Controller final : public QObject {
public:
    explicit Controller(QString rulesFile, QObject *parent = nullptr, Logger logger = {});
    ~Controller() override;
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    struct State;
    std::unique_ptr<State> state;
    void attach(QMenu *);
};
}
