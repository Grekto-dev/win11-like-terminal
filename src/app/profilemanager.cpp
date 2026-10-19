#include "profilemanager.h"

#include "newtabmenu.h"

#include "appsettings.h"

#include <QProcess>
#include <QRegularExpression>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>
#include <QSet>

namespace
{
// A fixed namespace, so a shell keeps the same guid from run to run (profiles
// are referred to by guid in settings.json).
const QUuid kNamespace = QUuid::fromString(QStringLiteral("{b453ae62-4e3d-5e58-b989-0a998ec441b8}"));

QString displayName(const QString &shell)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("bash"), QStringLiteral("Bash")},
        {QStringLiteral("zsh"), QStringLiteral("Zsh")},
        {QStringLiteral("fish"), QStringLiteral("Fish")},
        {QStringLiteral("sh"), QStringLiteral("Sh")},
        {QStringLiteral("dash"), QStringLiteral("Dash")},
        {QStringLiteral("pwsh"), QStringLiteral("PowerShell")},
        {QStringLiteral("ksh"), QStringLiteral("Ksh")},
        {QStringLiteral("tcsh"), QStringLiteral("Tcsh")},
    };
    return names.value(shell, shell.left(1).toUpper() + shell.mid(1));
}
}

ProfileManager::ProfileManager(QObject *parent)
    : QObject(parent)
{
    detect();
    rebuild();
    connect(AppSettings::instance(), &AppSettings::changed, this, [this] {
        rebuild();
        Q_EMIT profilesChanged();
    });
}

QString ProfileManager::expand(const QString &text)
{
    QString out = text;
    if (out == QLatin1String("~") || out.startsWith(QLatin1String("~/"))) {
        out.replace(0, 1, QDir::homePath());
    }
    // %VAR% (the original's spelling) and $VAR
    static const QRegularExpression percent(QStringLiteral("%([A-Za-z_][A-Za-z0-9_]*)%"));
    static const QRegularExpression dollar(QStringLiteral("\\$([A-Za-z_][A-Za-z0-9_]*)"));
    for (const QRegularExpression &re : {percent, dollar}) {
        QRegularExpressionMatchIterator it = re.globalMatch(out);
        QList<QRegularExpressionMatch> matches;
        while (it.hasNext()) {
            matches << it.next();
        }
        for (int i = int(matches.size()) - 1; i >= 0; --i) {
            const QString name = matches[i].captured(1);
            const QString value = name == QLatin1String("USERPROFILE") ? QDir::homePath()
                                                                       : qEnvironmentVariable(name.toLocal8Bit().constData());
            out.replace(matches[i].capturedStart(), matches[i].capturedLength(), value);
        }
    }
    return out;
}

void ProfileManager::rebuild()
{
    AppSettings *settings = AppSettings::instance();
    const QVariantMap defaults = settings->profileDefaults();
    m_profiles.clear();

    const auto resolve = [&](QVariantMap p) {
        QVariantMap out = defaults;
        for (auto it = p.begin(); it != p.end(); ++it) {
            out.insert(it.key(), it.value());
        }
        // The command line: the user's `commandline` (split like a shell would), else the shell found.
        const QString commandline = expand(out.value(QStringLiteral("commandline")).toString());
        if (!commandline.isEmpty()) {
            QStringList parts = QProcess::splitCommand(commandline);
            if (!parts.isEmpty()) {
                out[QStringLiteral("program")] = parts.takeFirst();
                out[QStringLiteral("arguments")] = parts;
            }
        }
        out[QStringLiteral("startingDirectory")] = expand(out.value(QStringLiteral("startingDirectory")).toString());
        return out;
    };

    QSet<QString> used;
    QHash<QString, int> unnamedSeen; // name -> how many guid-less entries of that name so far
    const QVariantList user = settings->userProfiles();
    // The user's list first, in its order; a generated profile it does not mention follows.
    for (const QVariant &uv : user) {
        QVariantMap u = uv.toMap();
        const QString guid = u.value(QStringLiteral("guid")).toString();
        QVariantMap base;
        for (const QVariant &gv : std::as_const(m_generated)) {
            if (gv.toMap().value(QStringLiteral("guid")).toString() == guid) {
                base = gv.toMap();
            }
        }
        for (auto it = u.begin(); it != u.end(); ++it) {
            base.insert(it.key(), it.value());
        }
        const QString name = base.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            continue; // nothing to show it by
        }
        if (base.value(QStringLiteral("guid")).toString().isEmpty()) {
            // The same guid on every reload (a tab keeps its profile when settings.json changes):
            // from the name, and its rank among guid-less entries of that name.
            const int rank = unnamedSeen[name]++;
            base[QStringLiteral("guid")] = QUuid::createUuidV5(kNamespace, QStringLiteral("user:%1#%2").arg(name).arg(rank)).toString();
        }
        used.insert(base.value(QStringLiteral("guid")).toString());
        m_profiles << resolve(base);
    }
    for (const QVariant &gv : std::as_const(m_generated)) {
        if (!used.contains(gv.toMap().value(QStringLiteral("guid")).toString())) {
            m_profiles << resolve(gv.toMap());
        }
    }

    // The default: settings' `defaultProfile` (a guid or a name), else the first visible.
    const QString wanted = settings->value(QStringLiteral("defaultProfile")).toString();
    m_defaultId.clear();
    for (const QVariant &v : std::as_const(m_profiles)) {
        const QVariantMap p = v.toMap();
        if (!wanted.isEmpty() && (p.value(QStringLiteral("guid")).toString() == wanted
                                  || p.value(QStringLiteral("name")).toString() == wanted)) {
            m_defaultId = p.value(QStringLiteral("guid")).toString();
        }
    }
    if (m_defaultId.isEmpty() && !m_profiles.isEmpty()) {
        m_defaultId = m_profiles.first().toMap().value(QStringLiteral("guid")).toString();
    }
    m_newTabMenu = NewTabMenu::resolve(settings->value(QStringLiteral("newTabMenu")).toList(), m_profiles);
}

void ProfileManager::detect()
{
    QStringList shells;
    const QString login = qEnvironmentVariable("SHELL");
    if (!login.isEmpty() && QFileInfo(login).isExecutable()) {
        shells << login;
    }
    QFile file(QStringLiteral("/etc/shells"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QStringList lines = QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'));
        for (const QString &raw : lines) {
            const QString line = raw.trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || !QFileInfo(line).isExecutable()) {
                continue;
            }
            shells << line;
        }
    }
    for (const QString &extra : {QStringLiteral("pwsh"), QStringLiteral("nu")}) {
        const QString found = QStandardPaths::findExecutable(extra);
        if (!found.isEmpty()) {
            shells << found;
        }
    }

    QStringList seenNames;
    for (const QString &path : std::as_const(shells)) {
        const QString base = QFileInfo(path).fileName();
        // /bin/bash and /usr/bin/bash are the same shell (usrmerge): one entry per name.
        if (base == QLatin1String("rbash") || base == QLatin1String("screen") || base == QLatin1String("tmux")
            || seenNames.contains(base)) {
            continue;
        }
        seenNames << base;
        QVariantMap p;
        p[QStringLiteral("guid")] = QUuid::createUuidV5(kNamespace, base).toString();
        p[QStringLiteral("name")] = displayName(base);
        p[QStringLiteral("commandline")] = path;
        p[QStringLiteral("program")] = path;
        p[QStringLiteral("arguments")] = QStringList();
        if (base == QLatin1String("bash")) {
            p[QStringLiteral("icon")] = QStringLiteral("bash");
        } else if (base == QLatin1String("sh")) {
            p[QStringLiteral("icon")] = QStringLiteral("sh");
        } else if (base == QLatin1String("dash")) {
            p[QStringLiteral("icon")] = QStringLiteral("dash");
        } else {
            p[QStringLiteral("icon")] = QStringLiteral("window_console");
        }
        p[QStringLiteral("hidden")] = false;
        m_generated << p;
    }
    if (m_generated.isEmpty()) {
        QVariantMap p;
        p[QStringLiteral("guid")] = QUuid::createUuidV5(kNamespace, QStringLiteral("sh")).toString();
        p[QStringLiteral("name")] = QStringLiteral("Sh");
        p[QStringLiteral("commandline")] = QStringLiteral("/bin/sh");
        p[QStringLiteral("program")] = QStringLiteral("/bin/sh");
        p[QStringLiteral("arguments")] = QStringList();
        p[QStringLiteral("icon")] = QStringLiteral("sh");
        p[QStringLiteral("hidden")] = false;
        m_generated << p;
    }
}

QString ProfileManager::matcherLabel(const QVariantMap &entry) const
{
    return NewTabMenu::matcherLabel(entry);
}

QVariantMap ProfileManager::profile(const QString &guid) const
{
    for (const QVariant &v : m_profiles) {
        const QVariantMap p = v.toMap();
        if (p.value(QStringLiteral("guid")).toString() == guid) {
            return p;
        }
    }
    return {};
}

QVariantMap ProfileManager::profileOrDefault(const QString &guid) const
{
    const QVariantMap p = profile(guid);
    return p.isEmpty() ? profile(m_defaultId) : p;
}

bool ProfileManager::isCustom(const QString &guid) const
{
    for (const QVariant &v : m_generated) {
        if (v.toMap().value(QStringLiteral("guid")).toString() == guid) {
            return false;
        }
    }
    return true;
}

QString ProfileManager::createProfile(const QString &basedOn)
{
    const QString guid = QUuid::createUuid().toString();
    QVariantMap entry;
    const QVariantMap base = basedOn.isEmpty() ? QVariantMap() : profile(basedOn);
    if (!base.isEmpty()) {
        entry[QStringLiteral("name")] = tr("%1 - Copy").arg(base.value(QStringLiteral("name")).toString());
        entry[QStringLiteral("commandline")] = base.value(QStringLiteral("commandline"));
        for (const char *key : {"startingDirectory", "tabTitle", "fontFace", "fontSize", "lineHeight", "cellWidth", "fontWeight",
                                "fontAxes", "fontFeatures", "useColorEmoji", "intenseTextStyle", "adjustIndistinguishableColors",
                                "colorScheme", "cursorShape", "padding", "historySize", "opacity", "useAcrylic", "closeOnExit",
                                "bellStyle", "icon"}) {
            if (base.contains(QLatin1String(key))) {
                entry[QLatin1String(key)] = base.value(QLatin1String(key));
            }
        }
    } else {
        entry[QStringLiteral("name")] = tr("Profile %1").arg(int(m_profiles.size()) + 1);
        entry[QStringLiteral("commandline")] = qEnvironmentVariable("SHELL", QStringLiteral("/bin/sh"));
    }
    AppSettings::instance()->applyChanges({{QStringLiteral("profiles"), QVariantMap{{guid, entry}}}});
    return guid;
}

void ProfileManager::deleteProfile(const QString &guid)
{
    if (isCustom(guid)) {
        AppSettings::instance()->removeProfile(guid);
    } else {
        AppSettings::instance()->applyChanges({{QStringLiteral("profiles"), QVariantMap{{guid, QVariantMap{{QStringLiteral("hidden"), true}}}}}});
    }
}
