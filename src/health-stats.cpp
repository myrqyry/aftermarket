#include "health-stats.h"
#include <QMainWindow>
#include <QStatusBar>
#include <QMenuBar>
#include <QHBoxLayout>
#include <QToolButton>
#include <QSettings>
#include <QMenu>
#include <QStyle>
#include <cmath>

namespace augment {
QStringList HealthPolicy::update(const HealthSample &s){
    if(!s.valid){reset();return {};}
    QStringList reasons;
    auto issue=[&](const QString &name,bool available,bool high,bool immediate){
        if(!available){states_.remove(name);return;}
        auto &state=states_[name];
        state.high=high?qMin(3,state.high+1):0;
        if(high&&(immediate||state.high>=3))state.hold=6;
        else if(state.hold>0)--state.hold;
        if(state.hold>0)reasons.append(name);
    };
    auto pressure=[&](const QString&name,std::optional<double> value,double threshold){
        const bool valid=value&&std::isfinite(*value)&&*value>=0;
        issue(name,valid,valid&&*value>=threshold,false);
    };
    const auto p=previous_;
    const bool rendering=p&&s.rendered>=p->rendered&&s.renderMissed>=p->renderMissed;
    const bool encoding=p&&s.encoded>=p->encoded&&s.encodeSkipped>=p->encodeSkipped;
    issue("Rendering lag",rendering,rendering&&s.renderMissed>p->renderMissed,true);
    issue("Encoding lag",encoding,encoding&&s.encodeSkipped>p->encodeSkipped,true);
    if(!p||!p->stream||p->streamId!=s.streamId){states_.remove("Reconnecting");states_.remove("Connection congestion");}
    const bool sameStream=s.stream&&p&&p->stream&&s.streamId==p->streamId&&s.networkDropped>=p->networkDropped;
    issue("Network drops",sameStream,sameStream&&s.networkDropped>p->networkDropped,true);
    issue("Reconnecting",s.stream,s.reconnecting,true);
    pressure("Connection congestion",s.stream?s.congestion:std::nullopt,.6);
    pressure("OBS CPU",s.obsCpu,90);
    pressure("System CPU",s.systemCpu,90);
    pressure("GPU load",s.gpu,90);
    pressure("Render budget",s.renderBudget,90);
    previous_=s;
    return reasons;
}
namespace {
QString percent(std::optional<double> value,double multiplier=1.){
    return value&&std::isfinite(*value)&&*value>=0?QString::number(*value*multiplier,'f',0)+"%":QStringLiteral("Unavailable");
}
}
HealthStats::HealthStats(QMainWindow *main,QString settingsPath,Provider provider,bool automatic)
    :main_(main),settingsPath_(std::move(settingsPath)),provider_(std::move(provider)),automatic_(automatic){
    if(!settingsPath_.isEmpty()){
        QSettings s(settingsPath_,QSettings::IniFormat);enabled_=s.value("health/enabled",true).toBool();
        const int position=s.value("health/placement",0).toInt();
        if(position>=0&&position<=2)placement_=static_cast<HealthPlacement>(position);
    }
    if(!main)return;
    button_=makeButton("aftermarket-health-stats");
    menuButton_=makeButton("aftermarket-health-stats-menu");
    placeButtons();
    timer_.setInterval(1000);connect(&timer_,&QTimer::timeout,this,&HealthStats::poll);
    if(enabled_&&automatic_)timer_.start();
}
QToolButton *HealthStats::makeButton(const QString &name){
    auto *button=new QToolButton(main_);
    button->setObjectName(name);
    button->setAutoRaise(true);button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setIcon(button->style()->standardIcon(QStyle::SP_MessageBoxWarning));
    button->setAccessibleName("Performance alerts");
    button->hide();
    connect(button,&QToolButton::clicked,this,[this,button]{
        auto *menu=new QMenu(button);menu->setAttribute(Qt::WA_DeleteOnClose);
        for(const auto &line:button->toolTip().split('\n')){
            auto*a=menu->addAction(line);a->setEnabled(false);
        }
        auto *close=menu->addAction("Close");
        connect(close,&QAction::triggered,menu,&QMenu::close);
        connect(menu,&QMenu::aboutToHide,menu,&QObject::deleteLater);
        const bool top=main_&&button->parentWidget()!=main_->statusBar();
        const auto pos=top?QPoint(0,button->height()):QPoint(0,-menu->sizeHint().height());
        menu->popup(button->mapToGlobal(pos));
    });
    return button;
}
void HealthStats::refreshMenuGeometry(){
    if(!main_)return;
    auto *bar=main_->menuBar();
    if(auto *host=bar->findChild<QWidget*>("aftermarket-menu-settings-host")){
        host->updateGeometry();
        if(auto *corner=bar->cornerWidget(Qt::TopRightCorner)){
            corner->updateGeometry();corner->adjustSize();
        }
        bar->updateGeometry();bar->update();
    }
}
void HealthStats::syncVisibility(){
    if(!main_||!button_||!menuButton_)return;
    const bool show=enabled_&&alert_;
    const bool firstChanged=button_->isHidden()==show;
    button_->setVisible(show);
    const bool second=show&&placement_==HealthPlacement::Both&&menuButton_->parentWidget()!=main_;
    const bool secondChanged=menuButton_->isHidden()==second;
    menuButton_->setVisible(second);
    if(firstChanged||secondChanged)refreshMenuGeometry();
}
void HealthStats::placeButtons(){
    if(!main_||!button_||!menuButton_)return;
    auto *bar=main_->menuBar();
    auto *host=bar->isNativeMenuBar()?nullptr:bar->findChild<QWidget*>("aftermarket-menu-settings-host");
    auto *layout=host?qobject_cast<QHBoxLayout*>(host->layout()):nullptr;
    auto *gear=host?host->findChild<QToolButton*>("aftermarket-menu-settings"):nullptr;
    const bool topAvailable=layout&&gear;
    auto move=[&](QToolButton *button,bool top,bool used){
        button->hide();
        if(button->parentWidget()==main_->statusBar())main_->statusBar()->removeWidget(button);
        else if(auto *parent=button->parentWidget();parent&&parent->layout())parent->layout()->removeWidget(button);
        if(!used)button->setParent(main_);
        else if(top&&topAvailable){layout->insertWidget(layout->indexOf(gear),button);}
        else main_->statusBar()->addPermanentWidget(button);
        button->hide();
    };
    move(button_,placement_==HealthPlacement::MenuBar,true);
    move(menuButton_,true,placement_==HealthPlacement::Both&&topAvailable);
    syncVisibility();refreshMenuGeometry();
}
HealthStats::~HealthStats(){timer_.stop();delete button_.data();delete menuButton_.data();refreshMenuGeometry();}
bool HealthStats::setPlacement(HealthPlacement placement){
    const int value=static_cast<int>(placement);if(value<0||value>2)return false;
    if(!settingsPath_.isEmpty()){
        QSettings s(settingsPath_,QSettings::IniFormat);s.setValue("health/placement",value);s.sync();
        if(s.status()!=QSettings::NoError)return false;
    }
    if(placement_==placement)return true;
    placement_=placement;placeButtons();return true;
}
bool HealthStats::setEnabled(bool enabled){
    if(!settingsPath_.isEmpty()){
        QSettings s(settingsPath_,QSettings::IniFormat);s.setValue("health/enabled",enabled);s.sync();
        if(s.status()!=QSettings::NoError)return false;
    }
    enabled_=enabled;policy_.reset();
    if(enabled_&&automatic_)timer_.start();else timer_.stop();
    alert_=false;syncVisibility();
    return true;
}
void HealthStats::poll(){
    if(!enabled_||!button_||!provider_)return;
    const auto s=provider_();const auto reasons=policy_.update(s);
    if(!s.valid){alert_=false;syncVisibility();return;}
    QString title=reasons.isEmpty()?QString{}:reasons.first();
    if(title=="GPU load")title+=" "+percent(s.gpu);
    else if(title=="System CPU")title+=" "+percent(s.systemCpu);
    else if(title=="OBS CPU")title+=" "+percent(s.obsCpu);
    else if(title=="Render budget")title+=" "+percent(s.renderBudget);
    else if(title=="Connection congestion")title+=" "+percent(s.congestion,100.);
    if(reasons.size()>1)title+=QString(" +%1").arg(reasons.size()-1);
    const QString tip=QStringList{
        reasons.isEmpty()?QStringLiteral("Performance normal"):"Performance alerts: "+reasons.join(", "),
        "OBS CPU: "+percent(s.obsCpu),"System CPU: "+percent(s.systemCpu),
        "GPU ("+(s.gpuLabel.isEmpty()?QStringLiteral("device load"):s.gpuLabel)+"): "+percent(s.gpu),
        "Render frame budget: "+percent(s.renderBudget),
        "Rendering missed (total): "+QString::number(s.renderMissed),
        "Encoding skipped (total): "+QString::number(s.encodeSkipped),
        s.stream?"Stream dropped (total): "+QString::number(s.networkDropped):QStringLiteral("Stream: inactive"),
        s.stream?"Connection congestion: "+percent(s.congestion,100.):QStringLiteral("Network metrics: inactive"),
        "Alerts clear after recovery."
    }.join('\n');
    bool changed=false;
    for(auto *button:{button_.data(),menuButton_.data()})if(button){
        if(button->text()!=title){button->setText(title);changed=true;}
        if(button->toolTip()!=tip)button->setToolTip(tip);
    }
    alert_=!reasons.isEmpty();syncVisibility();
    if(changed&&alert_)refreshMenuGeometry();
}
}
