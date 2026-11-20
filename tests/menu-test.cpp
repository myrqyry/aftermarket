#include "augment.h"
#include "inspector.h"
#include <QScreen>
#include <QSet>
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
        QMenu *page=&longMenu;
        QSet<QAction*> seen;
        while(page) {
            QMenu *next=nullptr;
            for(auto *a:page->actions()) {
                if(a->property("_aftermarketOverflowAction").toBool()){next=a->menu();continue;}
                REQUIRE(originals.contains(a)&&!seen.contains(a));seen.insert(a);
            }
            if(!next)break;
            page=next;
        }
        REQUIRE(seen.size()==originals.size()&&page!=&longMenu&&!longMenu.actions().contains(last)
                &&longMenu.actions().contains(nativeSub->menuAction())&&longMenu.actions().contains(originals.at(originals.size()-2)));
        longMenu.hide();app.processEvents();
        REQUIRE(longMenu.actions()==originals&&longMenu.styleSheet()==style);
        // Native submenu navigation and exec return must survive pagination.
        QTimer::singleShot(0,&longMenu,[&]{
            auto *current=&longMenu;
            for(int depth=0;depth<10&&!current->actions().contains(last);++depth) {
                QAction *more=nullptr;
                for(auto *a:current->actions())if(a->property("_aftermarketOverflowAction").toBool()){more=a;break;}
                if(!more)break;
                current->setActiveAction(more);
                QKeyEvent right(QEvent::KeyPress,Qt::Key_Right,Qt::NoModifier);
                QApplication::sendEvent(current,&right);
                app.processEvents();
                current=more->menu();
                if(current->height()>cap+8)break;
            }
            if(current->actions().contains(last)&&current->isVisible()) {
                current->setActiveAction(last);
                const auto snapshot=augment::inspectWidget(current,current->mapToGlobal(current->actionGeometry(last).center()));
                if(snapshot.menuLabels.contains("Entry 0")&&snapshot.menuLabels.contains("Entry 29")&&!last->icon().isNull())
                    count+=100;
                QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
                QApplication::sendEvent(current,&enter);
            } else longMenu.close();
        });
        QTimer::singleShot(1000,&longMenu,&QMenu::close);
        REQUIRE(longMenu.exec(QPoint(50,50))==last);
        app.processEvents();
        REQUIRE(count==101&&rootCount==1&&longMenu.actions()==originals);
        REQUIRE(originals.at(4)->isEnabled()==false);
        REQUIRE(controller.compactMenusEnabled());
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
        REQUIRE(controller.setCompactMenus(false));
        REQUIRE(!controller.compactMenusEnabled());
        longMenu.popup(QPoint(50,50));app.processEvents();
        REQUIRE(longMenu.actions()==originals&&longMenu.styleSheet()==style);
        longMenu.hide();app.processEvents();
    }
    std::cout << "menu rendering, preservation, reload, cleanup passed\n";
}
