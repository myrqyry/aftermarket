#include "augment.h"
#include "gradient.h"
#include <QApplication>
#include <QSlider>
#include <QDockWidget>
#include <QMainWindow>
#include <QLabel>
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
    const QVector<QColor> palette{QColor("#31748f"),QColor("#c4a7e7"),QColor("#ebbcba")};
    for(const QString &pattern:QStringList{"drift","aurora","tide","bloom"}){
        REQUIRE(augment::GradientSpec::patterns().contains(pattern));
        augment::GradientSpec parsed;
        REQUIRE(augment::GradientSpec::parse(QJsonObject{{"pattern",pattern},{"period",60000}},parsed));
        REQUIRE(parsed.pattern==pattern&&parsed.period==60000);
        const auto a=augment::gradientPatternStops(palette,pattern,0.);
        const auto b=augment::gradientPatternStops(palette,pattern,.31);
        REQUIRE(a.size()>=25&&a.size()<=65&&a!=b);
        REQUIRE(a.first().first==0.&&a.last().first==1.);
        REQUIRE(a.first().second==a.last().second&&b.first().second==b.last().second);
        REQUIRE(a==augment::gradientPatternStops(palette,pattern,1.));
        REQUIRE(b==augment::gradientPatternStops(palette,pattern,1.31));
        const auto near=augment::gradientPatternStops(palette,pattern,.9999);
        for(int i=0;i<a.size();++i){
            auto c=a[i].second,d=near[i].second;
            REQUIRE(qAbs(c.red()-d.red())<=2&&qAbs(c.green()-d.green())<=2&&qAbs(c.blue()-d.blue())<=2);
        }
        auto uniform=augment::gradientPatternStops({QColor("#336699"),QColor("#336699")},pattern,.37);
        for(auto stop:uniform)REQUIRE(stop.second==QColor("#336699"));
    }
    augment::GradientSpec parsed;REQUIRE(!augment::GradientSpec::parse(QJsonObject{{"period",60001}},parsed));
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
    QMainWindow main;main.setCentralWidget(new QLabel("Preview"));
    auto *dock=new QDockWidget("Sources",&main);dock->setWidget(new QLabel("Native sources"));
    main.addDockWidget(Qt::LeftDockWidgetArea,dock);main.resize(600,360);main.show();wait(60);
    const auto title=dock->windowTitle();auto *native=dock->widget();
    REQUIRE(controller.setMotionEnabled(true));
    spec["titlebars"]=true;spec["sliders"]=true;spec["progress"]=true;
    spec["colors"]=QJsonArray{"#ff00ff","#4820c0","#ff00ff"};
    for(const auto &pattern:augment::GradientSpec::patterns()){
        spec["pattern"]=pattern;
        REQUIRE(controller.setGradientSettings(spec));wait(60);
        auto *stripe=dock->findChild<QWidget*>("aftermarket-title-gradient");
        REQUIRE(stripe&&stripe->isVisible()&&stripe->height()<=3);
        REQUIRE(stripe->geometry().bottom()<native->geometry().top());
        REQUIRE(stripe->testAttribute(Qt::WA_TransparentForMouseEvents)&&stripe->focusPolicy()==Qt::NoFocus);
        const auto a=stripe->grab().toImage(),b=progress->grab().toImage();
        const bool animated=pattern!="static";
        bool stripeChanged=false,progressChanged=false;
        // Bounce/breathe can revisit the same pixels at symmetric phases.
        // Sample several points rather than requiring two arbitrary phases
        // to differ. Static must remain identical throughout.
        for(int sample=0;sample<6;++sample){
            wait(117);
            stripeChanged|=a!=stripe->grab().toImage();
            progressChanged|=b!=progress->grab().toImage();
            if(animated&&stripeChanged&&progressChanged)break;
        }
        if(stripeChanged!=animated||progressChanged!=animated)
            std::cerr<<"Pattern "<<pattern.toStdString()<<" title changed="<<stripeChanged<<" progress changed="<<progressChanged<<'\n';
        REQUIRE(stripeChanged==animated);
        REQUIRE(progressChanged==animated);
        REQUIRE(dock->windowTitle()==title&&dock->widget()==native&&dock->titleBarWidget()==nullptr);
    }
    auto invalid=spec;invalid["pattern"]="unknown";
    REQUIRE(!controller.setGradientSettings(invalid));
    spec["pattern"]="flow";REQUIRE(controller.setGradientSettings(spec));wait(60);
    auto *stripe=dock->findChild<QWidget*>("aftermarket-title-gradient");
    REQUIRE(controller.setMotionEnabled(false));wait(60);
    auto still=stripe->grab().toImage();wait(150);REQUIRE(still==stripe->grab().toImage());
    dock->setFloating(true);wait(60);REQUIRE(!stripe->isVisible());
    dock->setFloating(false);wait(60);REQUIRE(stripe->isVisible());
    spec["titlebars"]=false;REQUIRE(controller.setGradientSettings(spec));wait(40);
    REQUIRE(!dock->findChild<QWidget*>("aftermarket-title-gradient"));
    dock->setMinimumWidth(250);
    dock->setStyleSheet("QDockWidget::title { background: #282828; color: #eeeeee; padding: 4px; }");
    wait(40);
    const auto plain=dock->grab().toImage();
    spec["titlebarFill"]=true;spec["pattern"]="static";
    REQUIRE(controller.setGradientSettings(spec));wait(60);
    auto *header=dock->findChild<QWidget*>("aftermarket-title-gradient");
    REQUIRE(header&&header->isVisible()&&header->height()>10);
    REQUIRE(header->geometry().bottom()<native->geometry().top());
    auto decorated=dock->grab().toImage();
    int changed=0,ink=0,damaged=0;
    for(int y=0;y<native->geometry().top();++y)for(int x=0;x<dock->width();++x){
        const auto p=plain.pixelColor(x,y),q=decorated.pixelColor(x,y);
        if(p!=q)++changed;
        if(y>=1&&y<native->geometry().top()-2&&x<100&&p!=QColor("#282828")){++ink;if(p!=q)++damaged;}
    }
    std::cerr<<"header changed="<<changed<<" ink="<<ink<<" damaged="<<damaged<<" body top="<<native->geometry().top()<<" layer="<<header->geometry().x()<<","<<header->geometry().y()<<","<<header->width()<<","<<header->height()<<"\n";
    REQUIRE(changed>100&&ink>5&&damaged==0);
    REQUIRE(dock->widget()==native&&dock->windowTitle()==title&&!dock->titleBarWidget());
    dock->setFloating(true);wait(60);REQUIRE(!header->isVisible());
    dock->setFloating(false);wait(60);REQUIRE(header->isVisible());
    spec["titlebarFill"]=false;REQUIRE(controller.setGradientSettings(spec));wait(40);
    REQUIRE(!dock->findChild<QWidget*>("aftermarket-title-gradient"));
    REQUIRE(controller.setGradientSettings(QJsonObject{{"enabled",false}}));wait(40);
    REQUIRE(!slider->findChild<QWidget*>("aftermarket-gradient"));
    REQUIRE(!progress->findChild<QWidget*>("aftermarket-gradient"));
    std::cout<<"gradient pixels, fill boundaries, native position, visibility and reduced motion passed\n";
}
