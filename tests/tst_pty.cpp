#include <QSignalSpy>
#include <QtTest>

#include "pty/ptyprocess.h"

#include <QFileInfo>

#include <fcntl.h>
#include <unistd.h>

class PtyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void runsAProgramAndReportsItsExit()
    {
        PtyProcess pty;
        PtyProcess::Options options;
        options.program = QStringLiteral("/bin/sh");
        options.arguments = {QStringLiteral("-c"), QStringLiteral("echo hi; exit 3")};
        options.environment = PtyProcess::defaultEnvironment();
        QByteArray received;
        connect(&pty, &PtyProcess::dataReceived, this, [&](const QByteArray &d) { received += d; });
        QSignalSpy finished(&pty, &PtyProcess::finished);
        QString error;
        QVERIFY2(pty.start(options, &error), qPrintable(error));
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.first().at(0).toInt(), 3);
        QVERIFY(received.contains("hi"));
    }

    void childSeesATerminalOfTheRightSize()
    {
        PtyProcess pty;
        PtyProcess::Options options;
        options.program = QStringLiteral("/bin/sh");
        options.arguments = {QStringLiteral("-c"), QStringLiteral("stty size; echo $TERM $COLORTERM")};
        options.environment = PtyProcess::defaultEnvironment();
        options.columns = 100;
        options.rows = 40;
        QByteArray received;
        connect(&pty, &PtyProcess::dataReceived, this, [&](const QByteArray &d) { received += d; });
        QSignalSpy finished(&pty, &PtyProcess::finished);
        QVERIFY(pty.start(options));
        QVERIFY(finished.wait(5000));
        QVERIFY2(received.contains("40 100"), received.constData());
        QVERIFY2(received.contains("xterm-256color truecolor"), received.constData());
    }

    void writesReachTheChildAndResizeIsSeen()
    {
        PtyProcess pty;
        PtyProcess::Options options;
        options.program = QStringLiteral("/bin/sh");
        options.environment = PtyProcess::defaultEnvironment();
        QByteArray received;
        connect(&pty, &PtyProcess::dataReceived, this, [&](const QByteArray &d) { received += d; });
        QSignalSpy finished(&pty, &PtyProcess::finished);
        QVERIFY(pty.start(options));
        pty.resize(77, 33);
        pty.write("stty size; exit 0\n");
        QVERIFY(finished.wait(5000));
        QVERIFY2(received.contains("33 77"), received.constData());
    }

    void reportsAProgramThatDoesNotExist()
    {
        PtyProcess pty;
        PtyProcess::Options options;
        options.program = QStringLiteral("/nonexistent/program");
        options.environment = PtyProcess::defaultEnvironment();
        QSignalSpy finished(&pty, &PtyProcess::finished);
        QVERIFY(pty.start(options));
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.first().at(0).toInt(), 127);
    }

    void aBigWriteReachesTheChildWhole()
    {
        PtyProcess pty;
        PtyProcess::Options options;
        options.program = QStringLiteral("/bin/sh");
        options.arguments = {QStringLiteral("-c"), QStringLiteral("stty raw -echo; head -c 1000000 | wc -c")};
        options.environment = PtyProcess::defaultEnvironment();
        QByteArray received;
        connect(&pty, &PtyProcess::dataReceived, this, [&](const QByteArray &d) { received += d; });
        QSignalSpy finished(&pty, &PtyProcess::finished);
        QVERIFY(pty.start(options));
        QTest::qWait(300); // the raw mode first, or the line discipline cuts long lines
        pty.write(QByteArray(1000000, 'x'));
        QVERIFY(finished.wait(10000));
        QVERIFY2(received.contains("1000000"), received.constData());
    }

    void aChildLeftRunningIsCollected()
    {
        auto *pty = new PtyProcess;
        PtyProcess::Options options;
        options.program = QStringLiteral("/bin/sh");
        // It outlives the hangup: the terminal goes away first.
        options.arguments = {QStringLiteral("-c"), QStringLiteral("trap '' HUP; sleep 1")};
        options.environment = PtyProcess::defaultEnvironment();
        QVERIFY(pty->start(options));
        const pid_t pid = pty->pid();
        QTest::qWait(200);
        delete pty;
        const QString proc = QStringLiteral("/proc/%1").arg(pid);
        QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(proc), 5000); // reaped, not a zombie
    }

    void aSingleWriteCannotExceedThePendingInputLimit()
    {
        PtyProcess pty;
        PtyProcess::Options options;
        options.program = QStringLiteral("/bin/sleep");
        options.arguments = {QStringLiteral("2")};
        options.environment = PtyProcess::defaultEnvironment();
        QVERIFY(pty.start(options));
        QTest::qWait(100); // let the child start; it never reads from the pty
        QTest::ignoreMessage(QtWarningMsg, "win11terminal: the program does not read its input; dropping what is sent to it");
        pty.write(QByteArray(8 * 1024 * 1024 + 1, 'x'));
    }

    void ourFilesDoNotLeakIntoTheChild()
    {
        const int fd = ::open("/dev/null", O_RDONLY); // no O_CLOEXEC on purpose
        QVERIFY(fd >= 0);
        PtyProcess pty;
        PtyProcess::Options options;
        options.program = QStringLiteral("/bin/sh");
        options.arguments = {QStringLiteral("-c"), QStringLiteral("[ -e /proc/$$/fd/%1 ] && echo LEAK || echo CLEAN").arg(fd)};
        options.environment = PtyProcess::defaultEnvironment();
        QByteArray received;
        connect(&pty, &PtyProcess::dataReceived, this, [&](const QByteArray &d) { received += d; });
        QSignalSpy finished(&pty, &PtyProcess::finished);
        QVERIFY(pty.start(options));
        QVERIFY(finished.wait(5000));
        ::close(fd);
        QVERIFY2(received.contains("CLEAN"), received.constData());
    }
};

QTEST_MAIN(PtyTest)
#include "tst_pty.moc"
