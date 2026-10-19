#include "systemfont.h"

#include <QFontDatabase>
#include <QStringList>

SystemFont::SystemFont(QObject *parent)
    : QObject(parent)
{
}

QString SystemFont::family() const
{
    static const QString familyName = [] {
        const QStringList preferred = {
            QStringLiteral("Segoe UI Variable Text"),
            QStringLiteral("Segoe UI"),
            QStringLiteral("Ubuntu"),
            QStringLiteral("Inter"),
            QStringLiteral("Cantarell"),
            QStringLiteral("Noto Sans"),
            QStringLiteral("Liberation Sans"),
            QStringLiteral("DejaVu Sans")
        };
        const QStringList available = QFontDatabase::families();
        for (const QString &f : preferred) {
            if (available.contains(f, Qt::CaseInsensitive)) {
                return f;
            }
        }
        return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    }();
    return familyName;
}
