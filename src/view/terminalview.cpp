#include "terminalview.h"

#include "glyphmaterial.h"
#include "terminalsession.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QJsonObject>
#include <QKeyEvent>
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QMutexLocker>
#include <QSGOpacityNode>
#include <QSGTexture>
#include <QSGVertexColorMaterial>
#include <QStyleHints>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>

using vt::Cell;

namespace
{
// Windows Terminal reserves this much on the right for its scrollbar (measured:
// a 120-column window is 8 + 1080 + 8 + 17 wide).
constexpr int kScrollbarWidth = 17;
// Matches a search keeps (searching "e" through a full history must not take the memory, nor
// the time of every frame).
constexpr size_t kMaxMatches = 100000;

/**
 * A link printed by a program, Ctrl+clicked: web links open; a file:// one only when it is a
 * folder or a plain file that the default program would show (never something that runs:
 * an executable, a .desktop launcher).
 */
bool safeToOpen(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https") || scheme == QLatin1String("ftp")) {
        return true;
    }
    if (scheme != QLatin1String("file")) {
        return false;
    }
    const QFileInfo info(url.toLocalFile());
    if (info.isDir()) {
        return true;
    }
    return info.isFile() && !info.isExecutable() && info.suffix().compare(QLatin1String("desktop"), Qt::CaseInsensitive) != 0;
}

class ColorQuads
{
public:
    void rect(float x, float y, float w, float h, const QColor &c)
    {
        if (w <= 0 || h <= 0) {
            return;
        }
        const int a = c.alpha();
        const uchar r = uchar(c.red() * a / 255), g = uchar(c.green() * a / 255), b = uchar(c.blue() * a / 255);
        using P = QSGGeometry::ColoredPoint2D;
        const P p00{x, y, r, g, b, uchar(a)}, p10{x + w, y, r, g, b, uchar(a)};
        const P p01{x, y + h, r, g, b, uchar(a)}, p11{x + w, y + h, r, g, b, uchar(a)};
        m_vertices << p00 << p10 << p01 << p10 << p11 << p01;
    }
    const QVector<QSGGeometry::ColoredPoint2D> &vertices() const { return m_vertices; }
    void clear() { m_vertices.resize(0); } // keeps the capacity

private:
    QVector<QSGGeometry::ColoredPoint2D> m_vertices;
};

QSGGeometryNode *makeColorNode()
{
    auto *node = new QSGGeometryNode;
    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(new QSGVertexColorMaterial);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

void fillColorNode(QSGGeometryNode *node, const ColorQuads &quads)
{
    QSGGeometry *geometry = node->geometry();
    const auto &v = quads.vertices();
    geometry->allocate(int(v.size()));
    if (!v.isEmpty()) {
        memcpy(geometry->vertexData(), v.constData(), size_t(v.size()) * sizeof(QSGGeometry::ColoredPoint2D));
    }
    node->markDirty(QSGNode::DirtyGeometry);
}

const QSGGeometry::AttributeSet &glyphAttributes()
{
    static const QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
        QSGGeometry::Attribute::createWithAttributeType(2, 4, QSGGeometry::UnsignedByteType, QSGGeometry::ColorAttribute),
    };
    static const QSGGeometry::AttributeSet set = {3, sizeof(GlyphVertex), attributes};
    return set;
}

/** The scene graph subtree of one view; owns the GPU textures of the atlas pages. */
class TerminalNode : public QSGNode
{
public:
    struct PageNode
    {
        QSGGeometryNode *node = nullptr;
        GlyphMaterial *material = nullptr;
        QSGTexture *texture = nullptr;
        quint64 version = 0;
    };

    ~TerminalNode() override
    {
        for (PageNode &page : pages) {
            delete page.texture;
        }
    }

    void dropPages()
    {
        for (PageNode &page : pages) {
            removeChildNode(page.node);
            delete page.node;
            delete page.texture;
        }
        pages.clear();
    }

    QSGGeometryNode *background = nullptr;
    QSGGeometryNode *decorations = nullptr;
    QVector<PageNode> pages;
    // The cursor, over everything: the cell as the cursor shows it, and the cursor's shape. Its
    // own layer, so that the blink only flips this opacity instead of rebuilding every cell.
    QSGOpacityNode *cursorLayer = nullptr;
    QSGGeometryNode *cursorBack = nullptr;
    QSGGeometryNode *cursorGlyph = nullptr;
    GlyphMaterial *cursorMaterial = nullptr;
    QSGGeometryNode *cursorFront = nullptr;
    // The atlas (and its epoch) the textures come from: another one, and they are dropped.
    std::shared_ptr<GlyphAtlas> atlas;
    quint64 epoch = 0;
    // Reused from frame to frame (no allocation per frame once they are big enough).
    ColorQuads bg, deco, cursorBg, cursorFg;
    std::vector<std::vector<GlyphVertex>> glyphs;
    std::vector<GlyphVertex> cursorVertices;
    int cursorPage = -1;
};

// WCAG contrast ratio; the selected text keeps its colour when it reads on the selection,
// else it turns black or white (measured: #CCCCCC on the white selection is drawn #000000).
qreal linearChannel(int value)
{
    static const std::array<qreal, 256> table = [] {
        std::array<qreal, 256> values{};
        for (int i = 0; i < 256; ++i) {
            const qreal v = qreal(i) / 255.0;
            values[size_t(i)] = v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
        }
        return values;
    }();
    return table[size_t(std::clamp(value, 0, 255))];
}

qreal luminance(const QColor &c)
{
    return 0.2126 * linearChannel(c.red()) + 0.7152 * linearChannel(c.green()) + 0.0722 * linearChannel(c.blue());
}

QColor readableOn(const QColor &fg, const QColor &bg, qreal bgLuminance)
{
    const qreal a = luminance(fg), b = bgLuminance;
    const qreal ratio = (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
    if (ratio >= 3.0) {
        return fg;
    }
    return b > 0.5 ? QColor(Qt::black) : QColor(Qt::white);
}

QColor scaled(const QColor &c, qreal f)
{
    return QColor(int(c.red() * f), int(c.green() * f), int(c.blue() * f));
}

VTermModifier modifiersOf(Qt::KeyboardModifiers m)
{
    int mods = VTERM_MOD_NONE;
    if (m & Qt::ShiftModifier) {
        mods |= VTERM_MOD_SHIFT;
    }
    if (m & Qt::AltModifier) {
        mods |= VTERM_MOD_ALT;
    }
    if (m & Qt::ControlModifier) {
        mods |= VTERM_MOD_CTRL;
    }
    return VTermModifier(mods);
}

VTermKey specialKey(int key, Qt::KeyboardModifiers modifiers)
{
    switch (key) {
    case Qt::Key_Return:
        return VTERM_KEY_ENTER;
    case Qt::Key_Enter:
        return (modifiers & Qt::KeypadModifier) ? VTERM_KEY_KP_ENTER : VTERM_KEY_ENTER;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return VTERM_KEY_TAB;
    case Qt::Key_Backspace:
        return VTERM_KEY_BACKSPACE;
    case Qt::Key_Escape:
        return VTERM_KEY_ESCAPE;
    case Qt::Key_Up:
        return VTERM_KEY_UP;
    case Qt::Key_Down:
        return VTERM_KEY_DOWN;
    case Qt::Key_Left:
        return VTERM_KEY_LEFT;
    case Qt::Key_Right:
        return VTERM_KEY_RIGHT;
    case Qt::Key_Insert:
        return VTERM_KEY_INS;
    case Qt::Key_Delete:
        return VTERM_KEY_DEL;
    case Qt::Key_Home:
        return VTERM_KEY_HOME;
    case Qt::Key_End:
        return VTERM_KEY_END;
    case Qt::Key_PageUp:
        return VTERM_KEY_PAGEUP;
    case Qt::Key_PageDown:
        return VTERM_KEY_PAGEDOWN;
    default:
        break;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35) {
        return VTermKey(VTERM_KEY_FUNCTION(key - Qt::Key_F1 + 1));
    }
    return VTERM_KEY_NONE;
}
} // namespace

TerminalView::TerminalView(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setFlag(ItemIsFocusScope, false);
    setFlag(ItemAcceptsInputMethod, true);
    setActiveFocusOnTab(true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setCursor(Qt::IBeamCursor);
    setAcceptHoverEvents(true);

    m_autoScroll.setInterval(50);
    connect(&m_autoScroll, &QTimer::timeout, this, &TerminalView::autoScrollTick);
    connect(&m_blink, &QTimer::timeout, this, [this] {
        // Only the cursor layer changes: no rebuild of the cells (m_fullDirty stays as it is).
        m_cursorOn = !m_cursorOn;
        update();
    });
}

TerminalView::~TerminalView() = default;

VtCore *TerminalView::core() const
{
    return m_session ? m_session->core() : nullptr;
}

qreal TerminalView::dpr() const
{
    return window() ? window()->effectiveDevicePixelRatio() : 1.0;
}

qreal TerminalView::cellWidth() const
{
    return metrics().cellWidth / dpr();
}

qreal TerminalView::cellHeight() const
{
    return metrics().cellHeight / dpr();
}

void TerminalView::setSession(TerminalSession *session)
{
    if (m_session == session) {
        return;
    }
    if (m_session) {
        m_session->disconnect(this);
        if (m_session->core()) {
            m_session->core()->disconnect(this);
        }
    }
    m_session = session;
    m_scrollOffset = 0;
    if (m_session) {
        VtCore *c = m_session->core();
        m_lastHistory = c->historyCount();
        m_lastDropped = c->scrolledOff();
        connect(c, &VtCore::contentChanged, this, &TerminalView::onContentChanged);
        connect(c, &VtCore::modesChanged, this, [this] {
            restartBlink();
            repaint();
        });
        m_sessionSized = false;
        if (isComponentComplete() && width() >= 1 && height() >= 1) {
            m_sessionSized = true;
            m_session->resize(m_columns, m_rows);
        }
    }
    restartBlink();
    repaint();
    Q_EMIT sessionChanged();
}

void TerminalView::setFontFamily(const QString &family)
{
    if (m_fontFamily != family) {
        m_fontFamily = family;
        rebuildFont();
        Q_EMIT fontChanged();
    }
}

void TerminalView::setFontSize(qreal points)
{
    if (!qFuzzyCompare(m_fontSize, points) && points > 0) {
        m_fontSize = points;
        rebuildFont();
        Q_EMIT fontChanged();
    }
}

void TerminalView::setLineHeight(qreal value)
{
    if (!std::isfinite(value)) return;
    value = std::clamp(value, 0.0, 10.0);
    if (!qFuzzyCompare(m_lineHeight, value)) {
        m_lineHeight = value;
        rebuildFont();
        Q_EMIT fontChanged();
    }
}

void TerminalView::setCellWidthOverride(qreal value)
{
    if (!std::isfinite(value)) return;
    value = std::clamp(value, 0.0, 10.0);
    if (!qFuzzyCompare(m_cellWidthOverride, value)) {
        m_cellWidthOverride = value;
        rebuildFont();
        Q_EMIT fontChanged();
    }
}

void TerminalView::setFontWeight(const QVariant &value)
{
    if (m_fontWeight != value) {
        m_fontWeight = value;
        rebuildFont();
        Q_EMIT fontChanged();
    }
}

void TerminalView::setFontAxes(const QVariantMap &value)
{
    if (m_fontAxes != value) {
        m_fontAxes = value;
        rebuildFont();
        Q_EMIT fontChanged();
    }
}

void TerminalView::setFontFeatures(const QVariantMap &value)
{
    if (m_fontFeatures != value) {
        m_fontFeatures = value;
        rebuildFont();
        Q_EMIT fontChanged();
    }
}

void TerminalView::setUseColorEmoji(bool on)
{
    if (m_useColorEmoji != on) {
        m_useColorEmoji = on;
        rebuildFont();
        Q_EMIT fontChanged();
    }
}

void TerminalView::setIntenseTextStyle(const QString &value)
{
    const QString style = value == QLatin1String("none") || value == QLatin1String("bold")
                              || value == QLatin1String("bright") || value == QLatin1String("all")
                          ? value : QStringLiteral("bright");
    if (m_intenseTextStyle != style) {
        m_intenseTextStyle = style;
        repaint();
        Q_EMIT textStyleChanged();
    }
}

void TerminalView::setAdjustIndistinguishableColors(const QString &value)
{
    const QString mode = value == QLatin1String("never") || value == QLatin1String("always")
                             ? value : QStringLiteral("indexed");
    if (m_adjustIndistinguishableColors != mode) {
        m_adjustIndistinguishableColors = mode;
        repaint();
        Q_EMIT textStyleChanged();
    }
}

QMargins TerminalView::paddingPx() const
{
    const qreal d = dpr();
    return QMargins(int(std::lround(m_padLeft * d)), int(std::lround(m_padTop * d)), int(std::lround(m_padRight * d)),
                    int(std::lround(m_padBottom * d)));
}

void TerminalView::setPadding(const QString &padding)
{
    if (m_paddingText != padding) {
        m_paddingText = padding;
        const QStringList parts = padding.split(QLatin1Char(','), Qt::SkipEmptyParts);
        QList<int> v;
        for (const QString &part : parts) {
            v << std::max(0, part.trimmed().toInt());
        }
        if (v.size() == 1) {
            m_padLeft = m_padTop = m_padRight = m_padBottom = v[0];
        } else if (v.size() == 2) {
            m_padLeft = m_padRight = v[0];
            m_padTop = m_padBottom = v[1];
        } else if (v.size() >= 4) {
            m_padLeft = v[0]; m_padTop = v[1]; m_padRight = v[2]; m_padBottom = v[3];
        }
        recomputeGrid();
        repaint();
    }
}

void TerminalView::setCursorShape(const QString &shape)
{
    if (m_cursorShape != shape) {
        m_cursorShape = shape;
        repaint();
        Q_EMIT cursorShapeChanged();
    }
}

void TerminalView::setColorScheme(const QVariantMap &scheme)
{
    m_scheme = ColorScheme::fromJson(QJsonObject::fromVariantMap(scheme));
    repaint();
    Q_EMIT schemeChanged();
}

void TerminalView::componentComplete()
{
    QQuickItem::componentComplete();
    rebuildFont();
    if (m_session && width() >= 1 && height() >= 1) {
        m_sessionSized = true;
        m_session->resize(m_columns, m_rows);
    }
}

void TerminalView::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickItem::itemChange(change, value);
    if ((change == ItemDevicePixelRatioHasChanged || change == ItemSceneChange) && isComponentComplete()
        && !qFuzzyCompare(m_atlasDpr, dpr())) {
        rebuildFont();
    }
    if (change == ItemActiveFocusHasChanged) {
        if (VtCore *c = core()) {
            c->focus(hasActiveFocus());
        }
        restartBlink();
        repaint();
    }
}

void TerminalView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (isComponentComplete() && newGeometry.size() != oldGeometry.size()) {
        recomputeGrid();
    }
}

void TerminalView::setBuiltinGlyphs(bool on)
{
    if (m_builtinGlyphs == on) {
        return;
    }
    m_builtinGlyphs = on;
    if (isComponentComplete()) {
        rebuildFont();
    }
}

void TerminalView::rebuildFont()
{
    const qreal d = dpr();
    const double exact = m_fontSize * 96.0 / 72.0 * d;
    const int pixels = std::max(1, int(std::lround(exact)));
    // Shared with every other view of this font, size and scale (one cache, one rasterization).
    m_atlas = GlyphAtlas::shared(m_fontFamily, pixels, exact, m_builtinGlyphs, m_lineHeight, m_cellWidthOverride,
                                 GlyphAtlas::weightValue(m_fontWeight), m_fontAxes, m_fontFeatures, m_useColorEmoji);
    m_atlasDpr = d;
    recomputeGrid();
    repaint();
}

void TerminalView::recomputeGrid()
{
    const qreal d = dpr();
    const GlyphAtlas::Metrics &m = metrics();
    const QMargins pad = paddingPx();
    const int columns = std::max(1, int((width() * d - pad.left() - pad.right() - std::lround(kScrollbarWidth * d)) / m.cellWidth));
    const int rows = std::max(1, int((height() * d - pad.top() - pad.bottom()) / m.cellHeight));
    const bool changed = columns != m_columns || rows != m_rows;
    m_columns = columns;
    m_rows = rows;
    // Never start (or resize) a program for an item that has no size yet: it would
    // see a one-column terminal for a moment.
    const bool sized = width() >= 1 && height() >= 1;
    if ((changed || !m_sessionSized) && sized && m_session && isComponentComplete()) {
        m_sessionSized = true;
        m_session->resize(columns, rows);
    }
    updateWindowHints();
    Q_EMIT gridChanged();
    repaint();
}

// snapToGridOnResize: the window manager resizes in whole cells when told the
// increments (WM_NORMAL_HINTS), with the chrome and padding as the base size.
void TerminalView::updateWindowHints()
{
    QQuickWindow *w = window();
    if (!w || !m_snapToGrid || !isComponentComplete() || m_columns < 1) {
        return;
    }
    const qreal cw = cellWidth(), ch = cellHeight();
    const QSize base(int(std::lround(w->width() - m_columns * cw)), int(std::lround(w->height() - m_rows * ch)));
    w->setSizeIncrement(QSize(int(std::lround(cw)), int(std::lround(ch))));
    w->setBaseSize(base);
}

void TerminalView::onContentChanged()
{
    VtCore *c = core();
    if (!c) {
        return;
    }
    const int offsetBefore = m_scrollOffset, historyBefore = m_lastHistory;
    // Keep looking at the same lines while the program prints below.
    const qint64 pushed = qint64(c->historyCount()) + qint64(c->scrolledOff()) - m_lastHistory - qint64(m_lastDropped);
    if (m_scrollOffset > 0 && pushed > 0) {
        m_scrollOffset += int(pushed);
    }
    m_scrollOffset = std::clamp(m_scrollOffset, 0, c->historyCount());
    if (c->altScreen()) {
        m_scrollOffset = 0;
    }
    m_lastHistory = c->historyCount();
    m_lastDropped = c->scrolledOff();
    repaint();
    // Only when the scrollbar has something to show (not on every chunk of output).
    if (m_scrollOffset != offsetBefore || m_lastHistory != historyBefore) {
        Q_EMIT scrollChanged();
    }
}

void TerminalView::restartBlink()
{
    const int flash = QGuiApplication::styleHints()->cursorFlashTime();
    VtCore *c = core();
    m_cursorOn = true;
    if (hasActiveFocus() && flash > 0 && c && c->cursorBlink()) {
        m_blink.start(flash / 2);
    } else {
        m_blink.stop();
    }
}

void TerminalView::scrollBy(int lines)
{
    VtCore *c = core();
    if (!c) {
        return;
    }
    const int next = std::clamp(m_scrollOffset + lines, 0, c->historyCount());
    if (next != m_scrollOffset) {
        m_scrollOffset = next;
        repaint();
        Q_EMIT scrollChanged();
    }
}

void TerminalView::scrollTo(int offsetFromBottom)
{
    scrollBy(offsetFromBottom - m_scrollOffset);
}

void TerminalView::scrollToTop()
{
    if (VtCore *c = core()) {
        scrollTo(c->historyCount());
    }
}

void TerminalView::scrollToBottom()
{
    if (m_scrollOffset != 0) {
        m_scrollOffset = 0;
        repaint();
        Q_EMIT scrollChanged();
    }
}

int TerminalView::historyLines() const
{
    const VtCore *c = core();
    return c ? c->historyCount() : 0;
}

void TerminalView::clearBuffer()
{
    if (VtCore *c = core()) {
        c->clearScrollback();
        c->write(QByteArrayLiteral("\x1b[2J\x1b[H"));
        clearSelection();
        clearFind();
        m_scrollOffset = 0;
        Q_EMIT scrollChanged();
    }
}

// --- Selection ----------------------------------------------------------------------------

qint64 TerminalView::lineOf(int absRow) const
{
    return qint64(absRow) + qint64(core() ? core()->scrolledOff() : 0);
}

int TerminalView::absOf(qint64 line) const
{
    return int(line - qint64(core() ? core()->scrolledOff() : 0));
}

TerminalView::Pos TerminalView::posAt(const QPointF &p, bool boundary) const
{
    const qreal d = dpr();
    const GlyphAtlas::Metrics &m = metrics();
    const QMargins pad = paddingPx();
    const double cx = (p.x() * d - pad.left()) / m.cellWidth;
    const double cy = (p.y() * d - pad.top()) / m.cellHeight;
    const int row = std::clamp(int(std::floor(cy)), 0, m_rows - 1);
    const int col = boundary ? std::clamp(int(std::lround(cx)), 0, m_columns) : std::clamp(int(std::floor(cx)), 0, m_columns - 1);
    const VtCore *c = core();
    const int first = c ? std::max(0, c->totalRows() - m_rows - m_scrollOffset) : 0;
    return Pos{lineOf(first + row), col};
}

int TerminalView::cellClass(const vt::Cell &cell) const
{
    if (cell.ch == 0 || cell.ch == U' ') {
        return 2;
    }
    if (cell.ch < vt::kClusterBase && cell.ch < 0x10000 && m_wordDelimiters.contains(QChar(char16_t(cell.ch)))) {
        return 1;
    }
    return 0;
}

void TerminalView::wordBounds(qint64 line, int col, int &c0, int &c1) const
{
    const VtCore *c = core();
    const int row = absOf(line);
    col = std::clamp(col, 0, m_columns - 1);
    while (col > 0 && c->cell(row, col).width == 0) {
        --col; // the right half of a wide character: its left half
    }
    const int cls = cellClass(c->cell(row, col));
    c0 = col;
    while (c0 > 0 && (cellClass(c->cell(row, c0 - 1)) == cls || c->cell(row, c0 - 1).width == 0)) {
        --c0;
    }
    c1 = col + 1;
    while (c1 < m_columns && (cellClass(c->cell(row, c1)) == cls || c->cell(row, c1).width == 0)) {
        ++c1;
    }
}

bool TerminalView::selectionEmpty() const
{
    return m_selMode == SelMode::None || (m_selMode == SelMode::Char && m_selAnchor == m_selHead)
        || (m_selMode == SelMode::Block && m_selAnchor.col == m_selHead.col);
}

bool TerminalView::selectionSpan(qint64 line, int &c0, int &c1) const
{
    if (m_selMode == SelMode::None) {
        return false;
    }
    const Pos a = std::min(m_selAnchor, m_selHead), b = std::max(m_selAnchor, m_selHead);
    if (line < a.line || line > b.line) {
        return false;
    }
    switch (m_selMode) {
    case SelMode::Line:
        c0 = 0;
        c1 = m_columns;
        return true;
    case SelMode::Block:
        c0 = std::min(m_selAnchor.col, m_selHead.col);
        c1 = std::max(m_selAnchor.col, m_selHead.col);
        return c1 > c0;
    default:
        c0 = line == a.line ? a.col : 0;
        c1 = line == b.line ? b.col : m_columns;
        return c1 > c0;
    }
}

void TerminalView::extendSelection(const Pos &to)
{
    const Pos before = m_selHead;
    if (m_selMode == SelMode::Char || m_selMode == SelMode::Block) {
        m_selHead = to;
    } else if (m_selMode == SelMode::Word) {
        int c0, c1;
        wordBounds(to.line, std::min(to.col, m_columns - 1), c0, c1);
        const Pos ws{to.line, c0}, we{to.line, c1};
        if (to < m_wordStart) {
            m_selAnchor = m_wordEnd;
            m_selHead = ws;
        } else {
            m_selAnchor = m_wordStart;
            m_selHead = we;
        }
    } else if (m_selMode == SelMode::Line) {
        if (to.line < m_wordStart.line) {
            m_selAnchor = Pos{m_wordStart.line, m_columns};
            m_selHead = Pos{to.line, 0};
        } else {
            m_selAnchor = Pos{m_wordStart.line, 0};
            m_selHead = Pos{to.line, m_columns};
        }
    }
    if (!(before == m_selHead)) {
        repaint();
        Q_EMIT selectionChanged();
    }
}

void TerminalView::clearSelection()
{
    if (m_selMode != SelMode::None) {
        m_selMode = SelMode::None;
        m_dragging = false;
        m_autoScroll.stop();
        repaint();
        Q_EMIT selectionChanged();
    }
}

void TerminalView::selectAll()
{
    VtCore *c = core();
    if (!c) {
        return;
    }
    m_selMode = SelMode::Char;
    m_selAnchor = Pos{lineOf(0), 0};
    m_selHead = Pos{lineOf(c->totalRows() - 1), m_columns};
    repaint();
    Q_EMIT selectionChanged();
}

QString TerminalView::selectedText() const
{
    const VtCore *c = core();
    if (!c || selectionEmpty()) {
        return {};
    }
    const Pos a = std::min(m_selAnchor, m_selHead), b = std::max(m_selAnchor, m_selHead);
    QString out;
    for (qint64 line = a.line; line <= b.line; ++line) {
        int c0 = 0, c1 = 0;
        if (!selectionSpan(line, c0, c1)) {
            continue;
        }
        const int row = absOf(line);
        if (row < 0 || row >= c->totalRows()) {
            continue;
        }
        QString text = c->rowText(row, c0, c1);
        // The rest of a row is empty: do not copy its blanks (trimBlockSelection, and the
        // same for a stream that runs to the end of the line).
        // (A row the next one continues is cut mid-line: its last blank is a real space.)
        const bool cutMidLine = m_selMode != SelMode::Block && line < b.line && c->continuation(row + 1);
        if (!cutMidLine && (c1 >= m_columns || m_selMode == SelMode::Block || m_selMode == SelMode::Line)) {
            while (text.endsWith(QLatin1Char(' '))) {
                text.chop(1);
            }
        }
        if (line > a.line) {
            const bool joined = m_selMode != SelMode::Block && c->continuation(row);
            if (!joined) {
                out += QLatin1Char('\n');
            }
        }
        out += text;
    }
    return out;
}

bool TerminalView::exportText(const QString &path) const
{
    const VtCore *c = core();
    if (!c) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    int last = c->totalRows() - 1;
    while (last > 0 && c->rowText(last).isEmpty()) {
        --last;
    }
    QByteArray out;
    for (int r = 0; r <= last; ++r) {
        out += c->rowText(r).toUtf8();
        if (r + 1 <= last && !c->continuation(r + 1)) {
            out += '\n';
        }
    }
    out += '\n';
    return file.write(out) == out.size();
}

bool TerminalView::copy(bool keepSelection)
{
    const QString text = selectedText();
    if (text.isEmpty()) {
        return false;
    }
    QGuiApplication::clipboard()->setText(text);
    if (!keepSelection) {
        clearSelection();
    }
    return true;
}

// --- Mark mode -----------------------------------------------------------------------------

void TerminalView::toggleMarkMode()
{
    VtCore *c = core();
    if (!c) {
        return;
    }
    m_mark = !m_mark;
    m_markSelecting = false;
    if (m_mark) {
        clearSelection();
        m_markCursor = Pos{lineOf(c->historyCount() + c->cursorRow()), c->cursorCol()};
    } else {
        clearSelection();
    }
    repaint();
    Q_EMIT markModeChanged();
}

void TerminalView::moveMark(int dRow, int dCol, bool extend)
{
    VtCore *c = core();
    if (!c) {
        return;
    }
    if (extend && !m_markSelecting) {
        m_markSelecting = true;
        m_markAnchor = m_markCursor;
    } else if (!extend) {
        m_markSelecting = false;
        if (m_selMode != SelMode::None) {
            m_selMode = SelMode::None;
            Q_EMIT selectionChanged();
        }
    }
    int row = absOf(m_markCursor.line) + dRow;
    int col = m_markCursor.col + dCol;
    if (dCol != 0 && (col < 0 || col >= m_columns)) {
        // Past the end of a line: onto the neighbouring one.
        row += col < 0 ? -1 : 1;
        col = col < 0 ? m_columns - 1 : 0;
    }
    row = std::clamp(row, 0, c->totalRows() - 1);
    col = std::clamp(col, 0, m_columns - 1);
    m_markCursor = Pos{lineOf(row), col};
    if (m_markSelecting) {
        const Pos a = m_markAnchor, b = m_markCursor;
        m_selMode = SelMode::Char;
        if (!(b < a)) {
            m_selAnchor = Pos{a.line, a.col};
            m_selHead = Pos{b.line, b.col + 1};
        } else {
            m_selAnchor = Pos{a.line, a.col + 1};
            m_selHead = Pos{b.line, b.col};
        }
        Q_EMIT selectionChanged();
    }
    // Keep the mark on screen.
    const int first = std::max(0, c->totalRows() - m_rows - m_scrollOffset);
    if (row < first) {
        scrollBy(first - row);
    } else if (row >= first + m_rows) {
        scrollBy(-(row - (first + m_rows - 1)));
    }
    repaint();
}

void TerminalView::handleMarkKey(QKeyEvent *event)
{
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    switch (event->key()) {
    case Qt::Key_Escape:
        toggleMarkMode();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        copy();
        toggleMarkMode();
        break;
    case Qt::Key_Left:
        moveMark(0, -1, shift);
        break;
    case Qt::Key_Right:
        moveMark(0, 1, shift);
        break;
    case Qt::Key_Up:
        moveMark(-1, 0, shift);
        break;
    case Qt::Key_Down:
        moveMark(1, 0, shift);
        break;
    case Qt::Key_PageUp:
        moveMark(-(m_rows - 1), 0, shift);
        break;
    case Qt::Key_PageDown:
        moveMark(m_rows - 1, 0, shift);
        break;
    case Qt::Key_Home:
        if (ctrl) {
            moveMark(-core()->totalRows(), -m_columns, shift);
        } else {
            moveMark(0, -m_markCursor.col, shift);
        }
        break;
    case Qt::Key_End:
        if (ctrl) {
            moveMark(core()->totalRows(), m_columns, shift);
        } else {
            moveMark(0, m_columns - 1 - m_markCursor.col, shift);
        }
        break;
    case Qt::Key_A:
        if (ctrl) {
            selectAll();
        }
        break;
    default:
        break;
    }
    event->accept();
}

// --- Links --------------------------------------------------------------------------------------

TerminalView::Url TerminalView::urlAt(qint64 line, int col) const
{
    const VtCore *c = core();
    const int row = absOf(line);
    if (!c || row < 0 || row >= c->totalRows()) {
        return {};
    }
    QString text;
    std::vector<int> colOf;
    for (int i = 0; i < m_columns; ++i) {
        const vt::Cell cell = c->cell(row, i);
        if (cell.width == 0) {
            continue;
        }
        const QString part = cell.ch == 0 ? QStringLiteral(" ") : cell.ch >= vt::kClusterBase ? c->clusterText(cell.ch)
                                                                                               : QString::fromUcs4(&cell.ch, 1);
        for (int k = 0; k < part.size(); ++k) {
            colOf.push_back(i);
        }
        text += part;
    }
    static const QRegularExpression re(QStringLiteral("(?:https?|ftp|file)://[^\\s<>\"'`]+"));
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        QString url = m.captured();
        // A closing bracket or punctuation at the end usually belongs to the sentence.
        while (!url.isEmpty() && QStringLiteral(".,;:!?)]}>").contains(url.back())) {
            const bool balanced = url.back() == QLatin1Char(')') && url.count(QLatin1Char('(')) >= url.count(QLatin1Char(')'));
            if (balanced) {
                break;
            }
            url.chop(1);
        }
        const int start = int(m.capturedStart());
        const int end = start + int(url.size()) - 1;
        if (end < 0 || size_t(end) >= colOf.size()) {
            continue;
        }
        const int c0 = colOf[size_t(start)], c1 = colOf[size_t(end)] + 1;
        if (col >= c0 && col < c1) {
            return Url{line, c0, c1, url};
        }
    }
    return {};
}

// --- Mouse ------------------------------------------------------------------------------------

bool TerminalView::appWantsMouse(Qt::KeyboardModifiers m) const
{
    const VtCore *c = core();
    return c && c->mouseMode() != VtCore::MouseMode::None && !(m & Qt::ShiftModifier);
}

void TerminalView::forwardMouse(QMouseEvent *event, bool press, bool release)
{
    VtCore *c = core();
    const Pos p = posAt(event->position(), false);
    const int first = std::max(0, c->totalRows() - m_rows - m_scrollOffset);
    const int row = std::clamp(absOf(p.line) - first, 0, m_rows - 1);
    const VTermModifier mods = modifiersOf(event->modifiers());
    c->mouseMove(row, p.col, mods);
    if (press || release) {
        const int button = event->button() == Qt::LeftButton ? 1 : event->button() == Qt::MiddleButton ? 2 : 3;
        c->mouseButton(button, press, mods);
    }
}

void TerminalView::handlePress(QMouseEvent *event, int forcedCount)
{
    forceActiveFocus(Qt::MouseFocusReason);
    VtCore *c = core();
    if (!c) {
        return;
    }
    event->accept();
    if (appWantsMouse(event->modifiers())) {
        forwardMouse(event, true, false);
        m_dragging = false;
        return;
    }
    if (event->button() == Qt::RightButton) {
        // Right click copies a selection, or pastes, unless the menu is on (then the menu does it).
        if (m_rightClickMenu) {
            Q_EMIT contextMenuRequested(event->position());
        } else if (hasSelection()) {
            copy();
        } else {
            paste();
        }
        return;
    }
    if (event->button() == Qt::MiddleButton) {
        // The X11 convention: the primary selection.
        requestPaste(QGuiApplication::clipboard()->text(QClipboard::Selection));
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }

    int count = forcedCount;
    if (count == 0) {
        const bool near = (event->position() - m_lastClickPos).manhattanLength() < 6;
        const bool quick = m_clickTimer.isValid() && m_clickTimer.elapsed() < QGuiApplication::styleHints()->mouseDoubleClickInterval();
        count = (quick && near) ? m_clickCount + 1 : 1;
    }
    m_clickCount = std::min(count, 3);
    m_clickTimer.restart();
    m_lastClickPos = event->position();
    m_dragPos = event->position();

    const Pos boundary = posAt(event->position(), true);
    const Pos cell = posAt(event->position(), false);
    if (m_detectUrls && (event->modifiers() & Qt::ControlModifier)) {
        const Url u = urlAt(cell.line, cell.col);
        if (u.line >= 0) {
            const QUrl url = QUrl::fromUserInput(u.text);
            if (safeToOpen(url)) {
                QDesktopServices::openUrl(url);
            }
            return;
        }
    }
    if ((event->modifiers() & Qt::ShiftModifier) && m_selMode != SelMode::None) {
        m_dragging = true;
        extendSelection(boundary);
        return;
    }
    if (m_clickCount == 1) {
        m_selMode = (event->modifiers() & Qt::AltModifier) ? SelMode::Block : SelMode::Char;
        m_selAnchor = m_selHead = boundary;
    } else if (m_clickCount == 2) {
        int c0, c1;
        wordBounds(cell.line, cell.col, c0, c1);
        m_selMode = SelMode::Word;
        m_selAnchor = m_wordStart = Pos{cell.line, c0};
        m_selHead = m_wordEnd = Pos{cell.line, c1};
    } else {
        m_selMode = SelMode::Line;
        m_selAnchor = m_wordStart = Pos{cell.line, 0};
        m_selHead = m_wordEnd = Pos{cell.line, m_columns};
    }
    m_dragging = true;
    repaint();
    Q_EMIT selectionChanged();
}

void TerminalView::mousePressEvent(QMouseEvent *event)
{
    handlePress(event, 0);
}

void TerminalView::mouseDoubleClickEvent(QMouseEvent *event)
{
    // The press before it already counted the click (handlePress keeps its own count);
    // Qt sends this one as well.
    event->accept();
}

void TerminalView::mouseMoveEvent(QMouseEvent *event)
{
    VtCore *c = core();
    if (!c) {
        return;
    }
    if (appWantsMouse(event->modifiers())) {
        forwardMouse(event, false, false);
        event->accept();
        return;
    }
    if (!m_dragging || !(event->buttons() & Qt::LeftButton)) {
        return;
    }
    m_dragPos = event->position();
    const bool boundary = m_selMode == SelMode::Char || m_selMode == SelMode::Block;
    extendSelection(posAt(event->position(), boundary));
    // Past the top or bottom edge the view scrolls on its own while the button is down.
    if (event->position().y() < 0 || event->position().y() > height()) {
        if (!m_autoScroll.isActive()) {
            m_autoScroll.start();
        }
    } else {
        m_autoScroll.stop();
    }
    event->accept();
}

void TerminalView::hoverMoveEvent(QHoverEvent *event)
{
    VtCore *c = core();
    if (m_detectUrls && c && !appWantsMouse(event->modifiers())) {
        const Pos p = posAt(event->position(), false);
        const Url u = urlAt(p.line, p.col);
        if (u.line != m_hoverUrl.line || u.col0 != m_hoverUrl.col0 || u.col1 != m_hoverUrl.col1) {
            m_hoverUrl = u;
            setCursor(u.line >= 0 ? Qt::PointingHandCursor : Qt::IBeamCursor);
            repaint();
        }
    }
    if (c && c->mouseMode() == VtCore::MouseMode::Move && !(event->modifiers() & Qt::ShiftModifier)) {
        const Pos p = posAt(event->position(), false);
        const int first = std::max(0, c->totalRows() - m_rows - m_scrollOffset);
        c->mouseMove(std::clamp(absOf(p.line) - first, 0, m_rows - 1), p.col, modifiersOf(event->modifiers()));
    }
}

void TerminalView::mouseReleaseEvent(QMouseEvent *event)
{
    VtCore *c = core();
    if (!c) {
        return;
    }
    m_autoScroll.stop();
    if (appWantsMouse(event->modifiers()) && event->button() != Qt::NoButton) {
        forwardMouse(event, false, true);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        if (hasSelection()) {
            const QString text = selectedText();
            QGuiApplication::clipboard()->setText(text, QClipboard::Selection); // PRIMARY, as X terminals do
            if (m_copyOnSelect) {
                copy(true);
            }
        }
    }
    event->accept();
}

void TerminalView::autoScrollTick()
{
    if (!m_dragging) {
        m_autoScroll.stop();
        return;
    }
    const int step = m_dragPos.y() < 0 ? 1 : -1; // above: older lines
    scrollBy(step);
    const bool boundary = m_selMode == SelMode::Char || m_selMode == SelMode::Block;
    QPointF edge = m_dragPos;
    edge.setY(m_dragPos.y() < 0 ? 0 : height() - 1);
    extendSelection(posAt(edge, boundary));
}

// --- Search -------------------------------------------------------------------------------------

int TerminalView::find(const QString &text, bool matchCase, bool regex)
{
    m_matches.clear();
    m_matchIndex = -1;
    VtCore *c = core();
    if (c && !text.isEmpty()) {
        const QRegularExpression::PatternOptions opts = matchCase ? QRegularExpression::NoPatternOption
                                                                  : QRegularExpression::CaseInsensitiveOption;
        const QRegularExpression re(regex ? text : QRegularExpression::escape(text), opts);
        if (re.isValid()) {
            for (int row = 0; row < c->totalRows() && m_matches.size() < kMaxMatches; ++row) {
                QString line;
                std::vector<int> colOf; // column of every QChar
                for (int col = 0; col < c->cols(); ++col) {
                    const vt::Cell cell = c->cell(row, col);
                    if (cell.width == 0) {
                        continue;
                    }
                    const QString part = cell.ch == 0 ? QStringLiteral(" ")
                        : cell.ch >= vt::kClusterBase ? c->clusterText(cell.ch)
                                                      : QString::fromUcs4(&cell.ch, 1);
                    for (int i = 0; i < part.size(); ++i) {
                        colOf.push_back(col);
                    }
                    line += part;
                }
                auto it = re.globalMatch(line);
                while (it.hasNext() && m_matches.size() < kMaxMatches) {
                    const QRegularExpressionMatch m = it.next();
                    if (m.capturedLength() == 0) {
                        continue;
                    }
                    const int endChar = int(m.capturedStart() + m.capturedLength()) - 1;
                    const int col1 = colOf[size_t(endChar)] + std::max<int>(1, c->cell(row, colOf[size_t(endChar)]).width);
                    m_matches.push_back(Match{lineOf(row), colOf[size_t(m.capturedStart())], col1});
                }
            }
        }
    }
    if (!m_matches.empty()) {
        m_matchIndex = int(m_matches.size()) - 1; // the newest one first
        const Match &m = m_matches[size_t(m_matchIndex)];
        const int abs = absOf(m.line);
        const int first = std::max(0, c->totalRows() - m_rows - m_scrollOffset);
        if (abs < first || abs >= first + m_rows) {
            scrollTo(std::max(0, c->totalRows() - m_rows - abs + m_rows / 2));
        }
    }
    repaint();
    Q_EMIT findChanged();
    return int(m_matches.size());
}

void TerminalView::findNext(bool forward)
{
    VtCore *c = core();
    if (m_matches.empty() || !c) {
        return;
    }
    const int n = int(m_matches.size());
    m_matchIndex = ((m_matchIndex + (forward ? 1 : -1)) % n + n) % n;
    const Match &m = m_matches[size_t(m_matchIndex)];
    const int abs = absOf(m.line);
    const int first = std::max(0, c->totalRows() - m_rows - m_scrollOffset);
    if (abs < first || abs >= first + m_rows) {
        scrollTo(std::clamp(c->totalRows() - m_rows - abs + m_rows / 2, 0, c->historyCount()));
    }
    repaint();
    Q_EMIT findChanged();
}

void TerminalView::clearFind()
{
    if (!m_matches.empty() || m_matchIndex >= 0) {
        m_matches.clear();
        m_matchIndex = -1;
        repaint();
        Q_EMIT findChanged();
    }
}

// --- Input ------------------------------------------------------------------------------

QPointF TerminalView::contextAnchor() const
{
    const VtCore *c = core();
    const qreal cw = cellWidth(), ch = cellHeight();
    if (!c) {
        return {};
    }
    int row = c->historyCount() + c->cursorRow();
    int col = c->cursorCol();
    if (hasSelection()) {
        const Pos end = m_selAnchor < m_selHead ? m_selHead : m_selAnchor;
        row = absOf(end.line);
        col = end.col;
    }
    const int first = std::max(0, c->totalRows() - m_rows - m_scrollOffset);
    const int visible = std::clamp(row - first, 0, m_rows - 1);
    return QPointF(m_padLeft + col * cw, m_padTop + visible * ch);
}

void TerminalView::paste()
{
    requestPaste(QGuiApplication::clipboard()->text());
}

void TerminalView::requestPaste(const QString &text)
{
    const VtCore *c = core();
    if (!c || text.isEmpty()) {
        return;
    }
    // warning.multiLinePaste: false/"never", "always", or true/"automatic" (the default): only
    // when the program did not ask for bracketed paste, so a shell cannot tell it from typing.
    const QString mode = m_multiLinePasteWarning.typeId() == QMetaType::Bool
        ? (m_multiLinePasteWarning.toBool() ? QStringLiteral("automatic") : QStringLiteral("never"))
        : m_multiLinePasteWarning.toString();
    const bool multiLine = text.contains(QLatin1Char('\n')) || text.contains(QLatin1Char('\r'));
    const bool warnMultiLine = multiLine && mode != QLatin1String("never")
        && (mode == QLatin1String("always") || !c->bracketedPaste());
    const bool warnLarge = m_largePasteWarning && text.toUtf8().size() > 5 * 1024; // the original's 5 KiB
    if (warnMultiLine || warnLarge) {
        Q_EMIT pasteConfirmationRequested(text, warnMultiLine, warnLarge);
        return;
    }
    pasteText(text);
}

void TerminalView::pasteText(const QString &text)
{
    if (VtCore *c = core(); c && !text.isEmpty()) {
        scrollToBottom();
        c->paste(text);
    }
}

void TerminalView::keyPressEvent(QKeyEvent *event)
{
    VtCore *c = core();
    if (!c) {
        event->ignore();
        return;
    }
    const Qt::KeyboardModifiers qm = event->modifiers();
    const bool ctrl = qm & Qt::ControlModifier;
    const int key = event->key();

    if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt || key == Qt::Key_Meta
        || key == Qt::Key_AltGr || key == Qt::Key_CapsLock || key == Qt::Key_NumLock || key == Qt::Key_ScrollLock
        || key == Qt::Key_Super_L || key == Qt::Key_Super_R) {
        event->accept(); // a modifier alone sends nothing
        return;
    }

    if (m_mark) {
        handleMarkKey(event);
        return;
    }

    // A terminal whose program failed waits: Enter runs it again, Ctrl+D closes it.
    if (m_session && m_session->hasExited()) {
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            m_session->restart();
        } else if (ctrl && key == Qt::Key_D) {
            Q_EMIT m_session->closeRequested();
        }
        event->accept();
        return;
    }

    VTermModifier mods = modifiersOf(qm);
    clearSelection();
    if (m_snapOnInput) {
        scrollToBottom();
    }
    restartBlink();

    if (key == Qt::Key_Backtab) {
        mods = VTermModifier(mods | VTERM_MOD_SHIFT);
    }
    const VTermKey special = specialKey(key, qm);
    if (special != VTERM_KEY_NONE) {
        c->key(special, mods);
        event->accept();
        return;
    }

    if (qm & (Qt::ControlModifier | Qt::AltModifier)) {
        // With Ctrl or Alt the event's text is a control code (or empty): use
        // the key, which is the character on the keycap.
        char32_t ch = 0;
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            ch = char32_t(key - Qt::Key_A + 'a');
        } else if (key >= Qt::Key_Space && key <= Qt::Key_AsciiTilde) {
            ch = char32_t(key);
        } else if (!event->text().isEmpty() && event->text().at(0).unicode() >= 0x20) {
            ch = event->text().toUcs4().first();
        }
        if (ch != 0) {
            // Shift is already in the character ("(" not "9"), except for letters.
            if (!(key >= Qt::Key_A && key <= Qt::Key_Z)) {
                mods = VTermModifier(mods & ~VTERM_MOD_SHIFT);
            }
            c->unichar(ch, mods);
            event->accept();
            return;
        }
        event->ignore();
        return;
    }

    const QString text = event->text();
    if (!text.isEmpty() && text.at(0).unicode() >= 0x20 && text.at(0).unicode() != 0x7F) {
        for (uint cp : text.toUcs4()) {
            c->unichar(cp, VTERM_MOD_NONE);
        }
        event->accept();
        return;
    }
    event->ignore();
}

void TerminalView::inputMethodEvent(QInputMethodEvent *event)
{
    VtCore *c = core();
    if (c && !event->commitString().isEmpty()) {
        scrollToBottom();
        for (uint cp : event->commitString().toUcs4()) {
            c->unichar(cp, VTERM_MOD_NONE);
        }
    }
    event->accept();
}

QVariant TerminalView::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (query == Qt::ImEnabled) {
        return true;
    }
    if (query == Qt::ImCursorRectangle) {
        const VtCore *c = core();
        const qreal cw = cellWidth(), ch = cellHeight();
        return QRectF(m_padLeft + (c ? c->cursorCol() : 0) * cw, m_padTop + (c ? c->cursorRow() : 0) * ch, cw, ch);
    }
    return QQuickItem::inputMethodQuery(query);
}

void TerminalView::focusInEvent(QFocusEvent *event)
{
    QQuickItem::focusInEvent(event);
    restartBlink();
    repaint();
}

void TerminalView::focusOutEvent(QFocusEvent *event)
{
    QQuickItem::focusOutEvent(event);
    restartBlink();
    repaint();
}

void TerminalView::wheelEvent(QWheelEvent *event)
{
    VtCore *c = core();
    if (!c) {
        return;
    }
    const Qt::KeyboardModifiers mods = event->modifiers();
    const bool ctrl = mods & Qt::ControlModifier, shift = mods & Qt::ShiftModifier;
    if (ctrl && ((shift && m_scrollToOpacity) || (!shift && m_scrollToZoom))) {
        m_zoomRemainder += event->angleDelta().y() / 120.0;
        const int steps = int(m_zoomRemainder);
        m_zoomRemainder -= steps;
        if (steps != 0) {
            if (shift) {
                Q_EMIT opacityRequested(steps);
            } else {
                Q_EMIT zoomRequested(steps);
            }
        }
        event->accept();
        return;
    }
    // What the program gets: whole notches (a notch is 120 of angleDelta; a touchpad adds up to them).
    // What the history gets: three lines a notch like the original, or, for a touchpad (it reports a scroll phase), its pixels per cell height.
    const bool toProgram = (c->mouseMode() != VtCore::MouseMode::None || c->altScreen()) && !shift;
    if (toProgram) {
        m_wheelNotches += event->angleDelta().y() / 120.0;
        const int notches = int(m_wheelNotches);
        m_wheelNotches -= notches;
        if (notches != 0) {
            const qreal d = dpr();
            const GlyphAtlas::Metrics &m = metrics();
            const QMargins pp = paddingPx();
            const int col = std::clamp(int((event->position().x() * d - pp.left()) / m.cellWidth), 0, m_columns - 1);
            const int row = std::clamp(int((event->position().y() * d - pp.top()) / m.cellHeight), 0, m_rows - 1);
            c->wheel(notches, row, col, modifiersOf(mods));
        }
    } else {
        qreal lines = 0;
        if (!event->pixelDelta().isNull() && event->phase() != Qt::NoScrollPhase && metrics().cellHeight > 0) {
            lines = event->pixelDelta().y() * dpr() / metrics().cellHeight;
        } else {
            lines = event->angleDelta().y() / 120.0 * 3;
        }
        m_wheelLines += lines;
        const int whole = int(m_wheelLines);
        m_wheelLines -= whole;
        if (whole != 0) {
            scrollBy(whole);
        }
    }
    event->accept();
}

// --- Drawing ------------------------------------------------------------------------------

QSGNode *TerminalView::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    if (!m_atlas) {
        // No font yet (it comes with componentComplete): never set it up from here, on the
        // render thread; ask the GUI thread and draw nothing for now.
        QMetaObject::invokeMethod(this, [this] { rebuildFont(); }, Qt::QueuedConnection);
        return old;
    }
    auto *root = static_cast<TerminalNode *>(old);
    if (!root) {
        root = new TerminalNode;
        root->background = makeColorNode();
        root->decorations = makeColorNode();
        root->appendChildNode(root->background);
        root->appendChildNode(root->decorations);
        root->cursorLayer = new QSGOpacityNode;
        root->cursorBack = makeColorNode();
        root->cursorGlyph = new QSGGeometryNode;
        auto *geometry = new QSGGeometry(glyphAttributes(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        root->cursorGlyph->setGeometry(geometry);
        root->cursorGlyph->setFlag(QSGNode::OwnsGeometry);
        root->cursorMaterial = new GlyphMaterial;
        root->cursorGlyph->setMaterial(root->cursorMaterial);
        root->cursorGlyph->setFlag(QSGNode::OwnsMaterial);
        root->cursorFront = makeColorNode();
        root->cursorLayer->appendChildNode(root->cursorBack);
        root->cursorLayer->appendChildNode(root->cursorGlyph);
        root->cursorLayer->appendChildNode(root->cursorFront);
        root->appendChildNode(root->cursorLayer);
        m_fullDirty = true;
    }

    VtCore *c = core();
    const bool focused = hasActiveFocus();
    // The blink alone: show or hide the cursor layer, nothing else changed.
    const bool cursorShown = c && (!c->cursorBlink() || m_cursorOn || !focused);
    if (!m_fullDirty && root->atlas == m_atlas) {
        root->cursorLayer->setOpacity(cursorShown ? 1.0 : 0.0);
        return root;
    }
    m_fullDirty = false;

    GlyphAtlas &atlas = *m_atlas;
    QMutexLocker atlasLock(&atlas.lock()); // other windows' render threads share it
    // Between frames: no glyph of this one is taken yet (the others keep their own textures).
    atlas.trim();
    if (root->atlas != m_atlas || root->epoch != atlas.epoch()) {
        root->dropPages();
        root->atlas = m_atlas;
        root->epoch = atlas.epoch();
    }

    const qreal d = dpr();
    const float inv = float(1.0 / d);
    const GlyphAtlas::Metrics mt = atlas.metrics();
    const int cw = mt.cellWidth, ch = mt.cellHeight;
    const QMargins padM = paddingPx();
    const int padL = padM.left(), padT = padM.top();

    ColorQuads &bg = root->bg, &deco = root->deco, &cursorBg = root->cursorBg, &cursorFg = root->cursorFg;
    bg.clear();
    deco.clear();
    cursorBg.clear();
    cursorFg.clear();
    auto &glyphs = root->glyphs;
    for (auto &page : glyphs) {
        page.clear();
    }
    root->cursorVertices.clear();
    root->cursorPage = -1;

    QColor backdrop = m_scheme.background;
    backdrop.setAlphaF(m_bgOpacity);
    bg.rect(0, 0, float(width()), float(height()), backdrop);

    // Texture coordinates are in atlas pixels here, and become 0..1 once the frame is built
    // (a page may grow while it is).
    const auto glyphQuad = [&](std::vector<GlyphVertex> &out, const GlyphAtlas::Glyph &g, float x, float y, float w, float h,
                               const QColor &color) {
        uchar r = uchar(color.red()), gr = uchar(color.green()), b = uchar(color.blue()), a = 255;
        if (g.color) {
            r = gr = b = a = 0;
        }
        const float x0 = x * inv, y0 = y * inv, x1 = (x + w) * inv, y1 = (y + h) * inv;
        const float u0 = float(g.x), v0 = float(g.y), u1 = float(g.x + g.w), v1 = float(g.y + g.h);
        out.push_back({x0, y0, u0, v0, r, gr, b, a});
        out.push_back({x1, y0, u1, v0, r, gr, b, a});
        out.push_back({x0, y1, u0, v1, r, gr, b, a});
        out.push_back({x1, y0, u1, v0, r, gr, b, a});
        out.push_back({x1, y1, u1, v1, r, gr, b, a});
        out.push_back({x0, y1, u0, v1, r, gr, b, a});
    };
    const auto rect = [&](ColorQuads &q, float x, float y, float w, float h, const QColor &c) {
        q.rect(x * inv, y * inv, w * inv, h * inv, c);
    };
    // The glyph of a cell, from the cache; the text is only built for one it does not have yet.
    const auto glyphOf = [&](const Cell &cell, int cells) -> GlyphAtlas::Glyph {
        const bool boldStyle = (cell.attrs & vt::Bold)
                               && (m_intenseTextStyle == QLatin1String("bold") || m_intenseTextStyle == QLatin1String("all"));
        const GlyphAtlas::Style style = GlyphAtlas::Style((boldStyle ? 1 : 0) | ((cell.attrs & vt::Italic) ? 2 : 0));
        const quint64 key = GlyphAtlas::makeKey(cell.ch, style, cells);
        if (const GlyphAtlas::Glyph *g = atlas.find(key)) {
            return *g;
        }
        const QString text = cell.ch >= vt::kClusterBase ? c->clusterText(cell.ch) : QString::fromUcs4(&cell.ch, 1);
        return atlas.insert(key, text, style, cells);
    };

    if (c) {
        const int first = std::max(0, c->totalRows() - m_rows - m_scrollOffset);
        const int cursorRowAbs = c->historyCount() + c->cursorRow();
        // The profile's shape rules until the program asks for another (DECSCUSR).
        enum class Shape { Bar, Underscore, FilledBox, EmptyBox, Other };
        Shape shape = m_cursorShape == QLatin1String("bar") ? Shape::Bar
            : m_cursorShape == QLatin1String("underscore") ? Shape::Underscore
            : m_cursorShape == QLatin1String("filledBox")  ? Shape::FilledBox
            : m_cursorShape == QLatin1String("emptyBox")   ? Shape::EmptyBox
                                                           : Shape::Other;
        if (c->cursorShapeFromProgram()) {
            shape = c->cursorShape() == VtCore::CursorShape::Bar ? Shape::Bar
                : c->cursorShape() == VtCore::CursorShape::Underline ? Shape::Underscore
                                                                     : Shape::FilledBox;
        }
        // Drawn at all (the blink then only shows or hides the layer).
        const bool cursorDrawn = c->cursorVisible() && m_scrollOffset == 0;
        const int cursorPhysThickness = std::max(1, int(std::lround(d)));
        const bool reverseScreen = c->reverseScreen();
        const Match *current = m_matchIndex >= 0 ? &m_matches[size_t(m_matchIndex)] : nullptr;
        // Readable selected text: the luminance of the selection, and the last colour asked for.
        const QColor selectionBackground = m_scheme.selectionBackground;
        const qreal selectionLuminance = luminance(selectionBackground);
        QColor lastSelFg, lastSelOut;
        const auto readableOnSelection = [&](const QColor &fg) {
            if (fg != lastSelFg || !lastSelOut.isValid()) {
                lastSelFg = fg;
                lastSelOut = readableOn(fg, selectionBackground, selectionLuminance);
            }
            return lastSelOut;
        };

        for (int r = 0; r < m_rows; ++r) {
            const int abs = first + r;
            if (abs >= c->totalRows()) {
                break;
            }
            const float y = float(padT + r * ch);
            const qint64 lineId = lineOf(abs);
            int selC0 = 0, selC1 = 0;
            const bool rowHasSelection = selectionSpan(lineId, selC0, selC1) && !selectionEmpty();
            // The matches of this row: m_matches is in line order.
            const auto rowBegin = std::lower_bound(m_matches.begin(), m_matches.end(), lineId,
                                                   [](const Match &m, qint64 line) { return m.line < line; });
            auto rowEnd = rowBegin;
            while (rowEnd != m_matches.end() && rowEnd->line == lineId) {
                ++rowEnd;
            }

            // A cell's colours; `cursorOn`: as the filled-box cursor shows it.
            const auto colours = [&](const Cell &cell, int col, bool cursorOn, QColor &fg, QColor &bgc) {
                // Bold brightens the eight base colours (and the default foreground reads as
                // white: #F2F2F2, measured); faint halves.
                const vt::Color fgc = cell.fg;
                const bool bold = (cell.attrs & vt::Bold) != 0;
                const bool brighten = bold && (m_intenseTextStyle == QLatin1String("bright") || m_intenseTextStyle == QLatin1String("all"));
                bool schemeForeground = vt::isDefault(fgc) || vt::isIndexed(fgc);
                if (vt::isDefault(fgc)) {
                    fg = brighten ? m_scheme.ansi[15] : m_scheme.foreground;
                } else if (vt::isIndexed(fgc) && vt::colorIndex(fgc) < 8 && brighten) {
                    fg = m_scheme.indexed(vt::colorIndex(fgc) + 8);
                } else {
                    fg = m_scheme.resolveForeground(fgc);
                }
                bgc = m_scheme.resolveBackground(cell.bg);
                if (cell.attrs & vt::Faint) {
                    fg = scaled(fg, 0.5);
                }
                bool reversed = (cell.attrs & vt::Reverse) != 0;
                if (reverseScreen) {
                    reversed = !reversed;
                }
                if (reversed) {
                    std::swap(fg, bgc);
                    schemeForeground = vt::isDefault(cell.bg) || vt::isIndexed(cell.bg);
                }
                if (cursorOn && shape == Shape::FilledBox) {
                    bgc = m_scheme.cursorColor;
                    fg = m_scheme.background;
                    schemeForeground = true;
                }
                const bool adjust = m_adjustIndistinguishableColors == QLatin1String("always")
                                    || (m_adjustIndistinguishableColors == QLatin1String("indexed") && schemeForeground);
                if (adjust && !(cell.attrs & vt::Conceal)) {
                    fg = ColorScheme::adjustIndistinguishableForeground(fg, bgc);
                }
                if (cell.attrs & vt::Conceal) {
                    fg = bgc;
                }
                // Search matches (yellow; the current one orange, black text: measured),
                // then the selection over them.
                for (auto m = rowBegin; m != rowEnd; ++m) {
                    if (col >= m->col0 && col < m->col1) {
                        bgc = &*m == current ? QColor(0xFF, 0x96, 0x32) : QColor(0xFF, 0xFF, 0x00);
                        fg = Qt::black;
                    }
                }
                if (rowHasSelection && col >= selC0 && col < selC1) {
                    bgc = m_scheme.selectionBackground;
                    fg = readableOnSelection(fg);
                }
            };
            // The lines a cell draws in its foreground (and the mark of mark mode).
            const auto lines = [&](ColorQuads &q, const Cell &cell, int col, int cells, float x, const QColor &fg) {
                const int ul = vt::underlineOf(cell.attrs);
                const float w = float(cells * cw);
                const float th = float(mt.lineThickness);
                if (ul == 1) {
                    rect(q, x, y + mt.underlinePos, w, th, fg);
                } else if (ul == 2) {
                    rect(q, x, y + mt.underlinePos, w, th, fg);
                    rect(q, x, y + std::max(0, mt.underlinePos - 2 * mt.lineThickness), w, th, fg);
                } else if (ul == 3) {
                    static const int wave[8] = {1, 0, 0, 0, 1, 2, 2, 2}; // a squiggle, one pixel wide steps
                    for (int i = 0; i < cells * cw; ++i) {
                        rect(q, x + i, y + std::min(ch - 1, mt.underlinePos - 1 + wave[(col * cw + i) % 8]), 1, th, fg);
                    }
                }
                if (cell.attrs & vt::Strike) {
                    rect(q, x, y + mt.strikePos, w, th, fg);
                }
                if (m_hoverUrl.line == lineId && col >= m_hoverUrl.col0 && col < m_hoverUrl.col1) {
                    rect(q, x, y + mt.underlinePos, w, th, fg);
                }
                if (m_mark && m_markCursor.line == lineId && m_markCursor.col == col) {
                    const float t = float(cursorPhysThickness);
                    const QColor markColor = m_scheme.cursorColor;
                    rect(q, x, y, w, t, markColor);
                    rect(q, x, y + ch - t, w, t, markColor);
                    rect(q, x, y, t, float(ch), markColor);
                    rect(q, x + w - t, y, t, float(ch), markColor);
                }
                if (cell.attrs & vt::Overline) {
                    rect(q, x, y, w, th, fg);
                }
            };
            const auto glyphFor = [&](const Cell &cell) {
                return cell.ch != 0 && cell.ch != U' ' && !(cell.attrs & vt::Conceal);
            };

            int runStart = -1;
            QColor runColor;
            const auto flushRun = [&](int endCol) {
                if (runStart >= 0) {
                    rect(bg, float(padL + runStart * cw), y, float((endCol - runStart) * cw), float(ch), runColor);
                    runStart = -1;
                }
            };
            for (int col = 0; col < m_columns;) {
                const Cell cell = c->cell(abs, col);
                if (cell.width == 0) {
                    ++col;
                    continue;
                }
                const int cells = cell.width == 2 ? 2 : 1;
                const float x = float(padL + col * cw);
                QColor fg, bgc;
                colours(cell, col, false, fg, bgc);

                // Backgrounds: neighbours of one colour become one rectangle.
                if (bgc != m_scheme.background) {
                    if (runStart < 0 || runColor != bgc) {
                        flushRun(col);
                        runStart = col;
                        runColor = bgc;
                    }
                } else {
                    flushRun(col);
                }
                if (glyphFor(cell)) {
                    const GlyphAtlas::Glyph g = glyphOf(cell, cells);
                    if (g.page >= 0) {
                        if (glyphs.size() <= size_t(g.page)) {
                            glyphs.resize(size_t(g.page) + 1);
                        }
                        glyphQuad(glyphs[size_t(g.page)], g, x, y, float(cells * cw), float(ch), fg);
                    }
                }
                lines(deco, cell, col, cells, x, fg);

                // The cursor's layer: the cell as the cursor shows it (filled box), then its shape.
                if (cursorDrawn && abs == cursorRowAbs && col == c->cursorCol()) {
                    QColor onFg, onBg;
                    colours(cell, col, true, onFg, onBg);
                    if (onFg != fg || onBg != bgc) {
                        rect(cursorBg, x, y, float(cells * cw), float(ch), onBg);
                        if (glyphFor(cell)) {
                            const GlyphAtlas::Glyph g = glyphOf(cell, cells);
                            if (g.page >= 0) {
                                glyphQuad(root->cursorVertices, g, x, y, float(cells * cw), float(ch), onFg);
                                root->cursorPage = g.page;
                            }
                        }
                        lines(cursorFg, cell, col, cells, x, onFg);
                    }
                    const float w = float(cells * cw);
                    const float t = float(cursorPhysThickness);
                    if (shape == Shape::Bar) {
                        if (focused) {
                            rect(cursorFg, x, y, t, float(ch), m_scheme.cursorColor);
                        }
                    } else if (shape == Shape::Underscore) {
                        if (focused) {
                            rect(cursorFg, x, y + ch - t, w, t, m_scheme.cursorColor);
                        }
                    } else if (shape == Shape::EmptyBox || !focused) {
                        rect(cursorFg, x, y, w, t, m_scheme.cursorColor);
                        rect(cursorFg, x, y + ch - t, w, t, m_scheme.cursorColor);
                        rect(cursorFg, x, y, t, float(ch), m_scheme.cursorColor);
                        rect(cursorFg, x + w - t, y, t, float(ch), m_scheme.cursorColor);
                    }
                }
                col += cells;
            }
            flushRun(m_columns);
        }
    }

    fillColorNode(root->background, bg);
    fillColorNode(root->decorations, deco);
    fillColorNode(root->cursorBack, cursorBg);
    fillColorNode(root->cursorFront, cursorFg);
    root->cursorLayer->setOpacity(cursorShown ? 1.0 : 0.0);

    // From atlas pixels to 0..1, with each page's size as it is now.
    const auto normalize = [&](std::vector<GlyphVertex> &vertices, int page) {
        const QImage &image = atlas.pageImage(page);
        const float su = 1.0f / float(image.width()), sv = 1.0f / float(image.height());
        for (GlyphVertex &v : vertices) {
            v.u *= su;
            v.v *= sv;
        }
    };
    const auto upload = [](QSGGeometryNode *node, const std::vector<GlyphVertex> &vertices) {
        QSGGeometry *geometry = node->geometry();
        geometry->allocate(int(vertices.size()));
        if (!vertices.empty()) {
            memcpy(geometry->vertexData(), vertices.data(), vertices.size() * sizeof(GlyphVertex));
        }
        node->markDirty(QSGNode::DirtyGeometry);
    };

    // Atlas pages: one node (and one texture) each.
    for (int i = 0; i < atlas.pageCount(); ++i) {
        if (i >= root->pages.size()) {
            TerminalNode::PageNode page;
            page.node = new QSGGeometryNode;
            auto *geometry = new QSGGeometry(glyphAttributes(), 0);
            geometry->setDrawingMode(QSGGeometry::DrawTriangles);
            page.node->setGeometry(geometry);
            page.node->setFlag(QSGNode::OwnsGeometry);
            page.material = new GlyphMaterial;
            page.node->setMaterial(page.material);
            page.node->setFlag(QSGNode::OwnsMaterial);
            root->insertChildNodeBefore(page.node, root->decorations);
            root->pages.push_back(page);
        }
        TerminalNode::PageNode &page = root->pages[i];
        if (!page.texture || page.version != atlas.pageVersion(i)) {
            delete page.texture;
            page.texture = window()->createTextureFromImage(atlas.pageImage(i), QQuickWindow::TextureHasAlphaChannel);
            if (page.texture) {
                page.texture->setFiltering(QSGTexture::Nearest);
            }
            page.version = atlas.pageVersion(i);
            page.material->setTexture(page.texture);
            page.node->markDirty(QSGNode::DirtyMaterial);
        }
        if (size_t(i) < glyphs.size()) {
            normalize(glyphs[size_t(i)], i);
            upload(page.node, glyphs[size_t(i)]);
        } else {
            upload(page.node, {});
        }
    }
    if (root->cursorPage >= 0 && root->cursorPage < root->pages.size()) {
        normalize(root->cursorVertices, root->cursorPage);
        root->cursorMaterial->setTexture(root->pages[root->cursorPage].texture);
        root->cursorGlyph->markDirty(QSGNode::DirtyMaterial);
        upload(root->cursorGlyph, root->cursorVertices);
    } else {
        root->cursorMaterial->setTexture(nullptr); // never a texture dropPages() may have deleted
        upload(root->cursorGlyph, {});
    }
    return root;
}
