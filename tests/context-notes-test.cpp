#include "context-notes.h"
#include "quick-find.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QListWidget>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"line "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
int main(int argc,char **argv){
 QApplication app(argc,argv);
 QTemporaryDir tmp;
 const auto path=tmp.filePath("notes.json");
 augment::ContextNotes notes(path);
 const auto source=augment::noteKey("source","abc"),filter=augment::noteKey("filter","abc","owner");
 const auto item=augment::noteKey("item","abc","scene",42);
 CHECK(!source.isEmpty()&&source!=filter&&source!=item&&item!=augment::noteKey("item","abc","scene",43));
 CHECK(notes.put(source,{},"Check white balance <b>literal</b>"));
 CHECK(notes.put(filter,{},"Filter only")&&notes.put(item,{},"Placement only"));
 CHECK(!notes.put(source,{},"Stale overwrite"));
 augment::ContextNotes reopened(path);
 CHECK(reopened.get(source)=="Check white balance <b>literal</b>"&&reopened.get(filter)=="Filter only"&&reopened.get(item)=="Placement only");
 CHECK(!(QFileInfo(path).permissions()&(QFileDevice::ReadGroup|QFileDevice::ReadOther)));
 CHECK(!reopened.put(source,reopened.get(source),QString(5000,'x')));
 CHECK(reopened.put(source,reopened.get(source),{}));
 CHECK(reopened.get(source).isEmpty()&&reopened.get(filter)=="Filter only");
 QFile f(path);CHECK(f.open(QIODevice::WriteOnly|QIODevice::Truncate));f.write("broken");f.close();
 CHECK(!reopened.put(source,{},"Do not replace corrupt store"));
 CHECK(f.open(QIODevice::ReadOnly)&&f.readAll()=="broken");f.close();
 QListWidget list;list.resize(300,100);list.show();
 auto *row=new QListWidgetItem("Camera",&list);
 row->setData(augment::NoteTextRole,"Private <b>literal</b>");
 int clicked=0;
 augment::attachNoteRows(&list,[&](QListWidgetItem *hit){if(hit==row)++clicked;});
 app.processEvents();
 const QRect badge=augment::noteBadgeRect(list.visualItemRect(row),list.layoutDirection());
 auto p=badge.center();
 QMouseEvent press(QEvent::MouseButtonPress,p,list.viewport()->mapToGlobal(p),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
 QApplication::sendEvent(list.viewport(),&press);
 QMouseEvent release(QEvent::MouseButtonRelease,p,list.viewport()->mapToGlobal(p),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
 QApplication::sendEvent(list.viewport(),&release);
 CHECK(clicked==1);
 row->setData(augment::NoteTextRole,QString{});
 QApplication::sendEvent(list.viewport(),&press);QApplication::sendEvent(list.viewport(),&release);
 CHECK(clicked==1);
 return 0;
}
