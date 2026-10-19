#include "ptyprocess.h"

#include <QCoreApplication>
#include <QSet>
#include <QSocketNotifier>
#include <QTimer>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <algorithm>
#include <utility>
#include <vector>

namespace
{
// Variables of the terminal we may have been started from, which would
// mislead a shell that now runs in a different one.
const char *const kDropped[] = {
    "COLUMNS", "LINES", "TERMCAP", "TERM_PROGRAM", "TERM_PROGRAM_VERSION", "TERM_SESSION_ID",
    "VTE_VERSION", "KONSOLE_VERSION", "KONSOLE_DBUS_SERVICE", "KONSOLE_DBUS_SESSION",
    "WT_SESSION", "WT_PROFILE_ID", "TMUX", "TMUX_PANE", "STY", "WINDOW", "ALACRITTY_LOG",
    "ALACRITTY_SOCKET", "KITTY_WINDOW_ID", "GNOME_TERMINAL_SERVICE", "GNOME_TERMINAL_SCREEN",
};
constexpr qsizetype kReadChunk = 64 * 1024;
// Bytes queued for a child that does not read its input. Past this, more is dropped: a program
// that floods the terminal with queries (each one answered) and never reads must not eat the memory.
constexpr qsizetype kMaxPending = 8 * 1024 * 1024;
// The fd range a child closes by hand when close_range(2) is missing (Linux < 5.9).
constexpr int kMaxFdFallback = 65536;

int openPidfd(pid_t pid)
{
#ifdef SYS_pidfd_open
    return static_cast<int>(::syscall(SYS_pidfd_open, pid, 0));
#else
    Q_UNUSED(pid)
    return -1;
#endif
}

pid_t waitNoHang(pid_t pid, int *status)
{
    pid_t result;
    do {
        result = ::waitpid(pid, status, WNOHANG);
    } while (result < 0 && errno == EINTR);
    return result;
}

/**
 * Collects the children whose PtyProcess went away before they exited (a closed tab), so that
 * they do not stay zombies for the life of the program: by their pidfd when the kernel has one,
 * else by a slow poll that runs only while some are left.
 */
class ChildReaper : public QObject
{
public:
    static ChildReaper *instance()
    {
        static ChildReaper *reaper = new ChildReaper; // lives as long as the process
        return reaper;
    }

    /** Takes over `pid` (and its pidfd, which it closes). */
    void adopt(pid_t pid, int pidfd)
    {
        if (waitNoHang(pid, nullptr) != 0) {
            if (pidfd >= 0) {
                ::close(pidfd);
            }
            return; // already gone (or not ours)
        }
        if (pidfd >= 0) {
            auto *notifier = new QSocketNotifier(pidfd, QSocketNotifier::Read, this);
            connect(notifier, &QSocketNotifier::activated, this, [pid, pidfd, notifier] {
                ::waitpid(pid, nullptr, WNOHANG);
                notifier->setEnabled(false);
                notifier->deleteLater();
                ::close(pidfd);
            });
            return;
        }
        m_polled.insert(pid);
        if (!m_poll.isActive()) {
            m_poll.start();
        }
    }

private:
    ChildReaper()
    {
        m_poll.setInterval(1000);
        connect(&m_poll, &QTimer::timeout, this, [this] {
            for (auto it = m_polled.begin(); it != m_polled.end();) {
                it = ::waitpid(*it, nullptr, WNOHANG) != 0 ? m_polled.erase(it) : std::next(it);
            }
            if (m_polled.isEmpty()) {
                m_poll.stop();
            }
        });
    }

    QSet<pid_t> m_polled;
    QTimer m_poll;
};
}

PtyProcess::PtyProcess(QObject *parent)
    : QObject(parent)
{
}

PtyProcess::~PtyProcess()
{
    const bool orphan = m_pid > 0 && !m_exited;
    if (orphan) {
        // Like closing the window of a terminal: the whole job hears it.
        ::kill(-m_pid, SIGHUP);
        ::kill(m_pid, SIGHUP);
    }
    // The reaper owns the pidfd from here (it must outlive us to collect the child).
    const int pidfd = std::exchange(m_pidfd, -1);
    closeFds();
    if (orphan) {
        ChildReaper::instance()->adopt(m_pid, pidfd);
    } else if (pidfd >= 0) {
        ::close(pidfd);
    }
}

QProcessEnvironment PtyProcess::defaultEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const char *name : kDropped) {
        env.remove(QString::fromLatin1(name));
    }
    env.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    env.insert(QStringLiteral("COLORTERM"), QStringLiteral("truecolor"));
    return env;
}

bool PtyProcess::start(const Options &options, QString *error)
{
    const auto fail = [error](const QString &what) {
        if (error) {
            *error = what + QLatin1String(": ") + QString::fromLocal8Bit(std::strerror(errno));
        }
        return false;
    };
    if (m_pid > 0) {
        return fail(QStringLiteral("already started"));
    }

    // Everything the child needs is built before the fork: between fork and
    // exec only async-signal-safe calls are allowed.
    const QByteArray program = options.program.toLocal8Bit();
    std::vector<QByteArray> argStore{program};
    for (const QString &arg : options.arguments) {
        argStore.push_back(arg.toLocal8Bit());
    }
    std::vector<char *> argv;
    for (QByteArray &arg : argStore) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    std::vector<QByteArray> envStore;
    const QProcessEnvironment &environment = options.environment;
    for (const QString &key : environment.keys()) {
        envStore.push_back((key + QLatin1Char('=') + environment.value(key)).toLocal8Bit());
    }
    std::vector<char *> envp;
    for (QByteArray &entry : envStore) {
        envp.push_back(entry.data());
    }
    envp.push_back(nullptr);
    const QByteArray cwd = options.workingDirectory.toLocal8Bit();
    struct rlimit limit = {};
    const int maxFd = ::getrlimit(RLIMIT_NOFILE, &limit) == 0 && limit.rlim_cur != RLIM_INFINITY
        ? int(std::min<rlim_t>(limit.rlim_cur, kMaxFdFallback))
        : kMaxFdFallback;

    struct winsize size = {};
    size.ws_col = static_cast<unsigned short>(options.columns);
    size.ws_row = static_cast<unsigned short>(options.rows);

    int master = -1;
    const pid_t pid = ::forkpty(&master, nullptr, nullptr, &size);
    if (pid < 0) {
        return fail(QStringLiteral("forkpty"));
    }
    if (pid == 0) {
        // Child. A fresh signal state: Qt blocks and ignores some.
        sigset_t none;
        sigemptyset(&none);
        sigprocmask(SIG_SETMASK, &none, nullptr);
        for (int sig : {SIGPIPE, SIGINT, SIGQUIT, SIGTERM, SIGHUP, SIGCHLD, SIGWINCH}) {
            ::signal(sig, SIG_DFL);
        }
        if (!cwd.isEmpty() && ::chdir(cwd.constData()) != 0) {
            // Stay where we are: a shell in the wrong folder beats none.
        }
        // Nothing of ours leaks into the program (close_range is Linux 5.9+; else one by one).
        bool closed = false;
#ifdef SYS_close_range
        closed = ::syscall(SYS_close_range, 3u, ~0u, 0u) == 0;
#endif
        for (int fd = 3; !closed && fd < maxFd; ++fd) {
            ::close(fd);
        }
        ::execvpe(argv[0], argv.data(), envp.data());
        const char message[] = "win11terminal: could not run the program\r\n";
        if (::write(STDERR_FILENO, message, sizeof(message) - 1) < 0) {
            // Nothing else to do.
        }
        ::_exit(127);
    }

    m_master = master;
    m_pid = pid;
    m_exited = m_eof = false;
    ::fcntl(m_master, F_SETFL, ::fcntl(m_master, F_GETFL) | O_NONBLOCK);
    ::fcntl(m_master, F_SETFD, FD_CLOEXEC);

    m_readNotifier = new QSocketNotifier(m_master, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, &PtyProcess::readReady);
    m_writeNotifier = new QSocketNotifier(m_master, QSocketNotifier::Write, this);
    m_writeNotifier->setEnabled(false);
    connect(m_writeNotifier, &QSocketNotifier::activated, this, &PtyProcess::writeReady);

    // A pidfd turns "the child exited" into a readable fd for the event loop
    // (no SIGCHLD handler to share with the rest of the process).
    // (Linux 5.3+; without it reap() polls after the end of the output.)
    m_pidfd = openPidfd(m_pid);
    if (m_pidfd >= 0) {
        m_pidNotifier = new QSocketNotifier(m_pidfd, QSocketNotifier::Read, this);
        connect(m_pidNotifier, &QSocketNotifier::activated, this, &PtyProcess::childExited);
    }
    return true;
}

void PtyProcess::write(const QByteArray &data)
{
    if (m_master < 0 || data.isEmpty()) {
        return;
    }
    const qsizetype queued = m_pending.size() - m_pendingOffset;
    if (data.size() > kMaxPending || queued + data.size() > kMaxPending) {
        if (!m_overflowWarned) {
            m_overflowWarned = true;
            qWarning("win11terminal: the program does not read its input; dropping what is sent to it");
        }
        return;
    }
    m_pending.append(data);
    writeReady();
}

void PtyProcess::writeReady()
{
    // An offset, not remove(0, n): a big paste the child takes in small bites stays linear.
    while (m_pendingOffset < m_pending.size() && m_master >= 0) {
        const ssize_t n = ::write(m_master, m_pending.constData() + m_pendingOffset,
                                  static_cast<size_t>(m_pending.size() - m_pendingOffset));
        if (n > 0) {
            m_pendingOffset += n;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            break; // EAGAIN (the child is not reading) or a dead pty
        }
    }
    if (m_pendingOffset >= m_pending.size()) {
        m_pending = QByteArray(); // also gives back the memory of a big paste
        m_pendingOffset = 0;
        m_overflowWarned = false;
    } else if (m_pendingOffset > m_pending.size() / 2) {
        m_pending.remove(0, m_pendingOffset);
        m_pendingOffset = 0;
    }
    if (m_writeNotifier) {
        m_writeNotifier->setEnabled(m_pendingOffset < m_pending.size());
    }
}

void PtyProcess::resize(int columns, int rows, int pixelWidth, int pixelHeight)
{
    if (m_master < 0) {
        return;
    }
    struct winsize size = {};
    size.ws_col = static_cast<unsigned short>(columns);
    size.ws_row = static_cast<unsigned short>(rows);
    size.ws_xpixel = static_cast<unsigned short>(pixelWidth);
    size.ws_ypixel = static_cast<unsigned short>(pixelHeight);
    ::ioctl(m_master, TIOCSWINSZ, &size);
}

void PtyProcess::hangUp()
{
    if (m_pid > 0 && !m_exited) {
        ::kill(-m_pid, SIGHUP);
        ::kill(m_pid, SIGHUP);
    }
}

pid_t PtyProcess::foregroundProcess() const
{
    return m_master >= 0 ? ::tcgetpgrp(m_master) : -1;
}

bool PtyProcess::drain(qsizetype budget)
{
    if (m_readBuffer.isEmpty()) {
        m_readBuffer.resize(kReadChunk); // once: not an allocation per wake-up
    }
    char *buffer = m_readBuffer.data();
    while (budget > 0) {
        const ssize_t n = ::read(m_master, buffer, static_cast<size_t>(kReadChunk));
        if (n > 0) {
            budget -= n;
            Q_EMIT dataReceived(QByteArray(buffer, n));
            if (m_master < 0) {
                return true; // a slot closed us
            }
        } else if (n == 0) {
            return true;
        } else if (errno == EINTR) {
            continue;
        } else if (errno == EAGAIN) {
            return false;
        } else {
            return true; // EIO: every slave fd is closed
        }
    }
    return false;
}

void PtyProcess::readReady()
{
    // One chunk per wake-up keeps the window alive under `cat /dev/zero`: the
    // notifier fires again at once if there is more.
    if (drain(kReadChunk)) {
        m_eof = true;
        if (m_readNotifier) {
            m_readNotifier->setEnabled(false);
        }
        reap();
    }
}

void PtyProcess::childExited()
{
    if (m_pidNotifier) {
        m_pidNotifier->setEnabled(false);
    }
    // Deliver what the child wrote before it died, then report.
    if (m_master >= 0 && !m_eof) {
        drain(64 * kReadChunk);
    }
    m_eof = true;
    reap();
}

void PtyProcess::reap()
{
    if (m_exited || m_pid <= 0) {
        return;
    }
    int status = 0;
    const pid_t r = ::waitpid(m_pid, &status, WNOHANG);
    if (r == 0) {
        // EOF on the pty before the exit: the pidfd will tell, or, without one, a later look.
        if (!m_pidNotifier) {
            QTimer::singleShot(200, this, &PtyProcess::reap);
        }
        return;
    }
    m_exited = true;
    if (r > 0) {
        if (WIFEXITED(status)) {
            m_exitCode = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            m_exitCode = 128 + WTERMSIG(status);
            m_signaled = true;
        }
    }
    Q_EMIT finished(m_exitCode, m_signaled);
}

void PtyProcess::closeFds()
{
    delete m_readNotifier;
    delete m_writeNotifier;
    delete m_pidNotifier;
    m_readNotifier = m_writeNotifier = m_pidNotifier = nullptr;
    if (m_master >= 0) {
        ::close(m_master);
        m_master = -1;
    }
    if (m_pidfd >= 0) {
        ::close(m_pidfd);
        m_pidfd = -1;
    }
}
