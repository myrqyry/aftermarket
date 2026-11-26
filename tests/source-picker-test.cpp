#include "source-picker.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QDialog>
#include <QListWidget>
#include <QScrollArea>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTimer>
#include <QTemporaryDir>
#include "plugin-theme-fixture.h"
#include <QLabel>
#include <QToolButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTabBar>
#include <QScreen>
#include <QWindow>
#include <QImage>
#include <QPixmap>
#include <QEventLoop>
#include <QMouseEvent>
#include <iostream>
#define CHECK(expr) do {if(!(expr)){std::cerr<<"FAILED "<<__LINE__<<": "<<#expr<<"\n";return 1;}}while(false)
#define EXPECT(expr) do {if(!(expr)){std::cerr<<"FAILED callback "<<__LINE__<<": "<<#expr<<"\n";valid=false;}}while(false)
struct Native {
 QDialog dialog;
 QListWidget *types;
 QWidget *cards;
 QPushButton *create,*add;
 QCheckBox *visible;
 int created=0,added=0;
 QString createdType,addedName;
 Native(QMainWindow *parent):dialog(parent){
  dialog.setObjectName("OBSBasicSourceSelect");
  auto *root=new QVBoxLayout(&dialog);
  auto *inner=new QWidget(&dialog);inner->setObjectName("dialogInner");root->addWidget(inner);
  auto *layout=new QVBoxLayout(inner);
  types=new QListWidget(inner);types->setObjectName("sourceTypeList");
  types->setMinimumHeight(460);
  for(const auto &pair: {qMakePair(QString("Recently Created"),QString("_recent")),qMakePair(QString("Browser"),QString("browser_source")),qMakePair(QString("Image"),QString("image_source")),qMakePair(QString("Scene"),QString("scene"))}){
   auto *item=new QListWidgetItem(pair.first,types);item->setData(Qt::UserRole+1,pair.second);
  }
  types->setCurrentRow(0);layout->addWidget(types);
  auto *scroll=new QScrollArea(inner);scroll->setObjectName("existingScrollArea");
  cards=new QWidget;cards->setObjectName("existingListFrame");new QVBoxLayout(cards);
  scroll->setWidget(cards);layout->addWidget(scroll);
  create=new QPushButton("Create",inner);create->setObjectName("createNewSource");layout->addWidget(create);
  visible=new QCheckBox("Make source visible",inner);visible->setObjectName("sourceVisible");layout->addWidget(visible);
  add=new QPushButton("Add existing",inner);add->setObjectName("addExistingButton");layout->addWidget(add);add->setEnabled(false);
  QObject::connect(types,&QListWidget::currentItemChanged,&dialog,[this]{populate();});
  QObject::connect(create,&QPushButton::clicked,&dialog,[this]{++created;createdType=types->currentItem()->data(Qt::UserRole+1).toString();dialog.accept();});
  QObject::connect(add,&QPushButton::clicked,&dialog,[this]{
   for(auto *button:cards->findChildren<QPushButton*>())if(button->isChecked())addedName=button->accessibleName();
   ++added;dialog.accept();
  });
  populate();
 }
 void populate(){
  while(auto *item=cards->layout()->takeAt(0)){delete item->widget();delete item;}
  const auto type=types->currentItem()->data(Qt::UserRole+1).toString();
  const QStringList names=type=="image_source"?QStringList{"Logo","Backdrop"}:type=="scene"?QStringList{"Intro scene"}:QStringList{"Chat","Logo","Backdrop"};
  add->setEnabled(false);
  for(auto &name:names){
   auto *button=new QPushButton(cards);button->setAccessibleName(name);
   button->setCheckable(true);button->setMinimumSize(300,180);
   auto *label=new QLabel(name,button);label->setObjectName("name");
   auto *thumbnail=new QLabel(button);thumbnail->setObjectName("thumbnail");
   QPixmap pixels(160,90);pixels.fill(name=="Backdrop"?QColor("#c32a86"):QColor("#2c9470"));
   thumbnail->setPixmap(pixels);
   cards->layout()->addWidget(button);
   QObject::connect(button,&QPushButton::clicked,&dialog,[this,button](bool checked){
    for(auto *other:cards->findChildren<QPushButton*>())if(other!=button)other->setChecked(false);
    add->setEnabled(checked);
   });
  }
 }
};
int main(int argc,char **argv){
 QApplication app(argc,argv);QTemporaryDir temp;
 QMainWindow main;main.resize(680,440);auto *preview=new QLabel("Preview");
 preview->setAttribute(Qt::WA_NativeWindow);
 preview->setStyleSheet("background: #ed180b;");
 main.setCentralWidget(preview);
 auto *sources=new QDockWidget("Sources",&main);sources->setObjectName("sourcesDock");sources->setWidget(new QListWidget);
 main.addDockWidget(Qt::LeftDockWidgetArea,sources);main.show();app.processEvents();
 CHECK(writePluginThemeFixture(temp.path()));
 augment::SourcePicker picker(&main,temp.filePath("source-picker.ini"));
 CHECK(picker.enabled());
 {
  Native n(&main);bool valid=true;
  auto *clone=new QListWidgetItem("Localized clone",n.types);
  clone->setData(Qt::UserRole+1,"source-clone");
  auto *unknown=new QListWidgetItem("Source Clone",n.types);
  unknown->setData(Qt::UserRole+1,"not-a-clone");
  auto colors=n.types->palette();
  colors.setColor(QPalette::Text,QColor("#575279"));
  colors.setColor(QPalette::HighlightedText,QColor("#ebdbb2"));
  colors.setColor(QPalette::Disabled,QPalette::Text,QColor("#827569"));
  n.types->setPalette(colors);
  QTimer::singleShot(40,&n.dialog,[&]{
   EXPECT(!clone->icon().isNull());
   if(!clone->icon().isNull()){
    EXPECT(clone->icon().pixmap(16,16,QIcon::Normal).toImage().pixelColor(8,8)==QColor("#575279"));
    EXPECT(clone->icon().pixmap(16,16,QIcon::Selected).toImage().pixelColor(8,8)==QColor("#ebdbb2"));
   }
   int itemEdits=0;
   const auto decorationConnection=QObject::connect(n.types,&QListWidget::itemChanged,&n.dialog,[&]{++itemEdits;});
   auto changed=n.types->palette();
   changed.setColor(QPalette::Active,QPalette::Text,QColor("#31506e"));
   changed.setColor(QPalette::Inactive,QPalette::Text,QColor("#31506e"));
   n.types->setPalette(changed);app.processEvents();
   EXPECT(clone->icon().pixmap(16,16,QIcon::Normal).toImage().pixelColor(8,8)==QColor("#31506e"));
   EXPECT(clone->icon().pixmap(16,16,QIcon::Disabled).toImage().pixelColor(8,8)==QColor("#827569"));
   QObject::disconnect(decorationConnection);
   EXPECT(itemEdits==0);
   EXPECT(unknown->icon().isNull());
   auto *chooser=n.dialog.findChild<QComboBox*>("aftermarket-existing-type");
   EXPECT(chooser&&!chooser->itemIcon(chooser->findData("source-clone")).isNull());
   // Native graphics widgets can punch through an ordinary alien child.
   // The compact chooser must be its own frameless, modal transient surface.
   EXPECT(n.dialog.isWindow());
   EXPECT(n.dialog.windowFlags().testFlag(Qt::FramelessWindowHint));
   EXPECT(n.dialog.parentWidget()==&main);
   EXPECT(QApplication::activeModalWidget()==&n.dialog);
   EXPECT(main.rect().contains(QRect(main.mapFromGlobal(n.dialog.geometry().topLeft()),n.dialog.size())));
   if(n.dialog.windowHandle()&&main.windowHandle())
       EXPECT(n.dialog.windowHandle()->transientParent()==main.windowHandle());
   if(QApplication::platformName()=="xcb"){
       const QPoint p=n.dialog.mapToGlobal(QPoint(n.dialog.width()/2,45));
       const auto screen=main.screen()->grabWindow(0,p.x(),p.y(),1,1).toImage();
       EXPECT(!screen.isNull()&&screen.pixelColor(0,0)!=QColor("#ed180b"));
   }
   EXPECT(n.dialog.width()<=400&&n.dialog.height()<=420);
   auto *tabs=n.dialog.findChild<QTabBar*>("aftermarket-source-picker-tabs");
   EXPECT(tabs&&tabs->currentIndex()==0);EXPECT(n.types->isVisible());
   EXPECT(!n.cards->isVisible());EXPECT(!n.add->isVisible());EXPECT(n.types->item(0)->isHidden());EXPECT(n.types->item(3)->isHidden());
   EXPECT(n.types->item(2)->sizeHint().height()<=32);
   n.types->setCurrentRow(2);
   emit n.types->itemClicked(n.types->item(2));
  });
  QTimer::singleShot(800,&n.dialog,&QDialog::reject);
  CHECK(n.dialog.exec()==QDialog::Accepted);CHECK(valid&&n.created==1&&n.createdType=="image_source"&&n.added==0);
 }
 {
  Native n(&main);bool valid=true;
  QTimer::singleShot(40,&n.dialog,[&]{
   auto *tabs=n.dialog.findChild<QTabBar*>("aftermarket-source-picker-tabs");
   auto *types=n.dialog.findChild<QComboBox*>("aftermarket-existing-type");
   auto *query=n.dialog.findChild<QLineEdit*>("aftermarket-existing-search");
   auto *rows=n.dialog.findChild<QListWidget*>("aftermarket-existing-list");
   EXPECT(tabs&&types&&query&&rows);
   if(!tabs||!types||!query||!rows){n.dialog.reject();return;}
   tabs->setCurrentIndex(1);
   EXPECT(!n.types->isVisible()&&rows->isVisible()&&n.add->isVisible());
   types->setCurrentIndex(types->findData("image_source"));
   EXPECT(rows->count()==2&&n.created==0);
   EXPECT(rows->iconSize()==QSize(64,36));
   EXPECT(rows->item(0)->sizeHint().height()>=36);
   EXPECT(rows->item(0)->sizeHint().height()<=48);
   EXPECT(!rows->item(1)->icon().isNull());
   auto thumbnail=rows->item(1)->icon().pixmap(64,36).toImage();
   EXPECT(!thumbnail.isNull()&&thumbnail.pixelColor(32,18)==QColor("#c32a86"));
   // OBS updates a thumbnail asynchronously without replacing the card.
   auto *card=n.cards->findChildren<QPushButton*>().last();
   QPixmap changed(90,160);changed.fill(QColor("#2345cd"));
   card->findChild<QLabel*>("thumbnail")->setPixmap(changed);
   QEventLoop updates;QTimer::singleShot(190,&updates,&QEventLoop::quit);updates.exec();
   thumbnail=rows->item(1)->icon().pixmap(64,36).toImage();
   EXPECT(!thumbnail.isNull()&&thumbnail.pixelColor(32,18)==QColor("#2345cd"));
   EXPECT(thumbnail.hasAlphaChannel()&&thumbnail.pixelColor(0,18).alpha()==0);
   // A source without a thumbnail still has a usable row and identity.
   card->findChild<QLabel*>("thumbnail")->clear();
   QEventLoop cleared;QTimer::singleShot(190,&cleared,&QEventLoop::quit);cleared.exec();
   EXPECT(rows->item(1)->icon().isNull());
   query->setText("back");
   EXPECT(rows->item(0)->isHidden()&&!rows->item(1)->isHidden());
   emit rows->itemClicked(rows->item(1));
   EXPECT(n.add->isEnabled());EXPECT(n.created==0);EXPECT(main.rect().contains(QRect(main.mapFromGlobal(n.dialog.geometry().topLeft()),n.dialog.size())));
   n.add->click();
  });
  QTimer::singleShot(900,&n.dialog,&QDialog::reject);
  CHECK(n.dialog.exec()==QDialog::Accepted);CHECK(valid&&n.added==1&&n.addedName=="Backdrop"&&n.created==0);
 }
 {
  Native n(&main);bool valid=true;
  QTimer::singleShot(40,&n.dialog,[&]{
   // Qt delivers grabbed popup presses in popup-local coordinates.
   const QPoint outside(-12,-12);
   QMouseEvent press(QEvent::MouseButtonPress,QPointF(outside),QPointF(n.dialog.mapToGlobal(outside)),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
   QApplication::sendEvent(&n.dialog,&press);
   EXPECT(!n.dialog.isVisible());
  });
  QTimer::singleShot(800,&n.dialog,&QDialog::reject);
  CHECK(n.dialog.exec()==QDialog::Rejected);CHECK(valid&&n.created==0&&n.added==0);
 }
 CHECK(picker.setEnabled(false));
 {Native n(&main);bool valid=true;
  QTimer::singleShot(25,&n.dialog,[&]{EXPECT(n.dialog.isWindow());EXPECT(!n.dialog.windowFlags().testFlag(Qt::FramelessWindowHint));EXPECT(n.types->minimumHeight()==460);n.dialog.reject();});
  CHECK(n.dialog.exec()==QDialog::Rejected);CHECK(valid);}
 CHECK(picker.setEnabled(true));
 {augment::SourcePicker persisted(&main,temp.filePath("source-picker.ini"));CHECK(persisted.enabled());CHECK(persisted.setEnabled(false));}
 {augment::SourcePicker persisted(&main,temp.filePath("source-picker.ini"));CHECK(!persisted.enabled());CHECK(persisted.setEnabled(true));}
 {QDialog other(&main);other.setObjectName("OBSBasicSourceSelect");QTimer::singleShot(20,&other,&QDialog::reject);CHECK(other.exec()==QDialog::Rejected);CHECK(other.isWindow());}
 std::cout<<"Compact source picker: direct creation, separate filtered native existing selection, bounds, opt-out verified\n";
}
