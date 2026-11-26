#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QListView>
#include <QListWidget>
#include <QLabel>
#include <QCheckBox>
#include <QTimer>
#include <QDir>
#include <QIcon>
#include <QImage>
#include <QDialog>
#include <QItemSelectionModel>
#include <QPalette>
#include <QSvgRenderer>
#include <QPainter>
OBS_DECLARE_MODULE()
static QString root(){return QDir::searchPaths("theme").last()+"/icons/colorway/plugins/";}
static bool matches(QLabel *icon,QAbstractItemView *view,QModelIndex idx,QString id){
 if(!icon||icon->pixmap().isNull())return false;
 auto group=view->isActiveWindow()?QPalette::Active:QPalette::Inactive;
 auto fg=view->palette().color(group,view->selectionModel()->isSelected(idx)?QPalette::HighlightedText:QPalette::Text);
 auto path=root()+"normal/"+id+".svg";
 auto actual=icon->pixmap().toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
 QImage mask(actual.size(),QImage::Format_ARGB32_Premultiplied);mask.fill(Qt::transparent);
 QSvgRenderer svg(path);if(!svg.isValid())return false;
 {QPainter p(&mask);svg.render(&p);}
 int delta=0,visible=0;
 for(int y=0;y<actual.height();++y)for(int x=0;x<actual.width();++x){
   const auto a=actual.pixel(x,y);const int alpha=qAlpha(mask.pixel(x,y));
   if(alpha>80)++visible;
   const auto expected=qPremultiply(qRgba(fg.red(),fg.green(),fg.blue(),qRound(alpha*fg.alphaF())));
   for(auto c:{qAbs(qRed(a)-qRed(expected)),qAbs(qGreen(a)-qGreen(expected)),qAbs(qBlue(a)-qBlue(expected)),qAbs(qAlpha(a)-qAlpha(expected))})delta=qMax(delta,c);
 }
 blog(LOG_INFO,"PALETTECHECK %s ink=%s pixels=%d delta=%d",qPrintable(id),qPrintable(fg.name()),visible,delta);
 return visible>0&&delta<=2;
}
static void check(bool changed=false){
 auto *main=static_cast<QMainWindow*>(obs_frontend_get_main_window());
 bool sourcePass=false,filterPass=false;
 auto *tree=main->findChild<QListView*>("sources");
 if(tree)for(int n=0;n<tree->model()->rowCount();++n){
  auto idx=tree->model()->index(n,0);if(idx.data(Qt::AccessibleTextRole).toString()!="Icon probe clone")continue;
  auto *row=tree->indexWidget(idx);
  if(row)for(auto *label:row->findChildren<QLabel*>(QString{},Qt::FindDirectChildrenOnly))
    if(label->property("class").toString()=="source-icon")sourcePass=matches(label,tree,idx,"source-clone");
 }
 QDialog *filters=nullptr;
 for(auto *w:QApplication::topLevelWidgets())if(w->inherits("OBSBasicFilters"))filters=qobject_cast<QDialog*>(w);
 if(filters){
  for(auto *view:filters->findChildren<QListWidget*>()){
   if(view->objectName()!="effectFilters"&&view->objectName()!="asyncFilters")continue;
   for(int i=0;i<view->count();++i){
    auto *row=view->itemWidget(view->item(i));if(!row)continue;
    auto *icon=row->findChild<QLabel*>("aftermarket-native-filter-icon",Qt::FindDirectChildrenOnly);
    if(icon&&row->findChild<QCheckBox*>())filterPass=matches(icon,view,view->model()->index(i,0),"emulens");
   }
  }
  filters->grab().save(qEnvironmentVariable("PROBE_DIR")+"/filters.png");
 }
 main->grab().save(qEnvironmentVariable("PROBE_DIR")+"/sources.png");
 blog(LOG_INFO,"PALETTECHECK %s source=%d filter=%d",changed?"live":"initial",sourcePass,filterPass);
 if(!changed){
   if(tree){auto p=tree->palette();p.setColor(QPalette::Text,QColor("#80b8d8"));p.setColor(QPalette::HighlightedText,QColor("#d8b080"));tree->setPalette(p);tree->clearSelection();}
   if(filters)for(auto *view:filters->findChildren<QListWidget*>()){
     auto p=view->palette();p.setColor(QPalette::Text,QColor("#a8c870"));p.setColor(QPalette::HighlightedText,QColor("#d8a0c8"));view->setPalette(p);
     if(view->count())view->setCurrentRow(0);
   }
   QTimer::singleShot(900,qApp,[]{check(true);});return;
 }
 if(filters)filters->close();QTimer::singleShot(100,qApp,[main]{main->close();});
}
static void prepare(){
 auto *sceneSource=obs_frontend_get_current_scene();
 auto *scene=obs_scene_from_source(sceneSource);
 auto *clone=obs_source_create("source-clone","Icon probe clone",nullptr,nullptr);
 if(scene&&clone)obs_scene_add(scene,clone);
 auto *color=obs_source_create("color_source_v3","Color",nullptr,nullptr);
 if(scene&&color)obs_scene_add(scene,color);
 auto *lens=obs_source_create("emulens","Icon probe lens",nullptr,nullptr);
 if(color&&lens)obs_source_filter_add(color,lens);
 blog(LOG_INFO,"NATIVE ICON PROBE setup: scene=%d clone=%d color=%d lens=%d",scene!=nullptr,clone!=nullptr,color!=nullptr,lens!=nullptr);
 if(color)obs_frontend_open_source_filters(color);
 if(lens)obs_source_release(lens);if(color)obs_source_release(color);if(clone)obs_source_release(clone);if(sceneSource)obs_source_release(sceneSource);
 QTimer::singleShot(2500,qApp,[]{check();});
}
static void events(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(800,qApp,prepare);}
bool obs_module_load(){obs_frontend_add_event_callback(events,nullptr);return true;}
void obs_module_unload(){obs_frontend_remove_event_callback(events,nullptr);}
