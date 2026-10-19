import QtQuick
import QtQuick.Shapes

// The icon of a profile. Windows Terminal has a picture per shell; until the
// Fluent set has one, a shell gets the console window: a rounded box with ">_".
Item {
    id: root
    property string profileId
    property color color: Theme.dark ? "#dcdcdc" : "#303030"
    implicitWidth: 16
    implicitHeight: 16

    Rectangle {
        anchors.fill: parent
        anchors.margins: 0.5
        radius: 2
        color: "transparent"
        border.width: 1
        border.color: root.color
    }
    Shape {
        anchors.fill: parent
        ShapePath {
            strokeColor: root.color; strokeWidth: 1; fillColor: "transparent"
            PathMove { x: 3.5; y: 5.5 }
            PathLine { x: 6.5; y: 8 }
            PathLine { x: 3.5; y: 10.5 }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1; fillColor: "transparent"
            PathMove { x: 8; y: 11.5 }
            PathLine { x: 12; y: 11.5 }
        }
    }
}
