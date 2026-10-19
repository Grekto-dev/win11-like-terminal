#pragma once

#include <QVariantList>

/**
 * The `newTabMenu` of settings.json, resolved against the profiles: what the "⌄" menu lists.
 *
 * Entries (`type`): `profile` (by name or guid), `separator`, `folder` (`name`, `icon`, `entries`,
 * `inline` "auto"|"never", `allowEmpty`), `matchProfiles` (`name`, `commandline`, `source`: regular
 * expressions that must match the whole text; a profile is listed once, by the first entry that claims
 * it) and `remainingProfiles` (the profiles nothing else claimed, where the first such entry stands),
 * plus `action` (`id`). Without the key the menu is `[remainingProfiles]`, as in the original.
 *
 * The result is a list of maps: `{type: "profile", guid, name, index}` (index: the profile's place in
 * the list, for the Ctrl+Shift+N hint), `{type: "separator"}`, `{type: "folder", name, icon, entries}`,
 * `{type: "action", id}`. A separator never leads, trails or repeats.
 */
class NewTabMenu
{
public:
    static QVariantList defaultMenu();
    /** `menu`: the settings.json value (a list of maps); `profiles`: ProfileManager's list. */
    static QVariantList resolve(const QVariantList &menu, const QVariantList &profiles);
    /** The text the settings page shows for a profile matcher: "profile: Azure". */
    static QString matcherLabel(const QVariantMap &entry);
};
