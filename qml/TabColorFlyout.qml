import QtQuick
import QtQuick.Controls
import Win11Terminal

// "Alterar cor da guia": sixteen named colours in a 4x4 grid and two buttons (measured on the
// VM, refs/38): a 233x216 flyout, 30px squares 6px apart.
Popup {
    id: root
    signal picked(string color)     // "" restores the default
    readonly property var colors: ["#DC143C", "#4682B4", "#3CB371", "#FF8C00",
                                   "#C71585", "#1E90FF", "#32CD32", "#FFFF00",
                                   "#8A2BE2", "#6A5ACD", "#00FF00", "#D2B48C",
                                   "#FF00FF", "#00FFFF", "#87CEEB", "#A9A9A9"]
    property bool customMode: false
    padding: 0
    width: 233; height: 216
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: customMode = false

    background: Rectangle { color: Theme.flyout; radius: 8; border.width: 1; border.color: Theme.flyoutBorder }

    contentItem: Item {
        Grid {
            visible: !root.customMode
            x: 47; y: 16
            columns: 4; spacing: 6
            Repeater {
                model: root.colors
                Rectangle {
                    required property string modelData
                    width: 30; height: 30; radius: 4
                    color: modelData
                    border.width: sq.containsMouse ? 2 : 0
                    border.color: "white"
                    MouseArea { id: sq; anchors.fill: parent; hoverEnabled: true; onClicked: { root.close(); root.picked(modelData) } }
                }
            }
        }
        Column {
            visible: root.customMode
            x: 24; y: 40; spacing: 12
            Text { text: qsTr("Custom"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
            FieldBox {
                id: hex
                boxWidth: 185
                placeholder: "#RRGGBB"
                onEdited: function(t) {
                    const c = Qt.color(t.startsWith("#") ? t : "#" + t)
                    if (c.valid) { root.close(); root.picked(String(c).toUpperCase()) }
                }
            }
        }
        Row {
            x: 38; y: 168; spacing: 9
            Rectangle {
                width: 81; height: 30; radius: 4; color: Theme.choice
                Text { anchors.centerIn: parent; text: qsTr("Reset"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                MouseArea { anchors.fill: parent; onClicked: { root.close(); root.picked("") } }
            }
            Rectangle {
                width: 110; height: 30; radius: 4; color: Theme.choice
                Text { anchors.centerIn: parent; text: qsTr("Custom"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                MouseArea { anchors.fill: parent; onClicked: root.customMode = true }
            }
        }
    }
}
