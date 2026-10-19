#include "terminalsession.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

TerminalSession::TerminalSession(QObject *parent)
    : QObject(parent)
    , m_core(new VtCore(30, 120, 9001))
    , m_pty(new PtyProcess)
{
    connectPty();
    connect(m_core.get(), &VtCore::titleChanged, this, [this](const QString &title) {
        m_title = title;
        Q_EMIT titleChanged();
    });
    // A program can print thousands of BELs in one go: one ring per moment is enough (each may
    // start a sound player).
    connect(m_core.get(), &VtCore::bell, this, [this] {
        if (m_lastBell.isValid() && m_lastBell.elapsed() < kBellInterval) {
            return;
        }
        m_lastBell.start();
        Q_EMIT bell();
    });
}

void TerminalSession::connectPty()
{
    connect(m_pty.get(), &PtyProcess::dataReceived, m_core.get(), &VtCore::write);
    connect(m_core.get(), &VtCore::output, m_pty.get(), &PtyProcess::write, Qt::UniqueConnection);
    connect(m_pty.get(), &PtyProcess::finished, this, [this](int code, bool) {
        m_running = false;
        Q_EMIT runningChanged();
        const bool closing = m_closeOnExit == QLatin1String("always") || (m_closeOnExit != QLatin1String("never") && code == 0);
        if (!closing) {
            // The end stays on screen, with the way out.
            const QString status = tr("[process exited with code %1]")
                                       .arg(QStringLiteral("%1 (0x%2)").arg(code).arg(uint(code), 8, 16, QLatin1Char('0')));
            m_core->write(QByteArrayLiteral("\r\n") + status.toUtf8() + QByteArrayLiteral("\r\n"));
            waitAfterTheEnd();
        }
        Q_EMIT finished(code, closing);
    });
}

TerminalSession::~TerminalSession() = default;

void TerminalSession::waitAfterTheEnd()
{
    const QString hint = tr("You can now close this terminal with Ctrl+D, or press Enter to restart.");
    m_core->write(hint.toUtf8() + QByteArrayLiteral("\r\n"));
    m_exited = true;
    Q_EMIT exitedChanged();
}

void TerminalSession::setProgram(const QString &program)
{
    if (m_program != program) {
        m_program = program;
        Q_EMIT programChanged();
    }
}

void TerminalSession::setArguments(const QStringList &arguments)
{
    if (m_arguments != arguments) {
        m_arguments = arguments;
        Q_EMIT programChanged();
    }
}

void TerminalSession::setWorkingDirectory(const QString &directory)
{
    if (m_workingDirectory != directory) {
        m_workingDirectory = directory;
        Q_EMIT programChanged();
    }
}

void TerminalSession::setHistorySize(int lines)
{
    lines = std::clamp(lines, 0, kMaxHistory);
    if (m_historySize != lines) {
        m_historySize = lines;
        m_core->setScrollbackLimit(lines);
        Q_EMIT historySizeChanged();
    }
}

QString TerminalSession::currentDirectory() const
{
    if (!m_pty || !m_pty->isRunning()) {
        return {};
    }
    return QFileInfo(QStringLiteral("/proc/%1/cwd").arg(m_pty->pid())).symLinkTarget();
}

void TerminalSession::restart()
{
    if (!m_exited) {
        return;
    }
    m_exited = false;
    Q_EMIT exitedChanged();
    m_pty.reset(new PtyProcess);
    connectPty();
    m_core->write(QByteArrayLiteral("\x1b[2J\x1b[3J\x1b[H")); // a clean screen for the new run
    start(m_columns, m_rows);
}

void TerminalSession::resize(int columns, int rows)
{
    m_columns = columns;
    m_rows = rows;
    if (!m_started) {
        start(columns, rows);
        return;
    }
    m_core->resize(rows, columns);
    m_pty->resize(columns, rows);
}

void TerminalSession::hangUp()
{
    m_pty->hangUp();
}

void TerminalSession::start(int columns, int rows)
{
    m_started = true;
    m_core->resize(rows, columns);

    PtyProcess::Options options;
    options.program = m_program;
    if (options.program.isEmpty()) {
        options.program = qEnvironmentVariable("SHELL", QStringLiteral("/bin/bash"));
    }
    options.arguments = m_arguments;
    options.workingDirectory = m_workingDirectory.isEmpty() ? QDir::homePath() : m_workingDirectory;
    options.columns = columns;
    options.rows = rows;
    options.environment = PtyProcess::defaultEnvironment();
    options.environment.insert(QStringLiteral("WT_SESSION"),
                               QUuid::createUuid().toString(QUuid::WithoutBraces));

    QString error;
    if (!m_pty->start(options, &error)) {
        m_core->write(QByteArrayLiteral("\x1b[31m[win11terminal: ") + error.toUtf8() + QByteArrayLiteral("]\x1b[0m\r\n"));
        waitAfterTheEnd(); // Enter tries again, Ctrl+D closes: never a dead terminal
        return;
    }
    m_running = true;
    Q_EMIT runningChanged();
}
