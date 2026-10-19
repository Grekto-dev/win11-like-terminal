#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class SystemFont : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString family READ family NOTIFY familyChanged)

public:
    explicit SystemFont(QObject *parent = nullptr);

    static SystemFont *create(QQmlEngine *, QJSEngine *)
    {
        static auto *instance = new SystemFont();
        return instance;
    }

    QString family() const;

Q_SIGNALS:
    void familyChanged();
};
