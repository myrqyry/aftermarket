#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QPointer>
#include <QTimer>
#include <functional>

class QDialog;
class QWidget;

namespace augment {
struct CueChoice {
    QString id, name, group;
};
struct CueStep {
    QString type, target;
    int delayMs=0;
};
struct Cue {
    QString id, name;
    QList<CueStep> steps;
};

class CueDeck final : public QObject {
public:
    using Choices=std::function<QList<CueChoice>()>;
    using Dispatch=std::function<bool(const QString&)>;
    CueDeck(QString path,Choices layouts,Dispatch applyLayout,
            Choices hotkeys,Dispatch triggerHotkey,QObject *parent=nullptr);
    ~CueDeck() override;
    QList<Cue> cues() const{return cues_;}
    bool reload();
    bool save() const;
    QString addCue(const QString &name);
    bool updateCue(const QString &id,const QString &name,const QList<CueStep> &steps);
    bool removeCue(const QString &id);
    bool run(const QString &id);
    void cancel();
    bool running() const{return running_;}
    QString status() const{return status_;}
    QWidget *createPage(QWidget *parent);
    void closeManager();
    void refreshPage();
private:
    bool validSteps(const QList<CueStep> &steps) const;
    bool preflight(const Cue &cue);
    void advance();
    void announce(const QString &status);
    void changed();
    bool editCue(QWidget *parent,const QString &id={});
    QString path_;
    Choices layouts_,hotkeys_;
    Dispatch applyLayout_,triggerHotkey_;
    QList<Cue> cues_;
    QTimer timer_;
    QList<CueStep> activeSteps_;
    int cursor_=0, dispatched_=0;
    bool running_=false;
    QString status_="Ready";
    QPointer<QWidget> manager_;
    std::function<void()> refreshManager_;
};
}
