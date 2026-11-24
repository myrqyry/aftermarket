#pragma once
#include <QObject>
#include <QPointer>
#include <QList>
#include <QString>
#include <QPoint>
#include <functional>
class QMenuBar;
class QWidget;
class QTimer;
class QToolButton;
class QDialog;
class QEvent;
namespace augment {
struct StripItem {
    QString id,type,target,label;
    bool operator==(const StripItem &other) const {
        return id==other.id&&type==other.type&&target==other.target&&label==other.label;
    }
};
struct HotkeyChoice {
    QString binding,label,group;
};
struct LayoutChoice {
    QString id,name;
};
struct StatusSnapshot {
    bool recording=false,streaming=false;
    double fps=0,cpu=0,streamKbps=0,recordKbps=0;
    qint64 recordSeconds=-1,streamSeconds=-1;
    quint64 dropped=0,lagged=0;
};
class ActionStrip final : public QObject {
public:
    using StatusProvider=std::function<StatusSnapshot()>;
    using HotkeyList=std::function<QList<HotkeyChoice>()>;
    using HotkeyTrigger=std::function<bool(const QString &)>;
    using LayoutList=std::function<QList<LayoutChoice>()>;
    using LayoutApply=std::function<bool(const QString &)>;
    explicit ActionStrip(QString configFile,StatusProvider status={},HotkeyList hotkeys={},
                         HotkeyTrigger trigger={},QObject *parent=nullptr,
                         LayoutList layouts={},LayoutApply applyLayout={});
    ~ActionStrip() override;
    bool load();
    bool save() const;
    bool enabled() const;
    bool desiredEnabled() const;
    bool setEnabled(QMenuBar *bar,bool enabled);
    QList<StripItem> items() const;
    bool addItem(StripItem item);
    bool replaceItem(const QString &id,StripItem item);
    bool removeItem(const QString &id);
    bool moveItem(const QString &id,int index);
    bool trigger(const QString &id);
    QWidget *widget() const;
    void refresh();
    QString itemText(const QString &id) const;
    void showEditor(QWidget *parent=nullptr,QMenuBar *bar=nullptr);
    void closeEditor();
protected:
    bool eventFilter(QObject *,QEvent *) override;
private:
    QString path_;
    QList<StripItem> items_;
    QPointer<QMenuBar> bar_;
    QPointer<QWidget> originalCorner_,host_,content_;
    QPointer<QTimer> timer_;
    QPointer<QDialog> editor_;
    QPointer<QToolButton> overflow_;
    QStringList overflowIds_;
    bool overflowBusy_=false,overflowScheduled_=false;
    StatusProvider status_;
    HotkeyList hotkeys_;
    HotkeyTrigger trigger_;
    LayoutList layouts_;
    LayoutApply applyLayout_;
    StatusSnapshot latest_;
    QList<HotkeyChoice> cachedHotkeys_;
    qint64 lastHotkeyPoll_=0;
    bool enabled_=false,desiredEnabled_=false,shutdown_=false;
    QPoint dragStart_;
    QString dragId_;
    static bool validItem(const StripItem &item);
    void rebuild();
    void updateOverflow();
    void scheduleOverflow();
    void openButtonMenu(const QString &id,QWidget *owner,const QPoint &global);
    QString chooseHotkey(QWidget *parent) const;
    QString chooseDock(QWidget *parent) const;
    QString chooseLayout(QWidget *parent) const;
    bool addFromEditor(const QString &type,QWidget *parent);
};
}
