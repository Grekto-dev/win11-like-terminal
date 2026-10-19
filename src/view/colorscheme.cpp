#include "colorscheme.h"

#include <algorithm>
#include <cmath>

namespace
{
qreal linearChannel(qreal channel)
{
    return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
}

qreal encodedChannel(qreal channel)
{
    channel = std::clamp(channel, 0.0, 1.0);
    return channel <= 0.0031308 ? channel * 12.92 : 1.055 * std::pow(channel, 1.0 / 2.4) - 0.055;
}

qreal luminance(const QColor &color)
{
    return 0.2126 * linearChannel(color.redF()) + 0.7152 * linearChannel(color.greenF())
           + 0.0722 * linearChannel(color.blueF());
}
}

ColorScheme ColorScheme::fromJson(const QJsonObject &object)
{
    ColorScheme scheme;
    scheme.name = object.value(QLatin1String("name")).toString(scheme.name);
    const auto read = [&object](const char *key, QColor &into) {
        const QColor c(object.value(QLatin1String(key)).toString());
        if (c.isValid()) {
            into = c;
        }
    };
    read("foreground", scheme.foreground);
    read("background", scheme.background);
    read("cursorColor", scheme.cursorColor);
    // The original falls back to white when a scheme names none.
    read("selectionBackground", scheme.selectionBackground);
    static const char *const names[16] = {
        "black", "red", "green", "yellow", "blue", "purple", "cyan", "white",
        "brightBlack", "brightRed", "brightGreen", "brightYellow", "brightBlue", "brightPurple",
        "brightCyan", "brightWhite",
    };
    for (int i = 0; i < 16; ++i) {
        read(names[i], scheme.ansi[size_t(i)]);
    }
    return scheme;
}

QColor ColorScheme::indexed(int index) const
{
    if (index < 16) {
        return ansi[size_t(index)];
    }
    if (index < 232) {
        static const int levels[6] = {0x00, 0x5F, 0x87, 0xAF, 0xD7, 0xFF};
        const int i = index - 16;
        return QColor(levels[i / 36], levels[(i / 6) % 6], levels[i % 6]);
    }
    const int grey = 8 + (index - 232) * 10;
    return QColor(grey, grey, grey);
}

QColor ColorScheme::resolve(vt::Color c, const QColor &fallback) const
{
    if (vt::isDefault(c)) {
        return fallback;
    }
    if (vt::isIndexed(c)) {
        return indexed(vt::colorIndex(c));
    }
    return QColor(int((c >> 16) & 0xFF), int((c >> 8) & 0xFF), int(c & 0xFF));
}

qreal ColorScheme::contrastRatio(const QColor &a, const QColor &b)
{
    if (!a.isValid() || !b.isValid()) {
        return 1.0;
    }
    const qreal first = luminance(a), second = luminance(b);
    return (std::max(first, second) + 0.05) / (std::min(first, second) + 0.05);
}

QColor ColorScheme::adjustIndistinguishableForeground(const QColor &foreground, const QColor &background)
{
    if (!foreground.isValid() || !background.isValid() || contrastRatio(foreground, background) >= 3.0) {
        return foreground;
    }
    const qreal fg = luminance(foreground), bg = luminance(background);
    if (bg > 0.179) {
        const qreal target = std::max(0.0, (bg + 0.05) / 3.0 - 0.05);
        const qreal factor = fg > 0 ? target / fg : 0;
        return QColor::fromRgbF(encodedChannel(linearChannel(foreground.redF()) * factor),
                                encodedChannel(linearChannel(foreground.greenF()) * factor),
                                encodedChannel(linearChannel(foreground.blueF()) * factor), foreground.alphaF());
    }
    const qreal target = std::min(1.0, 3.0 * (bg + 0.05) - 0.05);
    const qreal factor = fg < 1.0 ? std::clamp((target - fg) / (1.0 - fg), 0.0, 1.0) : 1.0;
    return QColor::fromRgbF(encodedChannel(linearChannel(foreground.redF()) + factor * (1.0 - linearChannel(foreground.redF()))),
                            encodedChannel(linearChannel(foreground.greenF()) + factor * (1.0 - linearChannel(foreground.greenF()))),
                            encodedChannel(linearChannel(foreground.blueF()) + factor * (1.0 - linearChannel(foreground.blueF()))),
                            foreground.alphaF());
}
