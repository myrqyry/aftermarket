#include "obs-transform.h"
#include "transform-visual.h"
#include <obs.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <graphics/matrix4.h>
#include <QApplication>
#include <QAction>
#include <QPolygonF>
#include <QColor>
#include <QPointF>
#include <cmath>
#include <QLabel>
namespace augment {
namespace {
const char *const keys[]={"OverrideColors","SelectRed","SelectGreen","SelectBlue"};
int64_t packColor(const QColor &color){
    return (int64_t(0xff)<<24)|(int64_t(color.blue())<<16)|
           (int64_t(color.green())<<8)|int64_t(color.red());
}
QColor readable(const QColor &color,const QColor &fallback){
    return color.isValid()?color:fallback;
}
}
bool NativeTransformColors::apply(const QPalette &p){
    if(!config||blocked)return false;
    const QColor accent=readable(p.color(QPalette::Link),QColor("#70bed3"));
    const QColor crop=readable(p.color(QPalette::Highlight),QColor("#bea1e5"));
    const QColor hover=accent.lighter(128);
    const int64_t wanted[4]={1,packColor(accent),packColor(crop),packColor(hover)};
    const auto read=[this](int i)->int64_t{
        return i?config_get_int(config,"Accessibility",keys[i]):
                 int64_t(config_get_bool(config,"Accessibility",keys[i]));
    };
    if(!engaged){
        for(int i=0;i<4;++i){
            prior[i]={config_has_user_value(config,"Accessibility",keys[i]),read(i)};
        }
        engaged=true;
    }else{
        // If another settings editor changed values while we were active,
        // relinquish ownership instead of silently overwriting that edit.
        for(int i=0;i<4;++i)if(read(i)!=applied[i]){
            restore(); // recover every unchanged key but preserve the external edit
            blocked=true;
            return false;
        }
    }
    for(int i=0;i<4;++i){
        if(appliedEnabled&&wanted[i]==applied[i])continue;
        if(i==0)config_set_bool(config,"Accessibility",keys[i],true);
        else config_set_int(config,"Accessibility",keys[i],wanted[i]);
        applied[i]=wanted[i];
    }
    appliedEnabled=true;
    return true;
}
void NativeTransformColors::restore(){
    if(!engaged||!config)return;
    for(int i=0;i<4;++i){
        const int64_t now=i?config_get_int(config,"Accessibility",keys[i]):
            int64_t(config_get_bool(config,"Accessibility",keys[i]));
        if(now!=applied[i])continue; // external edit takes precedence
        if(!prior[i].had)config_remove_value(config,"Accessibility",keys[i]);
        else if(i==0)config_set_bool(config,"Accessibility",keys[i],prior[i].number!=0);
        else config_set_int(config,"Accessibility",keys[i],prior[i].number);
    }
    engaged=false;appliedEnabled=false;
}
namespace {
struct SceneShapes {
    QVector<QPolygonF> shapes;
    QSize canvas, viewport;
};
bool collectTransform(obs_scene_t *,obs_sceneitem_t *item,void *opaque){
    auto *out=static_cast<SceneShapes*>(opaque);
    if(!obs_sceneitem_selected(item)||obs_sceneitem_locked(item)||
       obs_sceneitem_is_group(item))return true; // group children use group-space coordinates
    auto *source=obs_sceneitem_get_source(item);
    if(!source||(obs_source_get_output_flags(source)&OBS_SOURCE_VIDEO)==0)return true;
    if(out->shapes.size()>40)return false;
    matrix4 box;
    obs_sceneitem_get_box_transform(item,&box);
    const auto position=[&](qreal x,qreal y){
        return QPointF(box.t.x+x*box.x.x+y*box.y.x,
                       box.t.y+x*box.x.y+y*box.y.y);
    };
    const QPolygonF world{position(0,0),position(1,0),position(1,1),position(0,1)};
    out->shapes.append(previewPolygon(world,out->canvas,out->viewport,10));
    return true;
}
}
ObsTransformStyle::ObsTransformStyle(QWidget *host,std::function<bool()> on)
    :main(host),enabled(std::move(on)){
    native=std::make_unique<NativeTransformColors>(obs_frontend_get_user_config());
    timer.setInterval(95);
    QObject::connect(&timer,&QTimer::timeout,&timer,[this]{refresh();});
    timer.start();
}
ObsTransformStyle::~ObsTransformStyle(){timer.stop();disable();}
void ObsTransformStyle::disable(){
    if(visual){delete visual.data();visual=nullptr;}
    preview=nullptr;
    if(native){native->restore();native->resetBlocked();}
}
void ObsTransformStyle::refresh(){
    if(!main||!enabled||!enabled()){disable();return;}
    // An independent tool window must not linger over other applications,
    // native menus, modal dialogs (including the compact source picker),
    // or OBS when its own main window is minimized.
    if(!main->isVisible()||main->isMinimized()||
       !main->window()->isActiveWindow()||QApplication::activePopupWidget()||
       QApplication::activeModalWidget()){
        if(visual)visual->hide();
        return;
    }
    auto *target=main->findChild<QWidget*>("preview");
    if(!target||!target->isVisible()||target->width()<50||target->height()<50){
        if(visual)visual->hide();
        return;
    }
    const auto palette=target->palette();
    if(!native||!native->apply(palette)){
        if(visual)visual->hide();
        return;
    }
    if(preview!=target||!visual){
        if(visual)delete visual.data();
        preview=target;
        auto *layer=new TransformVisual(target);
        layer->useTransientSurface();
        layer->setGeometry(QRect(target->mapToGlobal(QPoint(0,0)),target->size()));
        visual=layer;
    }
    const QRect overlayRect(target->mapToGlobal(QPoint(0,0)),target->size());
    if(visual->geometry()!=overlayRect)visual->setGeometry(overlayRect);
    visual->setPalette(palette);
    // OBS exposes the actual preview scale as a truncated percentage.
    // Custom fixed zoom and panning cannot be mapped accurately without
    // native OBS previewX/previewY, so hide only our visual adornments there.
    // The native theme color and OBS interaction remain functional.
    obs_video_info ovi{};
    if(!obs_get_video_info(&ovi)){visual->hide();return;}
    auto *label=main->findChild<QLabel*>("previewScalePercent");
    const QString caption=label?label->text().trimmed():QString{};
    bool valid=false;
    const int percent=caption.endsWith(QChar('%'))?
        caption.left(caption.size()-1).toInt(&valid):-1;
    if(!valid||!matchesFittedPreview(QSize(int(ovi.base_width),int(ovi.base_height)),target->size(),percent)){
        visual->hide();
        return;
    }
    auto *source=obs_frontend_get_current_preview_scene();
    if(!source)source=obs_frontend_get_current_scene();
    if(!source){visual->setShapes({});return;}
    SceneShapes data{{},QSize(int(ovi.base_width),int(ovi.base_height)),target->size()};
    if(auto *scene=obs_scene_from_source(source))obs_scene_enum_items(scene,collectTransform,&data);
    obs_source_release(source);
    visual->setShapes(data.shapes);
    if(data.shapes.isEmpty()){
        visual->hide();
    }else if(!visual->isVisible()){
        visual->show();
        visual->raise();
    }
}
}
