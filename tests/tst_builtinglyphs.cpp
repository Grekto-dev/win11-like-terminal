#include <QtTest>

#include "builtinglyphs.h"

class BuiltinGlyphsTest : public QObject
{
    Q_OBJECT

    static QImage drawn(char32_t ch, int w = 9, int h = 19, int t = 1)
    {
        QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        BuiltinGlyphs::draw(img, ch, t);
        return img;
    }
    static bool on(const QImage &img, int x, int y) { return qAlpha(img.pixel(x, y)) > 0; }

private Q_SLOTS:
    void handlesBoxAndBlocksOnly()
    {
        QVERIFY(BuiltinGlyphs::handles(0x2500));
        QVERIFY(BuiltinGlyphs::handles(0x259F));
        QVERIFY(!BuiltinGlyphs::handles(U'a'));
        QVERIFY(!BuiltinGlyphs::handles(0x25A0));
    }

    void linesTouchBothEdgesOfTheCell()
    {
        // The horizontal line meets the next cell's, the vertical one the cell above and below, at any size.
        for (const QSize s : {QSize(9, 19), QSize(7, 15), QSize(12, 25), QSize(14, 29)}) {
            const QImage h = drawn(0x2500, s.width(), s.height());
            const QImage v = drawn(0x2502, s.width(), s.height());
            int row = -1, col = -1;
            for (int y = 0; y < s.height(); ++y) if (on(h, 0, y)) row = y;
            for (int x = 0; x < s.width(); ++x) if (on(v, x, 0)) col = x;
            QVERIFY2(row >= 0 && on(h, s.width() - 1, row), "horizontal line reaches both sides");
            QVERIFY2(col >= 0 && on(v, col, s.height() - 1), "vertical line reaches both ends");
            for (int x = 0; x < s.width(); ++x) QVERIFY(on(h, x, row));   // no gap along it
        }
    }

    void cornersAndCrossesAreWhole()
    {
        const QImage cross = drawn(0x253C);
        const int cx = 4, cy = 9;
        for (int x = 0; x < 9; ++x) QVERIFY(on(cross, x, cy));
        for (int y = 0; y < 19; ++y) QVERIFY(on(cross, cx, y));
        const QImage corner = drawn(0x250C);   // down and right
        QVERIFY(on(corner, 4, 9) && on(corner, 8, 9) && on(corner, 4, 18));
        QVERIFY(!on(corner, 0, 9) && !on(corner, 4, 0));
    }

    void blocksCoverExactFractions()
    {
        const QImage full = drawn(0x2588);
        for (int y = 0; y < 19; ++y) for (int x = 0; x < 9; ++x) QVERIFY(on(full, x, y));
        const QImage lower = drawn(0x2584);   // lower half
        int first = -1;
        for (int y = 0; y < 19; ++y) if (on(lower, 0, y)) { first = y; break; }
        QVERIFY(first >= 9 && first <= 10);
        for (int y = first; y < 19; ++y) for (int x = 0; x < 9; ++x) QVERIFY(on(lower, x, y));
        const QImage left = drawn(0x258C);
        QVERIFY(on(left, 0, 0) && on(left, 4, 18) && !on(left, 8, 0));
        // The two halves tile the cell with nothing between.
        const QImage upper = drawn(0x2580);
        for (int y = 0; y < 19; ++y) QVERIFY(on(upper, 3, y) != on(lower, 3, y));
    }

    void doubleLinesKeepTheirGap()
    {
        const QImage dbl = drawn(0x2550);
        int count = 0;
        bool prev = false;
        for (int y = 0; y < 19; ++y) { const bool o = on(dbl, 4, y); if (o && !prev) ++count; prev = o; }
        QCOMPARE(count, 2);
    }
};

QTEST_MAIN(BuiltinGlyphsTest)
#include "tst_builtinglyphs.moc"
