#include "launcher.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

bool Launcher::openFile(const QString &path, bool create)
{
    if (path.isEmpty()) {
        return false;
    }
    if (create && !QFileInfo::exists(path)) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            f.write("{\n    \"$schema\": \"https://aka.ms/terminal-profiles-schema\"\n}\n");
        }
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool Launcher::openDefaultsFile()
{
    QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("win11terminal/defaults.json"));
#ifdef WIN11TERMINAL_SOURCE_DATA_DIR
    if (path.isEmpty()) {
        path = QStringLiteral(WIN11TERMINAL_SOURCE_DATA_DIR "/defaults.json");
    }
#endif
    return openFile(path);
}

bool Launcher::openFolder(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QString Launcher::documentsFolder() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return dir.isEmpty() ? QDir::homePath() : dir;
}

bool Launcher::openUrl(const QString &url)
{
    return QDesktopServices::openUrl(QUrl::fromUserInput(url));
}

void Launcher::clearCache()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!dir.isEmpty() && QDir(dir).exists()) {
        QDir(dir).removeRecursively();
    }
}

QString Launcher::chooseFile(const QString &title, const QString &filter)
{
    const QString zenity = QStandardPaths::findExecutable(QStringLiteral("zenity"));
    if (!zenity.isEmpty()) {
        QStringList args = {QStringLiteral("--file-selection")};
        if (!title.isEmpty()) {
            args << QStringLiteral("--title=") + title;
        }
        if (!filter.isEmpty()) {
            args << QStringLiteral("--file-filter=") + filter;
        }
        QProcess proc;
        proc.start(zenity, args);
        if (proc.waitForFinished(-1) && proc.exitCode() == 0) {
            return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        }
        return QString();
    }
    const QString kdialog = QStandardPaths::findExecutable(QStringLiteral("kdialog"));
    if (!kdialog.isEmpty()) {
        QStringList args = {QStringLiteral("--getopenfilename"), QDir::homePath()};
        if (!title.isEmpty()) {
            args << QStringLiteral("--title") << title;
        }
        QProcess proc;
        proc.start(kdialog, args);
        if (proc.waitForFinished(-1) && proc.exitCode() == 0) {
            return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        }
        return QString();
    }
    return QString();
}

void Launcher::playBell(const QString &soundPath)
{
    QString sound = soundPath;
    if (sound.isEmpty() || !QFileInfo::exists(sound)) {
        sound = QStringLiteral("/usr/share/sounds/freedesktop/stereo/bell.oga");
    }
    if (m_bellPlayer && m_bellPlayer->state() != QProcess::NotRunning) {
        return;
    }
    static const QString player = [] {
        const QStringList candidates = {
            QStringLiteral("pw-play"),
            QStringLiteral("paplay"),
            QStringLiteral("canberra-gtk-play"),
            QStringLiteral("aplay")
        };
        for (const QString &c : candidates) {
            const QString bin = QStandardPaths::findExecutable(c);
            if (!bin.isEmpty()) {
                return bin;
            }
        }
        return QString();
    }();
    if (player.isEmpty() || !QFileInfo::exists(sound)) {
        return;
    }
    if (!m_bellPlayer) {
        m_bellPlayer = new QProcess(this);
        m_bellPlayer->setStandardOutputFile(QProcess::nullDevice());
        m_bellPlayer->setStandardErrorFile(QProcess::nullDevice());
    }
    if (player.endsWith(QLatin1String("canberra-gtk-play"))) {
        m_bellPlayer->start(player, {QStringLiteral("-f"), sound});
    } else {
        m_bellPlayer->start(player, {sound});
    }
}
