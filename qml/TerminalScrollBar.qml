import QtQuick
import QtQuick.Shapes
import Win11Terminal

// The scrollbar of a terminal: a 16px track at the right edge with an arrow at each
// end. Like the original's it shows only while the pointer is over it (measured:
// the wheel alone does not bring it up).
Item {
    id: root

    property var view
    property bool alwaysShown: false   // scrollbarState "always"; "hidden" hides the item
    readonly property real total: view ? view.historyLines + view.rows : 1
    readonly property real trackTop: 16
    readonly property real trackHeight: Math.max(1, height - 32)
    readonly property real thumbHeight: Math.max(24, trackHeight * (view ? view.rows : 1) / total)
    readonly property real thumbTravel: Math.max(1, trackHeight - thumbHeight)
    // scrollOffset 0 is the bottom: the thumb rests at the bottom of the track.
    readonly property real thumbY: view && view.historyLines > 0
                                   ? trackTop + thumbTravel * (1 - view.scrollOffset / view.historyLines) : trackTop + thumbTravel

    width: 16
    opacity: alwaysShown || hover.hovered || dragging ? 1 : 0   // not `visible`: a hidden item never gets the hover that shows it
    property bool dragging: false

    HoverHandler { id: hover }

    Rectangle { anchors.fill: parent; color: Theme.flyout }

    component Arrow: Item {
        id: arrow
        property bool down: false
        signal pressedOnce
        width: 16; height: 16
        Shape {
            anchors.centerIn: parent
            width: 8; height: 6
            ShapePath {
                fillColor: "#9f9f9f"
                strokeColor: "transparent"
                PathMove { x: 0; y: arrow.down ? 0 : 6 }
                PathLine { x: 8; y: arrow.down ? 0 : 6 }
                PathLine { x: 4; y: arrow.down ? 6 : 0 }
            }
        }
        MouseArea {
            anchors.fill: parent
            onPressed: { arrow.pressedOnce(); repeat.start() }
            onReleased: repeat.stop()
            onCanceled: repeat.stop()
            Timer { id: repeat; interval: 60; repeat: true; onTriggered: arrow.pressedOnce() }
        }
    }
    Arrow { y: 0; onPressedOnce: if (root.view) root.view.scrollLines(1) }
    Arrow { y: root.height - 16; down: true; onPressedOnce: if (root.view) root.view.scrollLines(-1) }

    Rectangle {
        id: thumb
        x: 3; width: 10
        y: root.thumbY
        height: root.thumbHeight
        radius: 5
        color: drag.pressed ? "#8a8a8a" : "#5f5f5f"
        visible: root.view && root.view.historyLines > 0
        MouseArea {
            id: drag
            anchors.fill: parent
            preventStealing: true
            property real grabY: 0
            onPressed: function(mouse) { grabY = mouse.y; root.dragging = true }
            onReleased: root.dragging = false
            onPositionChanged: function(mouse) {
                if (!pressed || !root.view) return
                const p = mapToItem(root, 0, mouse.y - grabY).y - root.trackTop
                const frac = Math.max(0, Math.min(1, p / root.thumbTravel))
                root.view.scrollTo(Math.round((1 - frac) * root.view.historyLines))
            }
        }
    }
    // A click on the track above or below the thumb pages.
    MouseArea {
        anchors.fill: parent
        anchors.topMargin: 16; anchors.bottomMargin: 16
        z: -1
        onPressed: function(mouse) {
            if (!root.view) return
            const y = mouse.y + 16
            root.view.scrollPages(y < root.thumbY ? 1 : -1)
        }
    }
}
