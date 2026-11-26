#pragma once
#include "dock-header-button.h"
#include <QAction>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QPointer>

namespace augment {
// Keep the permanent native Settings shortcut separate from the optional
// editable strip. The strip may temporarily wrap this corner; detach it first.
class MenuSettingsCorner final {
public:
    MenuSettingsCorner(QMenuBar *bar,QAction *settings,std::function<bool()> motion={}){
        if(!bar||!settings||bar->isNativeMenuBar())return;
        bar_=bar;
        original_=bar->cornerWidget(Qt::TopRightCorner);
        host_=new QWidget(bar);
        host_->setObjectName("aftermarket-menu-settings-host");
        auto *layout=new QHBoxLayout(host_);
        layout->setContentsMargins(0,0,2,0);
        layout->setSpacing(4);
        if(original_){original_->setParent(host_);layout->addWidget(original_);}
        auto *gear=new DockHeaderButton(DockHeaderGlyph::Settings,host_);
        gear->setMotionProvider(std::move(motion));
        gear->setDefaultAction(settings);
        gear->setObjectName("aftermarket-menu-settings");
        gear->setAccessibleName(settings->text().remove('&'));
        gear->setToolButtonStyle(Qt::ToolButtonIconOnly);
        gear->setFixedSize(24,20);
        layout->addWidget(gear);
        bar->setCornerWidget(host_,Qt::TopRightCorner);
        host_->adjustSize();host_->show();
        bar->updateGeometry();bar->update();
    }
    ~MenuSettingsCorner(){
        if(!bar_||!host_)return;
        if(original_)original_->setParent(bar_);
        if(bar_->cornerWidget(Qt::TopRightCorner)==host_){
            bar_->setCornerWidget(original_,Qt::TopRightCorner);
            if(original_)original_->show();
        }
        delete host_.data();
        bar_->updateGeometry();bar_->update();
    }
    MenuSettingsCorner(const MenuSettingsCorner&)=delete;
    MenuSettingsCorner& operator=(const MenuSettingsCorner&)=delete;
private:
    QPointer<QMenuBar> bar_;
    QPointer<QWidget> original_,host_;
};
}
