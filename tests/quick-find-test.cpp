#include "quick-find.h"
#include "source-dock-pin.h"
#include <QGridLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <iostream>

#define REQUIRE(expr) do { if(!(expr)){std::cerr<<"FAIL "<<__LINE__<<" "<<#expr<<"\n";return 1;} }while(false)

int main(int argc,char**argv){
    QApplication app(argc,argv);
    using augment::QuickFindEntry;
    const QList<QuickFindEntry> data{
        {QuickFindEntry::Scene,"scene-one",{},"Starting Soon","Scene",{}, {},{}},
        {QuickFindEntry::Source,"source-a",{},"Game Capture","game_capture", "/home/videos/game.mp4",
            "Starting Soon","scene-one"},
        {QuickFindEntry::Source,"source-b",{},"Web View","browser_source","https://example.com/chat",
            "Starting Soon","scene-one"},
        {QuickFindEntry::Filter,"filter-a","source-b","Blur Pass","blur_filter",{},
            "Web View",{}},
        {QuickFindEntry::Source,"source-c",{},"Game Capture","game_capture", "/home/videos/game2.mp4",
            "Other Scene","scene-two"}
    };
    auto names=[](const QList<QuickFindEntry>&hits){QStringList result;for(const auto &hit:hits)result<<hit.id;return result;};
    REQUIRE(augment::QuickFind::search(data,"game").size()==2);
    REQUIRE(augment::QuickFind::search(data,"gaME capTURE").size()==2);
    REQUIRE(names(augment::QuickFind::search(data,"game2"))==QStringList{"source-c"});
    REQUIRE(names(augment::QuickFind::search(data,"example.com"))==QStringList{"source-b"});
    REQUIRE(names(augment::QuickFind::search(data,"blur"))==QStringList{"filter-a"});
    REQUIRE(names(augment::QuickFind::search(data,"starting soon source"))==QStringList{"source-b"});
    REQUIRE(names(augment::QuickFind::search(data,"game other"))==QStringList{"source-c"});
    REQUIRE(augment::QuickFind::search(data,"not-found").isEmpty());
    REQUIRE(augment::QuickFind::search(data,"",2).size()==2);
    REQUIRE(augment::QuickFind::search(data,"game",1).size()==1);
    REQUIRE(augment::QuickFind::search(data,"game",0).isEmpty());
    QMainWindow main;
    augment::QuickFind palette(&main,[&]{return data;},[&](const QuickFindEntry &hit,augment::QuickFind::Action action){
        return hit.id=="filter-a"&&action==augment::QuickFind::Filters;
    });
    palette.open();
    REQUIRE(palette.isVisible());
    auto *input=palette.findChild<QLineEdit*>("aftermarket-quick-find-input");
    auto *list=palette.findChild<QListWidget*>("aftermarket-quick-find-results");
    REQUIRE(input&&list);
    REQUIRE(list->count()==5);
    input->setText("blur");
    REQUIRE(list->count()==1);
    REQUIRE(list->item(0)->text().contains("Blur Pass"));
    REQUIRE(palette.activate(augment::QuickFind::Filters));
    REQUIRE(!palette.isVisible());
    QString pinned;
    augment::QuickFind pinPalette(&main,[&]{return data;},[&](const QuickFindEntry &hit,augment::QuickFind::Action action){
        if(action!=augment::QuickFind::PinSource)return false;
        pinned=hit.id;return true;
    },true);
    pinPalette.open();
    auto *pinInput=pinPalette.findChild<QLineEdit*>("aftermarket-quick-find-input");
    auto *pin=pinPalette.findChild<QPushButton*>("aftermarket-quick-find-pin");
    REQUIRE(pin&&pin->isVisible());
    pinInput->setText("blur");
    REQUIRE(!pin->isEnabled()&&!pinPalette.activate(augment::QuickFind::PinSource));
    pinInput->setText("web view");
    REQUIRE(pin->isEnabled());
    pin->click();
    REQUIRE(pinned=="source-b"&&!pinPalette.isVisible());
    QDialog form;
    auto *grid=new QGridLayout(&form);
    auto *combo=new QComboBox;combo->setEditable(true);
    combo->addItem("",QByteArray{});combo->addItem("Selected",QByteArray("CurrentSelectedSource"));
    combo->addItem("Camera",QByteArray("Camera"));grid->addWidget(combo,1,0);
    auto *title=new QLineEdit;grid->addWidget(title,1,1);
    grid->addWidget(new QLineEdit,1,2);
    for(int i=3;i<=13;++i)grid->addWidget(new QCheckBox,1,i);
    auto *add=new QPushButton("Add");grid->addWidget(add,1,14);
    REQUIRE(!augment::prepareSourceDockPin(&form,"missing","Pinned","Add",{true,true,false,false}));
    REQUIRE(title->text().isEmpty()&&combo->currentIndex()==0);
    REQUIRE(!augment::prepareSourceDockPin(&form,"Camera","Pinned","Wrong label",{}));
    REQUIRE(augment::prepareSourceDockPin(&form,"Camera","Pinned","Add",{true,true,false,false})==add);
    REQUIRE(combo->currentIndex()==2&&title->text()=="Pinned");
    REQUIRE(qobject_cast<QCheckBox*>(grid->itemAtPosition(1,4)->widget())->isChecked());
    REQUIRE(qobject_cast<QCheckBox*>(grid->itemAtPosition(1,5)->widget())->isChecked());
    REQUIRE(!qobject_cast<QCheckBox*>(grid->itemAtPosition(1,7)->widget())->isChecked());
    REQUIRE(!augment::prepareSourceDockPin(&form,"Camera","Another","Add",{}));
    std::cout<<"AFTERMARKET_QUICK_FIND_SEARCH_UI_PASS=1\n";
}
