import QtQuick

Item {
    id: root
    anchors.fill: parent
    clip: true

    // Scanlines layer
    Canvas {
        id: scanlines
        anchors.fill: parent
        opacity: 0.16
        renderTarget: Canvas.FramebufferObject

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = "#000000"
            for (let y = 0; y < height; y += 3) {
                ctx.fillRect(0, y, width, 1.2)
            }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    // CRT Screen Glass subtle phosphor green overlay
    Rectangle {
        anchors.fill: parent
        color: "#0500ff66"
        opacity: 0.35
    }

    // CRT Vignette effect (dark borders)
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 16
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#55000000" }
            GradientStop { position: 1.0; color: "#00000000" }
        }
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 16
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#00000000" }
            GradientStop { position: 1.0; color: "#55000000" }
        }
    }
    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: 16
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#55000000" }
            GradientStop { position: 1.0; color: "#00000000" }
        }
    }
    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 16
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#00000000" }
            GradientStop { position: 1.0; color: "#55000000" }
        }
    }

    // Subtle phosphor scanline flicker / pulse
    SequentialAnimation on opacity {
        loops: Animation.Infinite
        running: root.visible
        NumberAnimation { from: 0.94; to: 1.0; duration: 2400; easing.type: Easing.InOutQuad }
        NumberAnimation { from: 1.0; to: 0.94; duration: 2400; easing.type: Easing.InOutQuad }
    }
}
