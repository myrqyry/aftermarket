#include "health-stats.h"
#include <QMainWindow>
#include <QStatusBar>
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
    :settingsPath_(std::move(settingsPath)),provider_(std::move(provider)),automatic_(automatic){
    if(!settingsPath_.isEmpty()){
        QSettings s(settingsPath_,QSettings::IniFormat);enabled_=s.value("health/enabled",true).toBool();
    }
    if(!main)return;
    auto *bar=main->statusBar();
    auto *button=new QToolButton(bar);button_=button;
    button->setObjectName("aftermarket-health-stats");
    button->setAutoRaise(true);button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setIcon(button->style()->standardIcon(QStyle::SP_MessageBoxWarning));
    button->setAccessibleName("Performance alerts");
    button->hide();bar->addPermanentWidget(button);button->hide();
    connect(button,&QToolButton::clicked,this,[this]{
        if(!button_)return;
        // Modeless menu: sampling continues, and there is no focus-stealing
        // popup when an alert first occurs.
        auto *menu=new QMenu(button_);menu->setAttribute(Qt::WA_DeleteOnClose);
        for(const auto &line:button_->toolTip().split('\n')){
            auto*a=menu->addAction(line);a->setEnabled(false);
        }
        auto *close=menu->addAction("Close");
        connect(close,&QAction::triggered,menu,&QMenu::close);
        connect(menu,&QMenu::aboutToHide,menu,&QObject::deleteLater);
        menu->popup(button_->mapToGlobal(QPoint(0,0))-QPoint(0,menu->sizeHint().height()));
    });
    timer_.setInterval(1000);connect(&timer_,&QTimer::timeout,this,&HealthStats::poll);
    if(enabled_&&automatic_)timer_.start();
}
HealthStats::~HealthStats(){timer_.stop();delete button_.data();}
bool HealthStats::setEnabled(bool enabled){
    if(!settingsPath_.isEmpty()){
        QSettings s(settingsPath_,QSettings::IniFormat);s.setValue("health/enabled",enabled);s.sync();
        if(s.status()!=QSettings::NoError)return false;
    }
    enabled_=enabled;policy_.reset();
    if(enabled_&&automatic_)timer_.start();else timer_.stop();
    if(button_)button_->hide();
    return true;
}
void HealthStats::poll(){
    if(!enabled_||!button_||!provider_)return;
    const auto s=provider_();const auto reasons=policy_.update(s);
    if(!s.valid){button_->hide();return;}
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
    if(button_->text()!=title)button_->setText(title);
    if(button_->toolTip()!=tip)button_->setToolTip(tip);
    button_->setVisible(!reasons.isEmpty());
}
}
