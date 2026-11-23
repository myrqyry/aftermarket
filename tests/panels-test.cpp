#include "panels.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QMenu>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPointer>
#include <QToolButton>
#include <QButtonGroup>
#include <QFont>
#include <QScrollArea>
#include <QScrollBar>
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
 REQUIRE(panels.canCollapse(a));
 QMenu stateMenu;
 auto *collapse=panels.addCollapseAction(&stateMenu,a);
 REQUIRE(collapse==stateMenu.actions().first());
 REQUIRE(collapse->text()=="Collapse"&&collapse->isEnabled());
 collapse->trigger();app.processEvents();
 REQUIRE(panels.collapsed(a));
 stateMenu.clear();
 auto *expand=panels.addCollapseAction(&stateMenu,a);
 REQUIRE(expand->text()=="Expand"&&expand->isEnabled());
 expand->trigger();app.processEvents();
 REQUIRE(!panels.collapsed(a));
 REQUIRE(panels.toggle(a));app.processEvents();
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
 stateMenu.clear();
 auto *unavailable=panels.addCollapseAction(&stateMenu,a);
 REQUIRE(unavailable->text()=="Collapse"&&!unavailable->isEnabled());
 unavailable->trigger();app.processEvents();
 REQUIRE(!panels.collapsed(a));
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
 REQUIRE(centralLayout->indexOf(canvas)==1&&centralLayout->indexOf(context)>=0);
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
 // A real OBS-shaped studio layout has two native video surfaces and the
 // actual transition group. No replacement renderer or fake buttons.
 QMainWindow responsive;
 auto *workspace=new QWidget(&responsive);workspace->setObjectName("centralwidget");
 auto *workspaceLayout=new QVBoxLayout(workspace);
 auto *editor=new QWidget(workspace);editor->setObjectName("canvasEditor");
 auto *editorLayout=new QHBoxLayout(editor);
 auto *disabled=new QWidget(editor);disabled->setObjectName("previewDisabledWidget");disabled->hide();
 auto *previewPane=new QWidget(editor);previewPane->setObjectName("previewContainer");
 auto *previewLabel=new QLabel("Preview: Scene",previewPane);previewLabel->setObjectName("previewLabel");
 const QFont originalPreviewLabelFont=previewLabel->font();
 auto *previewDisplay=new QWidget(previewPane);previewDisplay->setObjectName("preview");
 auto *previewPaneLayout=new QVBoxLayout(previewPane);previewPaneLayout->addWidget(previewLabel);previewPaneLayout->addWidget(previewDisplay);
 editorLayout->addWidget(disabled);editorLayout->addWidget(previewPane);
 auto *options=new QWidget(editor);
 auto *optionsLayout=new QVBoxLayout(options);
 optionsLayout->addWidget(new QPushButton("Transition",options));
 optionsLayout->addWidget(new QLabel("Quick Transitions",options));
 int triggerCount=0;
 for(int i=0;i<12;++i){
     auto *native=new QPushButton(QString("Quick transition %1").arg(i),options);
     native->setProperty("id",i+1);
     QObject::connect(native,&QPushButton::clicked,[&]{++triggerCount;});
     optionsLayout->insertWidget(2+i,native);
 }
 auto *programPane=new QWidget(editor);
 auto *programPaneLayout=new QVBoxLayout(programPane);
 auto *programLabel=new QLabel("Program: Scene",programPane);programLabel->setObjectName("programLabel");
 auto *programDisplay=new QWidget(programPane);programDisplay->setObjectName("program");
 programPaneLayout->addWidget(programLabel);programPaneLayout->addWidget(programDisplay);
 editorLayout->addWidget(options);editorLayout->addWidget(programPane);
 workspaceLayout->addWidget(editor);
 auto *contextToolbar=new QWidget(workspace);contextToolbar->setObjectName("contextContainer");
 workspaceLayout->addWidget(contextToolbar);
 responsive.setCentralWidget(workspace);responsive.resize(820,620);responsive.show();app.processEvents();
 REQUIRE(panels.setPreviewDocked(&responsive,true));app.processEvents();
 auto *toolbar=workspace->findChild<QWidget*>("aftermarket-studio-toolbar");
 REQUIRE(toolbar&&toolbar->isVisible());
 auto *autoButton=toolbar->findChild<QToolButton*>("aftermarket-studio-auto");
 auto *bothButton=toolbar->findChild<QToolButton*>("aftermarket-studio-both");
 auto *programButton=toolbar->findChild<QToolButton*>("aftermarket-studio-program");
 auto *previewButtonMode=toolbar->findChild<QToolButton*>("aftermarket-studio-preview");
 REQUIRE(autoButton&&bothButton&&programButton&&previewButtonMode);
 REQUIRE(autoButton->isChecked());
 auto *studioTitle=toolbar->findChild<QLabel*>("aftermarket-studio-title");
 REQUIRE(studioTitle&&studioTitle->font().weight()==QFont::DemiBold);
 REQUIRE(studioTitle->font().family()==responsive.font().family());
 REQUIRE(studioTitle->font().pixelSize()<=responsive.font().pixelSize()+1);
 // Selected state is rendered by a thin theme-derived button, not by
 // Colorway's generic bold checked-state typography.
 REQUIRE(autoButton->property("aftermarket-thin-state").toBool());
 REQUIRE(autoButton->font().weight()<=QFont::Medium);
 REQUIRE(editorLayout->direction()==QBoxLayout::BottomToTop);
 REQUIRE(optionsLayout->direction()==QBoxLayout::LeftToRight);
 QPointer<QScrollArea> strip=editor->findChild<QScrollArea*>("aftermarket-studio-transition-strip");
 REQUIRE(strip&&strip->widget()==options);
 REQUIRE(strip->width()<=panels.previewDock(&responsive)->width());
 REQUIRE(strip->horizontalScrollBar()->maximum()>0);
 auto *left=toolbar->findChild<QToolButton*>("aftermarket-studio-scroll-left");
 auto *right=toolbar->findChild<QToolButton*>("aftermarket-studio-scroll-right");
 REQUIRE(left&&right&&right->isEnabled());
 const int beforeScroll=strip->horizontalScrollBar()->value();
 right->click();app.processEvents();
 REQUIRE(strip->horizontalScrollBar()->value()>beforeScroll);
 auto *native=options->findChildren<QPushButton*>().last();
 native->click();REQUIRE(triggerCount==1);
 const int originalScrollMax=strip->horizontalScrollBar()->maximum();
 auto *newNative=new QPushButton("Extra long native Quick Transition with dynamic options",options);
 newNative->setProperty("id",40);
 optionsLayout->insertWidget(3,newNative);app.processEvents();app.processEvents();
 REQUIRE(strip->horizontalScrollBar()->maximum()>originalScrollMax);
 optionsLayout->removeWidget(newNative);delete newNative;app.processEvents();
 REQUIRE(strip->horizontalScrollBar()->maximum()<=originalScrollMax);
 REQUIRE(previewPane->isVisible()&&programPane->isVisible());
 REQUIRE(editorLayout->stretch(editorLayout->indexOf(programPane)) >
         editorLayout->stretch(editorLayout->indexOf(previewPane)));
 const int bothProgramHeight=programPane->height();
 REQUIRE(previewLabel->font().family()==originalPreviewLabelFont.family());
 REQUIRE(previewLabel->font().weight()==QFont::Medium);
 if(originalPreviewLabelFont.pixelSize()>0)
     REQUIRE(previewLabel->font().pixelSize()==qMax(9,originalPreviewLabelFont.pixelSize()-1));
 else if(originalPreviewLabelFont.pointSizeF()>0)
     REQUIRE(qAbs(previewLabel->font().pointSizeF()-qMax(7.5,originalPreviewLabelFont.pointSizeF()-0.5))<0.01);
 previewButtonMode->click();app.processEvents();
 REQUIRE(previewPane->isVisible()&&!programPane->isVisible());
 programButton->click();app.processEvents();
 REQUIRE(!previewPane->isVisible()&&programPane->isVisible());
 REQUIRE(programPane->height()>bothProgramHeight+20);
 bothButton->click();app.processEvents();
 REQUIRE(previewPane->isVisible()&&programPane->isVisible());
 REQUIRE(panels.setStudioViewMode(&responsive,augment::Panels::StudioViewMode::Auto));app.processEvents();
 REQUIRE(autoButton->isChecked());
 // In a roomy floating dock, controls and views return to a wide split.
 auto *responsiveDock=panels.previewDock(&responsive);
 responsiveDock->setFloating(true);responsiveDock->resize(1050,650);app.processEvents();
 REQUIRE(editorLayout->direction()==QBoxLayout::LeftToRight);
 REQUIRE(optionsLayout->direction()==QBoxLayout::TopToBottom);
 REQUIRE(editorLayout->indexOf(options)>=0&&strip.isNull());
 responsiveDock->resize(430,580);app.processEvents();
 REQUIRE(editorLayout->direction()==QBoxLayout::BottomToTop);
 REQUIRE(optionsLayout->direction()==QBoxLayout::LeftToRight);
 strip=editor->findChild<QScrollArea*>("aftermarket-studio-transition-strip");
 REQUIRE(!strip.isNull()&&strip->widget()==options);
 REQUIRE(panels.setPreviewDocked(&responsive,false));app.processEvents();
 REQUIRE(responsive.centralWidget()==workspace);
 REQUIRE(workspace->findChild<QWidget*>("aftermarket-studio-toolbar")==nullptr);
 REQUIRE(previewPane->isVisible()&&programPane->isVisible());
 REQUIRE(editorLayout->direction()==QBoxLayout::LeftToRight);
 REQUIRE(optionsLayout->direction()==QBoxLayout::TopToBottom);
 REQUIRE(previewLabel->font().pixelSize()!=12);
 REQUIRE(contextToolbar->parentWidget()==workspace);
 // OBS deletes programOptions and programWidget on Studio Mode exit.
 // The temporary scroll container must disappear before a new studio set.
 REQUIRE(panels.setPreviewDocked(&responsive,true));app.processEvents();
 QPointer<QScrollArea> oldStrip=editor->findChild<QScrollArea*>("aftermarket-studio-transition-strip");
 REQUIRE(oldStrip);
 delete options;
 delete programPane;
 app.processEvents();
 REQUIRE(oldStrip.isNull());
 REQUIRE(editorLayout->count()==2);
 auto *nextOptions=new QWidget(editor);
 auto *nextOptionsLayout=new QVBoxLayout(nextOptions);
 nextOptionsLayout->addWidget(new QPushButton("Next session transition",nextOptions));
 auto *nextProgram=new QWidget(editor);
 auto *nextProgramLayout=new QVBoxLayout(nextProgram);
 nextProgramLayout->addWidget(new QLabel("Program scene",nextProgram));
 editorLayout->addWidget(nextOptions);editorLayout->addWidget(nextProgram);
 app.processEvents();
 auto *newStrip=editor->findChild<QScrollArea*>("aftermarket-studio-transition-strip");
 REQUIRE(newStrip&&newStrip->widget()==nextOptions);
 REQUIRE(panels.setPreviewDocked(&responsive,false));app.processEvents();
 REQUIRE(editorLayout->indexOf(nextOptions)>=0);
 REQUIRE(editorLayout->indexOf(nextProgram)>=0);
 REQUIRE(nextOptionsLayout->direction()==QBoxLayout::TopToBottom);
 QMainWindow invalid;invalid.setCentralWidget(new QLabel("nothing"));
 REQUIRE(!panels.setPreviewDocked(&invalid,true));
 std::cout<<"dock height, native content identity, restore, header gesture, tab guard and preview docking passed\n";
}
