#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <sys/types.h>

class QSocketNotifier;

/**
 * A program on a pseudo-terminal: the master side of a pty, a child process on
 * the slave, and the plumbing between them and the event loop. Nothing here
 * knows about escape sequences.
 */
class PtyProcess : public QObject
{
    Q_OBJECT

public:
    struct Options
    {
        QString program;
        QStringList arguments;
        QString workingDirectory;
        QProcessEnvironment environment; // the whole environment of the child
        int columns = 120;
        int rows = 30;
    };

    explicit PtyProcess(QObject *parent = nullptr);
    ~PtyProcess() override;

    /** Forks the child. False (with `error`) when the pty or the fork fails. */
    bool start(const Options &options, QString *error = nullptr);

    bool isRunning() const { return m_pid > 0; }
    pid_t pid() const { return m_pid; }
    int exitCode() const { return m_exitCode; }

    /** Bytes for the child's stdin (keys, paste, answers to queries). */
    void write(const QByteArray &data);
    /** Tells the kernel (and so the child, by SIGWINCH) the new size. */
    void resize(int columns, int rows, int pixelWidth = 0, int pixelHeight = 0);
    /** SIGHUP to the child's process group, like closing the terminal. */
    void hangUp();
    /** The foreground process group's leader (for "is a command running?"). */
    pid_t foregroundProcess() const;

    /** Environment for a new child: the parent's, minus what would mislead it. */
    static QProcessEnvironment defaultEnvironment();

Q_SIGNALS:
    void dataReceived(const QByteArray &data);
    /** The child is gone and all its output was delivered. */
    void finished(int exitCode, bool signaled);

private:
    void readReady();
    void writeReady();
    void childExited();
    /** Reads what is there (up to a budget); true when the pty is at EOF. */
    bool drain(qsizetype budget);
    void reap();
    void closeFds();

    int m_master = -1;
    int m_pidfd = -1;
    pid_t m_pid = -1;
    int m_exitCode = 0;
    bool m_signaled = false;
    bool m_eof = false;
    bool m_exited = false;
    QByteArray m_pending;         // writes the kernel did not take yet, from m_pendingOffset on
    qsizetype m_pendingOffset = 0;
    bool m_overflowWarned = false;
    QByteArray m_readBuffer;
    QSocketNotifier *m_readNotifier = nullptr;
    QSocketNotifier *m_writeNotifier = nullptr;
    QSocketNotifier *m_pidNotifier = nullptr;
};
