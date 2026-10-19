#pragma once

#include <QImage>

/**
 * The glyphs the terminal draws itself, as the original's `builtinGlyphs`: the box drawing
 * characters (U+2500..U+257F) and the block elements (U+2580..U+259F), made of rectangles on the
 * pixel grid of the cell, so they meet the next cell without a seam whatever the font and the scale.
 */
namespace BuiltinGlyphs
{
bool handles(char32_t ch);
/** Draws `ch` as white coverage into `cell` (the whole cell: cell.width() x cell.height()), which starts
 *  transparent. `thickness` is the light line's width in pixels. False when `ch` is not one of ours. */
bool draw(QImage &cell, char32_t ch, int thickness);
}
