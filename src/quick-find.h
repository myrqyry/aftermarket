#pragma once
#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <functional>

class QLineEdit;
class QListWidget;
class QPushButton;
class QComboBox;
class QLabel;
class QMenu;

namespace augment {
struct QuickFindPlacement {
    QString rootId, parentId, key, path, canvasId, canvasName;
    qint64 itemId=0;
    bool mainCanvas=false, revealable=false;
};
struct QuickFindFilter {
    enum Scope { All, Sources, Filters, Scenes, Unplaced };
    Scope scope=All;
    QString type,canvas;
};
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
    QString typeName;
    QStringList canvasIds,canvasNames;
    QList<QuickFindPlacement> placements;
    bool mainCanvas=true,unplaced=false;
    QString revealKey;
};

class QuickFind final: public QDialog {
public:
    enum Action { Default, Properties, Filters, SceneJump, PinSource, Performance, Note, RevealPlacement };
    using Provider=std::function<QList<QuickFindEntry>()>;
    using Performer=std::function<bool(const QuickFindEntry&,Action)>;
    explicit QuickFind(QWidget *parent,Provider provider,Performer perform,bool pinAvailable=false,bool notesAvailable=false);
    static QList<QuickFindEntry> search(const QList<QuickFindEntry> &all,
                                        const QString &query,int limit=80,const QuickFindFilter &filter={});
    void open();
    void refreshCatalog();
    void setRevealAvailable(bool available){revealAvailable_=available;}
    void setChangeProvider(std::function<quint64()> provider,std::function<void(bool)> watch={});
    void invalidateCollection();
    bool activate(Action action=Default);
protected:
    void hideEvent(QHideEvent *event)override;
private:
    void updateResults();
    void updateFacets();
    void refreshIfChanged();
    void appendUsage(QMenu &menu,const QuickFindEntry &entry);
    QuickFindFilter filter()const;
    void performEntry(const QuickFindEntry&,Action);
    QComboBox *scope_=nullptr,*type_=nullptr,*canvas_=nullptr;
    QLabel *count_=nullptr;
    QPushButton *used_=nullptr;
    QTimer refreshTimer_;
    std::function<quint64()> changeProvider_;
    std::function<void(bool)> watch_;
    quint64 generation_=0;
    bool revealAvailable_=false;
    QList<QuickFindEntry> cached_;
    Provider provider_;
    Performer perform_;
    QLineEdit *input_=nullptr;
    QListWidget *results_=nullptr;
    QPushButton *properties_=nullptr,*filters_=nullptr,*jump_=nullptr,*pin_=nullptr;
};

QList<QuickFindEntry> collectObsQuickFind();
void startQuickFindWatch();
void stopQuickFindWatch();
void refreshQuickFindWatch();
quint64 quickFindGeneration();
bool validateQuickFindPlacement(const QuickFindEntry&,const QuickFindPlacement&);
bool performObsQuickFind(const QuickFindEntry &entry,QuickFind::Action action);
} // namespace augment
