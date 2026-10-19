import QtQuick
import QtQuick.Controls.Basic
import Win11Terminal

// The menu of the "⌄" button: the profiles, then Settings, Command palette and
// About. 33px a row, a 28px pill under the pointer (measured, NOTES.md section 3).
Popup {
    id: root

    property string defaultProfileId
    property var shortcuts: ({})   // action id -> text ("Ctrl+Shift+1")
    // The resolved newTabMenu (NewTabMenu): profiles, separators, folders and actions. The root menu
    // adds Settings, Command palette and About; a folder's submenu is this same type, sized by its rows.
    property var entries: ProfileManager.newTabMenu
    property bool isRoot: true
    property real rowsWidth: 0
    signal profileChosen(string guid)
    signal settingsChosen
    signal paletteChosen
    signal aboutChosen

    // Popup.Window (a window of its own) would let the menu leave a small window, but under win11wm
    // it is a normal transient that swallows clicks; in-window it is.
    popupType: Popup.Item
    padding: 0
    margins: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    implicitWidth: isRoot ? 287 : Math.max(120, rowsWidth + 2)

    background: Rectangle {
        color: Theme.flyout
        radius: 8
        border.width: 1
        border.color: Theme.flyoutBorder
    }

    component Row33: Item {
        id: row
        property string label
        property string shortcut
        property string symbol
        property bool bold: false
        property string profileGuid   // draws that profile's icon
        property bool hasSub: false   // a folder: a chevron, no click of its own
        readonly property real wanted: 42 + labelText.implicitWidth + (shortcutText.text.length > 0 ? 24 + shortcutText.implicitWidth : 0) + 15
        signal triggered
        signal entered
        width: root.implicitWidth - 2
        height: 33

        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            radius: 4
            color: area.pressed ? Theme.hoverOverlay : area.containsMouse ? Theme.flyoutHover : "transparent"
        }
        Item {
            id: iconSlot
            x: 14; anchors.verticalCenter: parent.verticalCenter
            width: 16; height: 16
            ProfileIcon { anchors.fill: parent; visible: row.profileGuid.length > 0; profileId: row.profileGuid }
            FluentSymbol {
                anchors.centerIn: parent
                visible: row.symbol.length > 0
                name: row.symbol
                size: 16
                variant: "regular"
                color: Theme.text
            }
        }
        Text {
            id: labelText
            x: 42
            anchors.verticalCenter: parent.verticalCenter
            text: row.label
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: 14
            font.bold: row.bold
        }
        Glyph {
            visible: row.hasSub
            x: parent.width - 22; anchors.verticalCenter: parent.verticalCenter
            name: "chevronDown"; rotation: -90; color: Theme.text
        }
        Text {
            id: shortcutText
            anchors.right: parent.right
            anchors.rightMargin: 15
            anchors.verticalCenter: parent.verticalCenter
            text: row.shortcut
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 12
        }
        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            onEntered: row.entered()
            onClicked: { if (!row.hasSub) { root.close(); row.triggered() } }
        }
    }

    signal actionChosen(string id)
    // A submenu is as wide as its widest row (the root one is fixed, measured).
    function measure() {
        let w = 0
        for (let i = 0; i < rows.count; ++i) {
            const col = rows.itemAt(i)
            if (!col) continue
            for (const c of col.children) if (c.visible && c.wanted !== undefined) w = Math.max(w, c.wanted)
        }
        rowsWidth = w
    }
    onEntriesChanged: Qt.callLater(measure)
    Component.onCompleted: Qt.callLater(measure)
    function actionName(id) {
        for (const a of Actions.catalog) if (a.id === id) return a.name
        return id
    }
    property Popup sub
    function closeSub() { if (sub) { sub.close() } }
    function openSub(row, list) {
        if (!sub) {
            const comp = Qt.createComponent(Qt.resolvedUrl("ProfileFlyout.qml"))
            if (comp.status !== Component.Ready) return
            sub = comp.createObject(root, { isRoot: false, closePolicy: Popup.NoAutoClose, defaultProfileId: root.defaultProfileId })
            sub.profileChosen.connect(function(guid) { root.close(); root.profileChosen(guid) })
            sub.actionChosen.connect(function(id) { root.close(); root.actionChosen(id) })
        }
        sub.entries = list
        sub.parent = row
        sub.x = row.width - 4
        sub.y = -1
        if (!sub.opened) sub.open()
    }
    onClosed: closeSub()

    contentItem: Column {
        topPadding: 3
        bottomPadding: 1
        Repeater {
            id: rows
            model: root.entries
            delegate: Column {
                id: entry
                required property var modelData
                readonly property string kind: modelData.type
                Item {   // a separator
                    visible: entry.kind === "separator"
                    width: root.implicitWidth - 2; height: visible ? 5 : 0
                    Rectangle { anchors.centerIn: parent; width: parent.width; height: 1; color: Theme.flyoutSeparator }
                }
                Row33 {   // a profile
                    visible: entry.kind === "profile"; height: visible ? 33 : 0
                    label: entry.modelData.name || ""
                    bold: entry.modelData.guid === root.defaultProfileId
                    shortcut: entry.modelData.index !== undefined && entry.modelData.index < 9 ? "Ctrl+Shift+" + (entry.modelData.index + 1) : ""
                    profileGuid: visible ? entry.modelData.guid : ""
                    onEntered: root.closeSub()
                    onTriggered: root.profileChosen(entry.modelData.guid)
                }
                Row33 {   // an action
                    visible: entry.kind === "action"; height: visible ? 33 : 0
                    label: visible ? root.actionName(entry.modelData.id) : ""
                    onEntered: root.closeSub()
                    onTriggered: root.actionChosen(entry.modelData.id)
                }
                Row33 {   // a folder
                    id: folderRow
                    visible: entry.kind === "folder"; height: visible ? 33 : 0
                    label: entry.modelData.name || ""
                    hasSub: true
                    onEntered: root.openSub(folderRow, entry.modelData.entries)
                }
            }
        }
        Item {
            visible: root.isRoot
            width: root.implicitWidth - 2; height: visible ? 5 : 0
            Rectangle { anchors.centerIn: parent; width: parent.width; height: 1; color: Theme.flyoutSeparator }
        }
        Row33 { visible: root.isRoot; height: visible ? 33 : 0; label: qsTr("Settings"); symbol: "settings"; shortcut: "Ctrl+,"; onTriggered: root.settingsChosen() }
        Row33 { visible: root.isRoot; height: visible ? 33 : 0; label: qsTr("Command palette"); symbol: "flash"; shortcut: "Ctrl+Shift+P"; onTriggered: root.paletteChosen() }
        Row33 { visible: root.isRoot; height: visible ? 33 : 0; label: qsTr("About"); symbol: "question"; onTriggered: root.aboutChosen() }
    }
}
