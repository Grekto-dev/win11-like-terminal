import QtQuick

// A one-line text field in the settings style: #3f3f3f, 4px corners, an accent underline
// when it has the focus.
Rectangle {
    id: root
    property string text
    property alias placeholder: hint.text
    property int boxWidth: 248
    property bool numeric: false
    property alias typed: input.text      // what is in the box now (`text` is only the value it started from)
    function clear() { input.text = "" }
    signal edited(string text)

    width: boxWidth; height: 32
    radius: 4
    color: Theme.choice

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 12; anchors.rightMargin: 12
        verticalAlignment: TextInput.AlignVCenter
        text: root.text
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: 14
        clip: true
        selectionColor: Theme.accentLight
        selectedTextColor: "black"
        inputMethodHints: root.numeric ? Qt.ImhDigitsOnly : Qt.ImhNone
        onEditingFinished: root.edited(text)
        Keys.onEscapePressed: { text = root.text; focus = false }
        Text { id: hint; visible: input.text.length === 0; color: Theme.textPlaceholder; font: input.font; anchors.verticalCenter: parent.verticalCenter }
    }
    Rectangle {
        visible: input.activeFocus
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 2; radius: 1; color: Theme.accentLight
    }
}
