#pragma once
#include <QObject>
#include <QJsonObject>
#include <memory>
class QAbstractItemView;
namespace augment {
// Reversible palette-role augmentation of the OBS Scenes and Sources list views.
// The OBS models, source-row widgets, selections and stylesheets remain native.
class RowStripes final : public QObject {
public:
    explicit RowStripes(QObject *parent=nullptr);
    ~RowStripes() override;
    void configure(const QJsonObject &settings);
    QJsonObject settings() const;
protected:
    bool eventFilter(QObject *object,QEvent *event) override;
private:
    struct State;
    std::unique_ptr<State> state;
    void refresh();
    void updateView(QAbstractItemView *view);
    void restore(QAbstractItemView *view);
};
}
