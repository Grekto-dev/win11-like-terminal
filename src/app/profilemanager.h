#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

/**
 * The profiles the "+" menu offers, as settings.json resolves them: the shells found
 * on the system are the generated profiles (like the ones Windows Terminal generates
 * for PowerShell, Command Prompt...), the user's `profiles.list` entries are laid over
 * them by guid (or add profiles of their own), and `profiles.defaults` under all of them.
 * The rest of this note is the original comment.
 *
 * The profiles the "+" menu offers. On Windows they are PowerShell, Command
 * Prompt and friends; here, the login shell first (the default), then the other
 * shells /etc/shells lists that are installed. Later they come from
 * settings.json (phase 5); the shape of a profile is already that file's:
 * guid, name, commandline, icon, hidden.
 */
class ProfileManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(QString defaultProfileId READ defaultProfileId NOTIFY profilesChanged)
    Q_PROPERTY(QVariantList newTabMenu READ newTabMenu NOTIFY profilesChanged)   // the "⌄" menu, resolved (NewTabMenu)

public:
    explicit ProfileManager(QObject *parent = nullptr);

    QVariantList profiles() const { return m_profiles; }
    QString defaultProfileId() const { return m_defaultId; }
    QVariantList newTabMenu() const { return m_newTabMenu; }
    /** The label of a `matchProfiles` entry on the settings page: "profile: Azure". */
    Q_INVOKABLE QString matcherLabel(const QVariantMap &entry) const;
    Q_INVOKABLE QVariantMap profile(const QString &guid) const;
    /** The default profile when `guid` is empty or unknown. */
    Q_INVOKABLE QVariantMap profileOrDefault(const QString &guid) const;
    /** A new profile in settings.json: a copy of `basedOn`, or an empty one running the login shell. Returns its guid. */
    Q_INVOKABLE QString createProfile(const QString &basedOn = {});
    /** Deletes a profile of the user's; a profile found on the system is hidden instead. */
    Q_INVOKABLE void deleteProfile(const QString &guid);
    /** True for a profile that only settings.json defines. */
    Q_INVOKABLE bool isCustom(const QString &guid) const;

Q_SIGNALS:
    void profilesChanged();

private:
    void detect();
    void rebuild();
    static QString expand(const QString &text);

    QVariantList m_generated;   // what was found on the system
    QVariantList m_profiles;
    QString m_defaultId;
    QVariantList m_newTabMenu;
};
