#include "native-plugin-icons.h"
#include "plugin-theme-fixture.h"
#include <QApplication>
#include <QListWidget>
#include <QLabel>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QDialog>
#include <QTemporaryDir>
#include <iostream>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAILED "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
class SourceTree:public QListWidget{
 Q_OBJECT
 public:using QListWidget::QListWidget;};
class SourceTreeItem:public QWidget{
 Q_OBJECT
 public:using QWidget::QWidget;};
class VisibilityItemWidget:public QWidget{
 Q_OBJECT
 public:using QWidget::QWidget;};
class OBSBasicFilters:public QDialog{
 Q_OBJECT
 public:using QDialog::QDialog;};
class OBSSourceLabel:public QLabel{
 Q_OBJECT
 public:using QLabel::QLabel;};
int main(int argc,char**argv){
 QApplication app(argc,argv);
 QTemporaryDir tmp;REQUIRE(writePluginThemeFixture(tmp.path()));
 SourceTree sources;sources.setObjectName("sources");
 auto addSource=[&](QString name){
  auto *item=new QListWidgetItem(name,&sources);item->setData(Qt::AccessibleTextRole,name);
  auto *row=new SourceTreeItem;auto *layout=new QHBoxLayout(row);
  auto *icon=new QLabel(row);icon->setProperty("class","source-icon");
  QPixmap pix(16,16);pix.fill(Qt::red);icon->setPixmap(pix);layout->addWidget(icon);
  layout->addWidget(new OBSSourceLabel(name,row));layout->addWidget(new QCheckBox(row));sources.setItemWidget(item,row);return icon;
 };
 auto *mapped=addSource("actual source");auto *unknown=addSource("source-clone");
 OBSBasicFilters dialog;auto *dl=new QHBoxLayout(&dialog);auto *filters=new QListWidget(&dialog);filters->setObjectName("effectFilters");dl->addWidget(filters);
 auto *item=new QListWidgetItem(filters);auto *row=new VisibilityItemWidget;auto *layout=new QHBoxLayout(row);
 auto *check=new QCheckBox(row);layout->addWidget(check);auto *name=new OBSSourceLabel("arbitrary name",row);layout->addWidget(name);filters->setItemWidget(item,row);
 sources.show();dialog.show();app.processEvents(); sources.clearSelection();
 {
  augment::NativePluginIcons icons([](const QString &name){return name=="actual source"?QString("source-clone"):QString{};},[](const QVariant &v){return v.toString();});
  item->setData(Qt::UserRole,"source_record_filter");
  icons.refresh();
  REQUIRE(mapped->pixmap().toImage().pixelColor(8,8)!=Qt::red);
  REQUIRE(unknown->pixmap().toImage().pixelColor(8,8)==Qt::red);
  auto *added=row->findChild<QLabel*>("aftermarket-native-filter-icon");REQUIRE(added);
  REQUIRE(layout->indexOf(added)==1&&layout->indexOf(check)==0&&layout->indexOf(name)==2);
  REQUIRE(!added->pixmap().isNull());
  auto pal=sources.palette();pal.setColor(QPalette::Text,Qt::white);pal.setColor(QPalette::HighlightedText,Qt::black);sources.setPalette(pal);
  sources.setCurrentRow(0);icons.refresh();
  REQUIRE(mapped->pixmap().toImage().pixelColor(8,8)==QColor("#191724"));
  sources.clearSelection();icons.refresh();
  REQUIRE(mapped->pixmap().toImage().pixelColor(8,8)==QColor("#e0def4"));
  item->setData(Qt::UserRole,"unknown");icons.refresh();
  REQUIRE(!row->findChild<QLabel*>("aftermarket-native-filter-icon"));
  item->setData(Qt::UserRole,"source_record_filter");icons.refresh();
  REQUIRE(row->findChild<QLabel*>("aftermarket-native-filter-icon"));
 }
 REQUIRE(mapped->pixmap().toImage().pixelColor(8,8)==Qt::red);
 REQUIRE(!row->findChild<QLabel*>("aftermarket-native-filter-icon"));
 REQUIRE(layout->indexOf(check)==0&&layout->indexOf(name)==1);
 std::cout<<"Native plugin row decoration and restoration verified\n";
}
#include "native-plugin-icons-test.moc"
