import QtQuick

// Minimize, maximize/restore and close: 46px wide and as tall as the bar.
Rectangle {
    id: root

    property string glyph
    property bool danger: false
    property bool windowActive: true
    signal activated

    implicitWidth: Theme.captionWidth
    property int barHeight: Theme.barHeight
    implicitHeight: barHeight - 1 // the bar's last row is its line
    color: !area.containsMouse ? "transparent"
         : danger ? (area.pressed ? Theme.closePressed : Theme.closeHover)
         : (area.pressed ? Theme.captionPressed : Theme.captionHover)

    Glyph {
        anchors.centerIn: parent
        name: root.glyph
        color: area.containsMouse && root.danger ? "white" : (root.windowActive || area.containsMouse ? Theme.text : Theme.textDisabled)
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.activated()
    }
}
