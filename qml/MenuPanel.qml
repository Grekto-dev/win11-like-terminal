import QtQuick
import QtQuick.Controls.Basic
import Win11Terminal

// A flyout menu (measured on the tab's context menu, refs/34 to 36): 32px rows, a
// 4px margin, icons at x=15, text at x=44, a chevron for a submenu, thin separators.
// items: [{ label, icon, enabled, separator (a line before), submenu: [...], action: function }]
Popup {
    id: root

    property var items: []
    property int menuWidth: 174
    property Item anchorItem       // for a submenu: the row that opened it
    signal chosen(var item)

    popupType: Popup.Item
    padding: 4
    width: menuWidth
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: Theme.flyout
        radius: 8
        border.width: 1
        border.color: Theme.flyoutBorder
    }

    function closeAll() { close() }

    contentItem: Column {
        Repeater {
            model: root.items
            delegate: Column {
                id: entry
                required property var modelData
                width: root.menuWidth - 8
                Item {   // the separator: 3px of room and a line
                    visible: entry.modelData.separator === true
                    width: parent.width; height: visible ? 3 : 0
                    Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 1; color: Theme.flyoutSeparator }
                }
                Item {
                    id: row
                    width: parent.width; height: 32
                    readonly property bool enabledItem: entry.modelData.enabled !== false
                    readonly property bool hasSub: entry.modelData.submenu !== undefined
                    Rectangle {
                        anchors.fill: parent; anchors.margins: 1; radius: 4
                        color: row.enabledItem && (area.containsMouse || (row.subPanel !== null && row.subPanel.visible)) ? Theme.flyoutHover : "transparent"
                    }
                    FluentSymbol {
                        x: 11; anchors.verticalCenter: parent.verticalCenter
                        visible: (entry.modelData.icon || "").length > 0
                        name: entry.modelData.icon || ""; size: 16; variant: "regular"
                        color: row.enabledItem ? Theme.text : Theme.textDisabled
                    }
                    Text {
                        x: 40; anchors.verticalCenter: parent.verticalCenter
                        text: entry.modelData.label
                        color: row.enabledItem ? Theme.text : Theme.textDisabled
                        font.family: Theme.fontFamily; font.pixelSize: 14
                    }
                    Glyph {
                        visible: row.hasSub
                        x: parent.width - 20; anchors.verticalCenter: parent.verticalCenter
                        name: "chevronDown"; rotation: -90
                        color: row.enabledItem ? Theme.text : Theme.textDisabled
                    }
                    MouseArea {
                        id: area
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            const panel = row.ensureSub()
                            if (root.openSub && root.openSub !== panel) root.openSub.close()
                            root.openSub = null
                            if (row.hasSub && row.enabledItem && panel) { panel.open(); root.openSub = panel }
                        }
                        onClicked: {
                            if (!row.enabledItem || row.hasSub) return
                            // The action may rebuild `items` (and this row's context): keep what is needed first.
                            const panel = root, item = entry.modelData
                            panel.close()
                            if (item.action) item.action()
                            panel.chosen(item)
                        }
                    }
                    // The submenu is built the first time it is needed (a QML type cannot
                    // instantiate itself statically).
                    property var subPanel: null
                    function ensureSub() {
                        if (subPanel || !hasSub) return subPanel
                        const comp = Qt.createComponent(Qt.resolvedUrl("MenuPanel.qml"))
                        if (comp.status !== Component.Ready) return null
                        subPanel = comp.createObject(row, {
                            items: entry.modelData.submenu || [], menuWidth: 232, x: row.width + 6, y: -4,
                            closePolicy: Popup.NoAutoClose })
                        // `top` is captured here: the action may rebuild `items`, and this row's context with it.
                        const top = root
                        subPanel.chosen.connect(function() { top.close() })
                        return subPanel
                    }
                }
            }
        }
    }
    property Popup openSub
    onClosed: if (openSub) openSub.close()
}
