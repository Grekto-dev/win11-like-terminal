import QtQuick
import QtQuick.Shapes
import Win11Terminal

// The tabs of the title bar (tabWidthMode "equal": each as wide as fits, from 100
// to 240px) and the "+" / "⌄" pair after them. When even 100px each do not fit,
// the tabs scroll between two arrows. Every number is measured on the VM (NOTES.md).
Item {
    id: root

    property var model            // ListModel: title, profileId
    property int currentIndex: 0
    property color terminalBackground: "#0c0c0c"
    property bool windowActive: true
    property bool menuOpen: false
    property bool compact: false   // maximized: a 31px bar and tabs up to the edge
    readonly property int barHeight: compact ? 31 : Theme.barHeight
    readonly property int tabTop: compact ? 0 : Theme.tabTop
    readonly property int tabHeight: compact ? 31 : Theme.tabHeight
    readonly property int count: model ? model.count : 0
    readonly property int arrowWidth: 32
    // What the bar leaves for tabs: the margin, the buttons and a strip to grab the window by.
    readonly property real tabsAvailable: width - Theme.barMarginLeft - buttons.width - Theme.dragReserve
    readonly property real tabWidth: count > 0 ? Math.max(Theme.tabMinWidth, Math.min(Theme.tabMaxWidth, tabsAvailable / count)) : Theme.tabMaxWidth
    readonly property bool overflowing: count * Theme.tabMinWidth > tabsAvailable
    readonly property real buttonsX: Theme.barMarginLeft + (overflowing ? tabsAvailable : count * tabWidth) + 6
    // The visible window on the tabs.
    readonly property real viewLeft: overflowing ? Theme.barMarginLeft + 34 : Theme.barMarginLeft
    readonly property real viewRight: overflowing ? buttonsX - 10 - arrowWidth : Theme.barMarginLeft + count * tabWidth

    signal selected(int index)
    signal moveRequested(int from, int to)
    signal contextRequested(int index, real x, real y)
    signal renamed(int index, string text)
    property int renameIndex: -1
    function startRename(i) { renameIndex = i }
    signal closeRequested(int index)
    signal newTabRequested
    signal menuRequested(real x, real y)

    clip: true

    function ensureVisible(i) {
        if (!overflowing || i < 0) return
        const left = i * tabWidth, right = left + tabWidth
        if (left < flick.contentX) flick.contentX = left
        else if (right > flick.contentX + flick.width) flick.contentX = right - flick.width
    }
    onCurrentIndexChanged: ensureVisible(currentIndex)
    onOverflowingChanged: if (!overflowing) flick.contentX = 0
    onCountChanged: Qt.callLater(function() { ensureVisible(currentIndex) })

    // ---- the tabs -----------------------------------------------------------
    Flickable {
        id: flick
        x: root.viewLeft
        width: Math.max(0, root.viewRight - root.viewLeft)
        height: root.barHeight
        contentWidth: root.count * root.tabWidth
        contentHeight: height
        interactive: false
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Behavior on contentX { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

        Repeater {
            model: root.model
            delegate: Item {
                id: tab
                required property int index
                required property string title
                required property string profileId
                required property string kind
                required property string tabColor
                required property string custom
                readonly property bool colored: tabColor.length > 0
                readonly property color accentColor: colored ? tabColor : "transparent"
                readonly property bool renaming: root.renameIndex === index
                readonly property color barColor: root.windowActive ? Theme.barActive : Theme.barInactive
                // Light theme (measured): the Settings tab is #FFFFFF when selected and an inactive Settings tab
                // under the pointer is #F6F6F6 on the #E8E8E8 bar; a terminal tab turns the terminal colour + 36.2% white.
                readonly property color selectedBg: !Theme.dark && kind === "settings" ? "#ffffff" : root.terminalBackground
                readonly property color hoverBg: !Theme.dark && kind === "settings" ? "#f6f6f6" : Theme.mix(root.terminalBackground, Theme.mixTowards, Theme.tabHoverMix)
                readonly property color tabBg: colored ? (isSelected ? tabColor : Theme.mix(barColor, tabColor, hover.hovered ? 0.6 : 0.3))
                                            : isSelected ? selectedBg
                                            : hover.hovered ? hoverBg : barColor
                // Text and glyphs are dark on a light tab and white on a dark one.
                readonly property color fg: Theme.luminance(tabBg) > 0.45 ? "#1a1a1a" : "white"
                readonly property bool isSelected: index === root.currentIndex
                x: index * root.tabWidth
                width: root.tabWidth
                height: root.barHeight

                // The tab is a rounded shape from its top that runs off the bottom of the
                // bar, so only its top corners show.
                Rectangle {
                    y: root.tabTop
                    width: parent.width
                    height: root.tabHeight + Theme.tabRadius
                    radius: Theme.tabRadius
                    color: tab.colored ? (tab.isSelected ? tab.tabColor
                                        : Theme.mix(root.windowActive ? Theme.barActive : Theme.barInactive, tab.tabColor, hover.hovered ? 0.6 : 0.3))
                         : tab.isSelected ? tab.selectedBg
                         : hover.hovered ? tab.hoverBg
                         : "transparent"
                }
                // A 1px divider between two tabs that are neither selected nor hovered.
                Rectangle {
                    visible: !tab.isSelected && tab.index + 1 !== root.currentIndex && !hover.hovered
                             && tab.index + 1 < root.count
                    x: parent.width - 1
                    y: root.tabTop + (root.tabHeight - 16) / 2
                    width: 1; height: 16
                    color: Theme.tabDivider
                }
                Item {
                    y: root.tabTop
                    width: parent.width
                    height: root.tabHeight
                    ProfileIcon {
                        x: 9
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16; height: 16
                        profileId: tab.profileId
                        color: tab.fg
                        visible: tab.kind !== "settings"
                    }
                    FluentSymbol {
                        x: 9
                        anchors.verticalCenter: parent.verticalCenter
                        visible: tab.kind === "settings"
                        name: "settings"; size: 16; variant: "regular"; color: tab.fg
                    }
                    TextInput {
                        id: renameField
                        visible: tab.renaming
                        x: 33
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(0, parent.width - 33 - 4 - 32 - 4)
                        text: tab.title
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        selectionColor: Theme.accentLight
                        selectedTextColor: "black"
                        clip: true
                        onVisibleChanged: if (visible) { forceActiveFocus(); selectAll() }
                        onAccepted: { root.renamed(tab.index, text); root.renameIndex = -1 }
                        Keys.onEscapePressed: root.renameIndex = -1
                        onActiveFocusChanged: if (!activeFocus && tab.renaming) { root.renamed(tab.index, text); root.renameIndex = -1 }
                    }
                    Text {
                        visible: !tab.renaming
                        x: 35
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(0, parent.width - 35 - 4 - 32 - 4)
                        text: tab.title
                        color: tab.fg
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        elide: Text.ElideNone
                        clip: true
                    }
                    Rectangle {
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        width: 32; height: 24
                        radius: 4
                        color: closeArea.containsMouse ? (closeArea.pressed ? Theme.pressedOverlay : Theme.hoverOverlay) : "transparent"
                        visible: tab.isSelected || hover.hovered || closeArea.containsMouse
                        Glyph { anchors.centerIn: parent; name: "tabClose"; color: tab.fg }
                        MouseArea {
                            id: closeArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.closeRequested(tab.index)
                        }
                    }
                }
                HoverHandler { id: hover }
                MouseArea {
                    id: tabArea
                    preventStealing: true   // the bar's DragHandler would otherwise move the window
                    anchors.fill: parent
                    z: -1
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                    property real pressX: 0
                    property int dragIndex: -1
                    onPressed: function(mouse) {
                        if (mouse.button === Qt.MiddleButton) { root.closeRequested(tab.index); return }
                        if (mouse.button === Qt.RightButton) {
                            root.selected(tab.index)
                            const p = mapToItem(root, mouse.x, mouse.y)
                            root.contextRequested(tab.index, p.x, p.y)
                            return
                        }
                        root.selected(tab.index)
                        pressX = mouse.x
                        dragIndex = tab.index
                    }
                    onReleased: dragIndex = -1
                    // Dragging a tab along the bar moves it to the slot under the pointer.
                    onPositionChanged: function(mouse) {
                        if (dragIndex < 0 || !(mouse.buttons & Qt.LeftButton)) return
                        if (Math.abs(mouse.x - pressX) < 4 && dragIndex === tab.index) return
                        const x = tab.x + mouse.x            // in the strip's content
                        const to = Math.max(0, Math.min(root.count - 1, Math.floor(x / root.tabWidth)))
                        if (to !== dragIndex) {
                            root.moveRequested(dragIndex, to)
                            dragIndex = to
                        }
                    }
                }
            }
        }
    }
    // The wheel scrolls the tabs when they overflow.
    WheelHandler {
        enabled: root.overflowing
        onWheel: function(event) {
            const d = event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x
            flick.contentX = Math.max(0, Math.min(flick.contentWidth - flick.width, flick.contentX - d))
        }
    }

    // ---- scroll arrows, when the tabs do not fit -------------------------------------
    component Arrow: Item {
        id: arrow
        property bool pointsRight: false
        property bool active: true
        signal clicked
        width: root.arrowWidth; height: root.tabHeight
        y: root.tabTop
        visible: root.overflowing
        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: 4
            color: area.containsMouse && arrow.active ? Theme.splitButtonHover : "transparent"
        }
        Shape {
            anchors.centerIn: parent
            width: 6; height: 8
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: arrow.active ? Theme.text : Theme.textDisabled
                strokeColor: "transparent"
                PathMove { x: arrow.pointsRight ? 0 : 6; y: 0 }
                PathLine { x: arrow.pointsRight ? 6 : 0; y: 4 }
                PathLine { x: arrow.pointsRight ? 0 : 6; y: 8 }
                PathLine { x: arrow.pointsRight ? 0 : 6; y: 0 }
            }
        }
        MouseArea { id: area; anchors.fill: parent; hoverEnabled: true; onClicked: if (arrow.active) arrow.clicked() }
    }
    Arrow {
        x: Theme.barMarginLeft + 2 - 0
        pointsRight: false
        active: flick.contentX > 0
        onClicked: flick.contentX = Math.max(0, flick.contentX - root.tabWidth)
    }
    Arrow {
        x: root.viewRight
        pointsRight: true
        active: flick.contentX < flick.contentWidth - flick.width - 1
        onClicked: flick.contentX = Math.min(flick.contentWidth - flick.width, flick.contentX + root.tabWidth)
    }

    // ---- "+" and "⌄" -----------------------------------------------------------------
    Row {
        id: buttons
        x: root.buttonsX
        y: root.compact ? 2 : root.tabTop + (root.tabHeight - height) / 2
        spacing: 1
        component SplitPart: Rectangle {
            id: part
            property string glyph
            property bool pressedLook: false
            signal clicked
            width: Theme.splitButtonWidth; height: Theme.splitButtonHeight
            radius: 4
            color: area.pressed || pressedLook ? Theme.splitButtonPressed
                 : area.containsMouse ? Theme.splitButtonHover : "transparent"
            Glyph {
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: part.glyph === "chevronDown" ? -2 : 0
                name: part.glyph
                color: Theme.text
            }
            MouseArea { id: area; anchors.fill: parent; hoverEnabled: true; onClicked: part.clicked() }
        }
        SplitPart { glyph: "plus"; onClicked: root.newTabRequested() }
        SplitPart {
            // the divider of the pair lives in the 1px gap
            Rectangle { x: -1; y: 0; width: 1; height: parent.height; color: Theme.splitDivider }
            glyph: "chevronDown"
            pressedLook: root.menuOpen
            onClicked: root.menuRequested(root.buttonsX, root.barHeight)
        }
    }
}
