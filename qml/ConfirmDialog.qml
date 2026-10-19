import QtQuick
import Win11Terminal

// A two-button dialog (measured on the VM, "Deseja fechar todas as guias?"): 320x184,
// a 101px body, an 80px footer with the accent button on the left; the window dims.
Item {
    id: root
    property string title
    property string primaryText
    property string secondaryText
    property string body              // optional: a paragraph under the title (the box grows for it)
    signal accepted
    signal rejected
    visible: false

    function open() { visible = true; primary.forceActiveFocus() }
    function accept() { visible = false; accepted() }
    function reject() { visible = false; rejected() }

    Rectangle { anchors.fill: parent; color: "#66000000" }
    MouseArea { anchors.fill: parent }
    Rectangle {
        width: root.body.length > 0 ? 448 : 320
        height: root.body.length > 0 ? bodyText.y + bodyText.height + 24 + 80 : 184
        anchors.centerIn: parent
        radius: 8
        color: Theme.dialogBody
        border.width: 1
        border.color: "#050505"
        clip: true
        Text {
            x: 25; y: 26; width: parent.width - 50
            text: root.title
            color: Theme.text
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: 15
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: 20
            font.weight: Font.DemiBold
        }
        Text {
            id: bodyText
            visible: root.body.length > 0
            x: 25; y: 66; width: parent.width - 50
            text: root.body
            wrapMode: Text.WordWrap
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: 14
        }
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 80
            color: Theme.dialogFooter
            Row {
                x: 25; anchors.verticalCenter: parent.verticalCenter
                spacing: 9
                Rectangle {
                    id: primary
                    width: 131; height: 32; radius: 4; color: Theme.accentLight
                    focus: true
                    Keys.onPressed: function(e) {
                        if (e.key === Qt.Key_Return || e.key === Qt.Key_Enter) { root.accept(); e.accepted = true }
                        else if (e.key === Qt.Key_Escape) { root.reject(); e.accepted = true }
                    }
                    Text { anchors.centerIn: parent; text: root.primaryText; color: Theme.accentText; font.family: Theme.fontFamily; font.pixelSize: 14 }
                    MouseArea { anchors.fill: parent; onClicked: root.accept() }
                }
                Rectangle {
                    width: 131; height: 32; radius: 4; color: Theme.dialogButton
                    Text { anchors.centerIn: parent; text: root.secondaryText; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                    MouseArea { anchors.fill: parent; onClicked: root.reject() }
                }
            }
        }
    }
}
