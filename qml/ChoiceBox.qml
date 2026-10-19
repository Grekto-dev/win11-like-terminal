import QtQuick
import QtQuick.Controls

// A combo box: the current choice and a chevron; a list opens under it. 248x32, #3f3f3f.
Rectangle {
    id: root
    property var options: []          // [{ v: value, l: label }]
    property var value
    property int boxWidth: 248
    signal chosen(var value)

    readonly property string label: {
        for (const o of options) if (o.v === value) return o.l
        return value === undefined || value === null ? "" : String(value)
    }
    width: boxWidth; height: 32
    radius: 4
    color: area.containsMouse ? Theme.choiceHover : Theme.choice

    Text {
        x: 12
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - 44
        text: root.label
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: 14
        elide: Text.ElideRight
    }
    Glyph { x: parent.width - 24; anchors.verticalCenter: parent.verticalCenter; name: "chevronDown"; color: Theme.textSecondary }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            if (popup.opened) {
                popup.close()
            } else {
                popup.reposition()
                popup.open()
            }
        }
    }

    Popup {
        id: popup
        parent: Overlay.overlay
        padding: 4
        width: root.width
        height: Math.min(column.implicitHeight + 8, 320)
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: Theme.flyout; radius: 8; border.width: 1; border.color: Theme.flyoutBorder }

        function reposition() {
            var overlay = Overlay.overlay || (root.Window ? root.Window.contentItem : null)
            if (!overlay) return
            var p = root.mapToItem(overlay, 0, 0)
            popup.x = Math.max(4, Math.min(p.x, overlay.width - popup.width - 4))
            if (p.y + root.height + popup.height + 4 <= overlay.height) {
                popup.y = p.y + root.height + 2
            } else if (p.y - popup.height - 2 >= 0) {
                popup.y = p.y - popup.height - 2
            } else {
                popup.y = Math.max(4, overlay.height - popup.height - 4)
            }
        }

        onAboutToShow: reposition()
        contentItem: Flickable {
            clip: true
            contentHeight: column.implicitHeight
            Column {
                id: column
                width: parent.width
                Repeater {
                    model: root.options
                    delegate: Item {
                        required property var modelData
                        width: column.width; height: 32
                        Rectangle {
                            anchors.fill: parent; anchors.margins: 1; radius: 4
                            color: ia.containsMouse ? Theme.flyoutHover : "transparent"
                            Rectangle {
                                visible: modelData.v === root.value
                                x: 0; y: 10; width: 3; height: 12; radius: 1.5; color: Theme.accentLight
                            }
                        }
                        Text {
                            x: 12; anchors.verticalCenter: parent.verticalCenter
                            text: modelData.l; color: Theme.text
                            font.family: Theme.fontFamily; font.pixelSize: 14
                        }
                        MouseArea { id: ia; anchors.fill: parent; hoverEnabled: true; onClicked: { popup.close(); root.chosen(modelData.v) } }
                    }
                }
            }
        }
    }
}
