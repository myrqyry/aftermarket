#pragma once
#include <QObject>
#include <QList>
#include <QPointer>
#include <QString>
#include <memory>
#include <functional>
class QWidget;
class QTimer;
namespace augment {
struct AudioPropertyState {
    QString id,name;
    double balance=0.5; // native libobs balance [0,1], 0.5=center
    int offsetMs=0;
    bool stereo=true;
};
// Reversible per-source controls embedded into the EXISTING OBS Audio Mixer.
// No native fader is replaced; exact UUID identities and native setters apply.
class FrontAudioControls final:public QObject {
public:
    using Query=std::function<QList<AudioPropertyState>()>;
    using SetPan=std::function<bool(const QString &,double)>;
    using SetOffset=std::function<bool(const QString &,int)>;
    FrontAudioControls(QString settingsFile,Query query,SetPan pan,SetOffset offset,
                       std::function<void()> persist={},QObject *parent=nullptr);
    ~FrontAudioControls() override;
    bool attach(QWidget *nativeMixer);
    void detach();
    void refresh();
    bool setKnobs(bool value);
    bool knobs()const;
    int rowCount()const;
private:
    struct State;
    std::unique_ptr<State> state;
};
}
