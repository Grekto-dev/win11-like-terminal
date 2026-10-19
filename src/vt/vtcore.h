#pragma once

#include "cell.h"
#include "historyline.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

#include <deque>
#include <vector>
#include <vterm.h>

/**
 * The terminal emulation: bytes from the program in, a grid of cells out, and
 * keys/mouse/paste in the other direction as the bytes a program expects.
 * libvterm interprets the escape sequences; what it scrolls off the top is kept
 * here as the scrollback.
 *
 * Rows are addressed "absolutely": 0 is the oldest line of the scrollback and
 * historyCount() is the first line of the screen. That is the coordinate the
 * view, the selection and the search share; it does not change while the
 * program only prints (a line keeps its number until it falls off the history).
 */
class VtCore : public QObject
{
    Q_OBJECT

public:
    enum class CursorShape { Block, Underline, Bar };
    enum class MouseMode { None, Click, Drag, Move };
    Q_ENUM(CursorShape)

    VtCore(int rows, int cols, int scrollbackLimit = 9001, QObject *parent = nullptr);
    ~VtCore() override;

    // --- Program output in ------------------------------------------------
    void write(const QByteArray &bytes);
    void resize(int rows, int cols);

    // --- Content ------------------------------------------------------------
    int rows() const { return m_rows; }
    int cols() const { return m_cols; }
    int historyCount() const { return int(m_history.size()); }
    int totalRows() const { return historyCount() + m_rows; }
    /** Lines that fell off the history so far: `absRow + scrolledOff()` names a line for good. */
    quint64 scrolledOff() const { return m_dropped; }
    /** Blank cell outside the grid. By value: the history is stored packed. */
    vt::Cell cell(int absRow, int col) const;
    QString clusterText(char32_t ch) const;
    /** True when the row (history or screen) continues the one above it: the two are one wrapped line. */
    bool continuation(int absRow) const;
    /** The text of `col0..col1` (exclusive) of a row, without the trailing blanks. */
    QString rowText(int absRow, int col0 = 0, int col1 = -1) const;
    /** Bumps on every change of content, cursor or scrollback (a cheap "did it change?"). */
    quint64 generation() const { return m_generation; }
    void setScrollbackLimit(int lines);
    void clearScrollback();

    // --- State ---------------------------------------------------------------
    int cursorRow() const { return m_cursor.row; }
    int cursorCol() const { return m_cursor.col; }
    bool cursorVisible() const { return m_cursorVisible; }
    bool cursorBlink() const { return m_cursorBlink; }
    CursorShape cursorShape() const { return m_cursorShape; }
    /** True once the program itself chose a shape (DECSCUSR); before, the profile decides. */
    bool cursorShapeFromProgram() const { return m_shapeFromProgram; }
    bool altScreen() const { return m_altScreen; }
    MouseMode mouseMode() const { return m_mouseMode; }
    bool focusReporting() const { return m_focusReport; }
    bool reverseScreen() const { return m_reverse; }
    /** The program asked for bracketed paste (it can tell a paste from typing). */
    bool bracketedPaste() const;
    QString title() const { return m_title; }

    // --- Input to the program -------------------------------------------------
    void key(VTermKey key, VTermModifier mods);
    void unichar(char32_t c, VTermModifier mods);
    /** Sends a paste; control characters in it are dropped (see filterPaste). */
    void paste(const QString &text);
    /**
     * What of a paste reaches the program: no escape (it could end a bracketed paste and run what
     * follows as typed) nor any other C0/C1 control or DEL, except tab and line ends.
     */
    static QString filterPaste(const QString &text);
    void mouseMove(int row, int col, VTermModifier mods);
    void mouseButton(int button, bool pressed, VTermModifier mods);
    /**
     * The wheel, translated for the program: `notches` > 0 is up. With mouse reporting on, one button 4/5
     * (64/65 on the wire) press per notch at the cell (row, col); on the alternate screen without mouse
     * reporting, one Up/Down key per notch (application cursor mode applies). Returns false when the
     * program takes none of it, and the caller scrolls its own history.
     */
    bool wheel(int notches, int row, int col, VTermModifier mods);
    void focus(bool in);

Q_SIGNALS:
    /** Bytes the program should receive (keys, answers to its queries). */
    void output(const QByteArray &bytes);
    void titleChanged(const QString &title);
    void bell();
    /** The grid, the cursor or the history changed. */
    void contentChanged();
    /** A mode changed: alternate screen, mouse, cursor look. */
    void modesChanged();

private:
    static int onDamage(VTermRect rect, void *user);
    static int onMoveRect(VTermRect dest, VTermRect src, void *user);
    static int onMoveCursor(VTermPos pos, VTermPos oldPos, int visible, void *user);
    static int onSetTermProp(VTermProp prop, VTermValue *val, void *user);
    static int onBell(void *user);
    static int onResize(int rows, int cols, void *user);
    static int onPushLine(int cols, const VTermScreenCell *cells, void *user);
    static int onPopLine(int cols, VTermScreenCell *cells, void *user);
    static int onClearScrollback(void *user);
    static void onOutput(const char *bytes, size_t len, void *user);

    void refresh(VTermRect rect);
    void markDirty(VTermRect rect);
    void moveCells(VTermRect dest, VTermRect src);
    void syncDirty();
    vt::Cell convert(const VTermScreenCell &source);
    void toVTerm(const vt::Cell &source, VTermScreenCell &target) const;
    static vt::Color convert(const VTermColor &color);
    void finish();
    void syncCursor();

    VTerm *m_vterm = nullptr;
    VTermScreen *m_screen = nullptr;
    VTermState *m_state = nullptr;
    int m_rows;
    int m_cols;
    int m_limit;
    QVector<vt::Cell> m_grid; // the screen, m_rows * m_cols
    std::deque<vt::HistoryLine> m_history;
    std::vector<vt::Cell> m_lineBuffer; // a scrolled-off line, converted before it is packed
    QVector<QString> m_clusters;   // grows with what the program prints: capped (see convert())
    QHash<QString, int> m_clusterIndex;
    QByteArray m_out;
    QString m_title;
    QString m_titleFragment;
    VTermPos m_cursor = {0, 0};
    bool m_cursorVisible = true;
    bool m_cursorBlink = true;
    CursorShape m_cursorShape = CursorShape::Block;
    bool m_shapeFromProgram = false;
    bool m_ready = false;
    bool m_altScreen = false;
    bool m_focusReport = false;
    bool m_reverse = false;
    MouseMode m_mouseMode = MouseMode::None;
    quint64 m_generation = 1;
    quint64 m_dropped = 0;
    int m_dirtyFirst = 0;
    int m_dirtyLast = 0;
    bool m_dirty = false;
    bool m_modesDirty = false;
    vt::Cell m_blank;
};
