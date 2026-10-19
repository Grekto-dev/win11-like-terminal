import QtQuick
import Win11Terminal

// The command palette (Ctrl+Shift+P). Measured on the VM (NOTES.md section 4): 651x452,
// 10px under the title bar, centred; a 34px field, rows of 40px with the name and, under
// it, the English name; the shortcut in a small outlined box on the right.
Item {
    id: root

    signal closed
    property string query: ""
    property int current: 0
    visible: false

    function open() {
        query = ""
        field.text = ""
        current = 0
        visible = true
        field.forceActiveFocus()
    }
    function close() {
        visible = false
        closed()
    }
    function toggle() { visible ? close() : open() }

    // What matches the query, best first; alphabetical when the query is empty.
    function normalize(s) { return s.normalize("NFD").replace(/[̀-ͯ]/g, "").toLowerCase() }
    function score(entry, q) {
        const hay = normalize(entry.name), eng = normalize(entry.english)
        let best = -1
        for (const h of [hay, eng]) {
            let pos = 0, s = 0, last = -2, ok = true
            for (const ch of q) {
                const i = h.indexOf(ch, pos)
                if (i < 0) { ok = false; break }
                s += (i === last + 1 ? 5 : 1) + (i === 0 || h[i - 1] === " " ? 3 : 0)
                last = i; pos = i + 1
            }
            if (ok && s > best) best = s
        }
        return best
    }
    readonly property var results: {
        const q = normalize(query.trim())
        const list = Actions.catalog.map(function(e) { return { e: e, s: q.length ? score(e, q) : 0 } })
                                    .filter(function(r) { return r.s >= 0 })
        list.sort(function(a, b) { return b.s - a.s || a.e.name.localeCompare(b.e.name) })
        return list.map(function(r) { return r.e })
    }
    onResultsChanged: current = 0

    // Click outside closes it.
    MouseArea { anchors.fill: parent; onClicked: root.close() }

    Rectangle {
        id: panel
        width: 651
        height: 452
        anchors.horizontalCenter: parent.horizontalCenter
        y: 10
        radius: 8
        color: Theme.flyout
        border.width: 1
        border.color: Theme.flyoutBorder
        MouseArea { anchors.fill: parent }   // clicks inside are not "outside"

        Rectangle {
            id: fieldBox
            x: 8; y: 8
            width: parent.width - 16; height: 34
            radius: 4
            color: Theme.fieldBackground
            Text {
                id: prompt
                x: 9
                anchors.verticalCenter: parent.verticalCenter
                text: ">"
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 14
            }
            TextInput {
                id: field
                anchors.left: prompt.right
                anchors.leftMargin: 3
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 14
                selectionColor: Theme.accentLight
                selectedTextColor: "black"
                onTextChanged: root.query = text
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Escape) { root.close(); event.accepted = true }
                    else if (event.key === Qt.Key_Down) { root.current = Math.min(root.results.length - 1, root.current + 1); event.accepted = true }
                    else if (event.key === Qt.Key_Up) { root.current = Math.max(0, root.current - 1); event.accepted = true }
                    else if (event.key === Qt.Key_PageDown) { root.current = Math.min(root.results.length - 1, root.current + 9); event.accepted = true }
                    else if (event.key === Qt.Key_PageUp) { root.current = Math.max(0, root.current - 9); event.accepted = true }
                    else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { root.run(root.current); event.accepted = true }
                }
                Text {
                    visible: field.text.length === 0
                    text: qsTr("Type a command name...")
                    color: Theme.textPlaceholder
                    font: field.font
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Rectangle {   // focus underline
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                height: 2; radius: 1; color: Theme.accentLight
            }
        }

        ListView {
            id: list
            x: 0; y: 50
            width: parent.width
            height: parent.height - 50 - 4
            clip: true
            model: root.results
            currentIndex: root.current
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
            onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
            delegate: Item {
                id: row
                required property int index
                required property var modelData
                width: list.width
                height: 40
                readonly property bool selected: index === root.current
                readonly property string keys: AppSettings.keysFor(modelData.id)
                Rectangle {
                    x: 8; y: 2; width: parent.width - 16; height: 36
                    radius: 4
                    color: row.selected ? Theme.flyoutHover : "transparent"
                    Rectangle {   // the accent mark of the selected row
                        visible: row.selected
                        x: 0; y: 12; width: 3; height: 12; radius: 1.5
                        color: Theme.accentLight
                    }
                }
                Column {
                    x: 36
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 0
                    Text {
                        text: row.modelData.name
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: 14
                    }
                    Text {
                        visible: row.modelData.name !== row.modelData.english
                        text: row.modelData.english
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                    }
                }
                Rectangle {
                    visible: row.keys.length > 0
                    anchors.right: parent.right
                    anchors.rightMargin: 24
                    anchors.verticalCenter: parent.verticalCenter
                    width: chip.width + 12; height: 20
                    radius: 4
                    color: "transparent"
                    border.width: 1
                    border.color: Theme.chipBorder
                    Text {
                        id: chip
                        anchors.centerIn: parent
                        text: row.keys
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onPositionChanged: root.current = row.index
                    onClicked: root.run(row.index)
                }
            }
        }
    }

    function run(index) {
        const e = results[index]
        if (!e) return
        close()
        // After the palette has closed and the focus is back where it was.
        Qt.callLater(function() { Actions.run(e.id, {}, true) })
    }
}
