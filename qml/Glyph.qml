import QtQuick
import QtQuick.Shapes

// The small monochrome glyphs of the chrome, drawn as Windows draws them at 100%
// (pixel maps measured on the VM, NOTES.md section 2): caption glyphs are 10x10
// boxes with 1px lines; the tab's close is 7x7, the "+" 9x9, the chevron 8x4.
Item {
    id: root

    property string name
    property color color: "white"

    readonly property real box: name === "plus" ? 9 : name === "tabClose" ? 7 : name === "chevronDown" ? 8 : 10
    readonly property real boxHeight: name === "chevronDown" ? 4 : box
    implicitWidth: box
    implicitHeight: boxHeight

    component Stroke: ShapePath {
        strokeColor: root.color
        strokeWidth: 1
        fillColor: "transparent"
        capStyle: ShapePath.FlatCap
        joinStyle: ShapePath.MiterJoin
    }

    Shape {
        anchors.fill: parent
        visible: root.name === "minimize"
        Stroke { PathMove { x: 0; y: 4.5 } PathLine { x: 10; y: 4.5 } }
    }
    Shape {
        anchors.fill: parent
        visible: root.name === "plus"
        Stroke { PathMove { x: 0; y: 4.5 } PathLine { x: 9; y: 4.5 } }
        Stroke { PathMove { x: 4.5; y: 0 } PathLine { x: 4.5; y: 9 } }
    }
    Shape {
        anchors.fill: parent
        visible: root.name === "close" || root.name === "tabClose"
        Stroke { PathMove { x: 0.5; y: 0.5 } PathLine { x: root.box - 0.5; y: root.box - 0.5 } }
        Stroke { PathMove { x: root.box - 0.5; y: 0.5 } PathLine { x: 0.5; y: root.box - 0.5 } }
    }
    Rectangle {
        visible: root.name === "maximize"
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: root.color
        radius: 1.5
    }
    Item {
        visible: root.name === "restore"
        anchors.fill: parent
        Rectangle {
            x: 0; y: 2; width: 8; height: 8
            color: "transparent"; border.width: 1; border.color: root.color; radius: 1.5
        }
        Shape {
            anchors.fill: parent
                Stroke {
                PathMove { x: 2.5; y: 1.5 }
                PathLine { x: 2.5; y: 0.5 }
                PathLine { x: 8.5; y: 0.5 }
                PathLine { x: 9.5; y: 1.5 }
                PathLine { x: 9.5; y: 7.5 }
                PathLine { x: 8.5; y: 8.5 }
                PathLine { x: 8; y: 8.5 }
            }
        }
    }
    Shape {
        anchors.fill: parent
        visible: root.name === "arrowUp" || root.name === "arrowDown"
        rotation: root.name === "arrowDown" ? 180 : 0
        Stroke { PathMove { x: 5; y: 0.5 } PathLine { x: 5; y: 9.5 } }
        Stroke { PathMove { x: 1; y: 4.5 } PathLine { x: 5; y: 0.5 } PathLine { x: 9; y: 4.5 } }
    }
    Shape {
        anchors.fill: parent
        visible: root.name === "menu"
        Stroke { PathMove { x: 0; y: 1.5 } PathLine { x: 10; y: 1.5 } }
        Stroke { PathMove { x: 0; y: 5.5 } PathLine { x: 10; y: 5.5 } }
        Stroke { PathMove { x: 0; y: 9.5 } PathLine { x: 10; y: 9.5 } }
    }
    Shape {
        anchors.fill: parent
        visible: root.name === "chevronDown"
        Stroke { PathMove { x: 0.2; y: 0.5 } PathLine { x: 4; y: 4.3 } PathLine { x: 7.8; y: 0.5 } }
    }
}
