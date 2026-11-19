#include "augment.h"
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
    std::cout << "menu rendering, preservation, reload, cleanup passed\n";
}
