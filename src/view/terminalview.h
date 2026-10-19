#pragma once

#include <QMargins>
#include <QPointF>
#include <QPointer>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QQuickItem>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "colorscheme.h"
#include "glyphatlas.h"

class TerminalSession;
class VtCore;

/**
 * Draws a TerminalSession's grid on the scene graph and turns keys, the mouse
 * and the wheel into what the program expects. The cell is a whole number of
 * physical pixels (measured on Windows Terminal: 9x19 for Cascadia Mono 12pt at
 * 100%); the grid is as many cells as fit inside the padding.
 */
class TerminalView : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(TerminalSession *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontChanged)
    Q_PROPERTY(qreal fontSize READ fontSize WRITE setFontSize NOTIFY fontChanged) // points, as in profiles
    Q_PROPERTY(qreal lineHeight READ lineHeight WRITE setLineHeight NOTIFY fontChanged)
    Q_PROPERTY(qreal cellWidthOverride READ cellWidthOverride WRITE setCellWidthOverride NOTIFY fontChanged)
    Q_PROPERTY(QVariant fontWeight READ fontWeight WRITE setFontWeight NOTIFY fontChanged)
    Q_PROPERTY(QVariantMap fontAxes READ fontAxes WRITE setFontAxes NOTIFY fontChanged)
    Q_PROPERTY(QVariantMap fontFeatures READ fontFeatures WRITE setFontFeatures NOTIFY fontChanged)
    Q_PROPERTY(bool useColorEmoji READ useColorEmoji WRITE setUseColorEmoji NOTIFY fontChanged)
    Q_PROPERTY(QString intenseTextStyle READ intenseTextStyle WRITE setIntenseTextStyle NOTIFY textStyleChanged)
    Q_PROPERTY(QString adjustIndistinguishableColors READ adjustIndistinguishableColors WRITE setAdjustIndistinguishableColors NOTIFY textStyleChanged)
    Q_PROPERTY(QString padding READ padding WRITE setPadding NOTIFY gridChanged)  // DIPs: "8", "8, 4" or "left, top, right, bottom", as in profiles
    Q_PROPERTY(bool snapToGrid READ snapToGrid WRITE setSnapToGrid NOTIFY gridChanged)
    Q_PROPERTY(QString cursorShape READ cursorShape WRITE setCursorShape NOTIFY cursorShapeChanged)
    Q_PROPERTY(QColor background READ background NOTIFY schemeChanged)
    Q_PROPERTY(qreal backgroundOpacity READ backgroundOpacity WRITE setBackgroundOpacity NOTIFY schemeChanged)
    Q_PROPERTY(int columns READ columns NOTIFY gridChanged)
    Q_PROPERTY(int rows READ rows NOTIFY gridChanged)
    Q_PROPERTY(qreal cellWidth READ cellWidth NOTIFY gridChanged)  // logical pixels
    Q_PROPERTY(qreal cellHeight READ cellHeight NOTIFY gridChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(bool snapOnInput READ snapOnInput WRITE setSnapOnInput NOTIFY selectionChanged)
    Q_PROPERTY(bool markMode READ markMode NOTIFY markModeChanged)
    Q_PROPERTY(bool detectUrls READ detectUrls WRITE setDetectUrls NOTIFY selectionChanged)
    Q_PROPERTY(bool builtinGlyphs READ builtinGlyphs WRITE setBuiltinGlyphs NOTIFY fontChanged)   // box and block characters drawn by the program
    Q_PROPERTY(bool scrollToZoom READ scrollToZoom WRITE setScrollToZoom NOTIFY selectionChanged)               // Ctrl + wheel asks for a font size change
    Q_PROPERTY(bool scrollToChangeOpacity READ scrollToChangeOpacity WRITE setScrollToChangeOpacity NOTIFY selectionChanged)   // Ctrl+Shift + wheel, for the opacity
    Q_PROPERTY(bool rightClickMenu READ rightClickMenu WRITE setRightClickMenu NOTIFY selectionChanged)   // right click opens the context menu instead of copy/paste
    Q_PROPERTY(bool copyOnSelect READ copyOnSelect WRITE setCopyOnSelect NOTIFY selectionChanged)
    Q_PROPERTY(QString wordDelimiters READ wordDelimiters WRITE setWordDelimiters NOTIFY selectionChanged)
    Q_PROPERTY(int scrollOffset READ scrollOffset NOTIFY scrollChanged)   // lines above the bottom
    Q_PROPERTY(int historyLines READ historyLines NOTIFY scrollChanged)
    Q_PROPERTY(int matchCount READ matchCount NOTIFY findChanged)
    Q_PROPERTY(int matchIndex READ matchIndex NOTIFY findChanged)         // 0-based; -1 without a current one
    // warning.multiLinePaste (true/"automatic", "always", false/"never") and warning.largePaste, as in settings.json
    Q_PROPERTY(QVariant multiLinePasteWarning READ multiLinePasteWarning WRITE setMultiLinePasteWarning NOTIFY selectionChanged)
    Q_PROPERTY(bool largePasteWarning READ largePasteWarning WRITE setLargePasteWarning NOTIFY selectionChanged)

public:
    explicit TerminalView(QQuickItem *parent = nullptr);
    ~TerminalView() override;

    TerminalSession *session() const { return m_session; }
    void setSession(TerminalSession *session);
    QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString &family);
    qreal fontSize() const { return m_fontSize; }
    void setFontSize(qreal points);
    qreal lineHeight() const { return m_lineHeight; }
    void setLineHeight(qreal value);
    qreal cellWidthOverride() const { return m_cellWidthOverride; }
    void setCellWidthOverride(qreal value);
    QVariant fontWeight() const { return m_fontWeight; }
    void setFontWeight(const QVariant &value);
    QVariantMap fontAxes() const { return m_fontAxes; }
    void setFontAxes(const QVariantMap &value);
    QVariantMap fontFeatures() const { return m_fontFeatures; }
    void setFontFeatures(const QVariantMap &value);
    bool useColorEmoji() const { return m_useColorEmoji; }
    void setUseColorEmoji(bool on);
    QString intenseTextStyle() const { return m_intenseTextStyle; }
    void setIntenseTextStyle(const QString &value);
    QString adjustIndistinguishableColors() const { return m_adjustIndistinguishableColors; }
    void setAdjustIndistinguishableColors(const QString &value);
    QString padding() const { return m_paddingText; }
    void setPadding(const QString &padding);
    bool snapToGrid() const { return m_snapToGrid; }
    void setSnapToGrid(bool on) { if (m_snapToGrid != on) { m_snapToGrid = on; updateWindowHints(); Q_EMIT gridChanged(); } }
    QString cursorShape() const { return m_cursorShape; }
    void setCursorShape(const QString &shape);
    QColor background() const { return m_scheme.background; }
    qreal backgroundOpacity() const { return m_bgOpacity; }
    void setBackgroundOpacity(qreal o) { o = std::clamp(o, 0.0, 1.0); if (!qFuzzyCompare(o, m_bgOpacity)) { m_bgOpacity = o; repaint(); Q_EMIT schemeChanged(); } }
    int columns() const { return m_columns; }
    int rows() const { return m_rows; }
    qreal cellWidth() const;
    qreal cellHeight() const;

    Q_INVOKABLE void setColorScheme(const QVariantMap &scheme);
    /** Pastes the clipboard; a paste the settings warn about asks first (pasteConfirmationRequested). */
    Q_INVOKABLE void paste();
    /** Pastes `text` as it is, without asking (after the user confirmed). */
    Q_INVOKABLE void pasteText(const QString &text);
    /** Copies the selection (and clears it, like the original). False without a selection. */
    Q_INVOKABLE bool copy(bool keepSelection = false);
    Q_INVOKABLE void selectAll();
    /** Keyboard selection (Ctrl+Shift+M): arrows move a mark, Shift extends, Enter copies, Esc leaves. */
    Q_INVOKABLE void toggleMarkMode();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QString selectedText() const;
    /** Where the keyboard-invoked context menu hangs: the end of the selection, else the cursor (the cell's left edge, the row's top). */
    Q_INVOKABLE QPointF contextAnchor() const;
    /** Writes the whole buffer (history and screen) as text, trailing blanks trimmed. */
    Q_INVOKABLE bool exportText(const QString &path) const;
    Q_INVOKABLE void scrollTo(int offsetFromBottom);
    Q_INVOKABLE void scrollLines(int lines) { scrollBy(lines); }
    Q_INVOKABLE void scrollPages(int pages) { scrollBy(pages * std::max(1, m_rows - 1)); }
    Q_INVOKABLE void scrollToTop();
    Q_INVOKABLE void scrollToBottom();
    Q_INVOKABLE void clearBuffer();
    /** Finds every match and selects the first one at or below the view; returns how many. */
    Q_INVOKABLE int find(const QString &text, bool matchCase = false, bool regex = false);
    Q_INVOKABLE void findNext(bool forward = true);
    Q_INVOKABLE void clearFind();
    bool snapOnInput() const { return m_snapOnInput; }
    void setSnapOnInput(bool on) { m_snapOnInput = on; }
    bool markMode() const { return m_mark; }
    bool detectUrls() const { return m_detectUrls; }
    void setDetectUrls(bool on) { m_detectUrls = on; }
    bool hasSelection() const { return m_selMode != SelMode::None && !selectionEmpty(); }
    bool builtinGlyphs() const { return m_builtinGlyphs; }
    void setBuiltinGlyphs(bool on);
    bool scrollToZoom() const { return m_scrollToZoom; }
    void setScrollToZoom(bool on) { m_scrollToZoom = on; }
    bool scrollToChangeOpacity() const { return m_scrollToOpacity; }
    void setScrollToChangeOpacity(bool on) { m_scrollToOpacity = on; }
    bool rightClickMenu() const { return m_rightClickMenu; }
    void setRightClickMenu(bool on) { m_rightClickMenu = on; }
    bool copyOnSelect() const { return m_copyOnSelect; }
    void setCopyOnSelect(bool on) { m_copyOnSelect = on; }
    QString wordDelimiters() const { return m_wordDelimiters; }
    void setWordDelimiters(const QString &d) { m_wordDelimiters = d; }
    int scrollOffset() const { return m_scrollOffset; }
    int historyLines() const;
    int matchCount() const { return int(m_matches.size()); }
    QVariant multiLinePasteWarning() const { return m_multiLinePasteWarning; }
    void setMultiLinePasteWarning(const QVariant &v) { m_multiLinePasteWarning = v; }
    bool largePasteWarning() const { return m_largePasteWarning; }
    void setLargePasteWarning(bool on) { m_largePasteWarning = on; }
    int matchIndex() const { return m_matchIndex; }

Q_SIGNALS:
    void sessionChanged();
    void fontChanged();
    void textStyleChanged();
    void gridChanged();
    void cursorShapeChanged();
    void schemeChanged();
    void selectionChanged();
    void markModeChanged();
    void scrollChanged();
    void findChanged();
    /** Ctrl+Shift+F: the pane's search box should open. */
    void findRequested();
    /** Ctrl + wheel: font size steps (+1 a notch up); Ctrl+Shift + wheel: opacity steps (the pane keeps the offsets). */
    void zoomRequested(int steps);
    void opacityRequested(int steps);
    /** A paste to confirm: it has several lines (and the program cannot tell it from typing) or is large. */
    void pasteConfirmationRequested(const QString &text, bool multiLine, bool large);
    /** Right click with `rightClickMenu` on: the position, in the item's coordinates. */
    void contextMenuRequested(const QPointF &position);

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    void componentComplete() override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;

private:
    qreal dpr() const;
    /** update(), and the next frame rebuilds the cells (not only the cursor layer). */
    void repaint()
    {
        m_fullDirty = true;
        update();
    }
    const GlyphAtlas::Metrics &metrics() const
    {
        static const GlyphAtlas::Metrics none;
        return m_atlas ? m_atlas->metrics() : none;
    }
    void rebuildFont();
    void recomputeGrid();
    void updateWindowHints();
    void onContentChanged();
    void restartBlink();
    VtCore *core() const;
    void scrollBy(int lines);
    void requestPaste(const QString &text);

    // --- selection and search ---
    enum class SelMode { None, Char, Word, Line, Block };
    struct Pos
    {
        qint64 line = 0; // absRow + scrolledOff at the time: stable while the history scrolls
        int col = 0;     // a boundary: the position before cell `col`
        bool operator<(const Pos &o) const { return line != o.line ? line < o.line : col < o.col; }
        bool operator==(const Pos &o) const { return line == o.line && col == o.col; }
    };
    struct Match
    {
        qint64 line;
        int col0, col1;
    };
    void handlePress(QMouseEvent *event, int forcedCount);
    void handleMarkKey(QKeyEvent *event);
    void moveMark(int dRow, int dCol, bool extend);
    struct Url { qint64 line = -1; int col0 = 0, col1 = 0; QString text; };
    Url urlAt(qint64 line, int col) const;
    Pos posAt(const QPointF &p, bool boundary) const;
    bool appWantsMouse(Qt::KeyboardModifiers m) const;
    void forwardMouse(QMouseEvent *event, bool press, bool release);
    bool selectionEmpty() const;
    bool selectionSpan(qint64 line, int &c0, int &c1) const; // columns [c0, c1) of a line, if any
    void wordBounds(qint64 line, int col, int &c0, int &c1) const;
    void extendSelection(const Pos &to);
    void autoScrollTick();
    int cellClass(const vt::Cell &cell) const;
    qint64 lineOf(int absRow) const;
    int absOf(qint64 line) const;

    QPointer<TerminalSession> m_session;
    bool m_snapOnInput = true;
    bool m_mark = false;
    bool m_markSelecting = false;
    Pos m_markCursor, m_markAnchor;
    bool m_detectUrls = true;
    bool m_rightClickMenu = false;
    qreal m_wheelLines = 0;   // wheel remainder, in lines (scrollback)
    qreal m_wheelNotches = 0; // wheel remainder, in notches (program)
    bool m_scrollToZoom = true;
    bool m_scrollToOpacity = true;
    qreal m_zoomRemainder = 0;
    Url m_hoverUrl;
    SelMode m_selMode = SelMode::None;
    Pos m_selAnchor, m_selHead;
    Pos m_wordStart, m_wordEnd;    // the word/line first picked, when extending by words/lines
    bool m_dragging = false;
    bool m_copyOnSelect = false;
    QVariant m_multiLinePasteWarning = QStringLiteral("automatic");
    bool m_largePasteWarning = true;
    QString m_wordDelimiters = QStringLiteral(" /\\()\"'-.,:;<>~!@#$%^&*|+=[]{}~?\u2502");
    QElapsedTimer m_clickTimer;
    QPointF m_lastClickPos;
    int m_clickCount = 0;
    QPointF m_dragPos;
    QTimer m_autoScroll;
    std::vector<Match> m_matches;
    int m_matchIndex = -1;
    QString m_fontFamily = QStringLiteral("Cascadia Mono");
    qreal m_fontSize = 12;
    qreal m_lineHeight = 0;
    qreal m_cellWidthOverride = 0;
    QVariant m_fontWeight = QStringLiteral("normal");
    QVariantMap m_fontAxes;
    QVariantMap m_fontFeatures;
    bool m_useColorEmoji = true;
    QString m_intenseTextStyle = QStringLiteral("bright");
    QString m_adjustIndistinguishableColors = QStringLiteral("indexed");
    QString m_paddingText = QStringLiteral("8, 8, 8, 8");
    int m_padLeft = 8, m_padTop = 8, m_padRight = 8, m_padBottom = 8;   // DIPs
    QMargins paddingPx() const;   // physical pixels
    bool m_snapToGrid = true;
    bool m_sessionSized = false;
    QString m_cursorShape = QStringLiteral("bar"); // the profile's; an app may override
    ColorScheme m_scheme;
    qreal m_bgOpacity = 1.0;

    std::shared_ptr<GlyphAtlas> m_atlas; // shared by the views of the same font (GlyphAtlas::shared)
    bool m_builtinGlyphs = true;
    qreal m_atlasDpr = 0;
    /** Set by everything that changes what the cells look like; the cursor's blink alone leaves it. */
    bool m_fullDirty = true;
    int m_columns = 120;
    int m_rows = 30;
    int m_scrollOffset = 0; // lines above the bottom
    int m_lastHistory = 0;
    quint64 m_lastDropped = 0;
    bool m_cursorOn = true;
    QTimer m_blink;
};
