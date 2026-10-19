#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <QtQml/QJSEngine>
#include <QtQml/QQmlEngine>

/**
 * settings.json, in the format of Windows Terminal: global settings, `profiles`
 * (`defaults` + `list`), `schemes`, `themes`, `keybindings`. What ships is
 * data/defaults.json; the user's file (~/.config/win11terminal/settings.json,
 * comments and trailing commas allowed, like the original) is laid over it.
 * The file is watched: an edit shows up at once.
 */
class AppSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantMap globals READ globals NOTIFY changed)
    Q_PROPERTY(QVariantMap profileDefaults READ profileDefaults NOTIFY changed)
    Q_PROPERTY(QVariantList userProfiles READ userProfiles NOTIFY changed)
    Q_PROPERTY(QVariantList schemes READ schemes NOTIFY changed)
    Q_PROPERTY(QVariantList keybindings READ keybindings NOTIFY changed)
    Q_PROPERTY(QString filePath READ filePath CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)

public:
    QString qtVersion() const { return QString::fromLatin1(qVersion()); }
    static AppSettings *instance();
    /** For the QML singleton: the one instance, owned by C++. */
    static AppSettings *create(QQmlEngine *, QJSEngine *);

    /** Loads defaults and the user file again (a no-op file is fine). Emits changed(). */
    Q_INVOKABLE void reload();

    QVariantMap globals() const { return m_globals; }
    QVariantMap profileDefaults() const { return m_profileDefaults; }
    QVariantList userProfiles() const { return m_userProfiles; }
    QVariantList schemes() const { return m_schemes; }
    QVariantList keybindings() const { return m_keybindings; }
    QString filePath() const { return m_path; }
    QString error() const { return m_error; }
    Q_INVOKABLE QVariant value(const QString &key, const QVariant &fallback = {}) const;
    /** The scheme named `name` (any of the 16 colours, foreground, background...); Campbell when unknown. */
    Q_INVOKABLE QVariantMap scheme(const QString &name) const;

    // --- Keys ----------------------------------------------------------------------------
    /** The action bound to normalized keys ("ctrl+shift+t"), or "". */
    Q_INVOKABLE QString actionForKeys(const QString &normalizedKeys) const;
    Q_INVOKABLE QVariantMap argsForKeys(const QString &normalizedKeys) const;
    /** The first binding of an action as written in the settings ("ctrl+shift+p"), or "". */
    Q_INVOKABLE QString keysFor(const QString &actionId) const;
    /** The same as a menu shows it: "Ctrl+Shift+P". */
    Q_INVOKABLE QString displayKeysFor(const QString &actionId) const;
    /** "Shift+Ctrl+T" and friends to the canonical order and spelling ("ctrl+shift+t"). */
    static QString normalizeKeys(const QString &keys);
    /** What a key event is, in that spelling: ("ctrl+shift+t"). */
    static QString keysOf(int qtKey, Qt::KeyboardModifiers modifiers);
    static QString displayKeys(const QString &normalized);

    /** Strips // and block comments and trailing commas: the JSON Windows Terminal reads. */
    static QByteArray stripComments(const QByteArray &text);

    // --- Writing (the settings UI) ---------------------------------------------------------
    /** Sets a global key in the user file and saves. */
    Q_INVOKABLE bool setGlobal(const QString &key, const QVariant &value);
    Q_INVOKABLE bool setProfileDefault(const QString &key, const QVariant &value);
    Q_INVOKABLE bool setProfile(const QString &guid, const QString &key, const QVariant &value);
    /**
     * Applies what the settings page collected, in one write: {globals:{key:value}, defaults:{...},
     * profiles:{guid:{key:value}}}. A null value removes the key from the user file.
     */
    Q_INVOKABLE bool applyChanges(const QVariantMap &changes);
    /** Removes the profile's entry from the user's list (a generated profile then comes back as it was). */
    Q_INVOKABLE bool removeProfile(const QString &guid);
    /** Deletes the user's file: back to the defaults ("Redefinir para as configurações padrão"). */
    Q_INVOKABLE bool resetToDefaults();
    /**
     * Writes the user file from its parsed content (indented); comments are not kept. Refused while
     * the file has a parse error (error() is set), so a typo never costs the rest of the file.
     */
    bool save();

Q_SIGNALS:
    void changed();
    /** A write of the user's file did not happen (the file has an error, or the disk refused). */
    void saveFailed(const QString &reason);

    // Tests build their own (settings.json in a temp folder); the app has the one instance().
#ifdef WIN11TERMINAL_TESTING
public:
#else
private:
#endif
    explicit AppSettings(QObject *parent = nullptr);

private:
    void load();
    static QJsonObject readFile(const QString &path, QString *error);
    void rebuildBindings(const QJsonObject &defaults);
    QString stamp() const;

    QString m_path;
    QJsonObject m_user;              // the user's file as parsed (what save() writes back)
    QVariantMap m_globals;
    QVariantMap m_profileDefaults;
    QVariantList m_userProfiles;
    QVariantList m_schemes;
    QVariantList m_keybindings;      // resolved: [{keys, id, args}]
    QHash<QString, int> m_byKeys;    // normalized keys -> index in m_keybindings
    QString m_error;
    QFileSystemWatcher m_watcher;
    bool m_saving = false;
    QString m_stamp;
};
