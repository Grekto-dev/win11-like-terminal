import QtQuick
import QtQuick.Controls.Basic
import Win11Terminal

// The pane's search box (Ctrl+Shift+F), at the top right of the pane. Measured on
// the VM: a 592x50 box (border included) 7px from the top and 24 from the right,
// with the field, "n/m" or "No results found", and up, down, match case, regex, close.
Rectangle {
    id: root

    property var view                 // the TerminalView searched
    property bool searchUp: true      // the direction of Enter (the "↑" button starts on)
    property bool matchCase: false
    property bool useRegex: false
    signal closed

    width: 592
    height: 50
    radius: 8
    color: Theme.flyout
    border.width: 1
    border.color: Theme.flyoutBorder
    visible: false

    function open() {
        visible = true
        field.forceActiveFocus()
        field.selectAll()
        if (field.text.length > 0) search()
    }
    function close() {
        visible = false
        if (view) view.clearFind()
        closed()
    }
    function search() {
        searchDelay.stop()
        if (view) view.find(field.text, matchCase, useRegex)
    }
    Timer { id: searchDelay; interval: 150; onTriggered: root.search() }
    function step(up) {
        if (searchDelay.running) search() // Enter right after typing: the search first
        if (view) view.findNext(!up)
    }

    // The field.
    Rectangle {
        x: 8; y: 9
        width: 158; height: 31
        radius: 4
        color: Theme.fieldBackground
        TextInput {
            id: field
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: clear.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -1
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: 14
            selectionColor: Theme.accentLight
            selectedTextColor: "black"
            clip: true
            // After a pause in the typing, not at every key: each search reads the whole history.
            onTextChanged: searchDelay.restart()
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Escape) { root.close(); event.accepted = true }
                else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    root.step((event.modifiers & Qt.ShiftModifier) ? !root.searchUp : root.searchUp)
                    event.accepted = true
                }
            }
            Text {
                visible: field.text.length === 0
                text: qsTr("Find")
                color: Theme.textPlaceholder
                font: field.font
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Item {
            id: clear
            anchors.right: parent.right
            anchors.rightMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            width: 24; height: 24
            visible: field.text.length > 0
            Glyph { anchors.centerIn: parent; name: "tabClose"; color: Theme.textSecondary }
            MouseArea { anchors.fill: parent; onClicked: { field.text = ""; field.forceActiveFocus() } }
        }
        // The accent underline of a focused field.
        Rectangle {
            visible: field.activeFocus
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 2
            color: Theme.accentLight
            radius: 1
        }
    }

    // "3/4", or the message.
    Text {
        x: 178
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: 14
        visible: field.text.length > 0 && view
        text: view && view.matchCount > 0 ? (view.matchIndex + 1) + "/" + view.matchCount : qsTr("No results found")
    }

    component Btn: Rectangle {
        id: btn
        property string glyph
        property string label
        property bool on: false
        signal clicked
        width: 28; height: 30
        radius: 4
        color: area.pressed ? Theme.pressedOverlay : (on || area.containsMouse) ? Theme.flyoutHover : "transparent"
        Glyph { anchors.centerIn: parent; visible: btn.glyph.length > 0; name: btn.glyph; color: Theme.text }
        Text {
            anchors.centerIn: parent
            visible: btn.glyph.length === 0
            text: btn.label
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: 14
        }
        MouseArea { id: area; anchors.fill: parent; hoverEnabled: true; onClicked: btn.clicked() }
    }
    Row {
        y: 9
        anchors.right: parent.right
        anchors.rightMargin: 13
        spacing: 12
        Btn { glyph: "arrowUp"; on: root.searchUp; onClicked: { root.searchUp = true; root.step(true) } }
        Btn { glyph: "arrowDown"; on: !root.searchUp; onClicked: { root.searchUp = false; root.step(false) } }
        Btn { label: "Aa"; on: root.matchCase; onClicked: { root.matchCase = !root.matchCase; root.search() } }
        Btn { label: "^$"; on: root.useRegex; onClicked: { root.useRegex = !root.useRegex; root.search() } }
        Btn { glyph: "close"; onClicked: root.close() }
    }
}
