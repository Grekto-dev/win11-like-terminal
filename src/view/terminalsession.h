#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "pty/ptyprocess.h"
#include "vt/vtcore.h"

/**
 * One terminal: a program on a pty, and the emulation of what it prints. It has
 * no picture: a TerminalView shows it (and tells it how many columns and rows
 * fit). The program starts when the first size is known, so it never sees a
 * wrong-sized terminal at startup.
 */
class TerminalSession : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString program READ program WRITE setProgram NOTIFY programChanged)
    Q_PROPERTY(QStringList arguments READ arguments WRITE setArguments NOTIFY programChanged)
    Q_PROPERTY(QString workingDirectory READ workingDirectory WRITE setWorkingDirectory NOTIFY programChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool exited READ hasExited NOTIFY exitedChanged)
    Q_PROPERTY(QString closeOnExit READ closeOnExit WRITE setCloseOnExit NOTIFY programChanged)
    Q_PROPERTY(int historySize READ historySize WRITE setHistorySize NOTIFY historySizeChanged)

public:
    explicit TerminalSession(QObject *parent = nullptr);
    ~TerminalSession() override;

    VtCore *core() const { return m_core.get(); }
    PtyProcess *pty() const { return m_pty.get(); }

    QString program() const { return m_program; }
    void setProgram(const QString &program);
    QStringList arguments() const { return m_arguments; }
    void setArguments(const QStringList &arguments);
    QString workingDirectory() const { return m_workingDirectory; }
    void setWorkingDirectory(const QString &directory);
    QString title() const { return m_title; }
    bool isRunning() const { return m_running; }
    /** The program ended with a failure and the terminal is waiting: Enter restarts, Ctrl+D closes. */
    bool hasExited() const { return m_exited; }
    /** "graceful" (also "automatic"): close when the program ends well; "always"; "never". */
    QString closeOnExit() const { return m_closeOnExit; }
    void setCloseOnExit(const QString &mode) { if (m_closeOnExit != mode) { m_closeOnExit = mode; Q_EMIT programChanged(); } }
    int historySize() const { return m_historySize; }
    void setHistorySize(int lines);

    /** Starts the program at this size (or resizes when it already runs). */
    Q_INVOKABLE void resize(int columns, int rows);
    /** The shell's current folder (from /proc), for a duplicated tab or pane; empty when unknown. */
    Q_INVOKABLE QString currentDirectory() const;
    /** Runs the program again in the same terminal. */
    Q_INVOKABLE void restart();
    /** Closes like closing the window of a terminal: SIGHUP to the shell. */
    Q_INVOKABLE void hangUp();

Q_SIGNALS:
    void programChanged();
    void titleChanged();
    void runningChanged();
    void exitedChanged();
    /** The user asked to close a terminal whose program already ended. */
    void closeRequested();
    void historySizeChanged();
    /** The program ended (exit code, or 128 + signal). */
    /** The program ended; `close` says whether the terminal should close now (closeOnExit). */
    void finished(int exitCode, bool close);
    void bell();

private:
    void start(int columns, int rows);
    void connectPty();
    /** The program is gone (or never started): say how to go on, and wait for Enter or Ctrl+D. */
    void waitAfterTheEnd();

    static constexpr int kMaxHistory = 32767; // the original's limit for historySize
    static constexpr int kBellInterval = 150; // ms between two rings

    std::unique_ptr<VtCore> m_core;
    std::unique_ptr<PtyProcess> m_pty;
    QString m_program;
    QStringList m_arguments;
    QString m_workingDirectory;
    QString m_title;
    QString m_closeOnExit = QStringLiteral("automatic");
    bool m_running = false;
    bool m_started = false;
    bool m_exited = false;
    int m_columns = 120;
    int m_rows = 30;
    int m_historySize = 9001;
    QElapsedTimer m_lastBell;
};
