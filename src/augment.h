#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QIcon>
#include <QColor>
#include <memory>
#include <functional>
#include "audio-motion.h"
class QMenu;
namespace augment {
QString normalizeActionText(QString text);
QStringList iconNames();
QIcon previewIcon(const QString &name, const QColor &color);
using Logger = std::function<void(const QString &)>;
class Controller final : public QObject {
public:
    explicit Controller(QString rulesFile, QObject *parent = nullptr, Logger logger = {});
    ~Controller() override;
    // Returns false without overwriting an invalid or unreadable file.
    void setMeterProvider(MeterProvider);
    void setAudioProvider(AudioProvider provider,AudioReset reset={});
    QJsonObject gradientSettings() const;
    bool setGradientSettings(const QJsonObject &, QString *error = nullptr, const QJsonObject *expected = nullptr);
    bool hasHiddenRule(const QJsonObject &selector) const;
    bool setHiddenRule(const QJsonObject &selector,bool hidden,QString *error=nullptr);
    QJsonArray allHiddenRules() const;
    QJsonArray allCustomActions() const;
    QJsonArray hiddenRulesForMenu(const QString &menuName,const QStringList &labels) const;
    QJsonArray customActionsForMenu(const QString &menuName,const QStringList &labels) const;
    bool setCustomAction(const QJsonObject &entry,QString *error=nullptr);
    bool removeCustomAction(const QString &id,QString *error=nullptr);
    bool hasOverflowRule(const QJsonObject &selector) const;
    bool setOverflowRule(const QJsonObject &selector,bool enabled,QString *error=nullptr);
    bool compactMenusEnabled() const;
    bool setCompactMenus(bool enabled, QString *error = nullptr);
    bool motionEnabled() const;
    bool setMotionEnabled(bool enabled, QString *error = nullptr);
    bool saveRule(const QJsonObject &rule, QString *error = nullptr);
    bool removeRule(const QJsonObject &selector, QString *error = nullptr);
    QJsonObject effectiveRule(const QJsonObject &target, const QStringList &menuLabels) const;
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    struct State;
    std::unique_ptr<State> state;
    void attach(QMenu *);
};
}
