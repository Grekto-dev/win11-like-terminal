// Throughput of the emulation alone: a stream of short lines, then dense colour text.
#include <QElapsedTimer>
#include <QtTest>

#include "vt/vtcore.h"

class VtBench : public QObject
{
    Q_OBJECT

private:
    static void run(const char *name, const QByteArray &chunk, int repeats)
    {
        VtCore core(30, 120, 9001);
        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < repeats; ++i) {
            core.write(chunk);
        }
        const qint64 ms = std::max<qint64>(1, timer.elapsed());
        const double mb = double(chunk.size()) * repeats / 1e6;
        qInfo("%-22s %7.1f MB in %5lld ms = %6.1f MB/s", name, mb, ms, mb * 1000.0 / double(ms));
    }

private Q_SLOTS:
    void throughput()
    {
        QByteArray lines;
        for (int i = 0; i < 9000; ++i) {
            lines += QByteArray::number(i) + "\r\n"; // 64 KB of short lines
        }
        run("short lines", lines, 300);

        QByteArray colors;
        for (int i = 0; i < 700; ++i) {
            colors += "\x1b[3" + QByteArray::number(i % 8) + ";1mcolored text on a longer line to fill it up, ok.\x1b[0m\r\n";
        }
        run("coloured long lines", colors, 300);

        QByteArray wide;
        for (int i = 0; i < 300; ++i) {
            wide += QStringLiteral("日本語のテキスト、これは長い行です。日本語のテキスト\r\n").toUtf8();
        }
        run("wide characters", wide, 300);
    }
};

QTEST_MAIN(VtBench)
#include "bench_vtcore.moc"
