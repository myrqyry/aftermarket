#pragma once
#include <QString>
#include <QJsonObject>
#include <QRect>
#include <Qt>
#include <functional>
#include <memory>
class QWidget;
class QListWidget;
class QListWidgetItem;
namespace augment {
// Local, private text attachments; keys identify objects, not display names.
QString noteKey(const QString &kind,const QString &id,const QString &owner={},qint64 item=-1);
class ContextNotes {
public:
 explicit ContextNotes(QString path);
 QString get(const QString &key)const;
 bool reload();
 bool put(const QString &key,const QString &expected,const QString &text);
private:
 QString path_;
 QJsonObject notes_;
 bool read(QJsonObject &)const;
};
constexpr int NoteTextRole=Qt::UserRole+87;
QString noteTooltip(const QString &text);
QRect noteBadgeRect(const QRect &row,Qt::LayoutDirection direction);
void attachNoteRows(QListWidget *,std::function<void(QListWidgetItem *)> edit);
void editContextNote(QWidget *parent,std::shared_ptr<ContextNotes>,const QString &key,const QString &name,
                     std::function<bool()> valid,std::function<void()> changed);
}
