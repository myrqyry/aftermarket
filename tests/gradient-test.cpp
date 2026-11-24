#include "augment.h"
#include "gradient.h"
#include <QApplication>
#include <QSlider>
#include <QProgressBar>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QThread>
#include <QElapsedTimer>
#include <QTimer>
#include <QVBoxLayout>
#include <iostream>
#define REQUIRE(x) do { if (!(x)) { std::cerr << "FAILED line " << __LINE__ << ": " << #x << '\n'; return 1; } } while(false)
static int colored(const QImage &im,QRect rect) {
    int n=0;for(int y=rect.top();y<=rect.bottom();++y)for(int x=rect.left();x<=rect.right();++x) {
        if(!im.rect().contains(x,y))continue;
        auto c=im.pixelColor(x,y); if(c.red()>170 && c.blue()>140 && c.green()<90)++n;
    }return n;
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");
    QTemporaryDir tmp;auto path=tmp.filePath("rules.json");
    QFile f(path);REQUIRE(f.open(QIODevice::WriteOnly));f.write(R"({"enabled":true,"motion":true,"rules":[]})");f.close();
    augment::Controller controller(path);
    QWidget host;auto *layout=new QVBoxLayout(&host);
    auto *slider=new QSlider(Qt::Horizontal,&host);slider->setRange(0,100);slider->setValue(40);layout->addWidget(slider);
    auto *progress=new QProgressBar(&host);progress->setTextVisible(false);progress->setRange(0,100);progress->setValue(40);layout->addWidget(progress);
    auto *vertical=new QSlider(Qt::Vertical,&host);vertical->setRange(0,100);vertical->setValue(55);layout->addWidget(vertical);
    host.resize(360,300);host.show();app.processEvents();
    auto wait=[&](int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){app.processEvents();QThread::msleep(4);}app.processEvents();};
    REQUIRE(!slider->findChild<QWidget*>("aftermarket-gradient"));
    QJsonObject spec{{"enabled",true},{"period",1000},{"colors",QJsonArray{"#ff00ff","#d000bb","#ff00ff"}}};
    REQUIRE(controller.setGradientSettings(spec));
    wait(60);
    auto *overlay=slider->findChild<QWidget*>("aftermarket-gradient");
    REQUIRE(overlay && overlay->isVisible() && overlay->testAttribute(Qt::WA_TransparentForMouseEvents));
    auto fill=augment::gradientFill(slider);REQUIRE(fill.isValid() && fill.width()<slider->width()/2);
    auto im=slider->grab().toImage();REQUIRE(colored(im,fill)>10);
    REQUIRE(colored(im,QRect(slider->width()*3/4,0,slider->width()/4,slider->height()))==0);
    auto before=progress->grab().toImage();wait(160);auto after=progress->grab().toImage();REQUIRE(before!=after);
    REQUIRE(colored(after,augment::gradientFill(progress))>10);
    REQUIRE(augment::gradientFill(vertical).center().y()>vertical->height()/2);
    slider->setValue(80);wait(40);REQUIRE(augment::gradientFill(slider).width()>fill.width());
    slider->setInvertedAppearance(true);slider->setValue(25);wait(40);
    REQUIRE(augment::gradientFill(slider).center().x()>slider->width()/2);
    slider->setValue(0);wait(40);REQUIRE(!augment::gradientFill(slider).isValid() && !overlay->isVisible());
    progress->setRange(0,0);wait(60);REQUIRE(!augment::gradientFill(progress).isValid());
    progress->setRange(0,100);progress->setValue(60);
    slider->setValue(50);wait(60);
    REQUIRE(controller.setMotionEnabled(false));wait(60);
    before=progress->grab().toImage();wait(160);REQUIRE(before==progress->grab().toImage());
    progress->setTextVisible(true);wait(40);
    REQUIRE(augment::gradientFill(progress).height()<=3);
    slider->setDisabled(true);wait(40);REQUIRE(!overlay->isVisible());
    REQUIRE(!controller.setGradientSettings(QJsonObject{{"enabled",true},{"period",0}}));
    REQUIRE(controller.gradientSettings().value("period").toInt()==1000);
    slider->setEnabled(true);slider->setInvertedAppearance(false);slider->setValue(50);
    slider->setStyleSheet("QSlider::groove:horizontal { background:#29233a; height:14px; border-radius:7px; } QSlider::sub-page:horizontal { background:#31748f; } QSlider::handle:horizontal { background:white; width:14px; height:14px; margin:0px; border-radius:7px; }");
    wait(60);
    const auto styledFill=augment::gradientFill(slider);
    std::cout<<"Stylesheet fill "<<styledFill.width()<<"x"<<styledFill.height()<<" @ "<<styledFill.x()<<","<<styledFill.y()<<"\n";
    REQUIRE(styledFill.isValid() && styledFill.height()==14);
    REQUIRE(colored(slider->grab().toImage(),styledFill)>10);
    // Match Colorway: a round white handle on a cyan native filled page.
    slider->setStyleSheet("QSlider::groove:horizontal { background:#29233a; height:14px; border-radius:7px; } QSlider::sub-page:horizontal { background:#00ffff; border-top-left-radius:7px; border-bottom-left-radius:7px; } QSlider::add-page:horizontal { background:#29233a; border-top-right-radius:7px; border-bottom-right-radius:7px; } QSlider::handle:horizontal { background:white; width:14px; height:14px; margin:0px; border-radius:7px; }");
    auto checkJoin=[&](QSlider *target,int value,bool reverse) {
        target->setValue(value);wait(40);
        QStyleOptionSlider opt;opt.initFrom(target);opt.orientation=target->orientation();
        const bool h=opt.orientation==Qt::Horizontal;
        if(h)opt.state|=QStyle::State_Horizontal;
        opt.minimum=target->minimum();opt.maximum=target->maximum();opt.sliderPosition=value;opt.sliderValue=value;
        opt.upsideDown=reverse;opt.direction=Qt::LeftToRight;opt.subControls=QStyle::SC_All;
        auto thumb=target->style()->subControlRect(QStyle::CC_Slider,&opt,QStyle::SC_SliderHandle,target);
        auto joined=target->grab().toImage();
        const auto filled=augment::gradientFill(target);
        auto zone=thumb.adjusted(h&&!reverse?-6:0,!h&&!reverse?-6:0,h&&reverse?6:0,!h&&reverse?6:0).intersected(filled);
        // Exclude the physical groove's outer cap, not the thumb's corners.
        if(h)zone=zone.intersected(target->rect().adjusted(7,1,-7,-1));
        else zone=zone.intersected(target->rect().adjusted(1,7,-1,-7));
        int cyan=0;
        for(int y=zone.top();y<=zone.bottom();++y)for(int x=zone.left();x<=zone.right();++x) {
            const auto c=joined.pixelColor(x,y);
            if(c.red()<80&&c.green()>170&&c.blue()>170)++cyan;
        }
        std::cout<<"Native cyan seam pixels ("<<value<<", "<<h<<", "<<reverse<<"): "<<cyan<<"\n";
        return cyan==0&&joined.pixelColor(thumb.center())==QColor(Qt::white);
    };
    for(int value:{5,50,95,100})REQUIRE(checkJoin(slider,value,false));
    slider->setInvertedAppearance(true);
    slider->setStyleSheet("QSlider::groove:horizontal { background:#29233a; height:14px; border-radius:7px; } QSlider::sub-page:horizontal { background:#29233a; } QSlider::add-page:horizontal { background:#00ffff; border-top-right-radius:7px; border-bottom-right-radius:7px; } QSlider::handle:horizontal { background:white; width:14px; height:14px; margin:0px; border-radius:7px; }");
    REQUIRE(checkJoin(slider,50,true));
    slider->setInvertedAppearance(false);slider->setLayoutDirection(Qt::RightToLeft);
    REQUIRE(checkJoin(slider,50,true));
    vertical->setStyleSheet("QSlider::groove:vertical { background:#29233a; width:14px; border-radius:7px; } QSlider::sub-page:vertical { background:#29233a; } QSlider::add-page:vertical { background:#00ffff; border-bottom-left-radius:7px; border-bottom-right-radius:7px; } QSlider::handle:vertical { background:white; width:14px; height:14px; margin:0px; border-radius:7px; }");
    REQUIRE(checkJoin(vertical,55,true));
    REQUIRE(controller.setGradientSettings(QJsonObject{{"enabled",false}}));wait(40);
    REQUIRE(!slider->findChild<QWidget*>("aftermarket-gradient"));
    REQUIRE(!progress->findChild<QWidget*>("aftermarket-gradient"));
    std::cout<<"gradient pixels, fill boundaries, native position, visibility and reduced motion passed\n";
}
