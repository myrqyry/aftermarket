#include "source-picker.h"
#include <QApplication>
#include <QDialog>
#include <QDockWidget>
#include <QMainWindow>
#include <QEvent>
#include <QListWidget>
#include <QScrollArea>
#include <QCheckBox>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QFontMetrics>
#include <QTimer>
#include <QFrame>
#include <QBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QFont>
#include <QStyle>
#include <QPointer>
#include <QScreen>
#include <QWindow>
#include <QTabBar>
#include <QStackedWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QShortcut>
#include <QSignalBlocker>
#include <QAbstractButton>
#include <QVector>
#include <QPainter>
#include <QPixmap>
#include <memory>
#include <algorithm>

namespace augment {
SourcePicker::SourcePicker(QMainWindow *main,QString settingsFile,QObject *parent)
    :QObject(parent),main_(main),settingsFile_(std::move(settingsFile)) {
    if(!settingsFile_.isEmpty()){
        QSettings settings(settingsFile_,QSettings::IniFormat);
        enabled_=settings.value("sourcePicker/embedded",true).toBool();
    }
    if(qApp)qApp->installEventFilter(this);
}
SourcePicker::~SourcePicker(){
    if(qApp)qApp->removeEventFilter(this);
}
bool SourcePicker::setEnabled(bool value){
    if(!settingsFile_.isEmpty()){
        QSettings settings(settingsFile_,QSettings::IniFormat);
        settings.setValue("sourcePicker/embedded",value);
        settings.sync();
        if(settings.status()!=QSettings::NoError)return false;
    }
    enabled_=value;
    return true;
}
bool SourcePicker::eventFilter(QObject *watched,QEvent *event){
    // Qt posts Polish before QDialog::exec begins its local event loop.
    // Reparenting AFTER Show causes QDialog::exec to return prematurely.
    if(enabled_&&main_&&event->type()==QEvent::Polish&&
       watched->objectName()==QStringLiteral("OBSBasicSourceSelect")){
        if(auto *dialog=qobject_cast<QDialog*>(watched))embed(dialog);
    }
    return false;
}
namespace {
constexpr int TypeRole=Qt::UserRole+1;
constexpr int DeprecatedRole=Qt::UserRole+2;
const QSize ThumbnailSize(64,36);
constexpr int ThumbnailKeyRole=Qt::UserRole+3;

void compactList(QListWidget *list){
    list->setMinimumSize(0,0);
    list->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
    list->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding);
    list->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setIconSize(QSize(16,16));
    list->setSpacing(0);
    list->setTextElideMode(Qt::ElideRight);
    list->setStyleSheet("QListWidget::item {padding: 2px 5px; min-height: 18px;}");
    list->setUniformItemSizes(true);
}
void compactItem(QListWidgetItem *item,QListWidget *list){
    item->setSizeHint(QSize(0,qMax(22,list->fontMetrics().height()+6)));
    if(item->toolTip().isEmpty())item->setToolTip(item->text());
}

struct ExistingRows {
    QPointer<QWidget> frame;
    QPointer<QListWidget> list;
    QPointer<QLineEdit> search;
    QPointer<QLabel> empty;
    QVector<QPointer<QAbstractButton>> buttons;
    QStringList names;
    void updateThumbnail(QListWidgetItem *item,QAbstractButton *button){
        const auto *label=button?button->findChild<QLabel*>("thumbnail"):nullptr;
        const QPixmap native=label?label->pixmap():QPixmap{};
        const qint64 key=native.isNull()?0:native.cacheKey();
        if(item->data(ThumbnailKeyRole).isValid()&&
           item->data(ThumbnailKeyRole).toLongLong()==key)return;
        item->setData(ThumbnailKeyRole,key);
        if(native.isNull()){
            item->setIcon(QIcon{});
            return;
        }
        // Copy OBS's existing thumbnail, never capture or render the source
        // ourselves. Preserve aspect ratio and give every preview equal space.
        const qreal dpr=list->devicePixelRatioF();
        QPixmap tile(ThumbnailSize*dpr);
        tile.setDevicePixelRatio(dpr);
        tile.fill(Qt::transparent);
        const QPixmap scaled=native.scaled(tile.size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);
        QPainter painter(&tile);
        const QSizeF logical=QSizeF(scaled.size())/dpr;
        painter.drawPixmap(QRectF((ThumbnailSize.width()-logical.width())/2,
                                 (ThumbnailSize.height()-logical.height())/2,
                                 logical.width(),logical.height()),scaled,QRectF(scaled.rect()));
        painter.end();
        item->setIcon(QIcon(tile));
    }
    void refresh(){
        if(!frame||!frame->layout()||!list||!search)return;
        QVector<QPointer<QAbstractButton>> next;
        QStringList labels;
        // Read the active native layout, not findChildren: OBS deletes old
        // cards later when a source type changes.
        for(int i=0;i<frame->layout()->count();++i){
            auto *item=frame->layout()->itemAt(i);
            auto *button=item?qobject_cast<QAbstractButton*>(item->widget()):nullptr;
            if(!button||!button->isCheckable())continue;
            auto *label=button->findChild<QLabel*>("name");
            const auto name=label?label->text():button->accessibleName();
            if(name.isEmpty())continue;
            next.append(button);labels.append(name);
        }
        if(next!=buttons||labels!=names){
            buttons=next;names=labels;
            const QSignalBlocker block(list);
            list->clear();
            for(const auto &name:names){
                auto *item=new QListWidgetItem(name,list);
                compactItem(item,list);
                item->setSizeHint(QSize(0,qMax(ThumbnailSize.height()+8,list->fontMetrics().height()+6)));
            }
        }
        const QString query=search->text().trimmed();
        const QSignalBlocker block(list);
        int matching=0;
        for(int i=0;i<list->count();++i){
            auto *item=list->item(i);
            const bool shown=item->text().contains(query,Qt::CaseInsensitive);
            item->setHidden(!shown);
            if(shown)updateThumbnail(item,buttons[i]);
            item->setSelected(buttons[i]&&buttons[i]->isChecked());
            item->setFlags(buttons[i]&&buttons[i]->isEnabled()?
                Qt::ItemIsEnabled|Qt::ItemIsSelectable:Qt::NoItemFlags);
            if(shown)++matching;
        }
        if(empty)empty->setVisible(matching==0);
    }
    void choose(QListWidgetItem *item){
        if(!list||!item||item->isHidden())return;
        int index=list->row(item);
        if(index>=0&&index<buttons.size()&&buttons[index]&&buttons[index]->isEnabled())
            buttons[index]->click(); // Native selection and UUID ownership.
        refresh();
    }
};

void placePicker(QDialog *dialog,QMainWindow *main,QDockWidget *sources){
    // Keep the compact transient inside the owner's visible client area.
    // Window coordinates are global, unlike those of the former child widget.
    QRect work(main->mapToGlobal(QPoint(0,0)),main->size());
    const QPoint origin=sources->mapToGlobal(QPoint(0,0));
    auto *screen=QGuiApplication::screenAt(origin+QPoint(sources->width()/2,sources->height()/2));
    if(!screen)screen=main->screen();
    if(screen){
        const QRect visible=work.intersected(screen->availableGeometry());
        if(!visible.isEmpty())work=visible;
    }
    const int w=qMax(1,qMin(380,work.width()-16));
    const int h=qMax(1,qMin(400,work.height()-16));
    const bool left=origin.x()+sources->width()/2<work.center().x();
    const int x=qBound(work.left(),left?origin.x()+sources->width()-8:origin.x()-w+8,
                       work.right()-w+1);
    const int y=qBound(work.top(),origin.y()+sources->height()-h+8,
                       work.bottom()-h+1);
    const QRect next(x,y,w,h);
    if(dialog->geometry()!=next)dialog->setGeometry(next);
}
}
void SourcePicker::embed(QDialog *dialog){
    if(!main_||!dialog||!dialog->isWindow()||dialog->isVisible())return;
    auto *root=qobject_cast<QBoxLayout*>(dialog->layout());
    auto *inner=dialog->findChild<QWidget*>("dialogInner");
    auto *list=dialog->findChild<QListWidget*>("sourceTypeList");
    auto *existing=dialog->findChild<QScrollArea*>("existingScrollArea");
    auto *frame=dialog->findChild<QWidget*>("existingListFrame");
    auto *visible=dialog->findChild<QCheckBox*>("sourceVisible");
    auto *add=dialog->findChild<QPushButton*>("addExistingButton");
    auto *create=dialog->findChild<QPushButton*>("createNewSource");
    auto *sources=main_->findChild<QDockWidget*>("sourcesDock");
    // Check the exact native controls and type identities before modifying
    // the original dialog; otherwise leave unknown versions alone.
    if(!root||!inner||!list||!existing||!frame||!frame->layout()||!visible||
       !add||!create||!sources||sources->isHidden()||
       main_->width()<360||main_->height()<280)return;
    bool hasRecent=false,hasType=false;
    for(int i=0;i<list->count();++i){
        const auto id=list->item(i)->data(TypeRole).toString();
        hasRecent|=id=="_recent";hasType|=!id.isEmpty()&&id!="_recent"&&id!="scene";
    }
    if(!hasRecent||!hasType)return;

    // Retain OBS's dialog and hidden native card layout. Move only the real
    // list, visibility checkbox and Add existing button to a compact shell.
    // Native creation, undo, selection, refresh and properties remain intact.
    // A QWidget child can be occluded by OBSQTDisplay's native graphics
    // surface. Keep a real modal transient, with no desktop chrome, so its
    // stacking is owned by the window system and remains above that surface.
    dialog->setParent(main_,Qt::Dialog|Qt::FramelessWindowHint);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    dialog->setMinimumSize(0,0);
    dialog->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
    dialog->setAutoFillBackground(true);
    root->setSizeConstraint(QLayout::SetNoConstraint);
    root->setContentsMargins(6,4,6,6);
    root->setSpacing(4);
    inner->hide();

    auto *heading=new QWidget(dialog);
    heading->setObjectName("aftermarket-source-picker-heading");
    auto *row=new QHBoxLayout(heading);row->setContentsMargins(4,0,2,0);row->setSpacing(4);
    auto *title=new QLabel(dialog->windowTitle().isEmpty()?QStringLiteral("Add Source"):dialog->windowTitle(),heading);
    QFont font=title->font();font.setWeight(QFont::DemiBold);title->setFont(font);
    row->addWidget(title,1);
    auto *close=new QToolButton(heading);
    close->setObjectName("aftermarket-source-picker-close");
    close->setToolTip("Close Add Source");close->setAccessibleName(close->toolTip());
    close->setAutoRaise(true);close->setIcon(dialog->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    row->addWidget(close);
    QObject::connect(close,&QToolButton::clicked,dialog,&QDialog::reject);
    root->addWidget(heading);

    auto *tabs=new QTabBar(dialog);tabs->setObjectName("aftermarket-source-picker-tabs");
    tabs->setExpanding(true);tabs->setDrawBase(false);
    tabs->addTab("Add new");tabs->addTab("Add existing");root->addWidget(tabs);
    auto *pages=new QStackedWidget(dialog);
    pages->setMinimumSize(0,0);pages->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding);
    root->addWidget(pages,1);
    list->setParent(pages);compactList(list);pages->addWidget(list);
    auto *existingPage=new QWidget(pages);
    auto *existingLayout=new QVBoxLayout(existingPage);
    existingLayout->setContentsMargins(0,0,0,0);existingLayout->setSpacing(4);
    auto *types=new QComboBox(existingPage);types->setObjectName("aftermarket-existing-type");
    types->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    types->setAccessibleName("Existing source type");
    auto *search=new QLineEdit(existingPage);search->setObjectName("aftermarket-existing-search");
    search->setPlaceholderText("Filter by source name");search->setClearButtonEnabled(true);
    search->setAccessibleName("Filter existing sources");
    auto *rows=new QListWidget(existingPage);rows->setObjectName("aftermarket-existing-list");
    compactList(rows);rows->setIconSize(ThumbnailSize);rows->setSelectionMode(QAbstractItemView::ExtendedSelection);
    auto *empty=new QLabel("No matching sources",existingPage);empty->setAlignment(Qt::AlignCenter);
    existingLayout->addWidget(types);existingLayout->addWidget(search);
    existingLayout->addWidget(rows,1);existingLayout->addWidget(empty);
    add->setParent(existingPage);
    add->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    add->setMinimumWidth(0);existingLayout->addWidget(add);add->show();
    pages->addWidget(existingPage);
    visible->setParent(dialog);visible->setMinimumWidth(0);
    visible->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    root->addWidget(visible);visible->show();

    QVector<QListWidgetItem*> typeItems;
    int recentIndex=0;
    for(int i=0;i<list->count();++i){
        auto *item=list->item(i);
        const QString id=item->data(TypeRole).toString();
        compactItem(item,list);
        const bool selectable=!id.isEmpty()&&item->flags().testFlag(Qt::ItemIsEnabled)&&
            item->flags().testFlag(Qt::ItemIsSelectable);
        item->setHidden(!selectable||id=="_recent"||id=="scene");
        if(!selectable)continue;
        if(item->data(DeprecatedRole).toBool())item->setToolTip(item->text()+" (Deprecated)");
        if(id=="_recent")recentIndex=types->count();
        types->addItem(item->icon(),id=="_recent"?QStringLiteral("Recent sources"):item->text(),id);
        typeItems.append(item);
    }
    types->setCurrentIndex(recentIndex);
    auto state=std::make_shared<ExistingRows>();
    state->frame=frame;state->list=rows;state->search=search;state->empty=empty;
    auto selectType=[list,types,typeItems,state]{
        const int index=types->currentIndex();
        if(index>=0&&index<typeItems.size()){
            list->setCurrentItem(typeItems[index]);
            state->refresh();
        }
    };
    QObject::connect(types,qOverload<int>(&QComboBox::currentIndexChanged),dialog,[selectType](int){selectType();});
    QObject::connect(search,&QLineEdit::textChanged,dialog,[state]{state->refresh();});
    QObject::connect(rows,&QListWidget::itemClicked,dialog,[state](QListWidgetItem *item){state->choose(item);});
    // OBS's original double-click path would be redundant alongside the
    // single-click create action. Keep the actual native create button slot.
    QObject::disconnect(list,SIGNAL(itemDoubleClicked(QListWidgetItem*)),dialog,nullptr);
    auto createType=[list,create,pages](QListWidgetItem *item){
        if(pages->currentIndex()!=0||!item||item->isHidden())return;
        const QString id=item->data(TypeRole).toString();
        if(id.isEmpty()||id=="scene"||id=="_recent"||!item->flags().testFlag(Qt::ItemIsEnabled))return;
        list->setCurrentItem(item);
        create->click();
    };
    QObject::connect(list,&QListWidget::itemClicked,dialog,createType);
    for(auto key:{Qt::Key_Return,Qt::Key_Enter}){
        auto *shortcut=new QShortcut(QKeySequence(key),list);
        shortcut->setContext(Qt::WidgetShortcut);
        QObject::connect(shortcut,&QShortcut::activated,dialog,[list,createType]{createType(list->currentItem());});
        auto *choose=new QShortcut(QKeySequence(key),rows);
        choose->setContext(Qt::WidgetShortcut);
        QObject::connect(choose,&QShortcut::activated,dialog,[rows,state]{state->choose(rows->currentItem());});
    }
    QPointer<QMainWindow> main=main_;
    QPointer<QDockWidget> dock=sources;
    auto *refresh=new QTimer(dialog);refresh->setInterval(150);
    QObject::connect(refresh,&QTimer::timeout,dialog,[dialog,main,dock,state,pages]{
        if(!main||!dock||!main->isVisible()||main->isMinimized()||!dock->isVisible()){dialog->reject();return;}
        placePicker(dialog,main,dock);
        if(pages->currentIndex()==1)state->refresh();
    });
    QObject::connect(dialog,&QDialog::finished,refresh,&QTimer::stop);
    QObject::connect(tabs,&QTabBar::currentChanged,dialog,[pages,list,search,selectType](int index){
        pages->setCurrentIndex(index);
        if(index==1){selectType();search->setFocus();}
        else list->setFocus();
    });
    pages->setCurrentIndex(0);
    placePicker(dialog,main_,sources);
    QTimer::singleShot(0,dialog,[dialog,refresh,list]{
        if(dialog->isVisible()){dialog->raise();list->setFocus();refresh->start();}
    });
}
}
