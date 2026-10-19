#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlApplicationEngine;
class QQmlEngine;
class QJSEngine;

class LanguageManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString currentLanguage READ currentLanguage WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QVariantList availableLanguages READ availableLanguages CONSTANT)

public:
    static LanguageManager *instance();
    static LanguageManager *create(QQmlEngine *engine, QJSEngine *)
    {
        auto *self = instance();
        self->setEngine(engine);
        return self;
    }

    void init(QQmlApplicationEngine *engine);
    void setEngine(QQmlEngine *engine);

    QString currentLanguage() const { return m_current; }
    Q_INVOKABLE void setLanguage(const QString &lang);
    QVariantList availableLanguages() const;

Q_SIGNALS:
    void languageChanged();

private:
    explicit LanguageManager(QObject *parent = nullptr);

    QQmlEngine *m_engine = nullptr;
    QTranslator m_translator;
    bool m_installed = false;
    QString m_current = QStringLiteral("en");
};
