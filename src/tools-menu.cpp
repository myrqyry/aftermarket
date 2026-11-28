#include "tools-menu.h"
namespace augment {
ToolsMenu::ToolsMenu(QAction *anchor):anchor_(anchor){
    if(!anchor)return;
    menu_=new QMenu("Aftermarket",qobject_cast<QWidget*>(anchor->parent()));
    menu_->setObjectName("aftermarket-tools-menu");
    anchor->setObjectName("aftermarket-tools");
    anchor->setMenu(menu_);
    auto add=[this](const QString &name,const QString &id){
        auto *action=menu_->addAction(name);
        action->setObjectName("aftermarket-tools-"+id);
        action->setMenuRole(QAction::NoRole);
        actions_.append(action);
    };
    add("Workspace","workspace");
    add("Find Sources…","find");
    add("Command Center…","command-center");
    separators_.append(menu_->addSeparator());
    add("Emergency restore (Ctrl+Alt+Shift+F12)","emergency");
    add("Resume UI customizations","resume");
    add("Restart OBS…","restart");
    setState(false,false);
}
ToolsMenu::~ToolsMenu(){
    if(anchor_)anchor_->setMenu(nullptr);
    delete menu_.data();
    delete anchor_.data();
}
QAction *ToolsMenu::action(Command command)const{
    const auto index=static_cast<int>(command);
    return index>=0&&index<actions_.size()?actions_.at(index).data():nullptr;
}
void ToolsMenu::setState(bool active,bool frontendReady){
    for(int i=0;i<actions_.size();++i){
        auto *a=actions_[i].data();if(!a)continue;
        const bool resume=i==static_cast<int>(Command::Resume);
        const bool needsReady=i==static_cast<int>(Command::Find)||i==static_cast<int>(Command::Workspace)||i==static_cast<int>(Command::CommandCenter)||i==static_cast<int>(Command::Restart);
        a->setVisible(resume?!active:active);
        a->setEnabled(resume?!active:active&&(!needsReady||frontendReady));
    }
    for(const auto &separator:separators_)if(separator)separator->setVisible(active);
    if(anchor_)anchor_->setEnabled(true);
}
}
