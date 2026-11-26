#pragma once
#include <QWidget>
#include <memory>
class QMenu;
namespace augment {
class ObsMixerLoudness final : public QWidget {
public:
    explicit ObsMixerLoudness(const QString &settingsFile,QWidget *parent=nullptr);
    ~ObsMixerLoudness() override;
    void populateMenu(QMenu *menu);
protected:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;
private:
    struct State;
    std::unique_ptr<State> s_;
    void restart();
    void refresh();
};
}
