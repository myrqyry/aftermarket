#include "theme-picker.h"
#include <QComboBox>
#include <QFrame>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QScreen>
#include <QPointer>
#include <QPersistentModelIndex>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QShortcut>
#include <QTimer>
#include <QScrollBar>
#include <QApplication>
#include <QtMath>
namespace augment {
namespace {
bool eligible(QComboBox *combo) {
    if(!combo||!combo->isEnabled()||combo->isEditable()||combo->count()<16)return false;
    if(combo->objectName()!="theme"&&combo->objectName()!="themeVariant")return false;
    const auto *window=combo->window();
    return window&&(window->inherits("OBSBasicSettings")||window->objectName()=="OBSBasicSettings");
}
bool openPicker(QComboBox *combo) {
    if(!eligible(combo)||!combo->isVisible())return false;
    if(auto *existing=combo->findChild<QFrame*>("aftermarket-theme-picker")) {
        if(existing->isVisible()){existing->close();return true;}
    }
    auto *screen=combo->screen();
    if(!screen)return false;
    const QRect area=screen->availableGeometry().adjusted(12,12,-12,-12);
    if(area.width()<320||area.height()<220)return false;
    auto *popup=new QFrame(combo,Qt::Popup);
    popup->setObjectName("aftermarket-theme-picker");
    popup->setAccessibleName("Theme choices");
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setFrameShape(QFrame::StyledPanel);
    popup->setAutoFillBackground(true);
    auto *layout=new QVBoxLayout(popup);layout->setContentsMargins(8,8,8,8);layout->setSpacing(6);
    auto *filter=new QLineEdit(popup);filter->setPlaceholderText("Find a theme…");filter->setAccessibleName("Filter theme choices");layout->addWidget(filter);
    auto *list=new QListWidget(popup);
    list->setAccessibleName("Available themes");
    list->setViewMode(QListView::ListMode);
    list->setFlow(QListView::LeftToRight);list->setWrapping(true);
    list->setResizeMode(QListView::Adjust);list->setMovement(QListView::Static);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setTextElideMode(Qt::ElideRight);
    list->setUniformItemSizes(true);
    layout->addWidget(list,1);
    auto *countLabel=new QLabel(popup);layout->addWidget(countLabel);
    int itemWidth=180;
    for(int i=0;i<combo->count();++i) {
        const QModelIndex idx=combo->model()->index(i,combo->modelColumn(),combo->rootModelIndex());
        if(!idx.isValid())continue;
        auto *item=new QListWidgetItem(combo->itemIcon(i),combo->itemText(i),list);
        item->setData(Qt::UserRole,QVariant::fromValue(QPersistentModelIndex(idx)));
        item->setToolTip(item->text());
        item->setFlags((idx.flags()&Qt::ItemIsEnabled)?Qt::ItemFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable):Qt::NoItemFlags);
        itemWidth=qMax(itemWidth,list->fontMetrics().horizontalAdvance(item->text())+44);
        if(i==combo->currentIndex())list->setCurrentItem(item);
    }
    const int rowHeight=qMax(28,list->fontMetrics().height()+10);
    itemWidth=qMin(itemWidth,qMax(140,(area.width()-52)/2));
    const int columns=qBound(1,(area.width()-48)/itemWidth,5);
    list->setGridSize(QSize(itemWidth,rowHeight));
    // QListView's gridSize positions cells but does NOT size the item delegate.
    // Without an explicit item hint the text is still painted in its natural
    // narrow rect, so large spaced-out columns show only 'Catpp...' fragments.
    // Give the original labels their full grid cell; keep elision only when a
    // label genuinely exceeds the available width.
    for(int i=0;i<list->count();++i)
        list->item(i)->setSizeHint(QSize(itemWidth,rowHeight));
    const int visibleRows=qMin(qCeil(double(list->count())/columns),qMax(3,(qMin(540,area.height())-90)/rowHeight));
    const QSize size(qMin(area.width(),columns*itemWidth+48),qMin(area.height(),visibleRows*rowHeight+90));
    auto refresh=[list,countLabel](const QString &query){
        int visible=0;
        for(int i=0;i<list->count();++i){
            auto *item=list->item(i);
            item->setHidden(!item->text().contains(query,Qt::CaseInsensitive));
            if(!item->isHidden())++visible;
        }
        if(list->currentItem()&&list->currentItem()->isHidden())list->setCurrentItem(nullptr);
        countLabel->setText(QString("%1 of %2 choices · scroll to browse").arg(visible).arg(list->count()));
    };
    refresh({});
    QObject::connect(filter,&QLineEdit::textChanged,popup,refresh);
    QPointer<QComboBox> safe(combo);
    QPointer<QFrame> safePopup(popup);
    auto choose=[safe,safePopup](QListWidgetItem *item){
        if(!safe||!safePopup||!safePopup->isVisible()||!item||item->isHidden()||!(item->flags()&Qt::ItemIsEnabled))return;
        const auto idx=item->data(Qt::UserRole).value<QPersistentModelIndex>();
        if(!idx.isValid()||idx.model()!=safe->model()||idx.parent()!=safe->rootModelIndex()||idx.column()!=safe->modelColumn()||!(idx.flags()&Qt::ItemIsEnabled))return;
        const int row=idx.row();
        const QString label=safe->itemText(row);
        if(label!=item->text())return;
        safePopup->close();
        if(!safe)return;
        safe->setCurrentIndex(row);
        if(safe&&safe->currentIndex()==row){emit safe->activated(row);if(safe)emit safe->textActivated(label);}
    };
    QObject::connect(list,&QListWidget::itemClicked,popup,choose);
    QObject::connect(list,&QListWidget::itemActivated,popup,choose);
    QObject::connect(filter,&QLineEdit::returnPressed,popup,[list,choose]{
        if(list->currentItem()&&!list->currentItem()->isHidden()){choose(list->currentItem());return;}
        for(int i=0;i<list->count();++i)if(!list->item(i)->isHidden()&&(list->item(i)->flags()&Qt::ItemIsEnabled)){choose(list->item(i));break;}
    });
    auto *escape=new QShortcut(QKeySequence(Qt::Key_Escape),popup);
    escape->setContext(Qt::WidgetWithChildrenShortcut);QObject::connect(escape,&QShortcut::activated,popup,&QWidget::close);
    const auto anchor=combo->mapToGlobal(QPoint(0,combo->height()));
    QPoint pos(qBound(area.left(),anchor.x(),area.right()-size.width()+1),anchor.y());
    if(pos.y()+size.height()>area.bottom())pos.setY(combo->mapToGlobal(QPoint()).y()-size.height());
    pos.setY(qBound(area.top(),pos.y(),area.bottom()-size.height()+1));
    popup->setGeometry(QRect(pos,size));popup->show();list->setFocus(Qt::PopupFocusReason);
    return true;
}
}
bool handleThemePicker(QObject *object,QEvent *event,bool enabled) {
    if(!enabled)return false;
    if(event->type()!=QEvent::MouseButtonPress&&event->type()!=QEvent::KeyPress)return false;
    auto *combo=qobject_cast<QComboBox*>(object);
    if(!eligible(combo))return false;
    if(event->type()==QEvent::MouseButtonPress) {
        if(static_cast<QMouseEvent*>(event)->button()!=Qt::LeftButton)return false;
    } else {
        auto *key=static_cast<QKeyEvent*>(event);
        if(key->key()!=Qt::Key_F4&&key->key()!=Qt::Key_Space&&!(key->key()==Qt::Key_Down&&(key->modifiers()&Qt::AltModifier)))return false;
    }
    if(!openPicker(combo))return false;
    event->accept();return true;
}
void closeThemePickers() {
    if(!qApp)return;
    QList<QPointer<QFrame>> pickers;
    for(auto *w:QApplication::allWidgets())if(auto *frame=qobject_cast<QFrame*>(w);
        frame&&frame->objectName()=="aftermarket-theme-picker")pickers.append(frame);
    for(auto &picker:pickers)if(picker)delete picker.data();
}

}
