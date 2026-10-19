import QtQuick

// Two panes side by side (Qt.Horizontal) or one above the other (Qt.Vertical),
// with a 4px divider that drags. The divider's colour is the tab bar's (measured).
Item {
    id: root

    property int orientation: Qt.Horizontal
    property real ratio: 0.5
    property Item first
    property Item second
    property int sides: 0
    readonly property int dividerSize: 4
    readonly property bool horizontal: orientation === Qt.Horizontal

    function place(child, x, y, w, h) {
        child.x = x; child.y = y; child.width = Math.max(0, w); child.height = Math.max(0, h)
    }
    function relayout() {
        if (!first || !second) return
        if (horizontal) {
            const a = Math.round((width - dividerSize) * ratio)
            place(first, 0, 0, a, height)
            place(second, a + dividerSize, 0, width - dividerSize - a, height)
            divider.x = a; divider.y = 0; divider.width = dividerSize; divider.height = height
        } else {
            const a = Math.round((height - dividerSize) * ratio)
            place(first, 0, 0, width, a)
            place(second, 0, a + dividerSize, width, height - dividerSize - a)
            divider.x = 0; divider.y = a; divider.width = width; divider.height = dividerSize
        }
    }
    onWidthChanged: relayout()
    onHeightChanged: relayout()
    onRatioChanged: relayout()
    onOrientationChanged: relayout()

    function adopt(child, sideBit) {
        child.anchors.fill = undefined
        child.parent = root
        child.sides = root.sides | sideBit
        relayout()
    }
    function setFirst(item) { first = item; adopt(item, horizontal ? 2 : 8) }
    function setSecond(item) { second = item; adopt(item, horizontal ? 1 : 4) }
    function replaceChild(oldItem, newItem) {
        if (first === oldItem) setFirst(newItem)
        else setSecond(newItem)
    }
    onSidesChanged: {
        if (first) first.sides = sides | (horizontal ? 2 : 8)
        if (second) second.sides = sides | (horizontal ? 1 : 4)
    }

    // Painted first, so the panes (and their focus borders) draw over it.
    Rectangle {
        id: divider
        color: Theme.barActive
    }
    MouseArea {
        z: 2
        x: divider.x - (root.horizontal ? 3 : 0)
        y: divider.y - (root.horizontal ? 0 : 3)
        width: divider.width + (root.horizontal ? 6 : 0)
        height: divider.height + (root.horizontal ? 0 : 6)
        cursorShape: root.horizontal ? Qt.SplitHCursor : Qt.SplitVCursor
        preventStealing: true
        onPositionChanged: function(mouse) {
            if (!pressed) return
            const p = mapToItem(root, mouse.x, mouse.y)
            const total = (root.horizontal ? root.width : root.height) - root.dividerSize
            const at = (root.horizontal ? p.x : p.y) - root.dividerSize / 2
            root.ratio = Math.max(0.05, Math.min(0.95, at / total))
        }
    }
}
