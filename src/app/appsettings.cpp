#include "appsettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

namespace
{
QString defaultsPath()
{
    // The data/ folder of the install, or of the source tree in development.
    const QString installed = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                     QStringLiteral("win11terminal/defaults.json"));
    if (!installed.isEmpty()) {
        return installed;
    }
#ifdef WIN11TERMINAL_SOURCE_DATA_DIR
    return QStringLiteral(WIN11TERMINAL_SOURCE_DATA_DIR "/defaults.json");
#else
    return {};
#endif
}

const QStringList kSections = {QStringLiteral("profiles"), QStringLiteral("schemes"), QStringLiteral("themes"),
                               QStringLiteral("actions"), QStringLiteral("keybindings")};

AppSettings *g_instance = nullptr;
}

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
    g_instance = this;
    const QString override = qEnvironmentVariable("WIN11TERMINAL_SETTINGS");
    m_path = !override.isEmpty()
        ? override
        : QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/win11terminal/settings.json");
    load();
    // Editors save by replacing the file: watch the folder as well as the file.
    m_watcher.addPath(QFileInfo(m_path).absolutePath());
    if (QFileInfo::exists(m_path)) {
        m_watcher.addPath(m_path);
    }
    auto *debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(150);
    connect(debounce, &QTimer::timeout, this, [this] {
        if (QFileInfo::exists(m_path) && !m_watcher.files().contains(m_path)) {
            m_watcher.addPath(m_path);
        }
        reload();
    });
    const auto onFs = [this, debounce] {
        if (!m_saving) {
            debounce->start();
        }
    };
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, onFs);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, onFs);

    // The watcher loses a file that an editor replaces (rename over it); a slow poll of
    // the file's stamp is the net under it.
    auto *poll = new QTimer(this);
    poll->setInterval(1500);
    m_stamp = stamp();
    connect(poll, &QTimer::timeout, this, [this, debounce] {
        const QString now = stamp();
        if (now != m_stamp && !m_saving) {
            m_stamp = now;
            debounce->start();
        }
    });
    poll->start();
}

QString AppSettings::stamp() const
{
    const QFileInfo info(m_path);
    return info.exists() ? QString::number(info.lastModified().toMSecsSinceEpoch()) + QLatin1Char('/') + QString::number(info.size())
                         : QString();
}

AppSettings *AppSettings::instance()
{
    if (!g_instance) {
        new AppSettings; // lives as long as the process
    }
    return g_instance;
}

AppSettings *AppSettings::create(QQmlEngine *, QJSEngine *)
{
    AppSettings *s = instance();
    QJSEngine::setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

QByteArray AppSettings::stripComments(const QByteArray &text)
{
    QByteArray out;
    out.reserve(text.size());
    bool inString = false;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (inString) {
            out += c;
            if (c == '\\' && i + 1 < text.size()) {
                out += text[++i];
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
            out += c;
        } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            while (i < text.size() && text[i] != '\n') {
                ++i;
            }
            out += '\n';
        } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
            i += 2;
            while (i + 1 < text.size() && !(text[i] == '*' && text[i + 1] == '/')) {
                ++i;
            }
            ++i;
        } else {
            out += c;
        }
    }
    // Trailing commas: a comma followed only by whitespace and a closing bracket.
    QByteArray clean;
    clean.reserve(out.size());
    inString = false;
    for (qsizetype i = 0; i < out.size(); ++i) {
        const char c = out[i];
        if (inString) {
            clean += c;
            if (c == '\\' && i + 1 < out.size()) {
                clean += out[++i];
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == ',') {
            qsizetype j = i + 1;
            while (j < out.size() && (out[j] == ' ' || out[j] == '\n' || out[j] == '\r' || out[j] == '\t')) {
                ++j;
            }
            if (j < out.size() && (out[j] == '}' || out[j] == ']')) {
                continue;
            }
        }
        clean += c;
    }
    return clean;
}

QJsonObject AppSettings::readFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray data = file.readAll();
    if (data.startsWith("\xEF\xBB\xBF")) {
        data.remove(0, 3);
    }
    if (data.trimmed().isEmpty()) {
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(stripComments(data), &parseError);
    if (doc.isNull() || !doc.isObject()) {
        if (error) {
            *error = parseError.errorString() + QStringLiteral(" (offset %1)").arg(parseError.offset);
        }
        return {};
    }
    return doc.object();
}

void AppSettings::reload()
{
    m_stamp = stamp();
    load();
    Q_EMIT changed();
}

void AppSettings::load()
{
    m_error.clear();
    const QJsonObject defaults = readFile(defaultsPath(), nullptr);
    QString userError;
    m_user = readFile(m_path, &userError);
    if (!userError.isEmpty()) {
        m_error = userError; // the defaults still apply: a typo does not break the terminal
    }

    // Globals: the defaults' plain keys, the user's on top.
    m_globals.clear();
    for (const QJsonObject &layer : {defaults, m_user}) {
        for (auto it = layer.begin(); it != layer.end(); ++it) {
            if (!kSections.contains(it.key())) {
                m_globals.insert(it.key(), it.value().toVariant());
            }
        }
    }

    // Profiles: defaults + list.
    const auto profilesOf = [](const QJsonObject &o) {
        const QJsonValue p = o.value(QStringLiteral("profiles"));
        // The original also allows "profiles" to be just the list.
        return p.isArray() ? QJsonObject{{QStringLiteral("list"), p}} : p.toObject();
    };
    m_profileDefaults = profilesOf(defaults).value(QStringLiteral("defaults")).toObject().toVariantMap();
    const QVariantMap userDefaults = profilesOf(m_user).value(QStringLiteral("defaults")).toObject().toVariantMap();
    for (auto it = userDefaults.begin(); it != userDefaults.end(); ++it) {
        m_profileDefaults.insert(it.key(), it.value());
    }
    m_userProfiles = profilesOf(m_user).value(QStringLiteral("list")).toArray().toVariantList();

    // Schemes: the defaults, a user scheme of the same name replaces it.
    m_schemes.clear();
    QHash<QString, int> byName;
    for (const QJsonObject &layer : {defaults, m_user}) {
        for (const QJsonValue &v : layer.value(QStringLiteral("schemes")).toArray()) {
            const QVariantMap scheme = v.toObject().toVariantMap();
            const QString name = scheme.value(QStringLiteral("name")).toString();
            if (byName.contains(name)) {
                m_schemes[byName[name]] = scheme;
            } else {
                byName.insert(name, int(m_schemes.size()));
                m_schemes.push_back(scheme);
            }
        }
    }
    rebuildBindings(defaults);
}

void AppSettings::rebuildBindings(const QJsonObject &defaults)
{
    // The user's entries win over a default with the same keys; `"id": null` unbinds.
    m_keybindings.clear();
    m_byKeys.clear();
    const auto add = [this](const QJsonObject &o) {
        const QJsonValue keysValue = o.value(QStringLiteral("keys"));
        QStringList keys;
        if (keysValue.isString()) {
            keys << keysValue.toString();
        } else if (keysValue.isArray()) {
            for (const QJsonValue &k : keysValue.toArray()) {
                keys << k.toString();
            }
        }
        const QJsonValue idValue = o.value(QStringLiteral("id"));
        const QJsonValue commandValue = o.value(QStringLiteral("command"));
        QString id = idValue.toString();
        QVariantMap args;
        if (id.isEmpty() && commandValue.isString()) {
            id = commandValue.toString(); // old style: "command": "copy"
        } else if (id.isEmpty() && commandValue.isObject()) {
            id = commandValue.toObject().value(QStringLiteral("action")).toString();
            args = commandValue.toObject().toVariantMap();
            args.remove(QStringLiteral("action"));
        }
        for (const QString &raw : std::as_const(keys)) {
            const QString norm = normalizeKeys(raw);
            if (norm.isEmpty()) {
                continue;
            }
            QVariantMap b;
            b[QStringLiteral("keys")] = norm;
            b[QStringLiteral("id")] = id;
            b[QStringLiteral("args")] = args;
            if (m_byKeys.contains(norm)) {
                m_keybindings[m_byKeys[norm]] = b;
            } else {
                m_byKeys.insert(norm, int(m_keybindings.size()));
                m_keybindings.push_back(b);
            }
        }
    };
    for (const QJsonObject &layer : {defaults, m_user}) {
        for (const QJsonValue &v : layer.value(QStringLiteral("keybindings")).toArray()) {
            add(v.toObject());
        }
        for (const QJsonValue &v : layer.value(QStringLiteral("actions")).toArray()) {
            if (v.toObject().contains(QStringLiteral("keys"))) {
                add(v.toObject());
            }
        }
    }
}

QVariant AppSettings::value(const QString &key, const QVariant &fallback) const
{
    return m_globals.value(key, fallback);
}

QVariantMap AppSettings::scheme(const QString &name) const
{
    for (const QVariant &v : m_schemes) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("name")).toString() == name) {
            return m;
        }
    }
    for (const QVariant &v : m_schemes) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("name")).toString() == QLatin1String("Campbell")) {
            return m;
        }
    }
    return {};
}

// --- Keys -----------------------------------------------------------------------------------

QString AppSettings::normalizeKeys(const QString &keys)
{
    static const QHash<QString, QString> alias = {
        {QStringLiteral("control"), QStringLiteral("ctrl")}, {QStringLiteral("windows"), QStringLiteral("win")},
        {QStringLiteral("super"), QStringLiteral("win")}, {QStringLiteral("return"), QStringLiteral("enter")},
        {QStringLiteral("pageup"), QStringLiteral("pgup")}, {QStringLiteral("pagedown"), QStringLiteral("pgdn")},
        {QStringLiteral("esc"), QStringLiteral("escape")}, {QStringLiteral("del"), QStringLiteral("delete")},
        {QStringLiteral("ins"), QStringLiteral("insert")}, {QStringLiteral("spacebar"), QStringLiteral("space")},
        {QStringLiteral("+"), QStringLiteral("plus")}, {QStringLiteral("-"), QStringLiteral("minus")},
        {QStringLiteral(","), QStringLiteral("comma")}, {QStringLiteral("."), QStringLiteral("period")},
        {QStringLiteral("="), QStringLiteral("equals")}, {QStringLiteral("backspace"), QStringLiteral("backspace")},
    };
    bool ctrl = false, alt = false, shift = false, win = false;
    QString key;
    // "ctrl+plus" and "ctrl++" both mean Ctrl and the plus key.
    QString text = keys.trimmed().toLower();
    if (text.endsWith(QLatin1String("++"))) {
        text.chop(1);
        text += QStringLiteral("plus");
    }
    for (QString part : text.split(QLatin1Char('+'), Qt::SkipEmptyParts)) {
        part = alias.value(part.trimmed(), part.trimmed());
        if (part == QLatin1String("ctrl")) {
            ctrl = true;
        } else if (part == QLatin1String("alt")) {
            alt = true;
        } else if (part == QLatin1String("shift")) {
            shift = true;
        } else if (part == QLatin1String("win")) {
            win = true;
        } else {
            key = part;
        }
    }
    if (key.isEmpty()) {
        return {};
    }
    QString out;
    if (win) out += QStringLiteral("win+");
    if (ctrl) out += QStringLiteral("ctrl+");
    if (alt) out += QStringLiteral("alt+");
    if (shift) out += QStringLiteral("shift+");
    return out + key;
}

QString AppSettings::keysOf(int qtKey, Qt::KeyboardModifiers m)
{
    QString key;
    const bool keypad = m & Qt::KeypadModifier;
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        key = QString(QChar(char16_t('a' + (qtKey - Qt::Key_A))));
    } else if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        key = (keypad ? QStringLiteral("numpad_") : QString()) + QString::number(qtKey - Qt::Key_0);
    } else if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F35) {
        key = QStringLiteral("f%1").arg(qtKey - Qt::Key_F1 + 1);
    } else {
        switch (qtKey) {
        case Qt::Key_Plus: key = keypad ? QStringLiteral("numpad_plus") : QStringLiteral("plus"); break;
        case Qt::Key_Minus: key = keypad ? QStringLiteral("numpad_minus") : QStringLiteral("minus"); break;
        case Qt::Key_Asterisk: key = keypad ? QStringLiteral("numpad_multiply") : QStringLiteral("asterisk"); break;
        case Qt::Key_Slash: key = keypad ? QStringLiteral("numpad_divide") : QStringLiteral("slash"); break;
        case Qt::Key_Comma: key = QStringLiteral("comma"); break;
        case Qt::Key_Period: key = keypad ? QStringLiteral("numpad_period") : QStringLiteral("period"); break;
        case Qt::Key_Equal: key = QStringLiteral("equals"); break;
        case Qt::Key_Space: key = QStringLiteral("space"); break;
        case Qt::Key_Tab: case Qt::Key_Backtab: key = QStringLiteral("tab"); break;
        case Qt::Key_Return: case Qt::Key_Enter: key = QStringLiteral("enter"); break;
        case Qt::Key_Backspace: key = QStringLiteral("backspace"); break;
        case Qt::Key_Escape: key = QStringLiteral("escape"); break;
        case Qt::Key_Insert: key = QStringLiteral("insert"); break;
        case Qt::Key_Delete: key = QStringLiteral("delete"); break;
        case Qt::Key_Home: key = QStringLiteral("home"); break;
        case Qt::Key_End: key = QStringLiteral("end"); break;
        case Qt::Key_PageUp: key = QStringLiteral("pgup"); break;
        case Qt::Key_PageDown: key = QStringLiteral("pgdn"); break;
        case Qt::Key_Up: key = QStringLiteral("up"); break;
        case Qt::Key_Down: key = QStringLiteral("down"); break;
        case Qt::Key_Left: key = QStringLiteral("left"); break;
        case Qt::Key_Right: key = QStringLiteral("right"); break;
        case Qt::Key_Menu: key = QStringLiteral("menu"); break;
        case Qt::Key_Semicolon: key = QStringLiteral("semicolon"); break;
        case Qt::Key_Apostrophe: key = QStringLiteral("quote"); break;
        case Qt::Key_BracketLeft: key = QStringLiteral("["); break;
        case Qt::Key_BracketRight: key = QStringLiteral("]"); break;
        case Qt::Key_Backslash: key = QStringLiteral("\\"); break;
        case Qt::Key_QuoteLeft: key = QStringLiteral("backtick"); break;
        default: return {};
        }
    }
    QString out;
    if (m & Qt::MetaModifier) out += QStringLiteral("win+");
    if (m & Qt::ControlModifier) out += QStringLiteral("ctrl+");
    if (m & Qt::AltModifier) out += QStringLiteral("alt+");
    if (m & Qt::ShiftModifier || qtKey == Qt::Key_Backtab) out += QStringLiteral("shift+");
    return out + key;
}

QString AppSettings::displayKeys(const QString &normalized)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("plus"), QStringLiteral("+")}, {QStringLiteral("minus"), QStringLiteral("-")},
        {QStringLiteral("comma"), QStringLiteral(",")}, {QStringLiteral("period"), QStringLiteral(".")},
        {QStringLiteral("equals"), QStringLiteral("=")}, {QStringLiteral("pgup"), QStringLiteral("PgUp")},
        {QStringLiteral("pgdn"), QStringLiteral("PgDn")}, {QStringLiteral("space"), QStringLiteral("Space")},
        {QStringLiteral("enter"), QStringLiteral("Enter")}, {QStringLiteral("escape"), QStringLiteral("Esc")},
        {QStringLiteral("backspace"), QStringLiteral("Backspace")},
    };
    QStringList out;
    for (const QString &part : normalized.split(QLatin1Char('+'), Qt::SkipEmptyParts)) {
        if (names.contains(part)) {
            out << names.value(part);
        } else if (part == QLatin1String("ctrl") || part == QLatin1String("alt") || part == QLatin1String("shift")
                   || part == QLatin1String("win")) {
            out << (part == QLatin1String("ctrl") ? QStringLiteral("Ctrl") : part.left(1).toUpper() + part.mid(1));
        } else {
            out << part.left(1).toUpper() + part.mid(1);
        }
    }
    // A "+" key would read "Ctrl++": that is how Windows shows it.
    return out.join(QLatin1Char('+'));
}

QString AppSettings::actionForKeys(const QString &normalizedKeys) const
{
    const auto it = m_byKeys.constFind(normalizedKeys);
    return it == m_byKeys.constEnd() ? QString() : m_keybindings[it.value()].toMap().value(QStringLiteral("id")).toString();
}

QVariantMap AppSettings::argsForKeys(const QString &normalizedKeys) const
{
    const auto it = m_byKeys.constFind(normalizedKeys);
    return it == m_byKeys.constEnd() ? QVariantMap() : m_keybindings[it.value()].toMap().value(QStringLiteral("args")).toMap();
}

QString AppSettings::keysFor(const QString &actionId) const
{
    for (const QVariant &v : m_keybindings) {
        const QVariantMap b = v.toMap();
        if (b.value(QStringLiteral("id")).toString() == actionId) {
            return b.value(QStringLiteral("keys")).toString();
        }
    }
    return {};
}

QString AppSettings::displayKeysFor(const QString &actionId) const
{
    const QString k = keysFor(actionId);
    return k.isEmpty() ? QString() : displayKeys(k);
}

// --- Writing ----------------------------------------------------------------------------------

bool AppSettings::save()
{
    // A file we could not read (a typo) is never written over: what we hold of it is empty, and
    // saving would throw away everything the user had in it.
    if (!m_error.isEmpty()) {
        Q_EMIT saveFailed(tr("settings.json has an error and was not changed: %1").arg(m_error));
        return false;
    }
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    // Written aside and renamed over: a crash or a full disk never leaves half a file.
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        Q_EMIT saveFailed(file.errorString());
        return false;
    }
    m_saving = true;
    file.write(QJsonDocument(m_user).toJson(QJsonDocument::Indented));
    const bool ok = file.commit();
    // The watcher must not treat our own write as an edit; a later real edit still counts.
    QTimer::singleShot(400, this, [this] { m_saving = false; });
    if (!ok) {
        Q_EMIT saveFailed(file.errorString());
        return false;
    }
    m_stamp = stamp();
    return true;
}

bool AppSettings::applyChanges(const QVariantMap &changes)
{
    const auto put = [](QJsonObject &o, const QString &key, const QVariant &value) {
        if (!value.isValid() || value.isNull()) {
            o.remove(key);
        } else {
            o.insert(key, QJsonValue::fromVariant(value));
        }
    };
    const QVariantMap globals = changes.value(QStringLiteral("globals")).toMap();
    for (auto it = globals.begin(); it != globals.end(); ++it) {
        put(m_user, it.key(), it.value());
    }
    // Schemes: {name: {key: value}} lays the values over the user's scheme of that name, or over
    // a copy of the shipped one; a value of null removes the whole scheme from the user's file.
    const QVariantMap schemeChanges = changes.value(QStringLiteral("schemes")).toMap();
    if (!schemeChanges.isEmpty()) {
        QJsonArray userSchemes = m_user.value(QStringLiteral("schemes")).toArray();
        for (auto sit = schemeChanges.begin(); sit != schemeChanges.end(); ++sit) {
            int index = -1;
            for (int i = 0; i < userSchemes.size(); ++i) {
                if (userSchemes[i].toObject().value(QStringLiteral("name")).toString() == sit.key()) {
                    index = i;
                }
            }
            if (sit.value().isNull() || !sit.value().isValid()) {
                if (index >= 0) {
                    userSchemes.removeAt(index);
                }
                continue;
            }
            QJsonObject entry;
            if (index >= 0) {
                entry = userSchemes[index].toObject();
            } else {
                entry = QJsonObject::fromVariantMap(scheme(sit.key()));
                if (entry.value(QStringLiteral("name")).toString() != sit.key()) {
                    entry = QJsonObject{{QStringLiteral("name"), sit.key()}};
                }
            }
            const QVariantMap values = sit.value().toMap();
            for (auto it = values.begin(); it != values.end(); ++it) {
                entry.insert(it.key(), QJsonValue::fromVariant(it.value()));
            }
            if (index >= 0) {
                userSchemes[index] = entry;
            } else {
                userSchemes.append(entry);
            }
        }
        if (userSchemes.isEmpty()) {
            m_user.remove(QStringLiteral("schemes"));
        } else {
            m_user.insert(QStringLiteral("schemes"), userSchemes);
        }
    }
    QJsonObject profiles = m_user.value(QStringLiteral("profiles")).toObject();
    QJsonObject defaults = profiles.value(QStringLiteral("defaults")).toObject();
    const QVariantMap defChanges = changes.value(QStringLiteral("defaults")).toMap();
    for (auto it = defChanges.begin(); it != defChanges.end(); ++it) {
        put(defaults, it.key(), it.value());
    }
    QJsonArray list = profiles.value(QStringLiteral("list")).toArray();
    const QVariantMap profChanges = changes.value(QStringLiteral("profiles")).toMap();
    for (auto pit = profChanges.begin(); pit != profChanges.end(); ++pit) {
        int index = -1;
        for (int i = 0; i < list.size(); ++i) {
            if (list[i].toObject().value(QStringLiteral("guid")).toString() == pit.key()) {
                index = i;
            }
        }
        QJsonObject entry = index >= 0 ? list[index].toObject() : QJsonObject{{QStringLiteral("guid"), pit.key()}};
        const QVariantMap values = pit.value().toMap();
        for (auto it = values.begin(); it != values.end(); ++it) {
            put(entry, it.key(), it.value());
        }
        if (index >= 0) {
            list[index] = entry;
        } else {
            list.append(entry);
        }
    }
    if (!defaults.isEmpty()) {
        profiles.insert(QStringLiteral("defaults"), defaults);
    } else {
        profiles.remove(QStringLiteral("defaults"));
    }
    if (list.isEmpty()) {
        profiles.remove(QStringLiteral("list"));
    } else {
        profiles.insert(QStringLiteral("list"), list);
    }
    if (profiles.isEmpty()) {
        m_user.remove(QStringLiteral("profiles"));
    } else {
        m_user.insert(QStringLiteral("profiles"), profiles);
    }
    const bool ok = save();
    load();
    Q_EMIT changed();
    return ok;
}

bool AppSettings::removeProfile(const QString &guid)
{
    QJsonObject profiles = m_user.value(QStringLiteral("profiles")).toObject();
    QJsonArray list = profiles.value(QStringLiteral("list")).toArray();
    for (int i = list.size() - 1; i >= 0; --i) {
        if (list[i].toObject().value(QStringLiteral("guid")).toString() == guid) {
            list.removeAt(i);
        }
    }
    if (list.isEmpty()) {
        profiles.remove(QStringLiteral("list"));
    } else {
        profiles.insert(QStringLiteral("list"), list);
    }
    if (profiles.isEmpty()) {
        m_user.remove(QStringLiteral("profiles"));
    } else {
        m_user.insert(QStringLiteral("profiles"), profiles);
    }
    const bool ok = save();
    load();
    Q_EMIT changed();
    return ok;
}

bool AppSettings::resetToDefaults()
{
    m_saving = true;
    const bool ok = !QFileInfo::exists(m_path) || QFile::remove(m_path);
    QTimer::singleShot(400, this, [this] { m_saving = false; });
    load();
    Q_EMIT changed();
    return ok;
}

bool AppSettings::setGlobal(const QString &key, const QVariant &value)
{
    m_user.insert(key, QJsonValue::fromVariant(value));
    const bool ok = save();
    load();
    Q_EMIT changed();
    return ok;
}

bool AppSettings::setProfileDefault(const QString &key, const QVariant &value)
{
    QJsonObject profiles = m_user.value(QStringLiteral("profiles")).toObject();
    QJsonObject defaults = profiles.value(QStringLiteral("defaults")).toObject();
    defaults.insert(key, QJsonValue::fromVariant(value));
    profiles.insert(QStringLiteral("defaults"), defaults);
    m_user.insert(QStringLiteral("profiles"), profiles);
    const bool ok = save();
    load();
    Q_EMIT changed();
    return ok;
}

bool AppSettings::setProfile(const QString &guid, const QString &key, const QVariant &value)
{
    QJsonObject profiles = m_user.value(QStringLiteral("profiles")).toObject();
    QJsonArray list = profiles.value(QStringLiteral("list")).toArray();
    bool found = false;
    for (int i = 0; i < list.size(); ++i) {
        QJsonObject p = list[i].toObject();
        if (p.value(QStringLiteral("guid")).toString() == guid) {
            p.insert(key, QJsonValue::fromVariant(value));
            list[i] = p;
            found = true;
        }
    }
    if (!found) {
        list.append(QJsonObject{{QStringLiteral("guid"), guid}, {key, QJsonValue::fromVariant(value)}});
    }
    profiles.insert(QStringLiteral("list"), list);
    m_user.insert(QStringLiteral("profiles"), profiles);
    const bool ok = save();
    load();
    Q_EMIT changed();
    return ok;
}
