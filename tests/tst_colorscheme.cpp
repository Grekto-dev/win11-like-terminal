#include <QtTest>

#include "colorscheme.h"

class ColorSchemeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preservesTextThatAlreadyContrasts()
    {
        const QColor foreground(QStringLiteral("#F2F2F2"));
        const QColor background(QStringLiteral("#0C0C0C"));
        QCOMPARE(ColorScheme::adjustIndistinguishableForeground(foreground, background), foreground);
        QVERIFY(ColorScheme::contrastRatio(foreground, background) > 3.0);
    }

    void darkensTextOnBrightBackground()
    {
        const QColor foreground(QStringLiteral("#777777"));
        const QColor background(QStringLiteral("#808080"));
        const QColor adjusted = ColorScheme::adjustIndistinguishableForeground(foreground, background);
        QVERIFY(adjusted != foreground);
        QVERIFY(adjusted.lightness() < foreground.lightness());
        QVERIFY(ColorScheme::contrastRatio(adjusted, background) >= 2.99);
    }

    void lightensTextOnDarkBackground()
    {
        const QColor foreground(QStringLiteral("#333333"));
        const QColor background(QStringLiteral("#222222"));
        const QColor adjusted = ColorScheme::adjustIndistinguishableForeground(foreground, background);
        QVERIFY(adjusted != foreground);
        QVERIFY(adjusted.lightness() > foreground.lightness());
        QVERIFY(ColorScheme::contrastRatio(adjusted, background) >= 2.99);
    }
};

QTEST_GUILESS_MAIN(ColorSchemeTest)
#include "tst_colorscheme.moc"
