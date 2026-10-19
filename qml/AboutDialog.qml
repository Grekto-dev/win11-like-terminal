import QtQuick
import Win11Terminal

// "Sobre" (measured, NOTES.md section 6): a 320x419 box centred in the window.
Item {
    id: root
    property string version: Qt.application.version
    signal closed
    visible: false

    function open() { visible = true; ok.forceActiveFocus() }
    function close() { visible = false; closed() }

    Rectangle { anchors.fill: parent; color: "#66000000" }   // the window dims
    MouseArea { anchors.fill: parent }

    Rectangle {
        width: 320; height: 419
        anchors.centerIn: parent
        radius: 8
        color: Theme.dialogBody
        border.width: 1
        border.color: "#080808"
        clip: true

        Text {
            x: 24; y: 22
            text: qsTr("About")
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: 20
            font.weight: Font.DemiBold
        }
        Column {
            x: 24; y: 66
            spacing: 0
            Text { text: "Win11-Like Terminal"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 13; font.bold: true }
            Text { text: qsTr("Version: %1").arg(root.version); color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: 12 }
        }
        Column {
            x: 24; y: 140
            spacing: 14
            Text { text: qsTr("Source code"); color: Theme.accentLight; font.family: Theme.fontFamily; font.pixelSize: 12
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Launcher.openUrl("https://github.com/Grekto-dev/win11-like-terminal") }
            }
            Text { text: qsTr("Documentation"); color: Theme.accentLight; font.family: Theme.fontFamily; font.pixelSize: 12
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Launcher.openUrl("https://github.com/Grekto-dev/win11-like-terminal#readme") }
            }
            Text { text: qsTr("Release notes"); color: Theme.accentLight; font.family: Theme.fontFamily; font.pixelSize: 12
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Launcher.openUrl("https://github.com/Grekto-dev/win11-like-terminal/releases") }
            }
            Text { text: qsTr("License (MIT)"); color: Theme.accentLight; font.family: Theme.fontFamily; font.pixelSize: 12
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Launcher.openUrl("https://github.com/Grekto-dev/win11-like-terminal/blob/main/LICENSE") }
            }
        }
        Rectangle {   // the footer
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 80
            color: Theme.dialogFooter
            Row {
                anchors.centerIn: parent
                spacing: 8
                Rectangle {
                    width: 142; height: 32; radius: 4; color: Theme.dialogButton
                    Text { anchors.centerIn: parent; text: qsTr("Send feedback"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Launcher.openUrl("https://github.com/Grekto-dev/win11-like-terminal/issues") }
                }
                Rectangle {
                    id: ok
                    width: 120; height: 32; radius: 4; color: Theme.accentLight
                    focus: true
                    Keys.onPressed: function(e) { if (e.key === Qt.Key_Return || e.key === Qt.Key_Enter || e.key === Qt.Key_Escape) { root.close(); e.accepted = true } }
                    Text { anchors.centerIn: parent; text: qsTr("OK"); color: Theme.accentText; font.family: Theme.fontFamily; font.pixelSize: 14 }
                    MouseArea { anchors.fill: parent; onClicked: root.close() }
                }
            }
        }
    }
}
