#include "newtabmenu.h"

#include <QRegularExpression>
#include <QSet>

namespace
{
const QString kType = QStringLiteral("type");

QString str(const QVariantMap &m, const char *key) { return m.value(QLatin1String(key)).toString(); }

bool matches(const QString &pattern, const QString &text)
{
    const QRegularExpression re(QRegularExpression::anchoredPattern(pattern), QRegularExpression::CaseInsensitiveOption);
    return re.isValid() && re.match(text).hasMatch();
}

struct Resolver
{
    const QVariantList &profiles;
    QSet<QString> used;
    bool remainingTaken = false;

    int find(const QString &nameOrGuid) const
    {
        for (int i = 0; i < profiles.size(); ++i) {
            const QVariantMap p = profiles[i].toMap();
            if (str(p, "guid") == nameOrGuid) {
                return i;
            }
        }
        for (int i = 0; i < profiles.size(); ++i) {
            if (str(profiles[i].toMap(), "name") == nameOrGuid) {
                return i;
            }
        }
        return -1;
    }

    QVariantMap profileEntry(int i) const
    {
        const QVariantMap p = profiles[i].toMap();
        return {{kType, QStringLiteral("profile")}, {QStringLiteral("guid"), str(p, "guid")},
                {QStringLiteral("name"), str(p, "name")}, {QStringLiteral("index"), i}};
    }

    bool matcherHits(const QVariantMap &entry, const QVariantMap &p) const
    {
        for (const char *key : {"name", "source", "commandline"}) {
            const QString pattern = str(entry, key);
            if (pattern.isEmpty()) {
                continue;
            }
            const QString text = str(p, key);
            if (matches(pattern, text)) {
                return true;
            }
        }
        return false;
    }

    // Pass 1: what the explicit entries claim, in document order (folders included).
    void claim(const QVariantList &entries)
    {
        for (const QVariant &ev : entries) {
            const QVariantMap e = ev.toMap();
            const QString type = str(e, "type");
            if (type == QLatin1String("profile")) {
                const int i = find(str(e, "profile"));
                if (i >= 0) {
                    used.insert(str(profiles[i].toMap(), "guid"));
                }
            } else if (type == QLatin1String("matchProfiles")) {
                for (const QVariant &pv : profiles) {
                    const QVariantMap p = pv.toMap();
                    if (!p.value(QStringLiteral("hidden")).toBool() && !used.contains(str(p, "guid")) && matcherHits(e, p)) {
                        used.insert(str(p, "guid"));
                    }
                }
            } else if (type == QLatin1String("folder")) {
                claim(e.value(QStringLiteral("entries")).toList());
            }
        }
    }

    // Pass 2: the entries themselves; `listed` keeps a profile from appearing twice.
    QVariantList build(const QVariantList &entries, QSet<QString> &listed)
    {
        QVariantList out;
        for (const QVariant &ev : entries) {
            const QVariantMap e = ev.toMap();
            const QString type = str(e, "type");
            if (type == QLatin1String("profile")) {
                const int i = find(str(e, "profile"));
                if (i >= 0 && !listed.contains(str(profiles[i].toMap(), "guid"))) {
                    listed.insert(str(profiles[i].toMap(), "guid"));
                    out << profileEntry(i);
                }
            } else if (type == QLatin1String("separator")) {
                out << QVariantMap{{kType, QStringLiteral("separator")}};
            } else if (type == QLatin1String("matchProfiles")) {
                for (int i = 0; i < profiles.size(); ++i) {
                    const QVariantMap p = profiles[i].toMap();
                    if (!p.value(QStringLiteral("hidden")).toBool() && !listed.contains(str(p, "guid")) && matcherHits(e, p)) {
                        listed.insert(str(p, "guid"));
                        out << profileEntry(i);
                    }
                }
            } else if (type == QLatin1String("remainingProfiles")) {
                if (remainingTaken) {
                    continue;
                }
                remainingTaken = true;
                for (int i = 0; i < profiles.size(); ++i) {
                    const QVariantMap p = profiles[i].toMap();
                    if (!p.value(QStringLiteral("hidden")).toBool() && !used.contains(str(p, "guid")) && !listed.contains(str(p, "guid"))) {
                        listed.insert(str(p, "guid"));
                        out << profileEntry(i);
                    }
                }
            } else if (type == QLatin1String("action")) {
                if (!str(e, "id").isEmpty()) {
                    out << QVariantMap{{kType, QStringLiteral("action")}, {QStringLiteral("id"), str(e, "id")}};
                }
            } else if (type == QLatin1String("folder")) {
                const QVariantList inner = tidy(build(e.value(QStringLiteral("entries")).toList(), listed));
                const bool allowEmpty = e.value(QStringLiteral("allowEmpty")).toBool();
                if (inner.isEmpty() && !allowEmpty) {
                    continue;
                }
                if (inner.size() == 1 && str(e, "inline") == QLatin1String("auto")) {
                    out << inner.first();
                    continue;
                }
                out << QVariantMap{{kType, QStringLiteral("folder")}, {QStringLiteral("name"), str(e, "name")},
                                   {QStringLiteral("icon"), str(e, "icon")}, {QStringLiteral("entries"), inner}};
            }
        }
        return out;
    }

    static QVariantList tidy(const QVariantList &in)
    {
        QVariantList out;
        for (const QVariant &v : in) {
            const bool sep = v.toMap().value(kType).toString() == QLatin1String("separator");
            if (sep && (out.isEmpty() || out.last().toMap().value(kType).toString() == QLatin1String("separator"))) {
                continue;
            }
            out << v;
        }
        while (!out.isEmpty() && out.last().toMap().value(kType).toString() == QLatin1String("separator")) {
            out.removeLast();
        }
        return out;
    }
};
} // namespace

QVariantList NewTabMenu::defaultMenu()
{
    return {QVariantMap{{kType, QStringLiteral("remainingProfiles")}}};
}

QVariantList NewTabMenu::resolve(const QVariantList &menu, const QVariantList &profiles)
{
    const QVariantList source = menu.isEmpty() ? defaultMenu() : menu;
    Resolver r{profiles, {}, false};
    r.claim(source);
    QSet<QString> listed;
    return Resolver::tidy(r.build(source, listed));
}

QString NewTabMenu::matcherLabel(const QVariantMap &entry)
{
    QStringList parts;
    for (const char *key : {"name", "source", "commandline"}) {
        const QString v = entry.value(QLatin1String(key)).toString();
        if (!v.isEmpty()) {
            parts << (QString::fromLatin1(key == QByteArrayView("name") ? "profile" : key) + QStringLiteral(": ") + v);
        }
    }
    return parts.join(QStringLiteral(", "));
}
