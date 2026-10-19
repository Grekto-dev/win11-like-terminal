import QtQuick

// The WinUI toggle: 40x20; off is an outline with a light knob, on is the accent
// with a black knob (measured, NOTES.md section 5).
Item {
    id: root
    property bool checked: false
    signal toggled(bool value)
    implicitWidth: 40
    implicitHeight: 20

    Rectangle {
        anchors.fill: parent
        radius: 10
        color: root.checked ? Theme.accentLight : "#2e2e2e"
        border.width: root.checked ? 0 : 1
        border.color: "#959595"
    }
    Rectangle {
        y: 4; width: 12; height: 12; radius: 6
        x: root.checked ? 24 : 4
        color: root.checked ? "black" : "#cfcfcf"
        Behavior on x { NumberAnimation { duration: 100 } }
    }
    MouseArea { anchors.fill: parent; onClicked: root.toggled(!root.checked) }
    activeFocusOnTab: true
    Keys.onSpacePressed: root.toggled(!root.checked)
}
