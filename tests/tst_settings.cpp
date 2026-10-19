#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "appsettings.h"
#include "newtabmenu.h"

class SettingsTest : public QObject
{
    Q_OBJECT
    QTemporaryDir m_dir;

    QString write(const QByteArray &text)
    {
        const QString path = m_dir.filePath(QStringLiteral("settings.json"));
        QFile f(path);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write(text);
        return path;
    }

private Q_SLOTS:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void commentsAndTrailingCommasAreAllowed()
    {
        const QByteArray in = "{ // c\n \"a\": [1, 2,], /* x */ \"b\": \"http://x // not a comment\",\n}";
        const QJsonDocument doc = QJsonDocument::fromJson(AppSettings::stripComments(in));
        QVERIFY(!doc.isNull());
        QCOMPARE(doc.object().value("a").toArray().size(), 2);
        QCOMPARE(doc.object().value("b").toString(), QStringLiteral("http://x // not a comment"));
    }

    void keysAreNormalized()
    {
        QCOMPARE(AppSettings::normalizeKeys("Shift+Ctrl+T"), QStringLiteral("ctrl+shift+t"));
        QCOMPARE(AppSettings::normalizeKeys("alt+shift+plus"), QStringLiteral("alt+shift+plus"));
        QCOMPARE(AppSettings::normalizeKeys("ctrl++"), QStringLiteral("ctrl+plus"));
        QCOMPARE(AppSettings::normalizeKeys("ctrl+PageUp"), QStringLiteral("ctrl+pgup"));
        QCOMPARE(AppSettings::keysOf(Qt::Key_T, Qt::ControlModifier | Qt::ShiftModifier), QStringLiteral("ctrl+shift+t"));
        QCOMPARE(AppSettings::keysOf(Qt::Key_Backtab, Qt::ControlModifier), QStringLiteral("ctrl+shift+tab"));
        QCOMPARE(AppSettings::displayKeys("ctrl+shift+comma"), QStringLiteral("Ctrl+Shift+,"));
    }

    void defaultsLoadWithoutAUserFile()
    {
        qputenv("WIN11TERMINAL_SETTINGS", m_dir.filePath("none.json").toUtf8());
        AppSettings s;
        QCOMPARE(s.value("initialCols").toInt(), 120);
        QCOMPARE(s.profileDefaults().value("fontFace").toString(), QStringLiteral("Cascadia Mono"));
        QCOMPARE(s.scheme("Campbell").value("background").toString(), QStringLiteral("#0C0C0C"));
        QCOMPARE(s.actionForKeys("ctrl+shift+t"), QStringLiteral("Terminal.OpenNewTab"));
        QCOMPARE(s.actionForKeys("ctrl+c"), QStringLiteral("Terminal.CopyToClipboard"));
        QCOMPARE(s.displayKeysFor("Terminal.OpenSettingsUI"), QStringLiteral("Ctrl+,"));
        QVERIFY(s.error().isEmpty());
    }

    void userFileOverridesAndAddsBindings()
    {
        qputenv("WIN11TERMINAL_SETTINGS", write(
            "{ // my settings\n"
            "  \"copyOnSelect\": true,\n"
            "  \"profiles\": { \"defaults\": { \"fontSize\": 14, }, \"list\": [ { \"guid\": \"{x}\", \"name\": \"Mine\" } ] },\n"
            "  \"schemes\": [ { \"name\": \"Campbell\", \"background\": \"#000000\" } ],\n"
            "  \"keybindings\": [ { \"id\": \"Terminal.OpenNewTab\", \"keys\": \"ctrl+alt+t\" }, { \"id\": null, \"keys\": \"ctrl+shift+w\" } ]\n"
            "}").toUtf8());
        AppSettings s;
        QVERIFY2(s.error().isEmpty(), qPrintable(s.error()));
        QCOMPARE(s.value("copyOnSelect").toBool(), true);
        QCOMPARE(s.profileDefaults().value("fontSize").toInt(), 14);
        QCOMPARE(s.profileDefaults().value("fontFace").toString(), QStringLiteral("Cascadia Mono")); // kept
        QCOMPARE(s.userProfiles().size(), 1);
        QCOMPARE(s.scheme("Campbell").value("background").toString(), QStringLiteral("#000000"));
        QCOMPARE(s.actionForKeys("ctrl+alt+t"), QStringLiteral("Terminal.OpenNewTab"));
        QCOMPARE(s.actionForKeys("ctrl+shift+w"), QString()); // unbound
        QCOMPARE(s.actionForKeys("ctrl+shift+t"), QStringLiteral("Terminal.OpenNewTab")); // the default stays
    }

    void aBrokenFileFallsBackToTheDefaults()
    {
        qputenv("WIN11TERMINAL_SETTINGS", write("{ \"copyOnSelect\": tru").toUtf8());
        AppSettings s;
        QVERIFY(!s.error().isEmpty());
        QCOMPARE(s.value("initialCols").toInt(), 120);
    }

    void savingKeepsWhatIsThere()
    {
        qputenv("WIN11TERMINAL_SETTINGS", write("{\"theme\": \"light\"}").toUtf8());
        AppSettings s;
        QVERIFY(s.setGlobal("copyOnSelect", true));
        QVERIFY(s.setProfile("{g}", "fontSize", 10));
        QFile f(s.filePath());
        f.open(QIODevice::ReadOnly);
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        QCOMPARE(o.value("theme").toString(), QStringLiteral("light"));
        QCOMPARE(o.value("copyOnSelect").toBool(), true);
        QCOMPARE(o.value("profiles").toObject().value("list").toArray().at(0).toObject().value("fontSize").toInt(), 10);
    }

    void aBrokenFileIsNeverWrittenOver()
    {
        const QByteArray broken = "{ \"theme\": \"light\", \"copyOnSelect\": tru,\n \"profiles\": {\"list\": [{\"name\": \"Mine\"}]} }";
        qputenv("WIN11TERMINAL_SETTINGS", write(broken).toUtf8());
        AppSettings s;
        QVERIFY(!s.error().isEmpty());
        QSignalSpy failed(&s, &AppSettings::saveFailed);
        QVERIFY(!s.setGlobal("initialCols", 100));
        QVERIFY(!s.applyChanges({{"globals", QVariantMap{{"initialRows", 40}}}}));
        QCOMPARE(failed.size(), 2);
        QFile f(s.filePath());
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), broken); // untouched: the user fixes the typo, nothing is lost
    }

private:
    // --- newTabMenu ---------------------------------------------------------------------
    static QVariantList profilesFor(const QStringList &names)
    {
        QVariantList list;
        for (const QString &n : names) {
            list << QVariantMap{{"guid", "{" + n + "}"}, {"name", n}, {"commandline", "/bin/" + n.toLower()},
                                {"hidden", n == "Hidden"}};
        }
        return list;
    }
    static QVariantList parse(const char *json) { return QJsonDocument::fromJson(json).array().toVariantList(); }
    static QStringList names(const QVariantList &menu)
    {
        QStringList out;
        for (const QVariant &v : menu) {
            const QVariantMap m = v.toMap();
            const QString t = m.value("type").toString();
            out << (t == "profile" ? m.value("name").toString() : t == "folder" ? "[" + m.value("name").toString() + ": " + names(m.value("entries").toList()).join(",") + "]" : "-" + t);
        }
        return out;
    }

private Q_SLOTS:
    void newTabMenuDefaultsToTheRemainingProfiles()
    {
        const QVariantList menu = NewTabMenu::resolve({}, profilesFor({"Bash", "Zsh", "Hidden"}));
        QCOMPARE(names(menu), QStringList({"Bash", "Zsh"}));
        QCOMPARE(menu.at(1).toMap().value("index").toInt(), 1);
    }

    void newTabMenuEntriesFolderAndRemaining()
    {
        const QVariantList menu = NewTabMenu::resolve(parse(R"([
            {"type": "profile", "profile": "Zsh"},
            {"type": "separator"},
            {"type": "folder", "name": "More", "entries": [
                {"type": "profile", "profile": "{Fish}"}, {"type": "separator"}, {"type": "matchProfiles", "name": "S.*"} ]},
            {"type": "remainingProfiles"}, {"type": "remainingProfiles"} ])"),
                                                     profilesFor({"Bash", "Zsh", "Fish", "Sh"}));
        // Zsh is claimed once; the matcher takes Sh (the whole name must match: "S.*" does not take Zsh or Bash).
        QCOMPARE(names(menu), QStringList({"Zsh", "-separator", "[More: Fish,-separator,Sh]", "Bash"}));
    }

    void newTabMenuMatchersNeedAWholeMatchAndHiddenProfilesStayOut()
    {
        QCOMPARE(names(NewTabMenu::resolve(parse(R"([{"type": "matchProfiles", "name": "Ba"}, {"type": "remainingProfiles"}])"),
                                           profilesFor({"Bash", "Hidden", "Ba"}))),
                 QStringList({"Ba", "Bash"}));
        QCOMPARE(names(NewTabMenu::resolve(parse(R"([{"type": "matchProfiles", "commandline": "/bin/z.*"}])"), profilesFor({"Bash", "Zsh"}))),
                 QStringList({"Zsh"}));
    }

    void newTabMenuFoldersInlineAndEmpty()
    {
        const QVariantList profiles = profilesFor({"Bash"});
        QCOMPARE(names(NewTabMenu::resolve(parse(R"([{"type": "folder", "name": "F", "inline": "auto", "entries": [{"type": "profile", "profile": "Bash"}]}])"), profiles)),
                 QStringList({"Bash"}));
        QCOMPARE(names(NewTabMenu::resolve(parse(R"([{"type": "folder", "name": "F", "entries": [{"type": "profile", "profile": "Bash"}]}])"), profiles)),
                 QStringList({"[F: Bash]"}));
        QCOMPARE(names(NewTabMenu::resolve(parse(R"([{"type": "folder", "name": "E", "entries": []}, {"type": "separator"}])"), profiles)), QStringList());
        QCOMPARE(names(NewTabMenu::resolve(parse(R"([{"type": "folder", "name": "E", "allowEmpty": true, "entries": []}])"), profiles)), QStringList({"[E: ]"}));
    }

    void newTabMenuSeparatorsNeverLeadTrailOrRepeat()
    {
        QCOMPARE(names(NewTabMenu::resolve(parse(R"([{"type":"separator"},{"type":"profile","profile":"Bash"},{"type":"separator"},{"type":"separator"},{"type":"action","id":"Terminal.OpenAbout"},{"type":"separator"}])"),
                                           profilesFor({"Bash"}))),
                 QStringList({"Bash", "-separator", "-action"}));
    }

    void newTabMenuMatcherLabel()
    {
        QCOMPARE(NewTabMenu::matcherLabel({{"name", "Azure"}}), QStringLiteral("profile: Azure"));
    }
};

QTEST_MAIN(SettingsTest)
#include "tst_settings.moc"
