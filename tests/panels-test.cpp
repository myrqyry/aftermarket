#include "panels.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPointer>
#include <QScreen>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(false)
int main(int argc,char **argv) {
 QApplication app(argc,argv);app.setStyle("Fusion");
 QMainWindow main;main.setCentralWidget(new QLabel("Preview",&main));
 auto *a=new QDockWidget("Sources",&main);a->setObjectName("sources");
 auto *body=new QLabel("Native contents",a);body->setMinimumSize(100,120);a->setWidget(body);
 auto *b=new QDockWidget("Scenes",&main);b->setWidget(new QLabel("Scenes",b));
 main.addDockWidget(Qt::LeftDockWidgetArea,a);main.splitDockWidget(a,b,Qt::Vertical);
 main.resize(640,480);main.show();app.processEvents();
 augment::Panels panels;
 const int height=a->height(),max=a->maximumHeight(),min=a->minimumHeight(),width=a->width(),minWidth=a->minimumWidth();
 REQUIRE(panels.canCollapse(a));REQUIRE(panels.toggle(a));app.processEvents();
 REQUIRE(panels.collapsed(a)&&body->isHidden()&&!a->isHidden());
 REQUIRE(a->height()<height/2); REQUIRE(a->width()>=width);
 REQUIRE(panels.toggle(a));app.processEvents();
 REQUIRE(!panels.collapsed(a)&&a->widget()==body&&!body->isHidden());
 REQUIRE(a->maximumHeight()==max&&a->minimumHeight()==min&&a->height()>height/2); REQUIRE(a->minimumWidth()==minWidth);
 const QPointF point(12,4);
 QMouseEvent click(QEvent::MouseButtonRelease,point,a->mapToGlobal(point.toPoint()),Qt::MiddleButton,Qt::NoButton,Qt::NoModifier);
 QApplication::sendEvent(a,&click);app.processEvents();
 REQUIRE(panels.collapsed(a));
 panels.restoreAll();app.processEvents();REQUIRE(!body->isHidden()&&!panels.collapsed(a));
 main.tabifyDockWidget(a,b);app.processEvents();REQUIRE(!panels.canCollapse(a));
 main.removeDockWidget(b);b->hide();app.processEvents();
 REQUIRE(panels.canCollapse(a));REQUIRE(panels.toggle(a));a->hide();app.processEvents();
 REQUIRE(!panels.collapsed(a)&&!body->isHidden());
 // Preview must be the original OBS widget, including its live child
 // display and Studio Mode additions—not a duplicate renderer.
 QMainWindow previewMain;
 auto *central=new QWidget(&previewMain);central->setObjectName("centralwidget");
 auto *centralLayout=new QVBoxLayout(central);
 auto *canvas=new QWidget(central);canvas->setObjectName("canvasEditor");
 auto *canvasLayout=new QHBoxLayout(canvas);
 auto *previewContainer=new QWidget(canvas);previewContainer->setObjectName("previewContainer");
 auto *preview=new QWidget(previewContainer);preview->setObjectName("preview");
 canvasLayout->addWidget(previewContainer);
 centralLayout->addWidget(canvas);
 auto *context=new QLabel("Source tools",central);context->setObjectName("contextContainer");
 centralLayout->addWidget(context);
 previewMain.setCentralWidget(central);
 auto *otherDock=new QDockWidget("Sources",&previewMain);otherDock->setWidget(new QLabel("Source list"));
 previewMain.addDockWidget(Qt::LeftDockWidgetArea,otherDock);
 previewMain.resize(900,600);previewMain.show();app.processEvents();
 REQUIRE(!panels.previewDocked(&previewMain));
 auto *panelPage=panels.createPage(&previewMain,&previewMain);
 auto *previewButton=panelPage->findChild<QPushButton*>("augment-preview-toggle");
 auto *showButton=panelPage->findChild<QPushButton*>("augment-preview-show");
 REQUIRE(previewButton&&previewButton->isEnabled()&&previewButton->text()=="Make preview dockable");
 REQUIRE(showButton&&!showButton->isEnabled());
 previewButton->click();app.processEvents();
 REQUIRE(panels.previewDocked(&previewMain));
 REQUIRE(previewButton->text()=="Restore fixed preview");
 REQUIRE(panels.setPreviewDocked(&previewMain,true));app.processEvents();
 QPointer<QDockWidget> dock=panels.previewDock(&previewMain);
 REQUIRE(dock&&dock->isVisible()&&dock->widget()==central);
 REQUIRE(preview->parentWidget()==previewContainer&&canvas->findChild<QWidget*>("preview")==preview);
 REQUIRE(previewMain.centralWidget()==nullptr&&central->findChild<QWidget*>("contextContainer")==context);
 REQUIRE(centralLayout->indexOf(canvas)==0&&centralLayout->indexOf(context)>=0);
 REQUIRE(previewMain.dockWidgetArea(dock)!=Qt::NoDockWidgetArea);
 auto *program=new QWidget(canvas);program->setObjectName("program");canvasLayout->addWidget(program);
 app.processEvents();
 REQUIRE(canvas->findChild<QWidget*>("program")==program);
 REQUIRE(canvasLayout->direction()==QBoxLayout::BottomToTop);
 canvasLayout->removeWidget(program);program->hide();app.processEvents();
 REQUIRE(canvasLayout->direction()==QBoxLayout::LeftToRight);
 canvasLayout->addWidget(program);program->show();app.processEvents();
 REQUIRE(canvasLayout->direction()==QBoxLayout::BottomToTop);
 // If closed, the panel can be shown again without touching native display state.
 dock->close();app.processEvents();
 REQUIRE(panels.previewDocked(&previewMain)&&!dock->isVisible());
 showButton->click();app.processEvents();
 REQUIRE(panels.setPreviewDocked(&previewMain,true));app.processEvents();
 REQUIRE(dock->isVisible());
 const QSize originalDockSize=dock->size();
 dock->setFloating(true);app.processEvents();
 REQUIRE(dock->isFloating());
 REQUIRE(dock->width()<=qMax(originalDockSize.width()+64,app.primaryScreen()->availableGeometry().width()*4/5));
 const auto bounds=app.primaryScreen()->availableGeometry();
 REQUIRE(bounds.intersects(dock->frameGeometry()));
 REQUIRE(panels.setPreviewDocked(&previewMain,false));app.processEvents();
 REQUIRE(!panels.previewDocked(&previewMain));
 REQUIRE(previewMain.centralWidget()==central&&canvas->parentWidget()==central&&centralLayout->indexOf(canvas)==0);
 REQUIRE(canvas->findChild<QWidget*>("preview")==preview);
 REQUIRE(canvas->findChild<QWidget*>("program")==program);
 REQUIRE(canvasLayout->direction()==QBoxLayout::LeftToRight);
 REQUIRE(dock.isNull());
 REQUIRE(previewMain.findChildren<QDockWidget*>("aftermarket-preview-dock").isEmpty());
 REQUIRE(panels.setPreviewDocked(&previewMain,true));
 panels.restorePreview(&previewMain);app.processEvents();
 REQUIRE(!panels.previewDocked(&previewMain));
 // Invalid host: refuse without disturbing native content.
 // Native OBS starts Studio Mode with program widgets already in
 // previewLayout; capture of its current item count must not treat that
 // pre-existing program as the ordinary single-preview baseline.
 QMainWindow studioMain;
 auto *studioCentral=new QWidget(&studioMain);
 auto *studioRoot=new QVBoxLayout(studioCentral);
 auto *studioCanvas=new QWidget(studioCentral);studioCanvas->setObjectName("canvasEditor");
 auto *studioLayout=new QHBoxLayout(studioCanvas);
 auto *studioPreview=new QWidget(studioCanvas);
 studioPreview->setObjectName("preview");
 studioLayout->addWidget(studioPreview);
 studioLayout->addWidget(new QWidget(studioCanvas)); // disabled preview
 studioLayout->addWidget(new QWidget(studioCanvas)); // program options
 studioLayout->addWidget(new QWidget(studioCanvas)); // program video
 studioRoot->addWidget(studioCanvas);
 auto *studioContext=new QLabel("tools",studioCentral);studioContext->setObjectName("contextContainer");
 studioRoot->addWidget(studioContext);
 studioMain.setCentralWidget(studioCentral);studioMain.resize(600,580);studioMain.show();app.processEvents();
 REQUIRE(panels.setPreviewDocked(&studioMain,true));app.processEvents();
 REQUIRE(studioLayout->direction()==QBoxLayout::BottomToTop);
 REQUIRE(panels.setPreviewDocked(&studioMain,false));app.processEvents();
 REQUIRE(studioLayout->direction()==QBoxLayout::LeftToRight);
 REQUIRE(studioMain.centralWidget()==studioCentral);
 REQUIRE(panels.setPreviewDocked(&studioMain,true));app.processEvents();
 studioMain.close();app.processEvents();
 REQUIRE(!panels.previewDocked(&studioMain));
 REQUIRE(studioMain.centralWidget()==studioCentral);
 REQUIRE(studioMain.findChildren<QDockWidget*>("aftermarket-preview-dock").isEmpty());
 QMainWindow invalid;invalid.setCentralWidget(new QLabel("nothing"));
 REQUIRE(!panels.setPreviewDocked(&invalid,true));
 std::cout<<"dock height, native content identity, restore, header gesture, tab guard and preview docking passed\n";
}
