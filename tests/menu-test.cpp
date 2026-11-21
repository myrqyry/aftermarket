#include "augment.h"
#include "inspector.h"
#include <QScreen>
#include <QSet>
#include <QComboBox>
#include <QListWidget>
#include <QLineEdit>
#include <QScrollBar>
#include <QMouseEvent>
#include <QTimer>
#include <QKeyEvent>
#include <QApplication>
#include <QMenu>
#include <QPainter>
#include <QTemporaryDir>
#include <QFile>
#include <QImage>
#include <QIcon>
#include <QWidgetAction>
#include <iostream>
#define REQUIRE(x) do { if (!(x)) { std::cerr << "FAILED line " << __LINE__ << ": " << #x << '\n'; return 1; } } while(false)
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    QTemporaryDir dir;
    const auto path = dir.filePath("rules.json");
    auto write = [&](const QByteArray &json) { QFile f(path); return f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(json)==json.size(); };
    REQUIRE(write(R"({"enabled":true,"rules":[{"text":"Rename","icon":"pencil","color":"#ff00ff"},{"text":"Copy","icon":"copy","color":"#ff00ff"},{"text":"Remove","icon":"trash","color":"#ff00ff"}]})"));
    QMenu menu;
    auto *rename = menu.addAction("&Rename...");
    auto *copy = menu.addAction("Copy");
    QPixmap px(16,16); px.fill(Qt::green); copy->setIcon(QIcon(px));
    auto key = copy->icon().cacheKey();
    auto *check = menu.addAction("Remove"); check->setCheckable(true);
    auto *unknown = menu.addAction("Unknown");
    bool triggered = false;
    QObject::connect(rename, &QAction::triggered, [&]{triggered=true;});
    {
        augment::Controller controller(path);
        menu.popup(QPoint(40,40)); app.processEvents();
        REQUIRE(rename->icon().isNull() == false); // transparent layout reservation
        REQUIRE(copy->icon().cacheKey() == key);
        REQUIRE(check->icon().isNull());
        REQUIRE(unknown->icon().isNull());
        auto *overlay = menu.findChild<QWidget*>("obs-ui-augment-overlay");
        REQUIRE(overlay && overlay->isVisible());
        REQUIRE(overlay->testAttribute(Qt::WA_TransparentForMouseEvents));
        QImage img=menu.grab().toImage().convertToFormat(QImage::Format_ARGB32);
        const auto row=menu.actionGeometry(rename);
        int magenta = 0;
        for(int y=row.top(); y<=row.bottom(); ++y)
            for(int x=row.left(); x<qMin(row.left()+30,img.width()); ++x) {
                auto c=img.pixelColor(x,y);
                if(c.red()>180 && c.blue()>180 && c.green()<90) ++magenta;
            }
        REQUIRE(magenta>8); // actual independently painted pixels
        auto nativeIcon=rename->icon().pixmap(16,16).toImage();
        for(int y=0;y<nativeIcon.height();++y) for(int x=0;x<nativeIcon.width();++x)
            REQUIRE(nativeIcon.pixelColor(x,y).alpha()==0);
        menu.setActiveAction(rename);
        // Native action still dispatches and can be selected normally.
        REQUIRE(menu.activeAction()==rename);
        rename->trigger(); REQUIRE(triggered);
        menu.hide(); app.processEvents();
        REQUIRE(rename->icon().isNull());
        REQUIRE(copy->icon().cacheKey()==key);
        REQUIRE(write(R"({"enabled":false,"rules":[]})"));
        menu.popup(QPoint(40,40)); app.processEvents();
        REQUIRE(rename->icon().isNull());
        REQUIRE(!overlay->isVisible());
        menu.hide();
        REQUIRE(write(R"({"enabled":true,"rules":[{"text":"Rename","icon":"pencil","color":"#ff00ff"}]})"));
        menu.popup(QPoint(40,40)); app.processEvents();
        REQUIRE(rename->icon().isNull()==false);
        menu.hide();
        REQUIRE(write(R"({"enabled":true,"rules":[{"text":"Copy","icon":"clipboard","color":"#ff00ff","replaceExisting":true}]})"));
        menu.popup(QPoint(40,40)); app.processEvents();
        REQUIRE(copy->icon().cacheKey()!=key);
        REQUIRE(overlay->isVisible());
        menu.hide();
        REQUIRE(copy->icon().cacheKey()==key);
        // A malformed intermediate edit must retain the last valid rule.
        REQUIRE(write("{"));
        menu.popup(QPoint(40,40)); app.processEvents();
        REQUIRE(copy->icon().cacheKey()!=key);
    }
    REQUIRE(rename->icon().isNull());
    REQUIRE(!menu.findChild<QWidget*>("obs-ui-augment-overlay"));
    // Actual native popup: a long styled menu must remain bounded without
    // discarding, copying or changing the original QAction instances.
    REQUIRE(write(R"({"enabled":true,"rules":[{"text":"Entry 20","icon":"pencil","menuContains":["Entry 0","Entry 1"]}]})"));
    {
        augment::Controller controller(path);
        QMenu longMenu;
        longMenu.setStyleSheet("QMenu::item { padding: 8px 12px; margin: 2px; }");
        const auto style=longMenu.styleSheet();
        for(int i=0;i<30;++i) {
            if(i&&i%5==0)longMenu.addSeparator();
            auto *a=longMenu.addAction(QString("Entry %1").arg(i));
            if(i==4)a->setEnabled(false);
            if(i==8){a->setCheckable(true);a->setChecked(true);}
        }
        auto *nativeSub=longMenu.addMenu("Native submenu");
        nativeSub->addAction("Child");
        const auto originals=longMenu.actions();
        QAction *last=nullptr;for(auto *a:originals)if(a->text()=="Entry 20")last=a;
        REQUIRE(last);
        int count=0,rootCount=0;
        QObject::connect(last,&QAction::triggered,[&]{++count;});
        QObject::connect(&longMenu,&QMenu::triggered,[&](QAction *a){if(a==last)++rootCount;});
        longMenu.popup(QPoint(50,50));app.processEvents();
        const auto cap=qMin(500,int(longMenu.screen()->availableGeometry().height()*.57));
        REQUIRE(longMenu.height()<=cap+8);
        REQUIRE(longMenu.actions()==originals);
        QSet<int> columns;
        for(auto *a:originals) {
            if(!a->isVisible()||a->isSeparator())continue;
            const auto r=longMenu.actionGeometry(a);
            REQUIRE(r.isValid()&&longMenu.rect().contains(r));
            REQUIRE(longMenu.actionAt(r.center())==a);
            columns.insert(r.x());
        }
        REQUIRE(columns.size()>1);
        longMenu.hide();app.processEvents();
        REQUIRE(longMenu.actions()==originals&&longMenu.styleSheet()==style);
        // Native action execution in a second column, without opening overflow.
        QTimer::singleShot(0,&longMenu,[&]{
            longMenu.setActiveAction(last);
            const auto snapshot=augment::inspectWidget(&longMenu,longMenu.mapToGlobal(longMenu.actionGeometry(last).center()));
            if(snapshot.menuLabels.contains("Entry 0")&&snapshot.menuLabels.contains("Entry 29")&&!last->icon().isNull())
                count+=100;
            QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
            QApplication::sendEvent(&longMenu,&enter);
        });
        QTimer::singleShot(1000,&longMenu,&QMenu::close);
        REQUIRE(longMenu.exec(QPoint(50,50))==last);
        app.processEvents();
        REQUIRE(count==101&&rootCount==1&&longMenu.actions()==originals);
        REQUIRE(originals.at(4)->isEnabled()==false);
        REQUIRE(controller.compactMenusEnabled());
        // OBS Source context actions should remain a familiar single column,
        // even when their size would otherwise trigger the column adapter.
        QMenu sourceContext;
        sourceContext.setStyleSheet("QMenu::item { padding: 8px 12px; margin: 2px; }");
        for(int i=0;i<21;++i)sourceContext.addAction(QString("Source utility %1").arg(i));
        for(const auto &label:{"Order","Transform","Rename...","Remove","Filters","Properties"})
            sourceContext.addAction(label);
        const auto sourceActions=sourceContext.actions();
        sourceContext.popup(QPoint(55,55));app.processEvents();
        QSet<int> sourceColumns;
        for(auto *a:sourceActions) {
            const auto rect=sourceContext.actionGeometry(a);
            REQUIRE(rect.isValid());
            sourceColumns.insert(rect.x());
        }
        REQUIRE(sourceContext.actions()==sourceActions);
        REQUIRE(sourceColumns.size()==1);
        sourceContext.hide();app.processEvents();
        QMenu filters;
        for(int i=0;i<18;++i)filters.addAction(QString("Filter %1").arg(i));
        const auto choices=filters.actions();
        filters.popup(QPoint(50,50));app.processEvents();
        QSet<int> filterCols;
        for(auto *a:choices){
            const auto r=filters.actionGeometry(a);
            REQUIRE(filters.rect().contains(r)&&filters.actionAt(r.center())==a);
            filterCols.insert(r.x());
        }
        REQUIRE(filters.actions()==choices&&filterCols.size()>1);
        filters.hide();app.processEvents();

        QMenu embedded;
        embedded.setStyleSheet(style);
        for(int i=0;i<30;++i)embedded.addAction(QString::number(i));
        auto *widgetAction=new QWidgetAction(&embedded);
        widgetAction->setDefaultWidget(new QWidget);
        embedded.addAction(widgetAction);
        const auto embeddedActions=embedded.actions();
        embedded.popup(QPoint(40,40));app.processEvents();
        REQUIRE(embedded.actions()==embeddedActions&&embedded.styleSheet()==style);
        embedded.hide();app.processEvents();
        REQUIRE(nativeSub->actions().size()==1);
        // Only an explicitly scoped choice may enter overflow.
        REQUIRE(write(R"({"enabled":true,"menuOverflow":[{"text":"Entry 7","menuContains":["Entry 0","Entry 1"]}],"rules":[]})"));
        longMenu.popup(QPoint(50,50));app.processEvents();
        QMenu *more=nullptr;
        for(auto *a:longMenu.actions())if(a->text()=="More actions…")more=a->menu();
        REQUIRE(more&&more->actions().size()==1&&more->actions().first()->text()=="Entry 7");
        for(auto *a:originals)if(a->text()!="Entry 7")REQUIRE(longMenu.actions().contains(a));
        longMenu.hide();app.processEvents();
        REQUIRE(longMenu.actions()==originals);
        QMenu unrelated;unrelated.addAction("Entry 7");
        unrelated.popup(QPoint(50,50));app.processEvents();
        REQUIRE(unrelated.actions().size()==1&&unrelated.actions().first()->text()=="Entry 7");
        unrelated.hide();app.processEvents();
        // The actual OBS Appearance combo uses a model, not a QMenu.
        QWidget settings;settings.setObjectName("OBSBasicSettings");
        QComboBox themes(&settings);themes.setObjectName("themeVariant");
        for(int i=0;i<120;++i)themes.addItem(QString("Colorway %1").arg(i),i);
        settings.show();themes.show();app.processEvents();
        int activated=-1;
        QObject::connect(&themes,&QComboBox::activated,[&](int row){activated=row;});
        const QPoint center=themes.rect().center();
        QMouseEvent press(QEvent::MouseButtonPress,center,themes.mapToGlobal(center),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(&themes,&press);app.processEvents();
        auto *picker=settings.findChild<QWidget*>("aftermarket-theme-picker");
        REQUIRE(picker&&picker->isVisible());
        auto *grid=picker->findChild<QListWidget*>();
        REQUIRE(grid&&grid->count()==120&&grid->isWrapping());
        REQUIRE(grid->visualItemRect(grid->item(0)).x()!=grid->visualItemRect(grid->item(1)).x());
        auto *search=picker->findChild<QLineEdit*>();
        REQUIRE(search);
        search->setText("Colorway 83");
        app.processEvents();
        REQUIRE(grid->item(0)->isHidden()&&!grid->item(83)->isHidden());
        search->setText({});
        app.processEvents();
        for(int i=0;i<grid->count();++i)REQUIRE(!grid->item(i)->isHidden());
        auto *choice=grid->item(83);
        grid->setCurrentItem(choice);grid->scrollToItem(choice);app.processEvents();
        const auto click=grid->visualItemRect(choice).center();
        REQUIRE(grid->viewport()->rect().contains(click)&&grid->itemAt(click)==choice);
        QMouseEvent down(QEvent::MouseButtonPress,click,grid->viewport()->mapToGlobal(click),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent up(QEvent::MouseButtonRelease,click,grid->viewport()->mapToGlobal(click),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(grid->viewport(),&down);
        QApplication::sendEvent(grid->viewport(),&up);
        app.processEvents();
        REQUIRE(themes.count()==120&&themes.currentIndex()==83&&activated==83);
        settings.hide();
        REQUIRE(controller.setCompactMenus(false));
        REQUIRE(!controller.compactMenusEnabled());
        longMenu.popup(QPoint(50,50));app.processEvents();
        REQUIRE(longMenu.actions()==originals&&longMenu.styleSheet()==style);
        longMenu.hide();app.processEvents();
    }
    std::cout << "menu rendering, preservation, reload, cleanup passed\n";
}
