#include "context-notes.h"
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QLockFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QListWidget>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QPainter>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QPointer>
#include <QPushButton>
#include <QMessageBox>
namespace augment {
namespace {
constexpr qsizetype maxText=4096;
constexpr qint64 maxFile=4*1024*1024;
bool validKey(const QString &key){
 const auto a=QJsonDocument::fromJson(key.toUtf8()).array();
 if(a.size()!=4||!a[0].isString()||!a[1].isString()||!a[2].isString()||!a[3].isString())return false;
 const auto kind=a[0].toString(),id=a[1].toString(),owner=a[2].toString(),item=a[3].toString();
 auto token=[](const QString &v){return !v.isEmpty()&&v.size()<=128&&!v.contains(QChar(0));};
 if(!token(id))return false;
 if(kind=="source"||kind=="scene")return owner.isEmpty()&&item=="-1";
 if(kind=="filter")return token(owner)&&item=="-1";
 bool ok=false;const auto n=item.toLongLong(&ok);
 return kind=="item"&&token(owner)&&ok&&n>=0&&QString::number(n)==item;
}
class NoteRows final:public QStyledItemDelegate{
public:
 NoteRows(QListWidget *list,std::function<void(QListWidgetItem*)> edit)
  :QStyledItemDelegate(list),list_(list),edit_(std::move(edit)){
  list->viewport()->installEventFilter(this);
 }
 void paint(QPainter *p,const QStyleOptionViewItem &option,const QModelIndex &index)const override{
  const auto note=index.data(NoteTextRole).toString();
  if(note.isEmpty()){QStyledItemDelegate::paint(p,option,index);return;}
  auto content=option;
  const auto badge=noteBadgeRect(option.rect,option.direction);
  if(option.direction==Qt::RightToLeft)content.rect.adjust(22,0,0,0);
  else content.rect.adjust(0,0,-22,0);
  // Draw the native selection/background across the entire row first.
  auto bg=option;initStyleOption(&bg,index);bg.text.clear();bg.icon={};
  bg.features&=~(QStyleOptionViewItem::HasCheckIndicator|QStyleOptionViewItem::HasDecoration|QStyleOptionViewItem::HasDisplay);
  const auto *style=option.widget?option.widget->style():QApplication::style();
  style->drawControl(QStyle::CE_ItemViewItem,&bg,p,option.widget);
  QStyledItemDelegate::paint(p,content,index);
  p->save();p->setRenderHint(QPainter::Antialiasing);
  const auto group=option.state&QStyle::State_Enabled?QPalette::Active:QPalette::Disabled;
  const auto color=option.palette.color(group,option.state&QStyle::State_Selected?QPalette::HighlightedText:QPalette::Text);
  p->setPen(QPen(color,1.2,Qt::SolidLine,Qt::RoundCap));
  p->setBrush(Qt::NoBrush);
  const QRectF circle=QRectF(badge).adjusted(2.5,2.5,-2.5,-2.5);
  p->drawEllipse(circle);
  const QPointF c=circle.center();
  p->drawPoint(c+QPointF(0,-2.7));p->drawLine(c+QPointF(0,-.1),c+QPointF(0,3.));
  p->restore();
 }
 bool eventFilter(QObject *,QEvent *event)override{
  if(!list_)return false;
  const auto type=event->type();
  if(type==QEvent::ToolTip){
   auto *e=static_cast<QHelpEvent*>(event);
   auto *row=list_->itemAt(e->pos());
   if(row&&!row->data(NoteTextRole).toString().isEmpty()&&noteBadgeRect(list_->visualItemRect(row),list_->layoutDirection()).contains(e->pos())){
    QToolTip::showText(e->globalPos(),noteTooltip(row->data(NoteTextRole).toString()),list_->viewport(),noteBadgeRect(list_->visualItemRect(row),list_->layoutDirection()));
    return true;
   }
  }
  if(type==QEvent::MouseButtonPress||type==QEvent::MouseButtonRelease||type==QEvent::MouseButtonDblClick){
   auto *e=static_cast<QMouseEvent*>(event);if(e->button()!=Qt::LeftButton)return false;
   const auto pos=e->position().toPoint();
   auto *row=list_->itemAt(pos);
   const bool badge=row&&!row->data(NoteTextRole).toString().isEmpty()&&noteBadgeRect(list_->visualItemRect(row),list_->layoutDirection()).contains(pos);
   if(type==QEvent::MouseButtonPress){pressed_=badge?list_->indexAt(pos):QModelIndex{};return badge;}
   if(type==QEvent::MouseButtonDblClick)return badge;
   const bool had=pressed_.isValid(),same=had&&badge&&pressed_==list_->indexAt(pos);
   pressed_=QModelIndex{};
   if(same&&edit_)edit_(row);
   return had||badge;
  }
  return false;
 }
private:
 QPointer<QListWidget> list_;
 QPersistentModelIndex pressed_;
 std::function<void(QListWidgetItem*)> edit_;
};
}
QString noteKey(const QString &kind,const QString &id,const QString &owner,qint64 item){
 const auto key=QString::fromUtf8(QJsonDocument(QJsonArray{kind,id,owner,QString::number(item)}).toJson(QJsonDocument::Compact));
 return validKey(key)?key:QString{};
}
ContextNotes::ContextNotes(QString path):path_(std::move(path)){reload();}
QString ContextNotes::get(const QString &key)const{return notes_.value(key).toString();}
bool ContextNotes::read(QJsonObject &out)const{
 out={};QFile f(path_);if(!f.exists())return true;
 if(!f.open(QIODevice::ReadOnly)||f.size()>maxFile)return false;
 const auto doc=QJsonDocument::fromJson(f.read(maxFile+1));
 const auto root=doc.object();if(root.value("version").toInt()!=1||!root.value("notes").isObject())return false;
 out=root.value("notes").toObject();
 for(auto it=out.begin();it!=out.end();++it)
  if(!validKey(it.key())||!it.value().isString()||it.value().toString().isEmpty()||it.value().toString().size()>maxText||it.value().toString().contains(QChar(0)))return false;
 return true;
}
bool ContextNotes::reload(){QJsonObject next;if(!read(next))return false;if(notes_!=next){notes_=next;if(changed_)changed_();}return true;}
bool ContextNotes::put(const QString &key,const QString &expected,const QString &text){
 if(!validKey(key)||text.size()>maxText||text.contains(QChar(0))||!QDir().mkpath(QFileInfo(path_).absolutePath()))return false;
 QLockFile lock(path_+".lock");if(!lock.tryLock(0))return false;
 QJsonObject next;if(!read(next)||next.value(key).toString()!=expected)return false;
 if(text.trimmed().isEmpty())next.remove(key);else next.insert(key,text);
 const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"notes",next}}).toJson(QJsonDocument::Compact);
 if(bytes.size()>maxFile)return false;
 QSaveFile f(path_);f.setDirectWriteFallback(false);
 if(!f.open(QIODevice::WriteOnly)||!f.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner)||f.write(bytes)!=bytes.size()||!f.commit())return false;
 notes_=next;if(changed_)changed_();return true;
}
QString noteTooltip(const QString &text){return "<span>"+text.toHtmlEscaped().replace("\n","<br>")+"</span>";}
QRect noteBadgeRect(const QRect &row,Qt::LayoutDirection direction){
 const int side=qMin(18,row.height());
 return QRect(direction==Qt::RightToLeft?row.left()+2:row.right()-side-1,row.top()+(row.height()-side)/2,side,side);
}
void attachNoteRows(QListWidget *list,std::function<void(QListWidgetItem*)> edit){
 list->setItemDelegate(new NoteRows(list,std::move(edit)));
}
void editContextNote(QWidget *parent,std::shared_ptr<ContextNotes> notes,const QString &key,const QString &name,
                     std::function<bool()> valid,std::function<void()> changed){
 if(!parent||key.isEmpty()||!valid||!valid()||!notes)return;
 if(!notes->reload()){
  auto *error=new QMessageBox(QMessageBox::Warning,"Private note","The note store could not be read. Existing data has not been changed.",QMessageBox::Ok,parent);
  error->setAttribute(Qt::WA_DeleteOnClose);error->open();return;
 }
 const auto existing=notes->get(key);
 auto *dialog=new QDialog(parent);dialog->setObjectName("aftermarket-note-editor");
 dialog->setWindowTitle("Private note");dialog->setAttribute(Qt::WA_DeleteOnClose);
 auto *layout=new QVBoxLayout(dialog);
 auto *title=new QLabel(name,dialog);title->setTextFormat(Qt::PlainText);title->setWordWrap(true);layout->addWidget(title);
 auto *editor=new QPlainTextEdit(dialog);editor->setObjectName("aftermarket-note-text");editor->setPlainText(existing);editor->setPlaceholderText("Write a private note…");layout->addWidget(editor);
 auto *feedback=new QLabel("Visible only in Aftermarket. Clear the text and save to remove.",dialog);feedback->setWordWrap(true);layout->addWidget(feedback);
 auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,dialog);layout->addWidget(buttons);
 QObject::connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
 QObject::connect(buttons,&QDialogButtonBox::accepted,dialog,[dialog,editor,feedback,notes,key,existing,valid,changed]{
  const auto value=editor->toPlainText();
  if(!valid()){feedback->setText("Target no longer available. Nothing saved.");return;}
  if(!notes->put(key,existing,value)){feedback->setText("Could not save. Check the 4096-character limit or reopen if another edit changed this note.");return;}
  if(changed)changed();dialog->accept();
 });
 dialog->resize(390,230);dialog->open();editor->setFocus();
}
}
