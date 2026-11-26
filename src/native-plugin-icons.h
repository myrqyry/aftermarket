#pragma once
#include <QObject>
#include <QString>
#include <QVariant>
#include <functional>
#include <memory>
namespace augment {
// Only decorate known native rows. Identity resolution is supplied by the
// libobs frontend adapter; never infer a plugin from a translated label.
class NativePluginIcons final : public QObject {
public:
    using SourceType=std::function<QString(const QString &)>;
    using FilterType=std::function<QString(const QVariant &)>;
    NativePluginIcons(SourceType sourceType,FilterType filterType,QObject *parent=nullptr);
    ~NativePluginIcons() override;
    void refresh();
protected:
    bool eventFilter(QObject *,QEvent *) override;
private:
    void schedule();
    struct State;
    std::unique_ptr<State> state;
};
}
