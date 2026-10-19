#pragma once

#include <QObject>
#include <QProcess>
#include <QtQml/qqmlregistration.h>

/** Starting other programs: a new terminal window, an editor, the file manager. */
class Launcher : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Launcher(QObject *parent = nullptr) : QObject(parent) {}

    /** Opens a file with the user's default program; `create` makes an empty settings file first. */
    Q_INVOKABLE bool openFile(const QString &path, bool create = false);
    Q_INVOKABLE bool openDefaultsFile();
    Q_INVOKABLE bool openFolder(const QString &path);
    Q_INVOKABLE bool openUrl(const QString &url);
    /** Empties the program's cache folder (what "Limpar cache" does). */
    Q_INVOKABLE void clearCache();
    /** The terminal bell: custom sound if provided and valid, otherwise desktop sound. */
    Q_INVOKABLE void playBell(const QString &soundPath = QString());
    /** Opens a native system file selection dialog. */
    Q_INVOKABLE QString chooseFile(const QString &title = QString(), const QString &filter = QString());
    /** The user's Documents folder (home when there is none). */
    Q_INVOKABLE QString documentsFolder() const;

private:
    QProcess *m_bellPlayer = nullptr;
};
