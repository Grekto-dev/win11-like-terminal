#pragma once

#include <QColor>
#include <QJsonObject>
#include <QString>

#include <array>

#include "vt/cell.h"

/**
 * A colour scheme in the Windows Terminal sense: the 16 ANSI colours plus
 * foreground, background, cursor and selection. Colours 16..255 are the xterm
 * cube and greys, fixed; a cell's colour is resolved against this at draw time.
 */
struct ColorScheme
{
    QString name = QStringLiteral("Campbell");
    QColor foreground = QColor(0xCC, 0xCC, 0xCC);
    QColor background = QColor(0x0C, 0x0C, 0x0C);
    QColor cursorColor = QColor(0xFF, 0xFF, 0xFF);
    QColor selectionBackground = QColor(0xFF, 0xFF, 0xFF);
    // black red green yellow blue purple cyan white, then the bright ones
    std::array<QColor, 16> ansi = {
        QColor(0x0C, 0x0C, 0x0C), QColor(0xC5, 0x0F, 0x1F), QColor(0x13, 0xA1, 0x0E), QColor(0xC1, 0x9C, 0x00),
        QColor(0x00, 0x37, 0xDA), QColor(0x88, 0x17, 0x98), QColor(0x3A, 0x96, 0xDD), QColor(0xCC, 0xCC, 0xCC),
        QColor(0x76, 0x76, 0x76), QColor(0xE7, 0x48, 0x56), QColor(0x16, 0xC6, 0x0C), QColor(0xF9, 0xF1, 0xA5),
        QColor(0x3B, 0x78, 0xFF), QColor(0xB4, 0x00, 0x9E), QColor(0x61, 0xD6, 0xD6), QColor(0xF2, 0xF2, 0xF2),
    };

    static ColorScheme campbell() { return ColorScheme(); }
    /** From a `schemes` entry of settings.json ("black", "brightBlack", "purple"...). */
    static ColorScheme fromJson(const QJsonObject &object);

    /** The colour of palette entry 0..255. */
    QColor indexed(int index) const;
    QColor resolveForeground(vt::Color c) const { return resolve(c, foreground); }
    QColor resolveBackground(vt::Color c) const { return resolve(c, background); }
    static qreal contrastRatio(const QColor &a, const QColor &b);
    static QColor adjustIndistinguishableForeground(const QColor &foreground, const QColor &background);

private:
    QColor resolve(vt::Color c, const QColor &fallback) const;
};
