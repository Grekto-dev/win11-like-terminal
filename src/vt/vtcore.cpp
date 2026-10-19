#include "vtcore.h"

#include <algorithm>
#include <cstring>
#include <type_traits>
#include <vector>

using vt::Cell;

namespace
{
// Distinct grapheme clusters (a letter with combining marks, emoji sequences) the table keeps.
// Past it a new cluster shows its first code point only: a program printing endless distinct
// sequences must not grow the memory (nor the glyph atlas) without end.
constexpr qsizetype kMaxClusters = 16384;
// The longest window title taken from a program (OSC 0/2); the rest is ignored.
constexpr qsizetype kMaxTitle = 4096;

static_assert(std::is_trivially_copyable_v<Cell>, "the grid is moved with memmove");
}


VtCore::VtCore(int rows, int cols, int scrollbackLimit, QObject *parent)
    : QObject(parent)
    , m_rows(std::max(1, rows))
    , m_cols(std::max(1, cols))
    , m_limit(std::max(0, scrollbackLimit))
{
    m_grid.resize(m_rows * m_cols);

    m_vterm = vterm_new(m_rows, m_cols);
    vterm_set_utf8(m_vterm, 1);
    vterm_output_set_callback(m_vterm, &VtCore::onOutput, this);
    m_state = vterm_obtain_state(m_vterm);
    m_screen = vterm_obtain_screen(m_vterm);

    static const VTermScreenCallbacks callbacks = {
        &VtCore::onDamage, &VtCore::onMoveRect, &VtCore::onMoveCursor, &VtCore::onSetTermProp,
        &VtCore::onBell, &VtCore::onResize, &VtCore::onPushLine, &VtCore::onPopLine,
        &VtCore::onClearScrollback,
    };
    vterm_screen_set_callbacks(m_screen, &callbacks, this);
    vterm_screen_enable_altscreen(m_screen, 1);
    vterm_screen_enable_reflow(m_screen, true);
    vterm_screen_set_damage_merge(m_screen, VTERM_DAMAGE_SCROLL);
    vterm_screen_reset(m_screen, 1);
    refresh({0, m_rows, 0, m_cols});
    syncCursor();
    m_dirtyFirst = m_rows;
    m_dirtyLast = 0;
    m_ready = true;
}

VtCore::~VtCore()
{
    vterm_free(m_vterm);
}

// --- Input from the program ---------------------------------------------------------

void VtCore::write(const QByteArray &bytes)
{
    vterm_input_write(m_vterm, bytes.constData(), size_t(bytes.size()));
    vterm_screen_flush_damage(m_screen);
    syncDirty();
    syncCursor();
    finish();
}

void VtCore::resize(int rows, int cols)
{
    rows = std::max(1, rows);
    cols = std::max(1, cols);
    if (rows == m_rows && cols == m_cols) {
        return;
    }
    m_rows = rows;
    m_cols = cols;
    m_grid.fill(Cell(), m_rows * m_cols);
    vterm_set_size(m_vterm, rows, cols);
    vterm_screen_flush_damage(m_screen);
    refresh({0, m_rows, 0, m_cols});
    syncCursor();
    m_dirty = true;
    finish();
}

// --- Content ------------------------------------------------------------------------

Cell VtCore::cell(int absRow, int col) const
{
    if (absRow < 0 || col < 0) {
        return m_blank;
    }
    if (absRow < historyCount()) {
        return m_history[size_t(absRow)].cell(col);
    }
    const int row = absRow - historyCount();
    if (row >= m_rows || col >= m_cols) {
        return m_blank;
    }
    return m_grid[row * m_cols + col];
}

bool VtCore::continuation(int absRow) const
{
    if (absRow >= 0 && absRow < historyCount()) {
        return m_history[size_t(absRow)].continuation();
    }
    const int row = absRow - historyCount();
    if (row < 0 || row >= m_rows) {
        return false;
    }
    const VTermLineInfo *info = vterm_state_get_lineinfo(m_state, row);
    return info && info->continuation;
}

QString VtCore::clusterText(char32_t ch) const
{
    const qsizetype index = qsizetype(ch) - qsizetype(vt::kClusterBase);
    return index >= 0 && index < m_clusters.size() ? m_clusters[index] : QString();
}

QString VtCore::rowText(int absRow, int col0, int col1) const
{
    if (absRow < 0 || absRow >= totalRows()) {
        return {};
    }
    const int limit = absRow < historyCount() ? m_history[size_t(absRow)].size() : m_cols;
    const bool toEnd = col1 < 0;
    if (toEnd) {
        col1 = limit;
    }
    QString text;
    for (int c = std::max(0, col0); c < col1; ++c) {
        const Cell cl = cell(absRow, c);
        if (cl.width == 0) {
            continue;
        }
        if (cl.ch == 0) {
            text += QLatin1Char(' ');
        } else if (cl.ch >= vt::kClusterBase) {
            text += clusterText(cl.ch);
        } else {
            text += QString::fromUcs4(&cl.ch, 1);
        }
    }
    if (toEnd) {
        while (text.endsWith(QLatin1Char(' '))) {
            text.chop(1);
        }
    }
    return text;
}

void VtCore::setScrollbackLimit(int lines)
{
    m_limit = std::max(0, lines);
    while (int(m_history.size()) > m_limit) {
        m_history.pop_front();
        ++m_dropped;
        m_dirty = true;
    }
    finish();
}

void VtCore::clearScrollback()
{
    m_dropped += m_history.size();
    m_history.clear();
    m_history.shrink_to_fit();
    m_dirty = true;
    finish();
}

// --- Input to the program -------------------------------------------------------------

void VtCore::key(VTermKey key, VTermModifier mods)
{
    vterm_keyboard_key(m_vterm, key, mods);
    finish();
}

void VtCore::unichar(char32_t c, VTermModifier mods)
{
    vterm_keyboard_unichar(m_vterm, c, mods);
    finish();
}

bool VtCore::bracketedPaste() const
{
    return vterm_state_get_bracketpaste(m_state) != 0;
}

QString VtCore::filterPaste(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar ch : text) {
        const char16_t u = ch.unicode();
        const bool control = u < 0x20 || u == 0x7F || (u >= 0x80 && u <= 0x9F);
        if (!control || u == '\t' || u == '\n' || u == '\r') {
            out += ch;
        }
    }
    return out;
}

void VtCore::paste(const QString &raw)
{
    const QString text = filterPaste(raw);
    if (text.isEmpty()) {
        return;
    }
    vterm_keyboard_start_paste(m_vterm);
    const QList<uint> codePoints = text.toUcs4();
    for (size_t i = 0; i < size_t(codePoints.size()); ++i) {
        uint c = codePoints[qsizetype(i)];
        if (c == '\r' && i + 1 < size_t(codePoints.size()) && codePoints[qsizetype(i + 1)] == '\n') {
            continue; // CRLF: the LF that follows becomes the one CR
        }
        vterm_keyboard_unichar(m_vterm, c == '\n' ? uint('\r') : c, VTERM_MOD_NONE);
    }
    vterm_keyboard_end_paste(m_vterm);
    finish();
}

void VtCore::mouseMove(int row, int col, VTermModifier mods)
{
    vterm_mouse_move(m_vterm, row, col, mods);
    finish();
}

void VtCore::mouseButton(int button, bool pressed, VTermModifier mods)
{
    vterm_mouse_button(m_vterm, button, pressed, mods);
    finish();
}

bool VtCore::wheel(int notches, int row, int col, VTermModifier mods)
{
    if (notches == 0) {
        return m_mouseMode != MouseMode::None || m_altScreen;
    }
    const int count = std::abs(notches);
    if (m_mouseMode != MouseMode::None) {
        vterm_mouse_move(m_vterm, row, col, mods);
        for (int i = 0; i < count; ++i) {
            vterm_mouse_button(m_vterm, notches > 0 ? 4 : 5, true, mods);
        }
        finish();
        return true;
    }
    if (m_altScreen) {
        for (int i = 0; i < count; ++i) {
            vterm_keyboard_key(m_vterm, notches > 0 ? VTERM_KEY_UP : VTERM_KEY_DOWN, mods);
        }
        finish();
        return true;
    }
    return false;
}

void VtCore::focus(bool in)
{
    if (in) {
        vterm_state_focus_in(m_state);
    } else {
        vterm_state_focus_out(m_state);
    }
    finish();
}

// --- libvterm callbacks ---------------------------------------------------------------------

int VtCore::onDamage(VTermRect rect, void *user)
{
    auto *self = static_cast<VtCore *>(user);
    self->markDirty(rect);
    return 1;
}

int VtCore::onMoveRect(VTermRect dest, VTermRect src, void *user)
{
    // The same move on our copy: a scroll costs a memmove, not a conversion of every cell.
    static_cast<VtCore *>(user)->moveCells(dest, src);
    return 1;
}

int VtCore::onMoveCursor(VTermPos pos, VTermPos, int visible, void *user)
{
    auto *self = static_cast<VtCore *>(user);
    self->m_cursor = pos;
    Q_UNUSED(visible)
    self->m_dirty = true;
    return 1;
}

int VtCore::onSetTermProp(VTermProp prop, VTermValue *val, void *user)
{
    auto *self = static_cast<VtCore *>(user);
    switch (prop) {
    case VTERM_PROP_CURSORVISIBLE:
        self->m_cursorVisible = val->boolean;
        self->m_dirty = true;
        break;
    case VTERM_PROP_CURSORBLINK:
        self->m_cursorBlink = val->boolean;
        self->m_modesDirty = true;
        break;
    case VTERM_PROP_CURSORSHAPE:
        self->m_cursorShape = val->number == VTERM_PROP_CURSORSHAPE_UNDERLINE ? CursorShape::Underline
            : val->number == VTERM_PROP_CURSORSHAPE_BAR_LEFT                 ? CursorShape::Bar
                                                                             : CursorShape::Block;
        if (self->m_ready) {
            self->m_shapeFromProgram = true;
        }
        self->m_modesDirty = true;
        break;
    case VTERM_PROP_ALTSCREEN:
        self->m_altScreen = val->boolean;
        self->m_modesDirty = true;
        break;
    case VTERM_PROP_REVERSE:
        self->m_reverse = val->boolean;
        self->m_dirty = true;
        break;
    case VTERM_PROP_MOUSE:
        self->m_mouseMode = val->number == VTERM_PROP_MOUSE_CLICK ? MouseMode::Click
            : val->number == VTERM_PROP_MOUSE_DRAG                ? MouseMode::Drag
            : val->number == VTERM_PROP_MOUSE_MOVE                ? MouseMode::Move
                                                                  : MouseMode::None;
        self->m_modesDirty = true;
        break;
    case VTERM_PROP_FOCUSREPORT:
        self->m_focusReport = val->boolean;
        self->m_modesDirty = true;
        break;
    case VTERM_PROP_TITLE: {
        const VTermStringFragment &fragment = val->string;
        if (fragment.initial) {
            self->m_titleFragment.clear();
        }
        if (self->m_titleFragment.size() < kMaxTitle) {
            self->m_titleFragment += QString::fromUtf8(fragment.str, qsizetype(fragment.len)).left(kMaxTitle);
        }
        if (fragment.final) {
            self->m_title = self->m_titleFragment;
            Q_EMIT self->titleChanged(self->m_title);
        }
        break;
    }
    default:
        break;
    }
    return 1;
}

int VtCore::onBell(void *user)
{
    Q_EMIT static_cast<VtCore *>(user)->bell();
    return 1;
}

int VtCore::onResize(int, int, void *)
{
    return 1; // the grid was already resized by resize()
}

int VtCore::onPushLine(int cols, const VTermScreenCell *cells, void *user)
{
    auto *self = static_cast<VtCore *>(user);
    // Trailing empty cells are not worth their memory (nor the time to convert
    // them: most lines are short, and this runs once per scrolled line).
    int used = cols;
    while (used > 0) {
        const VTermScreenCell &last = cells[used - 1];
        const bool blank = (last.chars[0] == 0 || last.chars[0] == ' ') && last.width <= 1
            && VTERM_COLOR_IS_DEFAULT_BG(&last.bg) && !last.attrs.reverse && !last.attrs.underline
            && !last.attrs.strike;
        if (!blank) {
            break;
        }
        --used;
    }
    if (self->m_limit == 0) {
        ++self->m_dropped; // no scrollback: the line is gone at once
        self->m_dirty = true;
        return 1;
    }
    std::vector<Cell> &line = self->m_lineBuffer; // reused: no allocation (nor clearing) per line
    if (line.size() < size_t(used)) {
        line.resize(size_t(used));
    }
    for (int c = 0; c < used; ++c) {
        line[size_t(c)] = self->convert(cells[c]);
    }
    self->m_history.emplace_back(line.data(), used, vterm_screen_pushed_continuation(self->m_screen) != 0);
    if (int(self->m_history.size()) > self->m_limit) {
        self->m_history.pop_front();
        ++self->m_dropped;
    }
    self->m_dirty = true;
    return 1;
}

int VtCore::onPopLine(int cols, VTermScreenCell *cells, void *user)
{
    auto *self = static_cast<VtCore *>(user);
    if (self->m_history.empty()) {
        return 0;
    }
    const vt::HistoryLine line = std::move(self->m_history.back());
    self->m_history.pop_back();
    for (int c = 0; c < cols; ++c) {
        self->toVTerm(line.cell(c), cells[c]);
    }
    self->m_dirty = true;
    return 1;
}

int VtCore::onClearScrollback(void *user)
{
    auto *self = static_cast<VtCore *>(user);
    self->m_dropped += self->m_history.size();
    self->m_history.clear();
    self->m_history.shrink_to_fit();
    self->m_dirty = true;
    return 1;
}

void VtCore::onOutput(const char *bytes, size_t len, void *user)
{
    static_cast<VtCore *>(user)->m_out.append(bytes, qsizetype(len));
}

// --- Conversions ------------------------------------------------------------------------------

void VtCore::markDirty(VTermRect rect)
{
    m_dirtyFirst = std::min(m_dirtyFirst, std::max(0, rect.start_row));
    m_dirtyLast = std::max(m_dirtyLast, std::min(m_rows, rect.end_row));
    m_dirty = true;
}

void VtCore::moveCells(VTermRect dest, VTermRect src)
{
    const int dy = dest.start_row - src.start_row;
    const int dx = dest.start_col - src.start_col;
    const int height = dest.end_row - dest.start_row;
    const int width = dest.end_col - dest.start_col;
    const bool inside = dest.start_row >= 0 && dest.start_col >= 0 && dest.end_row <= m_rows && dest.end_col <= m_cols
        && src.start_row >= 0 && src.start_col >= 0 && src.end_row <= m_rows && src.end_col <= m_cols
        && src.end_row - src.start_row == height && src.end_col - src.start_col == width;
    if (!inside || width <= 0 || height <= 0) {
        markDirty(dest); // not a move we can mirror: read it again
        return;
    }
    // Rows in the order that never overwrites a source row before it is copied.
    Cell *grid = m_grid.data();
    for (int i = 0; i < height; ++i) {
        const int r = dy > 0 ? dest.end_row - 1 - i : dest.start_row + i;
        std::memmove(grid + r * m_cols + dest.start_col, grid + (r - dy) * m_cols + dest.start_col - dx,
                     size_t(width) * sizeof(Cell));
    }
    // Rows still waiting to be read moved too: what lands where they went is stale as well.
    const int first = std::max(m_dirtyFirst, src.start_row);
    const int last = std::min(m_dirtyLast, src.end_row);
    if (first < last) {
        markDirty({first + dy, last + dy, 0, m_cols});
    }
    m_dirty = true;
}

void VtCore::syncDirty()
{
    if (m_dirtyFirst < m_dirtyLast) {
        refresh({m_dirtyFirst, m_dirtyLast, 0, m_cols});
    }
    m_dirtyFirst = m_rows;
    m_dirtyLast = 0;
}

void VtCore::refresh(VTermRect rect)
{
    rect.start_row = std::max(0, rect.start_row);
    rect.start_col = std::max(0, rect.start_col);
    rect.end_row = std::min(m_rows, rect.end_row);
    rect.end_col = std::min(m_cols, rect.end_col);
    VTermScreenCell source;
    for (int r = rect.start_row; r < rect.end_row; ++r) {
        for (int c = rect.start_col; c < rect.end_col; ++c) {
            vterm_screen_get_cell(m_screen, {r, c}, &source);
            m_grid[r * m_cols + c] = convert(source);
        }
    }
    m_dirty = true;
}

vt::Color VtCore::convert(const VTermColor &color)
{
    if (VTERM_COLOR_IS_DEFAULT_FG(&color) || VTERM_COLOR_IS_DEFAULT_BG(&color)) {
        return vt::kDefaultColor;
    }
    if (VTERM_COLOR_IS_INDEXED(&color)) {
        return vt::indexedColor(color.indexed.idx);
    }
    return vt::rgbColor(color.rgb.red, color.rgb.green, color.rgb.blue);
}

Cell VtCore::convert(const VTermScreenCell &source)
{
    Cell cell;
    cell.fg = convert(source.fg);
    cell.bg = convert(source.bg);
    cell.attrs = uint16_t((source.attrs.bold ? vt::Bold : 0) | (source.attrs.italic ? vt::Italic : 0)
                          | (source.attrs.blink ? vt::Blink : 0) | (source.attrs.reverse ? vt::Reverse : 0)
                          | (source.attrs.conceal ? vt::Conceal : 0) | (source.attrs.strike ? vt::Strike : 0)
                          | (source.attrs.small ? vt::Small : 0) | (source.attrs.faint ? vt::Faint : 0)
                          | (source.attrs.overline ? vt::Overline : 0)
                          | (uint16_t(source.attrs.underline) << vt::kUnderlineShift));
    if (source.chars[0] == uint32_t(-1)) {
        cell.width = 0; // the right half of a wide character
        return cell;
    }
    cell.width = uint8_t(source.width > 0 ? source.width : 1);
    int n = 0;
    while (n < VTERM_MAX_CHARS_PER_CELL && source.chars[n] != 0) {
        ++n;
    }
    if (n == 1) {
        cell.ch = source.chars[0];
    } else if (n > 1) {
        const QString text = QString::fromUcs4(reinterpret_cast<const char32_t *>(source.chars), n);
        auto it = m_clusterIndex.constFind(text);
        if (it == m_clusterIndex.constEnd()) {
            if (m_clusters.size() >= kMaxClusters) {
                cell.ch = source.chars[0]; // the table is full: the base character alone
                return cell;
            }
            it = m_clusterIndex.insert(text, int(m_clusters.size()));
            m_clusters.push_back(text);
        }
        cell.ch = vt::kClusterBase + char32_t(it.value());
    }
    return cell;
}

void VtCore::toVTerm(const Cell &source, VTermScreenCell &target) const
{
    std::fill(std::begin(target.chars), std::end(target.chars), 0u);
    if (source.width == 0) {
        target.chars[0] = uint32_t(-1);
    } else if (source.ch >= vt::kClusterBase) {
        const QList<uint> chars = clusterText(source.ch).toUcs4();
        for (int i = 0; i < std::min<qsizetype>(chars.size(), VTERM_MAX_CHARS_PER_CELL); ++i) {
            target.chars[i] = chars[i];
        }
    } else {
        target.chars[0] = source.ch;
    }
    target.width = char(source.width == 0 ? 1 : source.width);
    target.attrs = {};
    target.attrs.bold = !!(source.attrs & vt::Bold);
    target.attrs.italic = !!(source.attrs & vt::Italic);
    target.attrs.blink = !!(source.attrs & vt::Blink);
    target.attrs.reverse = !!(source.attrs & vt::Reverse);
    target.attrs.conceal = !!(source.attrs & vt::Conceal);
    target.attrs.strike = !!(source.attrs & vt::Strike);
    target.attrs.small = !!(source.attrs & vt::Small);
    target.attrs.faint = !!(source.attrs & vt::Faint);
    target.attrs.overline = !!(source.attrs & vt::Overline);
    target.attrs.underline = vt::underlineOf(source.attrs);

    VTermColor defaultFg, defaultBg;
    vterm_state_get_default_colors(m_state, &defaultFg, &defaultBg);
    const auto fill = [](vt::Color c, const VTermColor &fallback, VTermColor &out) {
        if (vt::isDefault(c)) {
            out = fallback;
        } else if (vt::isIndexed(c)) {
            vterm_color_indexed(&out, vt::colorIndex(c));
        } else {
            vterm_color_rgb(&out, uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c));
        }
    };
    fill(source.fg, defaultFg, target.fg);
    fill(source.bg, defaultBg, target.bg);
}

void VtCore::syncCursor()
{
    VTermPos pos;
    vterm_state_get_cursorpos(m_state, &pos);
    if (pos.row != m_cursor.row || pos.col != m_cursor.col) {
        m_cursor = pos;
        m_dirty = true;
    }
}

void VtCore::finish()
{
    if (!m_out.isEmpty()) {
        const QByteArray out = std::move(m_out);
        m_out.clear();
        Q_EMIT output(out);
    }
    if (m_dirty) {
        m_dirty = false;
        ++m_generation;
        Q_EMIT contentChanged();
    }
    if (m_modesDirty) {
        m_modesDirty = false;
        Q_EMIT modesChanged();
    }
}
