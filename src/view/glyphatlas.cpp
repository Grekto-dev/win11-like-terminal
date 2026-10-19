#include "glyphatlas.h"

#include <QFontMetricsF>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

#include "builtinglyphs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
// Pages the cache may hold before it starts over (a full page is 4 MiB, 16 MiB at a large scale).
constexpr int kMaxPages = 4;

QFont makeFont(const QString &family, int pixelSize, int weight, bool bold, bool italic,
               const QVariantMap &fontAxes, const QVariantMap &fontFeatures)
{
    QFont font(family);
    font.setPixelSize(pixelSize);
    font.setWeight(static_cast<QFont::Weight>(std::clamp(weight, 100, 990)));
    if (bold) {
        font.setWeight(static_cast<QFont::Weight>(std::max(weight, int(QFont::Bold))));
    }
    font.setItalic(italic);
    // Grayscale anti-aliasing (the profile's default), no snapping of the
    // outlines to whole pixels horizontally: the cell already is one.
    font.setStyleStrategy(QFont::StyleStrategy(QFont::PreferAntialias | QFont::NoSubpixelAntialias));
    // Full hinting: DirectWrite runs Cascadia's own TrueType instructions, and this
    // is the closest to its pixels (measured against the VM: mean coverage error
    // 1.4% against 3.5% unhinted and 6.6% with vertical-only hinting).
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setKerning(false);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    for (auto it = fontAxes.cbegin(); it != fontAxes.cend(); ++it) {
        const auto tag = QFont::Tag::fromString(it.key());
        bool ok = false;
        const double value = it.value().toDouble(&ok);
        if (tag && ok && std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max()) {
            font.setVariableAxis(*tag, float(value));
        }
    }
    for (auto it = fontFeatures.cbegin(); it != fontFeatures.cend(); ++it) {
        const auto tag = QFont::Tag::fromString(it.key());
        bool ok = false;
        const double number = it.value().toDouble(&ok);
        if (tag && ok && std::isfinite(number) && number >= 0 && number <= std::numeric_limits<uint>::max()
            && std::floor(number) == number) {
            font.setFeature(*tag, static_cast<uint>(number));
        }
    }
#else
    Q_UNUSED(fontAxes);
    Q_UNUSED(fontFeatures);
#endif
    return font;
}
}

GlyphAtlas::Metrics GlyphAtlas::measure(const QString &family, int pixelSize, double exactPixelSize, double lineHeight,
                                        double cellWidth, int fontWeight, const QVariantMap &fontAxes,
                                        const QVariantMap &fontFeatures)
{
    const QFont font = makeFont(family, pixelSize, fontWeight, false, false, fontAxes, fontFeatures);
    const QFontMetricsF fm(font);
    Metrics m;
    // Measured on the VM (13 sizes, 7 to 24pt): the cell comes from the font at its exact,
    // fractional size (pt * 96 / 72), not from the whole-pixel size we draw with; the width and the
    // height both round. So the metrics are taken from a large reference size and scaled.
    qreal advance = fm.horizontalAdvance(QLatin1Char('0'));
    qreal ascent = fm.ascent(), descent = fm.descent();
    qreal underlinePos = fm.underlinePos(), strikePos = fm.strikeOutPos();
    if (exactPixelSize > 0) {
        const int ref = 2048;
        const QFontMetricsF rf(makeFont(family, ref, fontWeight, false, false, fontAxes, fontFeatures));
        const qreal k = exactPixelSize / ref;
        advance = rf.horizontalAdvance(QLatin1Char('0')) * k;
        ascent = rf.ascent() * k;
        descent = rf.descent() * k;
        underlinePos = rf.underlinePos() * k;
        strikePos = rf.strikeOutPos() * k;
    }
    const qreal fontSize = exactPixelSize > 0 ? exactPixelSize : pixelSize;
    const qreal widthOverride = std::isfinite(cellWidth) ? std::clamp(cellWidth, 0.0, 10.0) : 0.0;
    const qreal heightOverride = std::isfinite(lineHeight) ? std::clamp(lineHeight, 0.0, 10.0) : 0.0;
    m.cellWidth = std::max(1, int(std::lround(widthOverride > 0 ? fontSize * widthOverride : advance)));
    const qreal content = ascent + descent;
    m.cellHeight = std::max(1, int(std::lround(heightOverride > 0 ? fontSize * heightOverride : content)));
    m.baseline = std::clamp(int(std::lround(ascent + (m.cellHeight - content) / 2)), 0, m.cellHeight - 1);
    m.lineThickness = std::max(1, int(std::lround(fm.lineWidth())));
    // Box-drawing strokes (measured): round(size in whole pixels / 10), at least 1; the heavy
    // stroke is twice that and the double one has three bands of it.
    m.boxThickness = std::max(1, int(std::lround(pixelSize / 10.0)));
    m.underlinePos = std::clamp(m.baseline + std::max(1, int(std::lround(underlinePos))), 0,
                                 std::max(0, m.cellHeight - m.lineThickness));
    m.strikePos = std::clamp(m.baseline - int(std::lround(strikePos)), 0, m.cellHeight - 1);
    return m;
}

int GlyphAtlas::weightValue(const QVariant &setting)
{
    bool ok = false;
    const int numeric = setting.toInt(&ok);
    if (ok) {
        return std::clamp(numeric, 100, 990);
    }
    const QString name = setting.toString().toLower();
    if (name == QLatin1String("thin")) return 100;
    if (name == QLatin1String("extra-light")) return 200;
    if (name == QLatin1String("light")) return 300;
    if (name == QLatin1String("semi-light")) return 350;
    if (name == QLatin1String("medium")) return 500;
    if (name == QLatin1String("semi-bold")) return 600;
    if (name == QLatin1String("bold")) return 700;
    if (name == QLatin1String("extra-bold")) return 800;
    if (name == QLatin1String("black")) return 900;
    if (name == QLatin1String("extra-black")) return 950;
    return 400;
}

std::shared_ptr<GlyphAtlas> GlyphAtlas::shared(const QString &family, int pixelSize, double exactPixelSize, bool builtinGlyphs,
                                               double lineHeight, double cellWidth, int fontWeight, const QVariantMap &fontAxes,
                                               const QVariantMap &fontFeatures, bool useColorEmoji)
{
    static QMutex registryLock;
    static QHash<QString, std::weak_ptr<GlyphAtlas>> registry;
    QJsonObject optionObject;
    optionObject.insert(QLatin1String("axes"), QJsonObject::fromVariantMap(fontAxes));
    optionObject.insert(QLatin1String("features"), QJsonObject::fromVariantMap(fontFeatures));
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9")
                            .arg(family).arg(pixelSize).arg(exactPixelSize, 0, 'g', 12).arg(builtinGlyphs)
                            .arg(lineHeight, 0, 'g', 12).arg(cellWidth, 0, 'g', 12).arg(fontWeight)
                            .arg(useColorEmoji).arg(QString::fromUtf8(QJsonDocument(optionObject).toJson(QJsonDocument::Compact)));
    QMutexLocker locker(&registryLock);
    if (std::shared_ptr<GlyphAtlas> atlas = registry.value(key).lock()) {
        return atlas;
    }
    // Forget the ones nobody holds any more (a font size that was zoomed through).
    for (auto it = registry.begin(); it != registry.end();) {
        it = it.value().expired() ? registry.erase(it) : std::next(it);
    }
    auto atlas = std::make_shared<GlyphAtlas>(family, pixelSize, exactPixelSize, builtinGlyphs, lineHeight, cellWidth,
                                              fontWeight, fontAxes, fontFeatures, useColorEmoji);
    registry.insert(key, atlas);
    return atlas;
}

GlyphAtlas::GlyphAtlas(const QString &family, int pixelSize, double exactPixelSize, bool builtinGlyphs, double lineHeight,
                       double cellWidth, int fontWeight, const QVariantMap &fontAxes, const QVariantMap &fontFeatures,
                       bool useColorEmoji)
    : m_builtin(builtinGlyphs)
    , m_useColorEmoji(useColorEmoji)
    , m_family(family)
    , m_pixelSize(pixelSize)
    , m_fontWeight(std::clamp(fontWeight, 100, 990))
    , m_fontAxes(fontAxes)
    , m_fontFeatures(fontFeatures)
    , m_metrics(measure(family, pixelSize, exactPixelSize, lineHeight, cellWidth, m_fontWeight, fontAxes, fontFeatures))
{
    for (int s = 0; s < 4; ++s) {
        m_fonts[s] = makeFont(family, pixelSize, m_fontWeight, s & Bold, s & Italic, m_fontAxes, m_fontFeatures);
    }
    // Pages wide enough for a few hundred glyphs a row whatever the scale; they start low and grow.
    m_pageWidth = m_metrics.cellWidth * m_metrics.cellHeight > 400 ? 2048 : 1024;
    m_maxPageHeight = m_pageWidth;
}

void GlyphAtlas::trim()
{
    if (m_pages.size() > kMaxPages) {
        m_glyphs.clear();
        m_glyphs.squeeze();
        m_pages.clear();
        m_cursorX = m_cursorY = 0;
        ++m_epoch;
    }
}

GlyphAtlas::Glyph GlyphAtlas::insert(quint64 key, const QString &text, Style style, int cells)
{
    const Glyph g = rasterize(text, style, cells);
    m_glyphs.insert(key, g);
    return g;
}

void GlyphAtlas::allocate(int width, int &page, int &x, int &y)
{
    const int h = m_metrics.cellHeight;
    if (!m_pages.isEmpty() && m_cursorX + width > m_pageWidth) {
        m_cursorX = 0;
        m_cursorY += h;
    }
    if (!m_pages.isEmpty() && m_cursorY + h > m_pages.last().image.height()) {
        QImage &image = m_pages.last().image;
        if (image.height() < m_maxPageHeight) {
            // Grow the page (twice as high): the glyphs keep their pixel positions.
            const int height = std::min(m_maxPageHeight, std::max(image.height() * 2, m_cursorY + h));
            QImage bigger(m_pageWidth, height, QImage::Format_ARGB32_Premultiplied);
            bigger.fill(Qt::transparent);
            for (int row = 0; row < image.height(); ++row) {
                std::memcpy(bigger.scanLine(row), image.constScanLine(row), size_t(image.bytesPerLine()));
            }
            image = bigger;
            ++m_pages.last().version;
        }
    }
    if (m_pages.isEmpty() || m_cursorY + h > m_pages.last().image.height()) {
        Page p;
        p.image = QImage(m_pageWidth, std::min(m_maxPageHeight, std::max(64, h * 4)), QImage::Format_ARGB32_Premultiplied);
        p.image.fill(Qt::transparent);
        m_pages.push_back(p);
        m_cursorX = 0;
        m_cursorY = 0;
    }
    page = int(m_pages.size()) - 1;
    x = m_cursorX;
    y = m_cursorY;
    m_cursorX += width;
}

GlyphAtlas::Glyph GlyphAtlas::rasterize(const QString &text, Style style, int cells)
{
    const int w = std::min(m_metrics.cellWidth * std::max(1, cells), m_pageWidth);
    const int h = m_metrics.cellHeight;
    QImage cell(w, h, QImage::Format_ARGB32_Premultiplied);
    cell.fill(Qt::transparent);
    const char32_t first = text.isEmpty() ? 0 : QStringView(text).toUcs4().first();
    const bool builtin = m_builtin && cells == 1 && text.size() == 1 && BuiltinGlyphs::handles(first)
                         && BuiltinGlyphs::draw(cell, first, m_metrics.boxThickness);
    if (!builtin) {
        QPainter p(&cell);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.setFont(m_fonts[style]);
        p.setPen(Qt::white);
        p.drawText(QPointF(0, m_metrics.baseline), text);
    }

    bool any = false;
    bool color = false;
    for (int y = 0; y < h && !color; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(cell.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0) {
                continue;
            }
            any = true;
            if (qRed(px) != a || qGreen(px) != a || qBlue(px) != a) {
                color = true;
                break;
            }
        }
    }
    if (color && !m_useColorEmoji) {
        for (int y = 0; y < h; ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(cell.scanLine(y));
            for (int x = 0; x < w; ++x) {
                const int a = qAlpha(line[x]);
                if (a) {
                    line[x] = qRgba(a, a, a, a);
                }
            }
        }
        color = false;
    }
    // Contrast: DirectWrite's grayscale text is heavier than FreeType's. A coverage
    // curve of 0.75 minimises the error against the VM (mean 0.7% of full coverage).
    static const double gamma = qEnvironmentVariable("WIN11TERMINAL_GAMMA", QStringLiteral("0.75")).toDouble();
    static const std::array<uchar, 256> curve = [] { // once, not a pow() per pixel
        std::array<uchar, 256> table{};
        for (int a = 0; a < 256; ++a) {
            table[size_t(a)] = uchar(std::lround(255.0 * std::pow(a / 255.0, gamma)));
        }
        return table;
    }();
    if (!color && !builtin && gamma != 1.0) {
        for (int y = 0; y < h; ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(cell.scanLine(y));
            for (int x = 0; x < w; ++x) {
                const int a = qAlpha(line[x]);
                if (a) {
                    const int v = curve[size_t(a)];
                    line[x] = qRgba(v, v, v, v);
                }
            }
        }
    }
    Glyph g;
    if (!any) {
        return g; // a glyph with no ink (a space the font draws blank)
    }
    int page = 0, x = 0, y = 0;
    allocate(w, page, x, y);
    {
        QPainter p(&m_pages[page].image);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(x, y, cell);
    }
    ++m_pages[page].version;
    g.page = page;
    g.color = color;
    g.x = x;
    g.y = y;
    g.w = w;
    g.h = h;
    return g;
}
