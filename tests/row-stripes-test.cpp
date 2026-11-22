#include "row-stripes.h"
#include <QApplication>
#include <QListWidget>
#include <QListView>
#include <QMainWindow>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QPointer>
#include <QColor>
#include <iostream>
class SceneTree : public QListWidget {
    Q_OBJECT
public:
    using QListWidget::QListWidget;
};
class SourceTree : public QListView {
    Q_OBJECT
public:
    using QListView::QListView;
};
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED line "<<__LINE__<<": "<<#x<<"\\n";return 1;}}while(false)
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setStyle("Fusion");
 QWidget parent;auto *layout=new QVBoxLayout(&parent);
 SourceTree sources(&parent);sources.setObjectName("sources");
 sources.setStyleSheet("*[bgColor=\"1\"] { background:rgba(255,68,68,33%); }");
 QStandardItemModel model;model.appendRow(new QStandardItem("One"));model.appendRow(new QStandardItem("Two"));
 sources.setModel(&model);
 auto *native=new QWidget;native->setAttribute(Qt::WA_TranslucentBackground);
 native->setStyleSheet("background:none");sources.setIndexWidget(model.index(0,0),native);
 SceneTree scenes(&parent);scenes.setObjectName("scenes");scenes.addItems({"Scene A","Scene B","Scene C"});
 scenes.setStyleSheet("QListWidget::item:alternate { background-color: #26233A; }");
 QListWidget unrelated(&parent);unrelated.setObjectName("scenes");unrelated.addItem("unrelated");
 layout->addWidget(&sources);layout->addWidget(&scenes);layout->addWidget(&unrelated);
 auto normalSource=sources.palette(),normalScene=scenes.palette();
 const bool sourceWasStriped=sources.alternatingRowColors(),sceneWasStriped=scenes.alternatingRowColors();
 parent.resize(500,500);parent.show();app.processEvents();
 augment::RowStripes stripes;
 const QJsonObject both{{"sources",true},{"scenes",true},{"base","#203040"},{"alternate","#345678"}};
 stripes.configure(both);app.processEvents();
 REQUIRE(sources.alternatingRowColors()&&scenes.alternatingRowColors());
 REQUIRE(sources.palette().color(QPalette::Base)==QColor("#203040"));
 REQUIRE(sources.palette().color(QPalette::AlternateBase)==QColor("#345678"));
 REQUIRE(scenes.palette().color(QPalette::AlternateBase)==QColor("#345678"));
 const auto sceneRow=scenes.visualItemRect(scenes.item(1));
 REQUIRE(!sceneRow.isEmpty());
 const auto actualStripe=scenes.viewport()->grab().toImage().pixelColor(
     scenes.viewport()->width()-20,sceneRow.center().y());
 REQUIRE(actualStripe==QColor("#345678"));
 REQUIRE(unrelated.alternatingRowColors()==false);
 // If OBS refreshes its theme while striped, the adapter reapplies custom
 // colors but restores the *new* native palette when disabled.
 QPalette themeChange=sources.palette();
 themeChange.setColor(QPalette::Base,QColor("#181818"));
 themeChange.setColor(QPalette::AlternateBase,QColor("#292929"));
 sources.setPalette(themeChange);app.processEvents();
 REQUIRE(sources.palette().color(QPalette::Base)==QColor("#203040"));
 REQUIRE(sources.palette().color(QPalette::AlternateBase)==QColor("#345678"));
 REQUIRE(sources.indexWidget(model.index(0,0))==native);
 REQUIRE(native->styleSheet()=="background:none");
 REQUIRE(sources.palette().color(QPalette::Highlight)==normalSource.color(QPalette::Highlight));
 sources.setCurrentIndex(model.index(1,0));app.processEvents();
 REQUIRE(sources.currentIndex().row()==1);
 stripes.configure({{"sources",false},{"scenes",true},{"alternate","#506070"}});
 app.processEvents();
 REQUIRE(sources.palette().color(QPalette::Base)==QColor("#181818")&&!sources.alternatingRowColors());
 REQUIRE(scenes.palette().color(QPalette::Base)==normalScene.color(QPalette::Base));
 scenes.setViewMode(QListView::IconMode);
 scenes.setStyleSheet("*{padding:0;margin:0;}"); // native OBS grid-mode QSS
 QEvent layoutChanged(QEvent::LayoutRequest);
 QApplication::sendEvent(&scenes,&layoutChanged);app.processEvents();
 REQUIRE(scenes.alternatingRowColors()==sceneWasStriped);
 REQUIRE(!scenes.styleSheet().contains("SceneTree::item:"));
 scenes.setViewMode(QListView::ListMode);
 scenes.setStyleSheet("QListWidget::item:alternate { background-color: #26233A; }");
 QApplication::sendEvent(&scenes,&layoutChanged);app.processEvents();
 REQUIRE(scenes.alternatingRowColors());
 REQUIRE(scenes.palette().color(QPalette::AlternateBase)==QColor("#506070"));
 stripes.configure({});
 app.processEvents();
 REQUIRE(scenes.palette().color(QPalette::Base)==normalScene.color(QPalette::Base));
 REQUIRE(scenes.palette().color(QPalette::AlternateBase)==normalScene.color(QPalette::AlternateBase));
 REQUIRE(scenes.alternatingRowColors()==sceneWasStriped);
 REQUIRE(scenes.styleSheet()==QStringLiteral("QListWidget::item:alternate { background-color: #26233A; }"));
 {SceneTree later(&parent);later.setObjectName("scenes");later.addItems({"One","Two"});
  layout->addWidget(&later);
  stripes.configure({{"scenes",true},{"alternate","#223344"}});
  later.show();app.processEvents();
  REQUIRE(later.alternatingRowColors());
  REQUIRE(later.palette().color(QPalette::AlternateBase)==QColor("#223344"));
  stripes.configure({});
 }
 REQUIRE(!scenes.alternatingRowColors());
 std::cout<<"alternating OBS row colors verified\n";
 return 0;
}
#include "row-stripes-test.moc"
