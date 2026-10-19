import QtQuick
import Win11Terminal

// One row of the settings page: a title and a description on the left, its control on the
// right (put into `control`). 998px wide at most, at least 62px tall, radius 4, #333333.
Rectangle {
    id: root

    property string title
    property string description
    property string icon            // an optional 20px Fluent icon before the text
    default property alias control: slot.data
    property real controlWidth: 0
    property alias slotItem: slot
    property bool clickable: false
    signal clicked

    implicitHeight: Math.max(62, textColumn.implicitHeight + 26)
    radius: 4
    color: area.containsMouse && clickable ? Theme.cardHover : Theme.card
    border.width: 1
    border.color: Theme.cardBorder

    Column {
        id: textColumn
        x: root.icon.length > 0 ? 50 : 16
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - (root.icon.length > 0 ? 66 : 32) - (slot.childrenRect.width > 0 ? slot.childrenRect.width + 24 : 0)
        spacing: 2
        Text {
            width: parent.width
            text: root.title
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: 14
            wrapMode: Text.Wrap
        }
        Text {
            visible: root.description.length > 0
            width: parent.width
            text: root.description
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 12
            lineHeight: 1.1
            wrapMode: Text.Wrap
        }
    }
    FluentSymbol { visible: root.icon.length > 0; x: 16; anchors.verticalCenter: parent.verticalCenter; name: root.icon; size: 20; variant: "regular"; color: Theme.text }
    Item {
        id: slot
        anchors.right: parent.right
        anchors.rightMargin: 17
        anchors.verticalCenter: parent.verticalCenter
        width: childrenRect.width
        height: childrenRect.height
    }
    MouseArea { id: area; anchors.fill: parent; hoverEnabled: root.clickable; enabled: root.clickable; z: -1; onClicked: root.clicked() }
}
