#pragma once

#include <cstdint>

namespace vt
{

/**
 * A colour as a cell stores it: default, a palette index or true colour. It is
 * not resolved to RGB here, so a change of colour scheme needs no re-read of
 * the screen and "default" stays distinguishable (reverse video, bold colours).
 */
using Color = uint32_t;
constexpr Color kDefaultColor = 0;
constexpr Color indexedColor(uint8_t index)
{
    return (1u << 24) | index;
}
constexpr Color rgbColor(uint8_t r, uint8_t g, uint8_t b)
{
    return (2u << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}
constexpr bool isDefault(Color c)
{
    return (c >> 24) == 0;
}
constexpr bool isIndexed(Color c)
{
    return (c >> 24) == 1;
}
constexpr bool isRgb(Color c)
{
    return (c >> 24) == 2;
}
constexpr uint8_t colorIndex(Color c)
{
    return c & 0xFF;
}

enum Attr : uint16_t
{
    Bold = 1 << 0,
    Italic = 1 << 1,
    Blink = 1 << 2,
    Reverse = 1 << 3,
    Conceal = 1 << 4,
    Strike = 1 << 5,
    Small = 1 << 6,
    UnderlineMask = 3 << 7, // 0 none, 1 single, 2 double, 3 curly
    Faint = 1 << 9,         // set by the patched libvterm (third_party/libvterm/PATCHES.md)
    Overline = 1 << 10,
};
constexpr int kUnderlineShift = 7;
constexpr int underlineOf(uint16_t attrs)
{
    return (attrs & UnderlineMask) >> kUnderlineShift;
}

/** A character value from this one up is an index into VtCore's cluster table. */
constexpr char32_t kClusterBase = 0x110000;

struct Cell
{
    char32_t ch = 0;   // 0: empty; else a code point or kClusterBase + cluster index
    Color fg = kDefaultColor;
    Color bg = kDefaultColor;
    uint16_t attrs = 0;
    uint8_t width = 1; // 1, 2, or 0 for the right half of a wide character

    bool isBlank() const { return ch == 0 || ch == U' '; }
};

} // namespace vt
