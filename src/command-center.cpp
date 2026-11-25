#include "command-center.h"
#include <QMainWindow>
#include <QMenuBar>
#include <QAction>
#include <QLineEdit>
#include <QListWidget>
#include <QDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QApplication>
#include <QSignalBlocker>
namespace augment {
CommandCenter::CommandCenter(QMainWindow *main,QuickFind::Provider sources,SourceRunner sourceRunner,Commands commands,Runner runner)
 :main_(main),sourceProvider_(std::move(sources)),sourceRunner_(std::move(sourceRunner)),commandProvider_(std::move(commands)),runner_(std::move(runner)){
 if(!main)return;
 bar_=main->menuBar();
 if(bar_&&!bar_->isNativeMenuBar()){
 field_=new QLineEdit(bar_);field_->setObjectName("aftermarket-command-center");
 field_->setReadOnly(true);field_->setPlaceholderText("Search or run a command…");
 field_->setAccessibleName("Aftermarket command center");
 field_->setToolTip("@ sources, scenes and filters · / commands and hotkeys");
 field_->installEventFilter(this);
 bar_->installEventFilter(this);
 }
 main_->installEventFilter(this);
 popup_=new QDialog(main,Qt::Popup|Qt::FramelessWindowHint);
 popup_->setObjectName("aftermarket-command-popup");popup_->setWindowModality(Qt::NonModal);
 auto *layout=new QVBoxLayout(popup_);
 input_=new QLineEdit(popup_);input_->setObjectName("aftermarket-command-input");
 input_->setPlaceholderText("@ Search sources · / Commands and hotkeys");
 input_->setClearButtonEnabled(true);layout->addWidget(input_);
 list_=new QListWidget(popup_);list_->setObjectName("aftermarket-command-results");layout->addWidget(list_,1);
 auto *hint=new QLabel("↑ ↓ Select  ·  Enter Open  ·  Esc Close",popup_);layout->addWidget(hint);
 input_->installEventFilter(this);list_->installEventFilter(this);
 connect(input_,&QLineEdit::textChanged,this,[this]{updateResults();});
 connect(input_,&QLineEdit::returnPressed,this,[this]{activate();});
 connect(list_,&QListWidget::itemActivated,this,[this]{activate();});
 scheduleLayout();
}
CommandCenter::~CommandCenter(){detach();}
void CommandCenter::detach(){
 if(confirm_)delete confirm_.data();
 if(popup_)delete popup_.data();
 if(field_)delete field_.data();
 if(bar_)bar_->removeEventFilter(this);
 if(main_)main_->removeEventFilter(this);
 bar_.clear();main_.clear();
}
void CommandCenter::scheduleLayout(){
 if(scheduled_)return;scheduled_=true;
 QTimer::singleShot(0,this,[this]{scheduled_=false;layoutField();});
}
void CommandCenter::layoutField(){
 if(!bar_||!field_)return;
 // Do not reserve space in the menu bar's native layout or claim its
 // corner widget. Only occupy the proven unused interval.
 int left=6,right=bar_->width()-6;
 for(auto *a:bar_->actions())if(a->isVisible())left=qMax(left,bar_->actionGeometry(a).right()+8);
 if(auto *corner=bar_->cornerWidget(Qt::TopLeftCorner);corner&&corner->isVisible())left=qMax(left,corner->geometry().right()+8);
 if(auto *corner=bar_->cornerWidget(Qt::TopRightCorner);corner&&corner->isVisible())right=qMin(right,corner->geometry().left()-8);
 if(right-left<180||bar_->height()<20||!bar_->isVisible()){
  field_->hide();if(popup_&&popup_->isVisible())popup_->hide();return;
 }
 const int width=qMin(420,right-left);
 const int x=qBound(left,(bar_->width()-width)/2,right-width);
 const int height=qMin(bar_->height()-4,field_->sizeHint().height());
 const QRect rect(x,(bar_->height()-height)/2,width,height);
 if(field_->geometry()!=rect)field_->setGeometry(rect);
 if(!field_->isVisible()){field_->show();field_->raise();}
}
void CommandCenter::open(const QString &initial){
 if(!main_||!popup_||confirm_)return;
 sourceLoaded_=commandsLoaded_=false;sources_.clear();commands_.clear();
 {const QSignalBlocker blocker(input_);input_->setText(initial);}
 updateResults();
 const int width=qMin(560,qMax(260,main_->width()-16));
 popup_->resize(width,360);
 QPoint p=field_&&field_->isVisible()?field_->mapToGlobal(QPoint(field_->width()/2-width/2,field_->height()+4)):
     main_->mapToGlobal(QPoint((main_->width()-width)/2,40));
 if(auto *screen=QApplication::screenAt(p)){
  const auto bounds=screen->availableGeometry();
  p.setX(qBound(bounds.left(),p.x(),qMax(bounds.left(),bounds.right()-popup_->width()+1)));
  p.setY(qBound(bounds.top(),p.y(),qMax(bounds.top(),bounds.bottom()-popup_->height()+1)));
 }
 popup_->move(p);popup_->show();popup_->raise();input_->setFocus();input_->setCursorPosition(input_->text().size());
}
void CommandCenter::updateResults(){
 if(!list_||!input_)return;
 QString query=input_->text().trimmed();
 commandMode_=query.startsWith('/');
 if(query.isEmpty()){
  list_->clear();hits_.clear();commandHits_.clear();
  list_->addItem("@  Search sources, scenes and filters");
  list_->addItem("/  Commands and OBS hotkeys");
  list_->setCurrentRow(0);return;
 }
 if(query.startsWith('@')||commandMode_)query.remove(0,1);
 query=query.trimmed();list_->clear();hits_.clear();commandHits_.clear();
 if(commandMode_){
  if(!commandsLoaded_){commands_=commandProvider_?commandProvider_():QList<CommandChoice>{};commandsLoaded_=true;}
  const auto words=query.split(' ',Qt::SkipEmptyParts);
  for(const auto &entry:commands_){
   const QString search=entry.label+" "+entry.group;
   bool match=true;for(const auto &word:words)match&=search.contains(word,Qt::CaseInsensitive);
   if(!match)continue;
   commandHits_.append(entry);list_->addItem(entry.label+"  ·  "+entry.group);
   if(commandHits_.size()>=80)break;
  }
 }else{
  if(!sourceLoaded_){sources_=sourceProvider_?sourceProvider_():QList<QuickFindEntry>{};sourceLoaded_=true;}
  hits_=QuickFind::search(sources_,query);
  for(const auto &entry:hits_){
   const QString type=entry.kind==QuickFindEntry::Scene?"Scene":entry.kind==QuickFindEntry::Filter?"Filter":"Source";
   list_->addItem(entry.name+"  ·  "+type+(entry.location.isEmpty()?QString{}:"  ·  "+entry.location));
  }
 }
 if(list_->count())list_->setCurrentRow(0);
}
void CommandCenter::activate(){
 if(!list_||!popup_||!popup_->isVisible())return;
 const int row=list_->currentRow();
 if(row<0)return;
 if(input_->text().trimmed().isEmpty()){
  input_->setText(row==0?"@":"/");input_->setFocus();return;
 }
 if(commandMode_){
  if(row>=commandHits_.size())return;
  const auto choice=commandHits_[row];popup_->hide();
  if(choice.confirm){
   confirm_=new QMessageBox(QMessageBox::Question,"Run OBS hotkey?",choice.label+"\n"+choice.group,QMessageBox::Yes|QMessageBox::No,main_);
   confirm_->setTextFormat(Qt::PlainText);
   confirm_->setObjectName("aftermarket-command-confirm");confirm_->setDefaultButton(QMessageBox::No);
   confirm_->setAttribute(Qt::WA_DeleteOnClose);
   connect(confirm_,&QMessageBox::finished,this,[this,choice](int result){
    confirm_.clear();
    if(result==QMessageBox::Yes)QTimer::singleShot(0,this,[this,choice]{if(runner_)runner_(choice.id);});
   });
   confirm_->open();
  }else QTimer::singleShot(0,this,[this,choice]{if(runner_)runner_(choice.id);});
 }else{
  if(row>=hits_.size())return;
  const auto entry=hits_[row];popup_->hide();
  QTimer::singleShot(0,this,[this,entry]{if(sourceRunner_)sourceRunner_(entry);});
 }
}
bool CommandCenter::eventFilter(QObject *object,QEvent *event){
 const auto type=event->type();
 if(object==bar_||object==main_){
  if(type==QEvent::Resize||type==QEvent::Show||type==QEvent::Hide||type==QEvent::LayoutRequest||
     type==QEvent::ActionAdded||type==QEvent::ActionRemoved||type==QEvent::ActionChanged||type==QEvent::ChildAdded||type==QEvent::ChildRemoved||
     type==QEvent::StyleChange||type==QEvent::FontChange)scheduleLayout();
 }
 if(object==field_){
  if(type==QEvent::MouseButtonPress){open();return true;}
  if(type==QEvent::KeyPress){
   auto *key=static_cast<QKeyEvent*>(event);
   if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter||!key->text().isEmpty()){
    open(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter?QString{}:key->text());return true;
   }
  }
 }
 if((object==input_||object==list_)&&type==QEvent::KeyPress){
  auto *key=static_cast<QKeyEvent*>(event);
  if(key->key()==Qt::Key_Escape){popup_->reject();return true;}
  if(key->key()==Qt::Key_Down||key->key()==Qt::Key_Up){
   if(list_->count())list_->setCurrentRow(qBound(0,list_->currentRow()+(key->key()==Qt::Key_Down?1:-1),list_->count()-1));
   return true;
  }
 }
 return QObject::eventFilter(object,event);
}
}
