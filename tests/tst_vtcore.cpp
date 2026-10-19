#include <QSignalSpy>
#include <QtTest>

#include "vt/vtcore.h"

using vt::Cell;

class VtCoreTest : public QObject
{
    Q_OBJECT

private:
    static QString row(const VtCore &core, int screenRow)
    {
        return core.rowText(core.historyCount() + screenRow);
    }

private Q_SLOTS:
    void plainText()
    {
        VtCore core(5, 20);
        core.write("hello\r\nworld");
        QCOMPARE(row(core, 0), QStringLiteral("hello"));
        QCOMPARE(row(core, 1), QStringLiteral("world"));
        QCOMPARE(core.cursorRow(), 1);
        QCOMPARE(core.cursorCol(), 5);
    }

    void wrapsAtTheLastColumn()
    {
        VtCore core(4, 5);
        core.write("abcdefgh");
        QCOMPARE(row(core, 0), QStringLiteral("abcde"));
        QCOMPARE(row(core, 1), QStringLiteral("fgh"));
    }

    void colorsAndAttributes()
    {
        VtCore core(3, 20);
        core.write("\x1b[1;31mR\x1b[0m\x1b[38;5;200mI\x1b[38;2;10;20;30mT\x1b[0m\x1b[44mB\x1b[4mU\x1b[0m");
        const int r = core.historyCount();
        QVERIFY(core.cell(r, 0).attrs & vt::Bold);
        QVERIFY(vt::isIndexed(core.cell(r, 0).fg));
        QCOMPARE(int(vt::colorIndex(core.cell(r, 0).fg)), 1);
        QCOMPARE(int(vt::colorIndex(core.cell(r, 1).fg)), 200);
        QVERIFY(vt::isRgb(core.cell(r, 2).fg));
        QCOMPARE(core.cell(r, 2).fg, vt::rgbColor(10, 20, 30));
        QVERIFY(vt::isIndexed(core.cell(r, 3).bg));
        QCOMPARE(vt::underlineOf(core.cell(r, 4).attrs), 1);
        QVERIFY(vt::isDefault(core.cell(r, 0).bg));
    }

    void utf8AndWideCharacters()
    {
        VtCore core(3, 20);
        core.write(QStringLiteral("é日x").toUtf8());
        const int r = core.historyCount();
        QCOMPARE(core.cell(r, 0).ch, char32_t(0xE9));
        QCOMPARE(core.cell(r, 1).ch, char32_t(0x65E5));
        QCOMPARE(int(core.cell(r, 1).width), 2);
        QCOMPARE(int(core.cell(r, 2).width), 0);
        QCOMPARE(core.cell(r, 3).ch, char32_t('x'));
        QCOMPARE(row(core, 0), QStringLiteral("é日x"));
    }

    void combiningMarksShareACell()
    {
        VtCore core(2, 10);
        core.write(QStringLiteral("éx").toUtf8());
        const int r = core.historyCount();
        QVERIFY(core.cell(r, 0).ch >= vt::kClusterBase);
        QCOMPARE(core.clusterText(core.cell(r, 0).ch), QStringLiteral("é"));
        QCOMPARE(core.cell(r, 1).ch, char32_t('x'));
    }

    void wrappedLinesStayJoinedInTheScrollback()
    {
        VtCore core(3, 5);
        core.write("abcdefghijkl\r\nxyz\r\nq\r\nr\r\ns");   // 12 chars wrap over 3 rows, then short lines scroll it away
        // The wrapped line is in the history now: rows 0..2 are its pieces.
        QVERIFY(core.historyCount() >= 3);
        QCOMPARE(core.rowText(0), QStringLiteral("abcde"));
        QCOMPARE(core.rowText(1), QStringLiteral("fghij"));
        QCOMPARE(core.rowText(2), QStringLiteral("kl"));
        QVERIFY(!core.continuation(0));
        QVERIFY(core.continuation(1));
        QVERIFY(core.continuation(2));
        QVERIFY(!core.continuation(3));   // "xyz" starts a line of its own
    }

    void scrollbackKeepsWhatScrolledOff()
    {
        VtCore core(3, 10, 100);
        QSignalSpy changed(&core, &VtCore::contentChanged);
        for (int i = 0; i < 10; ++i) {
            core.write(QByteArray("line") + QByteArray::number(i) + "\r\n");
        }
        QVERIFY(changed.count() > 0);
        QVERIFY(core.historyCount() >= 7);
        QCOMPARE(core.rowText(0), QStringLiteral("line0"));
        QCOMPARE(core.rowText(1), QStringLiteral("line1"));
        QCOMPARE(core.totalRows(), core.historyCount() + 3);
    }

    void scrollbackLimitDropsTheOldest()
    {
        VtCore core(2, 10, 3);
        for (int i = 0; i < 10; ++i) {
            core.write(QByteArray("n") + QByteArray::number(i) + "\r\n");
        }
        QCOMPARE(core.historyCount(), 3);
        QVERIFY(core.scrolledOff() > 0);
        QCOMPARE(core.rowText(2), QStringLiteral("n8")); // the newest line of the history
    }

    void titleFromOsc()
    {
        VtCore core(3, 10);
        QSignalSpy title(&core, &VtCore::titleChanged);
        core.write("\x1b]0;my title\x07");
        QCOMPARE(title.count(), 1);
        QCOMPARE(core.title(), QStringLiteral("my title"));
    }

    void answersDeviceStatusReport()
    {
        VtCore core(3, 10);
        QSignalSpy output(&core, &VtCore::output);
        core.write("ab\x1b[6n");
        QCOMPARE(output.count(), 1);
        QCOMPARE(output.first().first().toByteArray(), QByteArray("\x1b[1;3R"));
    }

    void keysBecomeEscapeSequences()
    {
        VtCore core(3, 10);
        QSignalSpy output(&core, &VtCore::output);
        core.key(VTERM_KEY_UP, VTERM_MOD_NONE);
        QCOMPARE(output.takeFirst().first().toByteArray(), QByteArray("\x1b[A"));
        core.write("\x1b[?1h"); // application cursor keys
        core.key(VTERM_KEY_UP, VTERM_MOD_NONE);
        QCOMPARE(output.takeFirst().first().toByteArray(), QByteArray("\x1bOA"));
        core.unichar('c', VTERM_MOD_CTRL);
        QCOMPARE(output.takeFirst().first().toByteArray(), QByteArray("\x03"));
        core.unichar(0xE9, VTERM_MOD_NONE);
        QCOMPARE(output.takeFirst().first().toByteArray(), QStringLiteral("é").toUtf8());
        core.key(VTERM_KEY_ENTER, VTERM_MOD_NONE);
        QCOMPARE(output.takeFirst().first().toByteArray(), QByteArray("\r"));
    }

    void bracketedPaste()
    {
        VtCore core(3, 10);
        QSignalSpy output(&core, &VtCore::output);
        core.paste(QStringLiteral("a\nb"));
        QCOMPARE(output.takeFirst().first().toByteArray(), QByteArray("a\rb")); // plain: CR for LF
        core.write("\x1b[?2004h");
        core.paste(QStringLiteral("x\r\ny"));
        QByteArray all;
        while (!output.isEmpty()) {
            all += output.takeFirst().first().toByteArray();
        }
        QCOMPARE(all, QByteArray("\x1b[200~x\ry\x1b[201~"));
    }

    // The wheel, translated for the program .
    static QByteArray drain(QSignalSpy &output)
    {
        QByteArray all;
        while (!output.isEmpty()) {
            all += output.takeFirst().first().toByteArray();
        }
        return all;
    }

    void wheelNormalScreenIsLeftToTheView()
    {
        VtCore core(3, 10);
        QSignalSpy output(&core, &VtCore::output);
        QVERIFY(!core.wheel(1, 0, 0, VTERM_MOD_NONE));
        QVERIFY(!core.wheel(-2, 1, 1, VTERM_MOD_NONE));
        QCOMPARE(drain(output), QByteArray());
    }

    void wheelAlternateScreenSendsArrows()
    {
        VtCore core(3, 10);
        QSignalSpy output(&core, &VtCore::output);
        core.write("\x1b[?1049h");
        QVERIFY(core.wheel(2, 0, 0, VTERM_MOD_NONE));
        QCOMPARE(drain(output), QByteArray("\x1b[A\x1b[A"));
        QVERIFY(core.wheel(-1, 0, 0, VTERM_MOD_NONE));
        QCOMPARE(drain(output), QByteArray("\x1b[B"));
        core.write("\x1b[?1h"); // application cursor keys
        QVERIFY(core.wheel(1, 0, 0, VTERM_MOD_NONE));
        QCOMPARE(drain(output), QByteArray("\x1bOA"));
        // Not on the alternate screen any more: back to the view.
        core.write("\x1b[?1049l");
        QVERIFY(!core.wheel(1, 0, 0, VTERM_MOD_NONE));
    }

    void wheelWithMouseReportingSendsButtons64And65()
    {
        // 1000, 1002 and 1003 all report the wheel; 1006 (SGR) writes it as 64/65.
        for (const char *mode : {"\x1b[?1000h", "\x1b[?1002h", "\x1b[?1003h"}) {
            VtCore core(5, 20);
            QSignalSpy output(&core, &VtCore::output);
            core.write(mode);
            core.write("\x1b[?1006h");
            drain(output);
            QVERIFY(core.wheel(1, 2, 3, VTERM_MOD_NONE));
            const QByteArray up = drain(output);
            QVERIFY2(up.endsWith("\x1b[<64;4;3M"), up.constData());
            QVERIFY(core.wheel(-2, 2, 3, VTERM_MOD_NONE));
            const QByteArray down = drain(output);
            QVERIFY2(down.endsWith("\x1b[<65;4;3M\x1b[<65;4;3M"), down.constData());
        }
    }

    void wheelWithMouseReportingBeatsAlternateScreenAndKeepsModifiers()
    {
        VtCore core(5, 20);
        QSignalSpy output(&core, &VtCore::output);
        core.write("\x1b[?1049h\x1b[?1000h\x1b[?1006h");
        drain(output);
        QVERIFY(core.wheel(1, 0, 0, VTERM_MOD_CTRL));
        const QByteArray ctrlUp = drain(output);
        QVERIFY2(ctrlUp.endsWith("\x1b[<80;1;1M"), ctrlUp.constData()); // 64 + 16 (ctrl)
        QVERIFY(!ctrlUp.contains("\x1b[A"));
    }

    void wheelLegacyMouseEncoding()
    {
        VtCore core(5, 20);
        QSignalSpy output(&core, &VtCore::output);
        core.write("\x1b[?1000h"); // no 1006: X10 bytes, button 64 -> 32 + 64
        drain(output);
        QVERIFY(core.wheel(1, 0, 0, VTERM_MOD_NONE));
        QCOMPARE(drain(output), QByteArray("\x1b[M") + char(32 + 64) + char(33) + char(33));
    }

    void alternateScreenAndModes()
    {
        VtCore core(3, 10);
        QSignalSpy modes(&core, &VtCore::modesChanged);
        core.write("main");
        core.write("\x1b[?1049h");
        QVERIFY(core.altScreen());
        QCOMPARE(row(core, 0), QString());
        core.write("\x1b[?1049l");
        QVERIFY(!core.altScreen());
        QCOMPARE(row(core, 0), QStringLiteral("main"));
        core.write("\x1b[?25l");
        QVERIFY(!core.cursorVisible());
        core.write("\x1b[6 q");
        QCOMPARE(core.cursorShape(), VtCore::CursorShape::Bar);
        core.write("\x1b[?1000h");
        QCOMPARE(core.mouseMode(), VtCore::MouseMode::Click);
        QVERIFY(modes.count() >= 3);
    }

    void resizeKeepsTextAndReflows()
    {
        VtCore core(4, 10);
        core.write("0123456789ABCD");
        QCOMPARE(row(core, 0), QStringLiteral("0123456789"));
        core.resize(4, 5);
        QCOMPARE(core.cols(), 5);
        QString all;
        for (int r = 0; r < core.totalRows(); ++r) {
            all += core.rowText(r);
        }
        QCOMPARE(all, QStringLiteral("0123456789ABCD"));
        core.resize(4, 20);
        QCOMPARE(core.cols(), 20);
        all.clear();
        for (int r = 0; r < core.totalRows(); ++r) {
            all += core.rowText(r);
        }
        QCOMPARE(all, QStringLiteral("0123456789ABCD"));
    }

    void growingTheWindowPullsHistoryBack()
    {
        VtCore core(3, 10, 100);
        for (int i = 0; i < 6; ++i) {
            core.write(QByteArray("row") + QByteArray::number(i) + "\r\n");
        }
        const int before = core.historyCount();
        QVERIFY(before > 0);
        core.resize(6, 10);
        QVERIFY(core.historyCount() < before);
        QCOMPARE(core.totalRows(), core.historyCount() + 6);
    }

    void eraseAndScrollRegions()
    {
        VtCore core(4, 10);
        core.write("aaaa\r\nbbbb\r\ncccc\r\ndddd");
        core.write("\x1b[2;3r\x1b[3;1H\n"); // region rows 2..3, scroll it up
        QCOMPARE(row(core, 0), QStringLiteral("aaaa"));
        QCOMPARE(row(core, 1), QStringLiteral("cccc"));
        QCOMPARE(row(core, 2), QString());
        QCOMPARE(row(core, 3), QStringLiteral("dddd"));
        core.write("\x1b[2J\x1b[H");
        QCOMPARE(row(core, 0), QString());
    }

    void faintAndOverlineAreAttributes()
    {
        VtCore core(2, 20);
        core.write("\x1b[2mF\x1b[22mN\x1b[53mO\x1b[55mP\x1b[1;2mB\x1b[0mZ");
        const int r = core.historyCount();
        QVERIFY(core.cell(r, 0).attrs & vt::Faint);
        QVERIFY(!(core.cell(r, 1).attrs & vt::Faint));
        QVERIFY(core.cell(r, 2).attrs & vt::Overline);
        QVERIFY(!(core.cell(r, 3).attrs & vt::Overline));
        QVERIFY((core.cell(r, 4).attrs & vt::Faint) && (core.cell(r, 4).attrs & vt::Bold));
        QVERIFY(!(core.cell(r, 5).attrs & (vt::Faint | vt::Bold)));
    }

    void ringBufferKeepsRowsInOrderAcrossScrollsAndResizes()
    {
        VtCore core(5, 12, 1000);
        for (int i = 0; i < 23; ++i) {
            core.write("L" + QByteArray::number(i) + "\r\n");
        }
        core.write("last");
        // the screen now holds L19..L22 and "last"
        for (int k = 0; k < 4; ++k) {
            QCOMPARE(row(core, k), QStringLiteral("L%1").arg(19 + k));
        }
        QCOMPARE(row(core, 4), QStringLiteral("last"));
        core.resize(7, 12); // rows come back from the history
        QCOMPARE(row(core, 6), QStringLiteral("last"));
        QCOMPARE(row(core, 5), QStringLiteral("L22"));
        core.resize(3, 12);
        QCOMPARE(row(core, 2), QStringLiteral("last"));
        QCOMPARE(row(core, 1), QStringLiteral("L22"));
        // the history is intact and in order
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(core.rowText(i), QStringLiteral("L%1").arg(i));
        }
        // scrolling regions and the alternate screen still work after rotations
        core.write("\x1b[?1049h\x1b[Halt\x1b[?1049l");
        QCOMPARE(row(core, 2), QStringLiteral("last"));
    }

    void reverseVideoIsAnAttribute()
    {
        VtCore core(2, 10);
        core.write("\x1b[7mR\x1b[27mN");
        const int r = core.historyCount();
        QVERIFY(core.cell(r, 0).attrs & vt::Reverse);
        QVERIFY(!(core.cell(r, 1).attrs & vt::Reverse));
    }

    // --- Review fixes ------------------------------------------------------------------

    void pasteCannotEndABracketedPasteEarly()
    {
        QCOMPARE(VtCore::filterPaste(QStringLiteral("a\x1b[201~b\x07\tc\r\nd") + QChar(0x9B) + QChar(0x7F) + QStringLiteral("é")),
                 QStringLiteral("a[201~b\tc\r\ndé"));
        VtCore core(3, 10);
        QSignalSpy output(&core, &VtCore::output);
        core.write("\x1b[?2004h");
        QVERIFY(core.bracketedPaste());
        core.paste(QStringLiteral("ls\x1b[201~rm -rf x\n"));
        QCOMPARE(drain(output), QByteArray("\x1b[200~ls[201~rm -rf x\r\x1b[201~"));
        core.write("\x1b[?2004l");
        QVERIFY(!core.bracketedPaste());
        core.paste(QStringLiteral("\x1b\x1b"));
        QVERIFY(output.isEmpty()); // nothing left: nothing sent
    }

    void historyKeepsColoursWideCharactersAndClusters()
    {
        VtCore core(2, 20);
        core.write("\x1b[1;31mR\x1b[0m\x1b[38;2;10;20;30;48;5;17mT\x1b[0m\xe6\x97\xa5x");
        core.write("e\xcc\x81\xf0\x9f\x98\x80\x1b[4:3mU\x1b[0m");
        QVector<Cell> before;
        for (int c = 0; c < 12; ++c) {
            before << core.cell(core.historyCount(), c);
        }
        core.write("\r\n\r\n\r\n"); // two lines down into the history
        QCOMPARE(core.historyCount(), 2);
        for (int c = 0; c < 12; ++c) {
            const Cell got = core.cell(0, c), want = before[c];
            QVERIFY2(got.ch == want.ch && got.fg == want.fg && got.bg == want.bg && got.attrs == want.attrs
                         && got.width == want.width,
                     qPrintable(QStringLiteral("column %1").arg(c)));
        }
        QCOMPARE(core.rowText(0), QStringLiteral("RT日xé\U0001F600U"));
        QCOMPARE(core.cell(0, 2).width, uint8_t(2));
        QCOMPARE(core.cell(0, 3).width, uint8_t(0));
        QCOMPARE(core.cell(0, 50).ch, char32_t(0)); // past the end: blank
    }

    void historyLinesArePacked()
    {
        Cell cells[80];
        for (int c = 0; c < 80; ++c) {
            cells[c].ch = char32_t('a' + c % 26);
        }
        const vt::HistoryLine line(cells, 80, true);
        QVERIFY(line.continuation());
        QCOMPARE(line.size(), 80);
        QVERIFY2(line.byteSize() < 200, qPrintable(QString::number(line.byteSize()))); // 80 cells of 16 bytes before
        QCOMPARE(line.cell(79).ch, char32_t('a' + 79 % 26));
        cells[5].ch = 0x1F600; // beyond 16 bits: the line switches to 32-bit characters
        cells[10].ch = 0xFFFF; // the 16-bit marker as a real character
        const vt::HistoryLine wide(cells, 80, false);
        QVERIFY(!wide.continuation());
        QCOMPARE(wide.cell(5).ch, char32_t(0x1F600));
        QCOMPARE(wide.cell(10).ch, char32_t(0xFFFF));
        QCOMPARE(wide.cell(10).width, uint8_t(1));
    }

    void historyComesBackWholeWhenTheWindowGrows()
    {
        VtCore core(3, 20);
        core.write("\x1b[32mgreen\x1b[0m \xe6\x97\xa5\r\nline2\r\nline3\r\nline4");
        QCOMPARE(core.historyCount(), 1);
        core.resize(4, 20);
        QCOMPARE(core.historyCount(), 0);
        QCOMPARE(row(core, 0), QStringLiteral("green 日"));
        QCOMPARE(vt::colorIndex(core.cell(0, 0).fg), uint8_t(2));
        QCOMPARE(core.cell(0, 6).width, uint8_t(2));
    }

    void clustersAndTitlesAreCapped()
    {
        VtCore core(3, 40);
        QByteArray title("\x1b]0;");
        title += QByteArray(100000, 'x');
        title += "\x07";
        core.write(title);
        QCOMPARE(core.title().size(), 4096);
        // More distinct clusters than the table keeps: the later ones fall back to their base.
        QByteArray marks;
        for (int i = 0; i < 17000; ++i) {
            const char32_t mark[2] = {char32_t(0x4E00 + i), 0x0301};
            marks += QString::fromUcs4(mark, 2).toUtf8();
            if (i % 30 == 29) {
                marks += "\r\n";
            }
        }
        core.write(marks);
        const Cell last = core.cell(core.historyCount() + core.cursorRow(), core.cursorCol() - 2);
        QVERIFY(last.ch < vt::kClusterBase);
    }

    // Damage libvterm already handed over and a scroll after it, in one write: the row still
    // waiting to be read moves with the scroll.
    void aPendingRowMovesWithTheScroll()
    {
        VtCore core(5, 10, 100);
        // X on row 2; a scroll inside rows 0-1 (no overlap: libvterm hands row 2 over as damage);
        // then the whole screen scrolls down two lines, past the rows that scroll damages.
        core.write("\x1b[3;1HX\x1b[1;2r\x1b[2;1H\n\x1b[r\x1b[H\x1bM\x1bM");
        QCOMPARE(row(core, 4), QStringLiteral("X"));
        QCOMPARE(row(core, 2), QString());
    }

    // The screen copy follows libvterm's moves instead of being read again: whatever the
    // chunking (and so the order of moves and damage), the result is the same.
    void screenIsTheSameWhateverTheChunking()
    {
        // Cut between whole tokens only: libvterm itself decodes a UTF-8 character split
        // across writes differently, and that is not what is under test here.
        QList<QByteArray> tokens;
        quint32 seed = 12345;
        const auto rnd = [&seed](quint32 n) {
            seed = seed * 1103515245u + 12345u;
            return (seed >> 16) % n;
        };
        const char *const ops[] = {"\r\n", "\x1b[2;5r", "\x1b[r", "\x1b[3L", "\x1b[2M", "\x1b[4@", "\x1b[3P", "\x1bM",
                                   "\x1b[S", "\x1b[2T", "\x1b[H", "\x1b[10;3H", "\x1b[K", "\x1b[1J", "\x1b[31;44m",
                                   "\x1b[0m", "\xe6\x97\xa5", "\x1b[?1049h", "\x1b[?1049l", "\x1b" "D", "\x1b[3;8r\x1b[5;1H"};
        for (int i = 0; i < 6000; ++i) {
            tokens << (rnd(3) == 0 ? QByteArray(ops[rnd(std::size(ops))]) : QByteArray(1, char('a' + rnd(26))));
        }
        VtCore whole(12, 30, 100), each(12, 30, 100), groups(12, 30, 100);
        whole.write(tokens.join());
        QByteArray group;
        for (int i = 0; i < tokens.size(); ++i) {
            each.write(tokens[i]);
            group += tokens[i];
            if (i % 7 == 6) {
                groups.write(group);
                group.clear();
            }
        }
        groups.write(group);
        for (VtCore *other : {&each, &groups}) {
            QCOMPARE(other->historyCount(), whole.historyCount());
            for (int r = 0; r < whole.totalRows(); ++r) {
                for (int c = 0; c < 30; ++c) {
                    const Cell a = whole.cell(r, c), b = other->cell(r, c);
                    QVERIFY2(a.ch == b.ch && a.fg == b.fg && a.bg == b.bg && a.attrs == b.attrs && a.width == b.width,
                             qPrintable(QStringLiteral("row %1 col %2").arg(r).arg(c)));
                }
            }
        }
    }
};

QTEST_MAIN(VtCoreTest)
#include "tst_vtcore.moc"
