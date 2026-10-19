#include <QCommandLineParser>
#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTranslator>

namespace
{
/** Cascadia Mono ships with us (SIL OFL): from the install, or the source tree in development. */
void loadFonts()
{
    QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                                 QStringLiteral("win11terminal/fonts"),
                                                 QStandardPaths::LocateDirectory);
    dirs << QGuiApplication::applicationDirPath() + QStringLiteral("/../share/win11terminal/fonts")
         << QStringLiteral(WIN11TERMINAL_SOURCE_DATA_DIR "/fonts");
    for (const QString &dir : std::as_const(dirs)) {
        const QDir d(dir);
        if (!d.exists()) {
            continue;
        }
        for (const QString &file : d.entryList({QStringLiteral("*.ttf"), QStringLiteral("*.otf")}, QDir::Files)) {
            QFontDatabase::addApplicationFont(d.filePath(file));
        }
        return;
    }
    qWarning("win11terminal: fonts folder not found; using the system's monospace font");
}
}

int main(int argc, char *argv[])
{
    // The look is all ours: never a platform style or theme.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    qunsetenv("QT_QPA_PLATFORMTHEME");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("win11terminal"));
    app.setOrganizationName(QStringLiteral("pedro"));
    app.setDesktopFileName(QStringLiteral("org.pedro.win11terminal"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setWindowIcon(QIcon(QStringLiteral(":/data/icons/org.pedro.win11terminal.svg")));

    // The command line: options first, then `-e` or `--` and the program to run (the
    // convention of terminal emulators, so x-terminal-emulator style calls work).
    QStringList args = app.arguments();
    args.removeFirst();
    QStringList command;
    for (int i = 0; i < args.size(); ++i) {
        if (args[i] == QLatin1String("-e") || args[i] == QLatin1String("--command") || args[i] == QLatin1String("--")) {
            command = args.mid(i + 1);
            args = args.mid(0, i);
            break;
        }
    }
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Terminal emulator for Linux"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption directory({QStringLiteral("d"), QStringLiteral("working-directory")},
                                       QStringLiteral("Start in this folder."), QStringLiteral("folder"));
    const QCommandLineOption profile({QStringLiteral("p"), QStringLiteral("profile")},
                                     QStringLiteral("Open this profile (its name or guid)."), QStringLiteral("profile"));
    const QCommandLineOption maximized({QStringLiteral("M"), QStringLiteral("maximized")}, QStringLiteral("Start maximized."));
    const QCommandLineOption fullscreen({QStringLiteral("F"), QStringLiteral("fullscreen")}, QStringLiteral("Start in full screen."));
    parser.addOptions({directory, profile, maximized, fullscreen});
    parser.addPositionalArgument(QStringLiteral("command"), QStringLiteral("Program to run instead of the shell (after -e or --)."),
                                 QStringLiteral("[-e command [args...]]"));
    parser.process(QStringList{app.arguments().first()} + args);
    if (command.isEmpty()) {
        command = parser.positionalArguments();
    }

    loadFonts();

    // Portuguese-language sessions use the Brazilian Portuguese resources;
    // English is the source language.
    QTranslator translator;
    const QStringList languages = QLocale::system().uiLanguages();
    const QString language = qEnvironmentVariable("WIN11TERMINAL_LANG", languages.isEmpty() ? QLocale::system().name() : languages.constFirst());
    if (language.startsWith(QLatin1String("pt")) && translator.load(QStringLiteral(":/i18n/win11terminal_pt_BR.qm"))) {
        app.installTranslator(&translator);
    }

    const QString program = command.isEmpty() ? QString() : command.takeFirst();

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("program"), program},
        {QStringLiteral("arguments"), command},
        {QStringLiteral("workingDirectory"), parser.value(directory)},
        {QStringLiteral("profileName"), parser.value(profile)},
        {QStringLiteral("startMaximized"), parser.isSet(maximized)},
        {QStringLiteral("startFullscreen"), parser.isSet(fullscreen)},
    });
    engine.loadFromModule("Win11Terminal", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}
