#pragma once
#include <QPalette>
#include <QPointer>
#include <QWidget>
#include <QTimer>
#include <cstdint>
#include <functional>
#include <memory>
struct config_data;
typedef struct config_data config_t;
namespace augment {
class TransformVisual;
// Only temporary libobs/OBS frontend configuration changes: the original
// explicit values and their presence/absence are restored on opt-out and exit.
class NativeTransformColors {
public:
    explicit NativeTransformColors(config_t *cfg):config(cfg){}
    ~NativeTransformColors(){restore();}
    bool apply(const QPalette &palette);
    void restore();
    void resetBlocked(){blocked=false;}
    bool active() const{return engaged;}
private:
    struct Entry {bool had=false;int64_t number=0;};
    config_t *config=nullptr;
    bool engaged=false,blocked=false;
    bool appliedEnabled=false;
    Entry prior[4];
    int64_t applied[4]={};
};
class ObsTransformStyle {
public:
    explicit ObsTransformStyle(QWidget *main,std::function<bool()> enabled);
    ~ObsTransformStyle();
    void refresh();
    void disable();
private:
    QPointer<QWidget> main,preview;
    std::function<bool()> enabled;
    QTimer timer;
    std::unique_ptr<NativeTransformColors> native;
    QPointer<TransformVisual> visual;
};
}
