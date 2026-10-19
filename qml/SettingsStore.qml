import QtQuick
import Win11Terminal

// What the settings page has changed and not saved yet ("Salvar" writes it, "Descartar
// alterações" forgets it). Scopes: "g" the global settings, "d" profiles.defaults, or a
// profile's guid.
QtObject {
    id: root

    property var pending: ({ g: {}, d: {}, p: {}, s: {} })
    property int rev: 0
    readonly property bool dirty: rev > 0 && (Object.keys(pending.g).length + Object.keys(pending.d).length + Object.keys(pending.p).length + Object.keys(pending.s).length) > 0

    function profileOf(guid) {
        const list = ProfileManager.profiles
        for (const p of list) if (p.guid === guid) return p
        return null
    }

    // The value now: what is pending, else what settings.json (with the defaults) says.
    function get(scope, key, fallback) {
        if (rev < 0) return fallback      // a real read of `rev`, so bindings follow every edit
        let table = scope === "g" ? pending.g : scope === "d" ? pending.d : (pending.p[scope] || {})
        if (key in table) return table[key]
        let base
        if (scope === "g") base = AppSettings.globals
        else if (scope === "d") base = AppSettings.profileDefaults
        else base = profileOf(scope) || AppSettings.profileDefaults
        // A profile inherits what "Padrões" (pending too) says.
        if (scope !== "g" && scope !== "d" && !(base && key in base) && key in pending.d) return pending.d[key]
        if (base && key in base) return base[key]
        return fallback
    }
    function set(scope, key, value) {
        const p = { g: pending.g, d: pending.d, p: pending.p, s: pending.s }
        if (scope === "g") p.g[key] = value
        else if (scope === "d") p.d[key] = value
        else { if (!p.p[scope]) p.p[scope] = {}; p.p[scope][key] = value }
        pending = p
        rev++
    }
    // A colour scheme's value: pending, else the scheme itself.
    function schemeValue(name, key, fallback) {
        if (rev < 0) return fallback
        const pend = pending.s[name]
        if (pend && key in pend) return pend[key]
        const sc = AppSettings.scheme(name)
        return sc && key in sc ? sc[key] : fallback
    }
    function setScheme(name, key, value) {
        const p = { g: pending.g, d: pending.d, p: pending.p, s: pending.s }
        if (!p.s[name]) p.s[name] = {}
        p.s[name][key] = value
        pending = p
        rev++
    }
    function save() {
        AppSettings.applyChanges({ globals: pending.g, defaults: pending.d, profiles: pending.p, schemes: pending.s })
        discard()
    }
    function discard() {
        pending = { g: {}, d: {}, p: {}, s: {} }
        rev++
    }
}
