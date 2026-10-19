import QtQuick
import QtQuick.Controls.Basic
import Win11Terminal

// The terminal's context menu: a command bar (Copy, Paste, "…") over a list, as the original's
// CommandBarFlyout draws it (measured with the Menu key on the VM, refs/41 to 43): 1px border,
// a 46px bar of 36px buttons, a separator, 28px rows 4px apart, 5px of margin all around.
Popup {
    id: root

    property bool hasSelection: false
    property bool expanded: true         // the "…" button folds the list away
    property bool keyboard: false        // opened by the keyboard: a focus ring shows
    property string profileName
    property var profiles: []            // the profiles the split submenu lists
    property int current: -1             // the row the keyboard sits on (the buttons first)
    signal copyChosen
    signal pasteChosen
    signal findChosen
    signal duplicateTabChosen
    signal splitChosen(string guid)      // "" duplicates the current profile
    signal searchWebChosen
    signal closeTabChosen

    popupType: Popup.Item
    padding: 0
    margins: 0
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property var buttons: {
        const list = []
        if (hasSelection) list.push({ id: "copy", icon: "copy", size: 20 })
        list.push({ id: "paste", icon: "clipboard_paste", size: 20 })
        list.push({ id: "more", icon: "more_horizontal" })
        return list
    }
    readonly property var rows: {
        const list = [
            { id: "find", label: qsTr("Find..."), icon: "search" },
            { id: "duplicate", label: qsTr("Duplicate tab"), icon: "square_multiple", size: 20 },
            { id: "split", label: qsTr("Split pane"), icon: "layout_column_two_split_right", sub: true } ]
        if (hasSelection) list.push({ id: "web", label: qsTr("Web search"), icon: "globe_search" })
        list.push({ id: "close", label: qsTr("Close tab"), icon: "dismiss" })
        return list
    }
    readonly property int slotCount: buttons.length + (expanded ? rows.length : 0)
    readonly property color border: Theme.dark ? "#3b3b3b" : Theme.flyoutBorder

    width: expanded ? 177 : 13 + buttons.length * 40 - 4
    height: expanded ? 2 + 46 + 1 + rows.length * 32 + 6 : 48

    background: Rectangle {
        color: Theme.flyout
        radius: 8
        border.width: 1
        border.color: root.border
    }

    /** Opens with the top-left corner at (px, py) in the overlay's coordinates, kept inside it. */
    function openAt(px, py, byKeyboard) {
        keyboard = byKeyboard
        current = byKeyboard ? 0 : -1
        expanded = true
        const area = Overlay.overlay
        x = Math.max(0, Math.min(px, area.width - width))
        y = py + height > area.height ? Math.max(0, py - height) : py
        open()
    }

    function activate(id) {
        close()
        switch (id) {
        case "copy": copyChosen(); break
        case "paste": pasteChosen(); break
        case "find": findChosen(); break
        case "duplicate": duplicateTabChosen(); break
        case "web": searchWebChosen(); break
        case "close": closeTabChosen(); break
        }
    }

    function handleKey(event) {
        const n = root.buttons.length
        const k = event.key
        if (k === Qt.Key_Left || k === Qt.Key_Right) {
            if (root.current >= 0 && root.current < n) {
                root.current = (root.current + (k === Qt.Key_Right ? 1 : n - 1)) % n
                event.accepted = true
            } else if (k === Qt.Key_Right && root.expanded && root.current >= 0 && root.rows[root.current - n].sub) {
                root.splitRow.openSub()
                event.accepted = true
            }
        } else if (k === Qt.Key_Down || k === Qt.Key_Up) {
            const count = root.slotCount
            root.current = root.current < 0 ? 0 : (root.current + (k === Qt.Key_Down ? 1 : count - 1)) % count
            event.accepted = true
        } else if (k === Qt.Key_Return || k === Qt.Key_Enter || k === Qt.Key_Space) {
            if (root.current >= 0 && root.current < n) {
                const id = root.buttons[root.current].id
                if (id === "more") root.expanded = !root.expanded
                else root.activate(id)
            } else if (root.current >= n) {
                const row = root.rows[root.current - n]
                if (row.sub) root.splitRow.openSub()
                else root.activate(row.id)
            }
            event.accepted = true
        }
    }

    contentItem: Item {
        focus: true
        Keys.onPressed: function(event) { root.handleKey(event) }
        // The command bar.
        Repeater {
            model: root.buttons
            delegate: Rectangle {
                id: button
                required property var modelData
                required property int index
                readonly property bool isMore: modelData.id === "more"
                x: isMore && root.expanded ? root.width - 1 - 5 - 36 : 6 + index * 40
                y: 6
                width: 36; height: 36; radius: 4
                color: area.pressed ? Theme.hoverOverlay : (area.containsMouse) ? Theme.flyoutHover : "transparent"
                FluentSymbol {
                    anchors.centerIn: parent
                    name: button.modelData.icon; size: button.modelData.size || 16; variant: "regular"; color: Theme.text
                }
                Rectangle {   // the keyboard's focus ring
                    visible: root.keyboard && root.current === button.index
                    anchors.fill: parent; radius: 4
                    color: "transparent"; border.width: 2; border.color: Theme.text
                }
                MouseArea {
                    id: area
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (button.isMore) root.expanded = !root.expanded
                        else root.activate(button.modelData.id)
                    }
                }
            }
        }
        Rectangle {   // under the bar
            visible: root.expanded
            x: 1; y: 47; width: parent.width - 2; height: 1
            color: Theme.flyoutSeparator
        }
        // The list.
        Column {
            visible: root.expanded
            x: 6; y: 53
            spacing: 4
            Repeater {
                model: root.rows
                delegate: Item {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property bool isSplit: modelData.id === "split"
                    width: 165; height: 28
                    Rectangle {
                        anchors.fill: parent; radius: 4
                        color: rowArea.pressed ? Theme.hoverOverlay
                             : (rowArea.containsMouse || (row.isSplit && splitPanel.visible) || (root.keyboard && root.current === root.buttons.length + row.index)) ? Theme.flyoutHover : "transparent"
                    }
                    FluentSymbol {
                        x: 11; anchors.verticalCenter: parent.verticalCenter
                        name: row.modelData.icon; size: row.modelData.size || 16; variant: "regular"; color: Theme.text
                    }
                    Text {
                        x: 37; anchors.verticalCenter: parent.verticalCenter
                        text: row.modelData.label
                        color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14
                    }
                    Glyph {
                        visible: row.isSplit
                        x: parent.width - 20; anchors.verticalCenter: parent.verticalCenter
                        name: "chevronDown"; rotation: -90; color: Theme.text
                    }
                    function openSub() { if (isSplit) splitPanel.open() }
                    function closeSub() { if (isSplit) splitPanel.close() }
                    Component.onCompleted: if (isSplit) root.splitRow = row
                    MouseArea {
                        id: rowArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: { if (row.isSplit) splitPanel.open(); else splitPanel.close() }
                        onClicked: if (!row.isSplit) root.activate(row.modelData.id)
                    }
                    // Split pane: duplicate, then a profile of its own.
                    Popup {
                        id: splitPanel
                        visible: false
                        parent: row
                        x: row.width - 1; y: 0
                        width: 241
                        popupType: Popup.Item
                        padding: 0
                        closePolicy: Popup.NoAutoClose
                        background: Rectangle { color: Theme.flyout; radius: 8; border.width: 1; border.color: root.border }
                        contentItem: Column {
                            topPadding: 6; bottomPadding: 6; leftPadding: 6
                            spacing: 4
                            component SubRow: Item {
                                id: sub
                                property string label
                                property string guid
                                signal triggered
                                width: 229; height: 28
                                Rectangle { anchors.fill: parent; radius: 4; color: subArea.pressed ? Theme.hoverOverlay : subArea.containsMouse ? Theme.flyoutHover : "transparent" }
                                ProfileIcon { x: 11; anchors.verticalCenter: parent.verticalCenter; profileId: sub.guid }
                                Text { x: 37; anchors.verticalCenter: parent.verticalCenter; text: sub.label; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                                MouseArea { id: subArea; anchors.fill: parent; hoverEnabled: true; onClicked: sub.triggered() }
                            }
                            SubRow {
                                label: qsTr("Duplicate %1").arg(root.profileName)
                                guid: ""
                                onTriggered: { root.close(); root.splitChosen("") }
                            }
                            Item {   // the separator
                                width: 229; height: 5
                                Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 1; color: Theme.flyoutSeparator }
                            }
                            Repeater {
                                model: root.profiles
                                delegate: SubRow {
                                    required property var modelData
                                    label: modelData.name
                                    guid: modelData.guid
                                    onTriggered: { root.close(); root.splitChosen(modelData.guid) }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    property Item splitRow
    onClosed: if (splitRow) splitRow.closeSub()
}
