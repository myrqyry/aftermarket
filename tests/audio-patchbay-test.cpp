#include "audio-patchbay.h"
#include <QApplication>
#include <QToolButton>
#include <QComboBox>
#include <QAbstractItemModel>
#include <QMouseEvent>
#include <QPointer>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\\n";return 1;}}while(false)
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setStyle("Fusion");
 QVector<augment::RouteSource> saved={{"id-mic","Microphone",1u,0},{"id-desk","Desktop",3u,2}};
 int writes=0,monitors=0;
 augment::AudioPatchbay ui(
  [&]{return saved;},
  [&](const QString &id,int track){
    for(auto &source:saved)if(source.id==id&&track>=1&&track<=6){
      source.tracks^=1u<<(track-1);++writes;return true;
    }
    return false;
  },
  [&](const QString &id,int mode){
    for(auto &source:saved)if(source.id==id&&mode>=0&&mode<=2){
      source.monitor=mode;++monitors;return true;
    }
    return false;
  });
 ui.resize(560,510);ui.show();app.processEvents();
 REQUIRE(ui.routeCount()==2);
 // Polling an unchanged source list should not rebuild an open combo box.
 int removedRows=0;
 auto *sourceBox=ui.findChild<QComboBox*>("aftermarket-route-source");
 REQUIRE(sourceBox);
 QObject::connect(sourceBox->model(),&QAbstractItemModel::rowsRemoved,&ui,
                 [&](const QModelIndex&,int,int){++removedRows;});
 ui.refresh();REQUIRE(removedRows==0);
 REQUIRE(ui.selectSource("id-mic"));
 REQUIRE(ui.selectedId()=="id-mic");
 auto *track4=ui.findChild<QToolButton*>("aftermarket-track-4");
 REQUIRE(track4&&track4->isEnabled()&&!track4->isChecked());
 track4->click();app.processEvents();
 REQUIRE(writes==1&&saved[0].tracks==(1u|(1u<<3))&&track4->isChecked());
 auto *track1=ui.findChild<QToolButton*>("aftermarket-track-1");
 REQUIRE(track1&&track1->isChecked());track1->click();app.processEvents();
 REQUIRE(writes==2&&saved[0].tracks==(1u<<3)&&!track1->isChecked());
 auto *monitor=ui.findChild<QComboBox*>("aftermarket-monitor");
 REQUIRE(monitor&&monitor->currentIndex()==0);
 monitor->setCurrentIndex(1);app.processEvents();
 REQUIRE(monitors==1&&saved[0].monitor==1);
 saved[0].name="Renamed microphone";ui.refresh();
 REQUIRE(ui.selectedId()=="id-mic"&&ui.routeCount()==2);
 // Backend must remain authoritative when changed externally.
 saved[0].tracks=1u<<5;ui.refresh();
 REQUIRE(!track4->isChecked()&&ui.findChild<QToolButton*>("aftermarket-track-6")->isChecked());
 saved.removeFirst();ui.refresh();
 REQUIRE(ui.selectedId()=="id-desk"&&ui.routeCount()==1);
 REQUIRE(monitor->currentIndex()==2);
 ui.selectSource("id-desk");saved[0].id="id-changed";
 track4->click();app.processEvents(); // stale UUID fails safely
 REQUIRE(writes==2);
 saved.clear();ui.refresh();
 REQUIRE(ui.routeCount()==0&&ui.selectedId().isEmpty()&&!track4->isEnabled());
 std::cout<<"audio patchbay routing verified\n";
 return 0;
}
