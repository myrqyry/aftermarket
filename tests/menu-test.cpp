#include "augment.h"
#include "inspector.h"
#include "menu-layout.h"
#include <QScreen>
#include <QCursor>
#include <QGuiApplication>
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
#include <QElapsedTimer>
#include <QThread>
#include <QIcon>
#include <QWidgetAction>
#include <QPushButton>
#include <iostream>
#define REQUIRE(x) do { if (!(x)) { std::cerr << "FAILED line " << __LINE__ << ": " << #x << '\n'; return 1; } } while(false)
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    QTemporaryDir dir;
    // Density adaptation must track native Qt font metrics, rather than
    // squeezing large theme fonts into a hardcoded 16px minimum row.
    {
        QMenu fontMenu;
        auto large=fontMenu.font();
        large.setPixelSize(27);
        fontMenu.setFont(large);
        for(int n=0;n<15;++n)fontMenu.addAction(QString("Choice %1").arg(n));
        const auto nativeStyle=fontMenu.styleSheet();
        augment::MenuLayout layout(&fontMenu);
        layout.prepare(true,{}, {},true);
        const int wantedRow=qMax(16,fontMenu.fontMetrics().height()+2);
        REQUIRE(fontMenu.styleSheet().contains(
            QStringLiteral("min-height: %1px").arg(wantedRow)));
        layout.restore();
        REQUIRE(fontMenu.styleSheet()==nativeStyle);
        REQUIRE(fontMenu.font().pixelSize()==27);
    }
    const auto path = dir.filePath("rules.json");
    auto write = [&](const QByteArray &json) { QFile f(path); return f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(json)==json.size(); };
    REQUIRE(write(R"({"enabled":true,"rules":[{"text":"Rename","icon":"pencil","color":"#ff00ff"},{"text":"Copy","icon":"copy","color":"#ff00ff"},{"text":"Remove","icon":"trash","color":"#ff00ff"}]})"));
    // A real dual-head X11 run exercises screens with nonzero origins.
    // The cursor can be on another screen when a menu opens by keyboard.
    // Native popup geometry, not the cursor's matching x coordinate, owns it.
    if(app.screens().size()>1){
        augment::Controller controller(path);
        const QPoint saved=QCursor::pos();
        for(auto *target:app.screens()){
            const QRect work=target->availableGeometry();
            QScreen *other=nullptr;
            for(auto *candidate:app.screens())if(candidate!=target){other=candidate;break;}
            const QRect otherWork=other->availableGeometry();
            const int overlapLeft=qMax(work.left(),otherWork.left());
            const int overlapRight=qMin(work.right(),otherWork.right());
            const int x=overlapRight-overlapLeft>400?overlapLeft+100:work.left()+100;
            const QPoint anchor(x,work.top()+100);
            const QPoint unrelated(qBound(otherWork.left(),x,otherWork.right()),otherWork.center().y());
            QCursor::setPos(unrelated);
            QMenu root;
            for(int i=0;i<42;++i)root.addAction(QString("Filter %1 / effect options").arg(i));
            root.popup(anchor);app.processEvents();
            std::cerr<<"multi-head root "<<qPrintable(target->name())<<" anchor "<<anchor.x()<<","<<anchor.y()
                     <<" cursor "<<unrelated.x()<<","<<unrelated.y()<<" actual "
                     <<root.x()<<","<<root.y()<<" "<<root.width()<<"x"<<root.height()<<"\n";
            REQUIRE(work.contains(root.geometry()));
            QSet<int> columns;
            for(auto *action:root.actions()){
                const auto r=root.actionGeometry(action);
                REQUIRE(root.rect().contains(r)&&root.actionAt(r.center())==action);
                columns.insert(r.x());
            }
            REQUIRE(columns.size()>1);
            root.hide();app.processEvents();
            // Open a genuine submenu through Qt's keyboard navigation.
            QMenu parent;
            parent.addAction("Remove");
            auto *child=parent.addMenu("Add");
            for(int i=0;i<42;++i)child->addAction(QString("Filter %1 / effect options").arg(i));
            parent.popup(QPoint(work.right()-230,work.bottom()-100));app.processEvents();
            const auto row=parent.actionGeometry(child->menuAction());
            const QRect globalRow(parent.mapToGlobal(row.topLeft()),row.size());
            QCursor::setPos(unrelated);
            parent.setActiveAction(child->menuAction());
            QKeyEvent right(QEvent::KeyPress,Qt::Key_Right,Qt::NoModifier);
            QApplication::sendEvent(&parent,&right);app.processEvents();
            REQUIRE(child->isVisible());
            std::cerr<<"multi-head submenu "<<qPrintable(target->name())<<" row "
                     <<globalRow.x()<<","<<globalRow.y()<<" actual "
                     <<child->x()<<","<<child->y()<<" "<<child->width()<<"x"<<child->height()<<"\n";
            REQUIRE(work.contains(child->geometry()));
            REQUIRE(qAbs(child->geometry().right()-globalRow.left())<=20||
                    qAbs(child->geometry().left()-globalRow.right())<=20);
            columns.clear();
            for(auto *action:child->actions()){
                const auto r=child->actionGeometry(action);
                REQUIRE(child->rect().contains(r)&&child->actionAt(r.center())==action);
                columns.insert(r.x());
            }
            REQUIRE(columns.size()>1);
            child->hide();parent.hide();app.processEvents();
        }
        QCursor::setPos(saved);
    }
    QMenu menu;
    auto *rename = menu.addAction("&Rename...");
    auto *copy = menu.addAction("Copy");
    QPixmap px(16,16); px.fill(Qt::green); copy->setIcon(QIcon(px));
    auto key = copy->icon().cacheKey();
    auto *check = menu.addAction("Remove"); check->setCheckable(true);
    auto *unknown = menu.addAction("Unknown");
    bool triggered = false;
    QObject::connect(rename, &QAction::triggered, [&]{triggered=true;});
    // Startup with a recovery marker must not attach menu decoration at all.
    {
        augment::Controller startSafe(path,nullptr,{},true);
        REQUIRE(startSafe.runtimeSafeMode());
        menu.popup(QPoint(40,40));app.processEvents();
        REQUIRE(rename->icon().isNull());
        REQUIRE(!menu.findChild<QWidget*>("aftermarket-overlay"));
        menu.hide();app.processEvents();
        startSafe.setRuntimeSafeMode(false);
        menu.popup(QPoint(40,40));app.processEvents();
        REQUIRE(!rename->icon().isNull());
        menu.hide();app.processEvents();
    }
    {
        augment::Controller controller(path);
        menu.popup(QPoint(40,40)); app.processEvents();
        REQUIRE(rename->icon().isNull() == false); // transparent layout reservation
        REQUIRE(copy->icon().cacheKey() == key);
        REQUIRE(check->icon().isNull());
        REQUIRE(unknown->icon().isNull());
        auto *overlay = menu.findChild<QWidget*>("aftermarket-overlay");
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
        // Emergency suspension must immediately undo menu layout, artwork,
        // and the entire live presentation, without changing rules.json.
        controller.setRuntimeSafeMode(true);
        REQUIRE(controller.runtimeSafeMode());
        menu.popup(QPoint(40,40));app.processEvents();
        REQUIRE(rename->icon().isNull()&&!overlay->isVisible());
        menu.hide();app.processEvents();
        controller.setRuntimeSafeMode(false);
        REQUIRE(!controller.runtimeSafeMode());
        menu.popup(QPoint(40,40));app.processEvents();
        REQUIRE(!rename->icon().isNull()&&overlay->isVisible());
        menu.hide();app.processEvents();
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
    REQUIRE(!menu.findChild<QWidget*>("aftermarket-overlay"));
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
        // An interior-column submenu must not cover another column.
        // Exercise both a short native child and a long compact child.
        {
            const QRect work=QGuiApplication::primaryScreen()->availableGeometry();
            const QPoint saved=QCursor::pos();
            for(bool longChild:{false,true})for(bool nearRight:{false,true}){
                QMenu parent;
                if(nearRight)for(int n=0;n<28;++n)parent.addAction(QString("Item %1").arg(n));
                auto *child=parent.addMenu("Options");
                for(int n=0;n<(longChild?18:3);++n)child->addAction(QString("Choice %1").arg(n));
                if(!nearRight)for(int n=0;n<28;++n)parent.addAction(QString("Item %1").arg(n));
                const QPoint anchor(nearRight?work.right()-40:work.left()+40,work.top()+40);
                QCursor::setPos(anchor);
                parent.popup(anchor);app.processEvents();
                QSet<int> cols;
                for(auto *action:parent.actions())cols.insert(parent.actionGeometry(action).x());
                REQUIRE(cols.size()>1);
                const QRect originalParent=parent.geometry();
                parent.setActiveAction(child->menuAction());
                QKeyEvent right(QEvent::KeyPress,Qt::Key_Right,Qt::NoModifier);
                QApplication::sendEvent(&parent,&right);app.processEvents();
                REQUIRE(child->isVisible());
                REQUIRE(work.contains(child->geometry()));
                REQUIRE(!child->geometry().intersects(originalParent));
                REQUIRE(parent.geometry()==originalParent);
                REQUIRE(nearRight?child->geometry().right()<originalParent.left():child->geometry().left()>originalParent.right());
                for(auto *action:child->actions()){
                    auto rect=child->actionGeometry(action);
                    REQUIRE(rect.isValid()&&child->actionAt(rect.center())==action);
                }
                child->hide();parent.hide();app.processEvents();
            }
            QCursor::setPos(saved);
        }
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
        // Hiding one of the sentinel Source commands must not make the
        // remaining source menu turn into a multi-column choice catalogue.
        REQUIRE(write(R"({"enabled":true,"rules":[],"menuHidden":[{"text":"Transform","menuContains":["Order","Filters"]}]})"));
        sourceContext.popup(QPoint(55,55));app.processEvents();
        auto *transform=sourceActions.at(sourceActions.size()-5);
        REQUIRE(transform->text()=="Transform"&&!sourceContext.actions().contains(transform));
        QSet<int> remainingColumns;
        for(auto *a:sourceContext.actions())if(!a->isSeparator()&&a->isVisible())
            remainingColumns.insert(sourceContext.actionGeometry(a).x());
        REQUIRE(remainingColumns.size()==1);
        sourceContext.hide();app.processEvents();
        REQUIRE(sourceContext.actions()==sourceActions);
        REQUIRE(write(R"({"enabled":true,"rules":[]})"));
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
        // A multi-column popup belongs at its invocation point, NOT at the
        // original Qt long-menu position after OBS/Aftermarket reflow.
        // Reproduce the real Filters Add popup near the lower screen edge.
        QMenu anchored;
        anchored.setObjectName("aftermarket-anchor-regression");
        for(int i=0;i<58;++i)anchored.addAction(
            QString("Effect %1 - Additional filter settings").arg(i));
        const QRect work=QGuiApplication::primaryScreen()->availableGeometry();
        const QPoint savedCursor=QCursor::pos();
        const QList<QPoint> anchors{
            QPoint(work.center().x(),work.bottom()-200),
            QPoint(work.right()-420,work.bottom()-200),
            QPoint(work.center().x(),work.bottom()-20),
            QPoint(work.right()-20,work.center().y()),
            QPoint(work.right()-20,work.bottom()-20),
            QPoint(work.left()+30,work.top()+30)
        };
        for(const auto &anchor:anchors){
            QCursor::setPos(anchor);
            anchored.popup(anchor);app.processEvents();
            QSet<int> columns;
            for(auto *a:anchored.actions())if(a->isVisible()){
                const QRect cell=anchored.actionGeometry(a);
                REQUIRE(cell.isValid()&&anchored.rect().contains(cell));
                REQUIRE(anchored.actionAt(cell.center())==a);
                columns.insert(cell.left());
            }
            const auto actual=anchored.geometry();
            REQUIRE(columns.size()>1);
            const bool nearBottom=anchor.y()>work.center().y();
            const bool nearRight=anchor.x()>work.center().x();
            const int verticalRoom=nearBottom ? anchor.y()-work.top()+1
                                               : work.bottom()-anchor.y()+1;
            if(actual.height()<=verticalRoom)
                REQUIRE(qAbs((nearBottom?actual.bottom():actual.top())-anchor.y())<=30);
            // If there is enough width on the chosen side, preserve the
            // trigger's horizontal edge. Otherwise Qt may clamp to screen.
            const int horizontalRoom=nearRight ? anchor.x()-work.left()+1
                                               : work.right()-anchor.x()+1;
            if(actual.width()<=horizontalRoom)
                REQUIRE(qAbs((nearRight?actual.right():actual.left())-anchor.x())<=30);
            anchored.hide();app.processEvents();
        }

        // Reproduce OBS Filters/Add: Qt first positions the *tall* list.
        // Aftermarket then reshapes it into columns, after the mouse may
        // already have moved onto a choice. The menu must remain anchored
        // at the original popup point, not the stale tall-menu top edge.
        QMenu delayed;
        delayed.setObjectName("aftermarket-delayed-anchor");
        for(int i=0;i<58;++i)
            delayed.addAction(QString("Effect %1 - Additional filter settings").arg(i));
        const QPoint trigger(work.center().x(),work.bottom()-32);
        QCursor::setPos(trigger);
        delayed.popup(trigger);app.processEvents();
        augment::MenuLayout lateLayout(&delayed);
        lateLayout.prepare(true);
        QCursor::setPos(work.topLeft()+QPoint(6,6));
        lateLayout.present();app.processEvents();
        REQUIRE(qAbs(delayed.geometry().bottom()-trigger.y())<=30);
        QSet<int> delayedColumns;
        for(auto *action:delayed.actions()){
            auto r=delayed.actionGeometry(action);
            REQUIRE(r.isValid()&&delayed.rect().contains(r));
            REQUIRE(delayed.actionAt(r.center())==action);
            delayedColumns.insert(r.x());
        }
        REQUIRE(delayedColumns.size()>1);
        delayed.hide();lateLayout.restore();app.processEvents();
        QCursor::setPos(savedCursor);

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
        REQUIRE(write(R"({"enabled":true,"menuHidden":[{"text":"Entry 7","menuContains":["Entry 0","Entry 1"]}],"menuOverflow":[{"text":"Entry 7","menuContains":["Entry 0","Entry 1"]}],"rules":[]})"));
        longMenu.popup(QPoint(50,50));app.processEvents();
        REQUIRE(!longMenu.actions().contains(originals.at(8)));
        bool leakedInOverflow=false;
        for(auto *a:longMenu.actions())if(a->menu()&&a->text()=="More actions…")
            leakedInOverflow|=a->menu()->actions().contains(originals.at(8));
        REQUIRE(!leakedInOverflow);
        longMenu.hide();app.processEvents();
        REQUIRE(longMenu.actions()==originals);
        QMenu unrelated;unrelated.addAction("Entry 7");
        unrelated.popup(QPoint(50,50));app.processEvents();
        REQUIRE(unrelated.actions().size()==1&&unrelated.actions().first()->text()=="Entry 7");
        unrelated.hide();app.processEvents();
        // Hide a native command only in a selected menu; do not destroy the
        // QAction or its signal. Restore original actions on dismiss.
        REQUIRE(write(R"({"enabled":true,"rules":[],"menuHidden":[{"text":"Remove","menuObjectName":"sourceActions"},{"text":"Mute","menuObjectName":"sourceActions"}],"menuCustom":[{"id":"open-audit","label":"Open audit log","program":"/usr/bin/touch","args":["AUDIT_TARGET"],"menuObjectName":"sourceActions","icon":"camera"}]})"));
        QMenu source;source.setObjectName("sourceActions");
        auto *sourceRename=source.addAction("Rename");
        auto *sourceRemove=source.addAction("Remove");
        auto *sourceFilters=source.addAction("Filters");
        auto *sourceMute=source.addAction("Mute");
        sourceMute->setCheckable(true);sourceMute->setChecked(true);
        const auto sourceOriginal=source.actions();
        int removeTriggered=0;
        QObject::connect(sourceRemove,&QAction::triggered,[&]{++removeTriggered;});
        QMenu another;another.setObjectName("differentContext");
        auto *otherRemove=another.addAction("Remove");
        another.popup(QPoint(50,50));app.processEvents();
        REQUIRE(another.actions().contains(otherRemove));
        REQUIRE(another.actions().size()==1);
        another.hide();app.processEvents();
        const auto auditTarget=dir.filePath("audit created");
        // Replace a test-only target; the production command must use exact argv.
        REQUIRE(controller.setCustomAction(QJsonObject{{"id","open-audit"},{"label","Open audit log"},{"program","/usr/bin/touch"},{"args",QJsonArray{auditTarget}},{"menuObjectName","sourceActions"},{"icon","camera"}}));
        source.popup(QPoint(50,50));app.processEvents();
        REQUIRE(source.actions().contains(sourceRename)&&source.actions().contains(sourceFilters));
        REQUIRE(!source.actions().contains(sourceRemove));
        REQUIRE(!source.actions().contains(sourceMute));
        REQUIRE(sourceMute->isChecked()&&sourceMute->isCheckable());
        REQUIRE(!sourceRemove->isVisible()==false);
        QAction *custom=nullptr;
        for(auto *a:source.actions())if(a->text()=="Open audit log")custom=a;
        REQUIRE(custom&&custom->property("_aftermarketCustomId").toString()=="open-audit");
        REQUIRE(!custom->icon().isNull());
        custom->trigger();
        QElapsedTimer fired;fired.start();
        while(!QFile::exists(auditTarget)&&fired.elapsed()<1200){app.processEvents();QThread::msleep(10);}
        REQUIRE(QFile::exists(auditTarget));
        source.hide();app.processEvents();
        REQUIRE(source.actions()==sourceOriginal);
        // Action editing is independent of the visual compact-menu toggle.
        REQUIRE(controller.setCompactMenus(false));
        source.popup(QPoint(50,50));app.processEvents();
        REQUIRE(!source.actions().contains(sourceRemove));
        bool customStillPresent=false;
        for(auto *a:source.actions())customStillPresent|=a->text()=="Open audit log";
        REQUIRE(customStillPresent);
        source.hide();app.processEvents();
        REQUIRE(source.actions()==sourceOriginal);
        REQUIRE(controller.setCompactMenus(true));
        sourceRemove->trigger();REQUIRE(removeTriggered==1);
        REQUIRE(sourceMute->isChecked());
        REQUIRE(controller.hasHiddenRule(QJsonObject{{"text","Remove"},{"menuObjectName","sourceActions"}}));
        REQUIRE(controller.setHiddenRule(QJsonObject{{"text","Remove"},{"menuObjectName","sourceActions"}},false));
        REQUIRE(controller.setHiddenRule(QJsonObject{{"text","Mute"},{"menuObjectName","sourceActions"}},false));
        REQUIRE(controller.removeCustomAction("open-audit"));
        source.popup(QPoint(50,50));app.processEvents();
        REQUIRE(source.actions()==sourceOriginal);
        source.hide();app.processEvents();
        REQUIRE(write(R"({"enabled":true,"rules":[],"menuCustom":[{"id":"bad","label":"oops","program":"x","args":"not-an-array","menuObjectName":"sourceActions"}]})"));
        REQUIRE(!controller.setHiddenRule(QJsonObject{{"text","Remove"}},true));
        REQUIRE(write(R"({"enabled":true,"rules":[]})"));
        // The actual OBS Appearance combo uses a model, not a QMenu.
        QWidget settings;settings.setObjectName("OBSBasicSettings");
        QComboBox themes(&settings);themes.setObjectName("themeVariant");
        // The real picker contains 166 styles, many with long shared prefixes.
        // Each column must display enough text to distinguish them.
        for(int i=0;i<166;++i)
            themes.addItem(i<52 ? QString("Catppuccin Macchiato — Variant %1").arg(i)
                                : QString("Colorway %1").arg(i),i);
        settings.show();themes.show();app.processEvents();
        int activated=-1;
        QObject::connect(&themes,&QComboBox::activated,[&](int row){activated=row;});
        const QPoint center=themes.rect().center();
        QMouseEvent press(QEvent::MouseButtonPress,center,themes.mapToGlobal(center),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(&themes,&press);app.processEvents();
        auto *picker=settings.findChild<QWidget*>("aftermarket-theme-picker");
        REQUIRE(picker&&picker->isVisible());
        auto *grid=picker->findChild<QListWidget*>();
        REQUIRE(grid&&grid->count()==166&&grid->isWrapping());
        // Before the fix, a 180px grid cell could paint its item in only
        // ~83px (the text's natural width), leaving wide empty gaps and
        // shortening related Colorway variants to indistinguishable names.
        REQUIRE(grid->visualItemRect(grid->item(0)).width()>=grid->gridSize().width()-4);
        REQUIRE(grid->visualItemRect(grid->item(1)).width()>=grid->gridSize().width()-4);
        REQUIRE(grid->visualItemRect(grid->item(0)).x()!=grid->visualItemRect(grid->item(1)).x());
        auto *search=picker->findChild<QLineEdit*>();
        REQUIRE(search);
        search->setText("Macchiato");app.processEvents();
        REQUIRE(!grid->item(0)->isHidden()&&grid->item(83)->isHidden());
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
        REQUIRE(themes.count()==166&&themes.currentIndex()==83&&activated==83);
        settings.hide();
        REQUIRE(controller.setCompactMenus(false));
        REQUIRE(!controller.compactMenusEnabled());
        longMenu.popup(QPoint(50,50));app.processEvents();
        REQUIRE(longMenu.actions()==originals&&longMenu.styleSheet()==style);
        longMenu.hide();app.processEvents();
    }
    {
        REQUIRE(write(R"({"enabled":true,"rules":[],"showSubmenuSelections":false})"));
        augment::Controller c(path);
        QMenu root;
        auto *scale=root.addMenu("Scale Filtering");
        scale->setObjectName("filterOptions");
        QAction *disable=scale->addAction("Disable");disable->setCheckable(true);
        QAction *bilinear=scale->addAction("Bilinear");bilinear->setCheckable(true);
        QAction *lanczos=scale->addAction("Lanczos");lanczos->setCheckable(true);lanczos->setChecked(true);
        auto *color=root.addMenu("Set Color");
        auto *swatch=color->addAction("");
        swatch->setCheckable(true);swatch->setChecked(true);
        QPixmap red(18,18);red.fill(QColor("#ca4141"));swatch->setIcon(QIcon(red));
        swatch->setData(QColor("#ca4141"));
        auto *other=color->addAction("Clear");other->setCheckable(true);
        auto *empty=root.addMenu("Other");empty->addAction("Open dialog");
        root.addSeparator();
        auto *rename=root.addAction("Rename");
        root.popup(QPoint(50,50));app.processEvents();
        REQUIRE(scale->menuAction()->text()=="Scale Filtering");
        root.hide();app.processEvents();
        REQUIRE(c.setShowSubmenuSelections(true));
        REQUIRE(c.showSubmenuSelections());
        root.popup(QPoint(50,50));app.processEvents();
        REQUIRE(scale->menuAction()->text()=="Scale Filtering");
        REQUIRE(scale->menuAction()->property("_aftermarketSelectedChoice").toString()=="Lanczos");
        REQUIRE(root.findChild<QWidget*>("aftermarket-overlay")->isVisible());
        const auto selectedRow=root.actionGeometry(scale->menuAction());
        REQUIRE(root.actionAt(selectedRow.center())==scale->menuAction());
        REQUIRE(root.width()>=root.fontMetrics().horizontalAdvance("Scale Filtering")+
            root.fontMetrics().horizontalAdvance("(Lanczos)")+60);
        QImage selectedPixels=root.grab().toImage().convertToFormat(QImage::Format_RGB32);
        int coloredRightPixels=0;
        const auto accent=root.palette().color(QPalette::Link);
        const auto textColor=root.palette().color(QPalette::Text);
        for(int y=selectedRow.top()+2;y<selectedRow.bottom()-2;++y)
            for(int x=selectedRow.center().x();x<selectedRow.right()-24;++x){
                const auto c=selectedPixels.pixelColor(x,y);
                const int dl=qAbs(c.red()-accent.red())+qAbs(c.green()-accent.green())+qAbs(c.blue()-accent.blue());
                const int dt=qAbs(c.red()-textColor.red())+qAbs(c.green()-textColor.green())+qAbs(c.blue()-textColor.blue());
                if(dl<dt)++coloredRightPixels;
            }
        REQUIRE(coloredRightPixels>10);
        REQUIRE(color->menuAction()->text()=="Set Color");
        REQUIRE(color->menuAction()->property("_aftermarketSelectedChoice").toString()=="#ca4141");
        REQUIRE(empty->menuAction()->text()=="Other");
        auto snap=augment::inspectWidget(&root,root.mapToGlobal(root.actionGeometry(scale->menuAction()).center()));
        REQUIRE(snap.isMenu&&snap.actions.size()==4);
        REQUIRE(snap.allActions.size()>=10);
        REQUIRE(snap.allActions.at(0).text=="Scale Filtering");
        REQUIRE(snap.allActions.at(1).text=="Disable"&&snap.allActions.at(1).depth==1);
        REQUIRE(snap.allActions.at(3).text=="Lanczos"&&snap.allActions.at(3).checked);
        snap.actions=snap.allActions;
        snap.actions[3].checkable=false; // checkable entries are scopeable for placement, not icon painting
        const auto nestedSelector=augment::selectorFor(snap,3,true);
        REQUIRE(nestedSelector.value("menuObjectName").toString()=="filterOptions");
        REQUIRE(snap.allActions.at(3).scopeLabels.contains("Bilinear"));
        root.hide();app.processEvents();
        REQUIRE(scale->menuAction()->text()=="Scale Filtering");
        REQUIRE(color->menuAction()->text()=="Set Color");
        REQUIRE(!scale->menuAction()->property("_aftermarketSelectedChoice").isValid());
        REQUIRE(root.minimumWidth()==0);
        // OBS's preset color buttons are not checkable QActions. The chosen
        // one receives a 2px border on its QWidgetAction button.
        swatch->setChecked(false);
        auto *colors=new QWidget;
        colors->setObjectName("ColorSelect");
        auto *preset=new QPushButton(colors);
        preset->setObjectName("preset3");preset->setProperty("bgColor",3);
        preset->setStyleSheet("border: 2px solid black");
        auto *swatches=new QWidgetAction(color);
        swatches->setDefaultWidget(colors);
        color->addAction(swatches);
        root.popup(QPoint(50,50));app.processEvents();
        REQUIRE(color->menuAction()->text()=="Set Color");
        REQUIRE(color->menuAction()->property("_aftermarketSelectedChoice").toString()=="Green");
        const auto colorSnapshot=augment::inspectWidget(&root,
            root.mapToGlobal(root.actionGeometry(color->menuAction()).center()));
        bool greenPreset=false;
        for(const auto &a:colorSnapshot.allActions)if(a.text=="Green"&&a.checked&&a.depth==2)
            greenPreset=true;
        REQUIRE(greenPreset);
        root.hide();app.processEvents();
        REQUIRE(color->menuAction()->text()=="Set Color");
        lanczos->setChecked(false);bilinear->setChecked(true);
        root.popup(QPoint(50,50));app.processEvents();
        REQUIRE(scale->menuAction()->text()=="Scale Filtering");
        REQUIRE(scale->menuAction()->property("_aftermarketSelectedChoice").toString()=="Bilinear");
        root.hide();app.processEvents();
        bilinear->setChecked(false);disable->setChecked(true);
        REQUIRE(c.setShowSubmenuSelections(false));
        root.popup(QPoint(50,50));app.processEvents();
        REQUIRE(scale->menuAction()->text()=="Scale Filtering");
        root.hide();app.processEvents();
        REQUIRE(c.setShowSubmenuSelections(true));
        QMenu lazyRoot;
        auto *lazy=lazyRoot.addMenu("Lazy options");
        auto *first=lazy->addAction("First");first->setCheckable(true);first->setChecked(true);
        auto *second=lazy->addAction("Second");second->setCheckable(true);
        QObject::connect(lazy,&QMenu::aboutToShow,lazy,[&] {
            first->setChecked(false);second->setChecked(true);
        });
        lazyRoot.popup(QPoint(60,60));app.processEvents();
        REQUIRE(lazy->menuAction()->text()=="Lazy options");
        REQUIRE(lazy->menuAction()->property("_aftermarketSelectedChoice").toString()=="First");
        lazy->popup(QPoint(290,60));app.processEvents();
        REQUIRE(lazy->menuAction()->text()=="Lazy options");
        REQUIRE(lazy->menuAction()->property("_aftermarketSelectedChoice").toString()=="Second");
        lazy->hide();lazyRoot.hide();app.processEvents();
        REQUIRE(lazy->menuAction()->text()=="Lazy options");
        REQUIRE(!lazy->menuAction()->property("_aftermarketSelectedChoice").isValid());
        // Multiple checked options are not an unambiguous single choice.
        first->setChecked(true);
        lazyRoot.popup(QPoint(60,60));app.processEvents();
        REQUIRE(lazy->menuAction()->text()=="Lazy options");
        lazyRoot.hide();app.processEvents();
        // RTL keeps the choice on the trailing (left) edge, and never
        // takes over Qt's native submenu hit targets.
        QMenu rtl;
        rtl.setLayoutDirection(Qt::RightToLeft);
        auto *choiceMenu=rtl.addMenu("Filtering");
        auto *choice=choiceMenu->addAction("Lanczos");
        choice->setCheckable(true);choice->setChecked(true);
        rtl.popup(QPoint(55,55));app.processEvents();
        const auto rtlRow=rtl.actionGeometry(choiceMenu->menuAction());
        REQUIRE(rtl.actionAt(rtlRow.center())==choiceMenu->menuAction());
        REQUIRE(choiceMenu->menuAction()->text()=="Filtering");
        REQUIRE(choiceMenu->menuAction()->property("_aftermarketSelectedChoice").toString()=="Lanczos");
        REQUIRE(rtl.findChild<QWidget*>("aftermarket-overlay")->isVisible());
        rtl.hide();app.processEvents();
        REQUIRE(choiceMenu->menuAction()->text()=="Filtering");
        REQUIRE(!choiceMenu->menuAction()->property("_aftermarketSelectedChoice").isValid());
    }
    std::cout << "menu rendering, preservation, reload, cleanup passed\n";
}
