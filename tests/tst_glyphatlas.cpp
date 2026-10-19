#include <QtTest>

#include <QFontDatabase>

#include "glyphatlas.h"

class GlyphAtlasTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cellOverridesUseTheExactFontSize()
    {
        const GlyphAtlas::Metrics metrics = GlyphAtlas::measure(QStringLiteral("Inter"), 16, 16.0, 1.5, 0.75);
        QCOMPARE(metrics.cellHeight, 24);
        QCOMPARE(metrics.cellWidth, 12);
        QVERIFY(metrics.baseline >= 0 && metrics.baseline < metrics.cellHeight);
        QVERIFY(metrics.underlinePos >= 0 && metrics.underlinePos < metrics.cellHeight);
        QVERIFY(metrics.strikePos >= 0 && metrics.strikePos < metrics.cellHeight);
    }

    void naturalMetricsRemainFontMeasured()
    {
        const GlyphAtlas::Metrics natural = GlyphAtlas::measure(QStringLiteral("Inter"), 16, 16.0);
        const GlyphAtlas::Metrics overrideZero = GlyphAtlas::measure(QStringLiteral("Inter"), 16, 16.0, 0, 0);
        QCOMPARE(overrideZero.cellWidth, natural.cellWidth);
        QCOMPARE(overrideZero.cellHeight, natural.cellHeight);
    }

    void fontWeightsAcceptNamesAndNumericOpenTypeValues()
    {
        QCOMPARE(GlyphAtlas::weightValue(QStringLiteral("thin")), 100);
        QCOMPARE(GlyphAtlas::weightValue(QStringLiteral("semi-light")), 350);
        QCOMPARE(GlyphAtlas::weightValue(QStringLiteral("normal")), 400);
        QCOMPARE(GlyphAtlas::weightValue(QStringLiteral("extra-black")), 950);
        QCOMPARE(GlyphAtlas::weightValue(625), 625);
        QCOMPARE(GlyphAtlas::weightValue(9999), 990);
        QCOMPARE(GlyphAtlas::weightValue(QStringLiteral("custom")), 400);
    }

    void cacheSeparatesEveryFontOption()
    {
        const QVariantMap axes{{QStringLiteral("wght"), 550.0}};
        const QVariantMap features{{QStringLiteral("liga"), 0}};
        const auto base = GlyphAtlas::shared(QStringLiteral("Inter"), 16, 16.0, true, 0, 0, 400, {}, {}, true);
        const auto same = GlyphAtlas::shared(QStringLiteral("Inter"), 16, 16.0, true, 0, 0, 400, {}, {}, true);
        const auto weighted = GlyphAtlas::shared(QStringLiteral("Inter"), 16, 16.0, true, 0, 0, 500, {}, {}, true);
        const auto taller = GlyphAtlas::shared(QStringLiteral("Inter"), 16, 16.0, true, 1.5, 0, 400, {}, {}, true);
        const auto wider = GlyphAtlas::shared(QStringLiteral("Inter"), 16, 16.0, true, 0, 0.75, 400, {}, {}, true);
        const auto varied = GlyphAtlas::shared(QStringLiteral("Inter"), 16, 16.0, true, 0, 0, 400, axes, {}, true);
        const auto featured = GlyphAtlas::shared(QStringLiteral("Inter"), 16, 16.0, true, 0, 0, 400, {}, features, true);
        const auto monochrome = GlyphAtlas::shared(QStringLiteral("Inter"), 16, 16.0, true, 0, 0, 400, {}, {}, false);
        QVERIFY(base == same);
        QVERIFY(base != weighted);
        QVERIFY(base != taller);
        QVERIFY(base != wider);
        QVERIFY(base != varied);
        QVERIFY(base != featured);
        QVERIFY(base != monochrome);
    }

    void colorEmojiCanBeRenderedAndTinted()
    {
        if (!QFontDatabase::families().contains(QStringLiteral("Noto Color Emoji"))) {
            QSKIP("Noto Color Emoji is not installed on this host");
        }
        const QString emoji = QString::fromUcs4(U"😀");
        GlyphAtlas color(QStringLiteral("Noto Color Emoji"), 32, 32.0, false, 0, 0, 400, {}, {}, true);
        GlyphAtlas monochrome(QStringLiteral("Noto Color Emoji"), 32, 32.0, false, 0, 0, 400, {}, {}, false);
        GlyphAtlas::Glyph coloredGlyph, tintedGlyph;
        {
            QMutexLocker lock(&color.lock());
            coloredGlyph = color.insert(GlyphAtlas::makeKey(0x1F600, GlyphAtlas::Regular, 2), emoji, GlyphAtlas::Regular, 2);
        }
        {
            QMutexLocker lock(&monochrome.lock());
            tintedGlyph = monochrome.insert(GlyphAtlas::makeKey(0x1F600, GlyphAtlas::Regular, 2), emoji, GlyphAtlas::Regular, 2);
        }
        QVERIFY(coloredGlyph.page >= 0);
        QVERIFY(tintedGlyph.page >= 0);
        QVERIFY(coloredGlyph.color);
        QVERIFY(!tintedGlyph.color);
        const QImage &image = monochrome.pageImage(tintedGlyph.page);
        bool ink = false;
        for (int y = tintedGlyph.y; y < tintedGlyph.y + tintedGlyph.h; ++y) {
            for (int x = tintedGlyph.x; x < tintedGlyph.x + tintedGlyph.w; ++x) {
                const QRgb pixel = image.pixel(x, y);
                if (qAlpha(pixel)) {
                    ink = true;
                    QCOMPARE(qRed(pixel), qAlpha(pixel));
                    QCOMPARE(qGreen(pixel), qAlpha(pixel));
                    QCOMPARE(qBlue(pixel), qAlpha(pixel));
                }
            }
        }
        QVERIFY(ink);
    }
};

QTEST_MAIN(GlyphAtlasTest)
#include "tst_glyphatlas.moc"
