#include "languagemanager.h"
#include "appsettings.h"

#include <QCoreApplication>
#include <QQmlApplicationEngine>

LanguageManager *LanguageManager::instance()
{
    static LanguageManager s_instance;
    return &s_instance;
}

LanguageManager::LanguageManager(QObject *parent)
    : QObject(parent)
{
}

void LanguageManager::setEngine(QQmlEngine *engine)
{
    if (engine && !m_engine) {
        m_engine = engine;
    }
}

void LanguageManager::init(QQmlApplicationEngine *engine)
{
    m_engine = engine;

    // Default is English ("en"), unless the user explicitly configured pt-BR in settings
    const QString saved = AppSettings::instance()->globals().value(QStringLiteral("language"), QStringLiteral("en")).toString();
    const QString lang = (saved.startsWith(QLatin1String("pt"), Qt::CaseInsensitive)) ? QStringLiteral("pt_BR") : QStringLiteral("en");

    if (lang == QLatin1String("pt_BR")) {
        if (m_translator.load(QStringLiteral(":/i18n/win11terminal_pt_BR.qm"))) {
            QCoreApplication::installTranslator(&m_translator);
            m_installed = true;
            m_current = QStringLiteral("pt_BR");
        }
    } else {
        m_current = QStringLiteral("en");
    }
}

void LanguageManager::setLanguage(const QString &lang)
{
    const QString target = (lang.startsWith(QLatin1String("pt"), Qt::CaseInsensitive)) ? QStringLiteral("pt_BR") : QStringLiteral("en");
    if (target == m_current && m_installed == (target == QLatin1String("pt_BR"))) {
        return;
    }

    if (target == QLatin1String("pt_BR")) {
        if (!m_installed) {
            if (m_translator.load(QStringLiteral(":/i18n/win11terminal_pt_BR.qm"))) {
                QCoreApplication::installTranslator(&m_translator);
                m_installed = true;
            }
        }
    } else {
        if (m_installed) {
            QCoreApplication::removeTranslator(&m_translator);
            m_installed = false;
        }
    }

    m_current = target;

    // Persist choice to settings.json
    AppSettings::instance()->setGlobal(QStringLiteral("language"), m_current);

    // Live update all active QML bindings with qsTr()
    if (m_engine) {
        m_engine->retranslate();
    }

    Q_EMIT languageChanged();
}

QVariantList LanguageManager::availableLanguages() const
{
    return {
        QVariantMap{
            {QStringLiteral("v"), QStringLiteral("en")},
            {QStringLiteral("l"), QStringLiteral("English")}
        },
        QVariantMap{
            {QStringLiteral("v"), QStringLiteral("pt_BR")},
            {QStringLiteral("l"), QStringLiteral("Português (Brasil)")}
        }
    };
}
