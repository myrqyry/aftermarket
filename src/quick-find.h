#pragma once
#include <QDialog>
#include <QList>
#include <QString>
#include <functional>

class QLineEdit;
class QListWidget;
class QPushButton;

namespace augment {
struct QuickFindEntry {
    enum Kind { Scene, Source, Filter };
    Kind kind=Source;
    QString id;          // stable OBS UUID
    QString ownerId;     // for filters: parent source UUID
    QString name;        // user-visible source/filter name
    QString type;        // OBS source type
    QString detail;      // explicitly permitted file path or URL
    QString location;    // parent scene or filter owner name
    QString sceneId;     // parent scene UUID; only for a source hit
    bool performanceAvailable=false;
    QString note; // private annotation, separate from native source detail
};

class QuickFind final: public QDialog {
public:
    enum Action { Default, Properties, Filters, SceneJump, PinSource, Performance, Note };
    using Provider=std::function<QList<QuickFindEntry>()>;
    using Performer=std::function<bool(const QuickFindEntry&,Action)>;
    explicit QuickFind(QWidget *parent,Provider provider,Performer perform,bool pinAvailable=false,bool notesAvailable=false);
    static QList<QuickFindEntry> search(const QList<QuickFindEntry> &all,
                                        const QString &query,int limit=80);
    void open();
    bool activate(Action action=Default);
private:
    void updateResults();
    QList<QuickFindEntry> cached_;
    Provider provider_;
    Performer perform_;
    QLineEdit *input_=nullptr;
    QListWidget *results_=nullptr;
    QPushButton *properties_=nullptr,*filters_=nullptr,*jump_=nullptr,*pin_=nullptr;
};

QList<QuickFindEntry> collectObsQuickFind();
bool performObsQuickFind(const QuickFindEntry &entry,QuickFind::Action action);
} // namespace augment
