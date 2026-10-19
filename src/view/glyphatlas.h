#pragma once

#include <QFont>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QSize>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

#include <memory>

/**
 * The terminal's glyph cache: every distinct (character, style, width) is drawn
 * once, into a cell-sized slot of a premultiplied-RGBA image (a "page"), and
 * the view then draws each cell as a textured quad. Text is stored as white
 * coverage and tinted on the GPU; colour glyphs (emoji) are stored as they are.
 *
 * All sizes are in physical pixels: the cell is a whole number of them, so the
 * grid is crisp at any display scale.
 *
 * One atlas serves every view with the same font, size and glyph options (shared()):
 * the panes of a window, and the windows of the process. Its font and metrics never
 * change after creation; the glyph cache does, under lock() (each window has its own
 * render thread). A page starts low and doubles in height as glyphs come; past a few
 * pages the cache starts over (epoch() changes), so what a program prints cannot grow
 * it without end.
 */
class GlyphAtlas
{
public:
    enum Style { Regular = 0, Bold = 1, Italic = 2, BoldItalic = 3 };

    struct Metrics
    {
        int cellWidth = 9;
        int cellHeight = 19;
        int baseline = 15;          // from the top of the cell
        int underlinePos = 17;      // top of the underline, from the top of the cell
        int lineThickness = 1;
        int boxThickness = 1;       // the light stroke of the box-drawing glyphs
        int strikePos = 9;
    };

    /** Where a glyph is: a page and a rectangle in its pixels (the page may grow; the rectangle stays). */
    struct Glyph
    {
        int page = -1; // -1: nothing to draw
        int x = 0, y = 0, w = 0, h = 0;
        bool color = false;
    };

    /** How big a cell of `family` at `pixelSize` is (Windows Terminal's rules, measured). */
    static Metrics measure(const QString &family, int pixelSize, double exactPixelSize = 0, double lineHeight = 0,
                           double cellWidth = 0, int fontWeight = 400, const QVariantMap &fontAxes = {},
                           const QVariantMap &fontFeatures = {});
    static int weightValue(const QVariant &setting);

    /** The atlas for this font and options, made on first use and shared while someone holds it. */
    static std::shared_ptr<GlyphAtlas> shared(const QString &family, int pixelSize, double exactPixelSize, bool builtinGlyphs,
                                              double lineHeight, double cellWidth, int fontWeight, const QVariantMap &fontAxes,
                                              const QVariantMap &fontFeatures, bool useColorEmoji);

    GlyphAtlas(const QString &family, int pixelSize, double exactPixelSize, bool builtinGlyphs, double lineHeight, double cellWidth,
               int fontWeight, const QVariantMap &fontAxes, const QVariantMap &fontFeatures, bool useColorEmoji);

    const Metrics &metrics() const { return m_metrics; }
    bool builtinGlyphs() const { return m_builtin; }
    bool useColorEmoji() const { return m_useColorEmoji; }

    // --- The cache: everything below is called with lock() held ------------------------
    QMutex &lock() const { return m_lock; }
    /** Starts over when the pages passed the limit (call between frames, never in the middle of one). */
    void trim();
    /** Bumps whenever the cache starts over: glyphs and pages from before are gone. */
    quint64 epoch() const { return m_epoch; }
    /** The cached glyph for `key`, or null (then insert() it). */
    const Glyph *find(quint64 key) const
    {
        const auto it = m_glyphs.constFind(key);
        return it == m_glyphs.constEnd() ? nullptr : &it.value();
    }
    /** Draws `text` (the character `key` names) and caches it. */
    Glyph insert(quint64 key, const QString &text, Style style, int cells);
    static quint64 makeKey(char32_t ch, Style style, int cells)
    {
        return quint64(ch) | (quint64(style) << 32) | (quint64(cells) << 35);
    }

    int pageCount() const { return int(m_pages.size()); }
    /** The page's pixels (implicitly shared: a copy is cheap, and stays as it was). */
    const QImage &pageImage(int page) const { return m_pages[page].image; }
    /** Changes whenever the page's pixels do (to know when to upload it again). */
    quint64 pageVersion(int page) const { return m_pages[page].version; }

private:
    struct Page
    {
        QImage image;
        quint64 version = 1;
    };
    Glyph rasterize(const QString &text, Style style, int cells);
    void allocate(int width, int &page, int &x, int &y);

    const bool m_builtin;
    const bool m_useColorEmoji;
    const QString m_family;
    const int m_pixelSize;
    const int m_fontWeight;
    const QVariantMap m_fontAxes;
    const QVariantMap m_fontFeatures;
    const Metrics m_metrics;
    QFont m_fonts[4];
    int m_pageWidth = 1024;
    int m_maxPageHeight = 1024;

    mutable QMutex m_lock;
    QHash<quint64, Glyph> m_glyphs;
    QVector<Page> m_pages;
    int m_cursorX = 0;
    int m_cursorY = 0;
    quint64 m_epoch = 1;
};
