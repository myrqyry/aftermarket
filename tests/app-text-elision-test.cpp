#include "app-text-elision.h"
#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QStyle>
#include <QTreeView>
#include <QListView>
#include <QTabWidget>
#include <QTabBar>
#include <QVBoxLayout>
#include <QStringListModel>
#include <QEventLoop>
#include <iostream>
class NativeButton final : public QPushButton {
public:
    using QPushButton::QPushButton;
    int painted=0;
protected:
    void paintEvent(QPaintEvent *event) override {
        ++painted;
        QPushButton::paintEvent(event);
    }
};
class NativeCombo final : public QComboBox {
public:
    using QComboBox::QComboBox;
    int painted=0;
protected:
    void paintEvent(QPaintEvent *event) override {
        ++painted;
        QComboBox::paintEvent(event);
    }
};
class NativeLabel final : public QLabel {
public:
    using QLabel::QLabel;
    int painted=0;
protected:
    void paintEvent(QPaintEvent *event) override {
        ++painted;
        QLabel::paintEvent(event);
    }
};
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)

int main(int argc,char **argv){
 QApplication app(argc,argv);
 QWidget window;
 auto *layout=new QVBoxLayout(&window);
 auto *label=new NativeLabel("The Longest Notable Audio Source Identifier In An OBS Panel",&window);
 label->setObjectName("panel-label");
 label->setTextFormat(Qt::PlainText);
 label->setFixedWidth(128);
 layout->addWidget(label);
 auto *wrapped=new QLabel("A wrapped multi-line help paragraph is not an ellipsis target.",&window);
 wrapped->setWordWrap(true);layout->addWidget(wrapped);
 auto *view=new QListView(&window);
 QStringListModel model({"Extremely Long Source Name In An OBS Project"},view);
 view->setModel(&model);
 view->setTextElideMode(Qt::ElideRight);
 layout->addWidget(view);
 auto *tabs=new QTabWidget(&window);
 tabs->addTab(new QLabel("First",&window),"Extremely Long Output Monitoring Controls");
 tabs->addTab(new QLabel("Second",&window),"Second Tab");
 tabs->tabBar()->setElideMode(Qt::ElideRight);
 layout->addWidget(tabs);
 auto *action=new QPushButton("Output Device Settings",&window);
 action->setObjectName("source-control");layout->addWidget(action);
 auto *combo=new NativeCombo(&window);
 combo->setFixedWidth(120);
 combo->addItems({"Output Monitoring For A Very Long Source Name", "Another choice"});
 layout->addWidget(combo);
 auto *button=new NativeButton("A Very Long Button Label Describing An Action",&window);
 button->setFixedWidth(125);layout->addWidget(button);
 auto *check=new QCheckBox("A Very Long Setting Label On A Checkbox",&window);
 check->setFixedWidth(125);check->setChecked(true);layout->addWidget(check);
 // Model OBS's context-toolbar Filters button. Its real native paint
 // must survive Colorway/QSS margins, icons, and app-wide middle mode.
 auto *filters=new NativeButton("Filters",&window);
 filters->setObjectName("sourceFiltersButton");
 filters->setIcon(filters->style()->standardIcon(QStyle::SP_FileDialogDetailedView));
 filters->setIconSize(QSize(16,16));
 filters->setStyleSheet("QPushButton { padding: 7px 10px; border: 1px solid #888; border-radius: 4px; }");
 filters->setFixedSize(filters->sizeHint());
 layout->addWidget(filters);
 // OBS source names have a narrowly supported display-only adapter.
 auto *obsSource=new QLabel("Extremely Long Source Name With A Distinct Ending",&window);
 obsSource->setObjectName("OBSSourceLabel");
 obsSource->setTextFormat(Qt::PlainText);
 obsSource->setFixedWidth(105);
 layout->addWidget(obsSource);
 window.resize(275,510);window.show();app.processEvents();
 const QString full=label->text();
 const QSize fullHint=label->sizeHint();
 const QImage nativeClipped=label->grab().toImage();
 const QString buttonText=action->text();
 const QImage comboBefore=combo->grab().toImage();
 const QImage buttonBefore=button->grab().toImage();
 const QImage checkBefore=check->grab().toImage();
 const QImage filtersBefore=filters->grab().toImage();
 const QImage sourceBefore=obsSource->grab().toImage();
 const QSize sourceHint=obsSource->sizeHint();
 const int filtersPainted=filters->painted;
 const int nativeLabelPainted=label->painted;
 const int nativeButtonPainted=button->painted;
 const int nativeComboPainted=combo->painted;
 {
  augment::AppTextElision global(&window);
  REQUIRE(global.mode()==Qt::ElideRight);
  REQUIRE(augment::AppTextElision::currentCustomMode()==Qt::ElideRight);
  REQUIRE(view->textElideMode()==Qt::ElideRight);
  REQUIRE(tabs->tabBar()->elideMode()==Qt::ElideRight);
  REQUIRE(label->text()==full); // default must leave OBS's existing UI alone
  REQUIRE(global.setMode(Qt::ElideMiddle));
  REQUIRE(augment::AppTextElision::currentCustomMode()==Qt::ElideMiddle);
  app.processEvents();
  REQUIRE(view->textElideMode()==Qt::ElideMiddle);
  REQUIRE(tabs->tabBar()->elideMode()==Qt::ElideMiddle);
  // App-wide ellipsis is PRESENTATION only. Never replace QLabel::text:
  // doing so changes QSizeHint and shrinks layouts around the abbreviation.
  REQUIRE(label->text()==full);
  REQUIRE(label->sizeHint()==fullHint);
  REQUIRE(label->grab().toImage()==nativeClipped); // ordinary QLabel stays native
  REQUIRE(label->toolTip().isEmpty());
  REQUIRE(label->painted>nativeLabelPainted); // native paint must run
  // A short name that fits must remain untouched, even in middle mode.
  auto *fits=new QLabel("Live Latent: TTS",&window);
  fits->setFixedWidth(fits->fontMetrics().horizontalAdvance(fits->text())+8);
  layout->addWidget(fits);fits->show();app.processEvents();
  const auto nativeFit=fits->grab().toImage();
  global.refresh();app.processEvents();
  REQUIRE(fits->text()=="Live Latent: TTS");
  REQUIRE(fits->toolTip().isEmpty());
  REQUIRE(fits->grab().toImage()==nativeFit);
  REQUIRE(wrapped->text()=="A wrapped multi-line help paragraph is not an ellipsis target.");
  REQUIRE(action->text()==buttonText); // native OBS source/button bindings untouched
  REQUIRE(combo->currentText()=="Output Monitoring For A Very Long Source Name");
  REQUIRE(button->text()=="A Very Long Button Label Describing An Action");
  REQUIRE(check->text()=="A Very Long Setting Label On A Checkbox");
  REQUIRE(check->isChecked());
  REQUIRE(combo->grab().toImage()==comboBefore); // Qt style remains owner
  REQUIRE(button->grab().toImage()==buttonBefore);
  REQUIRE(check->grab().toImage()==checkBefore);
  REQUIRE(filters->text()=="Filters");
  REQUIRE(filters->grab().toImage()==filtersBefore);
  REQUIRE(filters->painted>filtersPainted);
  REQUIRE(combo->painted>nativeComboPainted);
  REQUIRE(button->painted>nativeButtonPainted);
  REQUIRE(view->model()->index(0,0).data().toString()=="Extremely Long Source Name In An OBS Project");
  REQUIRE(tabs->tabText(0)=="Extremely Long Output Monitoring Controls");
  // Known OBS source-label painting is display-only. Never let
  // elision change source identity, widget text or full-text sizeHint.
  REQUIRE(obsSource->grab().toImage()!=sourceBefore);
  REQUIRE(obsSource->toolTip()==obsSource->text());
  REQUIRE(obsSource->sizeHint()==sourceHint);
  REQUIRE(obsSource->text()=="Extremely Long Source Name With A Distinct Ending");
  // New widgets and model views added after setup inherit the active choice.
  auto *late=new QListView(&window);
  late->setObjectName("late-item-view");layout->addWidget(late);
  late->show();app.processEvents();
  REQUIRE(late->textElideMode()==Qt::ElideMiddle);
  auto *lateTabs=new QTabWidget(&window);
  lateTabs->addTab(new QWidget(lateTabs),"Very Long Dynamic Tab Name");
  lateTabs->tabBar()->setElideMode(Qt::ElideRight);
  layout->addWidget(lateTabs);
  lateTabs->show();app.processEvents();
  REQUIRE(lateTabs->tabBar()->elideMode()==Qt::ElideMiddle);
  // New external label text must remain the authority; it must not be lost
  // because our earlier visible text had been shortened.
  label->setText("A New Very Long Selected Source With An Ending");
  app.processEvents();
  REQUIRE(label->text()=="A New Very Long Selected Source With An Ending");
  REQUIRE(label->toolTip().isEmpty());
  REQUIRE(global.setMode(Qt::ElideRight));
  REQUIRE(augment::AppTextElision::currentCustomMode()==Qt::ElideRight);
  app.processEvents();
  REQUIRE(augment::AppTextElision::currentCustomMode()==Qt::ElideRight);
 REQUIRE(label->text()=="A New Very Long Selected Source With An Ending");
  REQUIRE(view->textElideMode()==Qt::ElideRight);
  REQUIRE(tabs->tabBar()->elideMode()==Qt::ElideRight);
  REQUIRE(obsSource->grab().toImage()==sourceBefore);
  REQUIRE(obsSource->toolTip().isEmpty());
  // Saving/restoring is separate; this class only applies in-memory preference.
  REQUIRE(global.setMode(Qt::ElideMiddle));
  app.processEvents();
 }
 app.processEvents();
 REQUIRE(augment::AppTextElision::currentCustomMode()==Qt::ElideRight);
 REQUIRE(label->text()=="A New Very Long Selected Source With An Ending");
 REQUIRE(view->textElideMode()==Qt::ElideRight);
 REQUIRE(tabs->tabBar()->elideMode()==Qt::ElideRight);
 REQUIRE(action->text()==buttonText);
 std::cout<<"App-wide native item views, tabs and safe display labels restored\n";
 return 0;
}
