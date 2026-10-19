import QtQuick
import QtQuick.Controls.Basic
import Win11Terminal

// The "Menu nova guia" page: the entries of `newTabMenu` in a list (checkbox, label, edit, up, down,
// delete), move/delete of the checked ones, and the "Adicionar Entrada" cards; a folder opens its
// own list under "Propriedades da Pasta Atual". Measured on the VM, refs/47 to 53.
Column {
    id: root
    property var store
    property var path: []                     // indices of the folders opened, from the root
    property var checked: ({})                // row index -> true
    spacing: 6

    readonly property var fullMenu: {
        const v = store.get("g", "newTabMenu", undefined)
        return v !== undefined && v !== null && v.length > 0 ? JSON.parse(JSON.stringify(v)) : [{ type: "remainingProfiles" }]
    }
    readonly property var folderChain: {      // the folder objects along `path`
        const chain = []
        let list = fullMenu
        for (const i of path) {
            const f = list[i]
            if (!f || f.type !== "folder") break
            chain.push(f)
            list = f.entries || []
        }
        return chain
    }
    readonly property var entries: folderChain.length > 0 ? (folderChain[folderChain.length - 1].entries || []) : fullMenu
    readonly property bool inFolder: path.length > 0
    readonly property bool hasRemaining: {
        for (const e of entries) if (e.type === "remainingProfiles") return true
        return false
    }
    readonly property int checkedCount: { let n = 0; for (const k in checked) if (checked[k]) n++; return n }
    readonly property color edge: Theme.dark ? "#7e7e7e" : "#8a8a8a"

    // Edits a copy of the whole menu: fn gets the list the page shows (or the folder object) and changes it.
    function edit(fn) {
        const copy = JSON.parse(JSON.stringify(fullMenu))
        let list = copy, folder = null
        for (const i of path) { folder = list[i]; if (!folder.entries) folder.entries = []; list = folder.entries }
        fn(list, folder)
        store.set("g", "newTabMenu", copy)
    }
    function open(newPath) { path = newPath; checked = ({}) }
    function profileOf(ref) {
        for (const p of ProfileManager.profiles) if (p.guid === ref || p.name === ref) return p
        return null
    }
    function labelOf(e) {
        switch (e.type) {
        case "profile": { const p = profileOf(e.profile); return p ? p.name : String(e.profile) }
        case "separator": return qsTr("<Separator>")
        case "remainingProfiles": return qsTr("<Remaining profiles>")
        case "folder": return e.name || ""
        case "matchProfiles": return ProfileManager.matcherLabel(e)
        case "action": return String(e.id)
        }
        return String(e.type)
    }
    function move(i, d) {
        edit(function(list) {
            const j = i + d
            if (j < 0 || j >= list.length) return
            const t = list[i]; list[i] = list[j]; list[j] = t
        })
        checked = ({})
    }
    function remove(i) { edit(function(list) { list.splice(i, 1) }); checked = ({}) }
    function add(entry) { edit(function(list) { list.push(entry) }) }
    function removeChecked() {
        edit(function(list) { for (let i = list.length - 1; i >= 0; --i) if (checked[i]) list.splice(i, 1) })
        checked = ({})
    }
    // The folders a checked entry can move into: the root and every folder, as "path" lists.
    function folderTargets() {
        const out = [{ label: qsTr("<Root>"), depth: 0, path: [] }]
        const walk = function(list, prefix, depth) {
            list.forEach(function(e, i) {
                if (e.type === "folder") {
                    out.push({ label: e.name || "", depth: depth, path: prefix.concat([i]) })
                    walk(e.entries || [], prefix.concat([i]), depth + 1)
                }
            })
        }
        walk(fullMenu, [], 1)
        return out
    }
    function moveChecked(target) {
        const copy = JSON.parse(JSON.stringify(fullMenu))
        const at = function(p) { let l = copy; for (const i of p) { l = l[i].entries = l[i].entries || [] } return l }
        const here = at(path)
        const dest = at(target)        // taken before the removal: the indices below change after it
        // The folders on the way to the target: a folder cannot move into itself or its own subfolders.
        const way = []
        { let l = copy; for (const i of target) { way.push(l[i]); l = l[i].entries } }
        const moving = []
        for (let i = here.length - 1; i >= 0; --i) {
            if (!checked[i]) continue
            if (way.indexOf(here[i]) >= 0) continue
            moving.unshift(here.splice(i, 1)[0])
        }
        for (const m of moving) dest.push(m)
        store.set("g", "newTabMenu", copy)
        checked = ({})
    }

    // --- header: "Menu nova guia > Pasta" ------------------------------------------------
    Row {
        spacing: 12
        Text {
            text: qsTr("New tab menu")
            color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 20; font.weight: Font.DemiBold
            height: 32; verticalAlignment: Text.AlignVCenter
            MouseArea { anchors.fill: parent; enabled: root.inFolder; onClicked: root.open([]) }
        }
        Repeater {
            model: root.folderChain
            delegate: Row {
                id: crumb
                required property var modelData
                required property int index
                spacing: 12
                Glyph { anchors.verticalCenter: parent.verticalCenter; name: "chevronDown"; rotation: -90; color: Theme.text }
                Text {
                    text: crumb.modelData.name || ""
                    color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 20; font.weight: Font.DemiBold
                    height: 32; verticalAlignment: Text.AlignVCenter
                    MouseArea { anchors.fill: parent; onClicked: root.open(root.path.slice(0, crumb.index + 1)) }
                }
            }
        }
    }

    // --- the entries ----------------------------------------------------------------------
    Rectangle {
        width: root.width; height: 300
        color: "transparent"; radius: 4
        border.width: 1; border.color: root.edge
        Flickable {
            anchors.fill: parent; anchors.margins: 1
            contentHeight: rowsColumn.height; clip: true
            boundsBehavior: Flickable.StopAtBounds
            Column {
                id: rowsColumn
                width: parent.width
                Repeater {
                    model: root.entries
                    delegate: Item {
                        id: line
                        required property var modelData
                        required property int index
                        readonly property bool isFolder: modelData.type === "folder"
                        readonly property bool italic: modelData.type !== "profile" && !isFolder
                        readonly property var prof: modelData.type === "profile" ? root.profileOf(modelData.profile) : null
                        width: rowsColumn.width; height: 40
                        Rectangle { visible: root.checked[line.index] === true; x: 4; width: parent.width - 8; height: 40; radius: 4; color: "#353535" }
                        Rectangle {   // the checkbox
                            x: 14; y: 10; width: 20; height: 20; radius: 4
                            color: root.checked[line.index] ? Theme.accentLight : (Theme.dark ? "#242424" : "#ffffff")
                            border.width: root.checked[line.index] ? 0 : 1; border.color: Theme.dark ? "#8f8f8f" : "#6b6b6b"
                            FluentSymbol { visible: root.checked[line.index] === true; anchors.centerIn: parent; name: "checkmark"; size: 16; variant: "regular"; color: Theme.accentText }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: { const c = Object.assign({}, root.checked); c[line.index] = !c[line.index]; root.checked = c }
                            }
                        }
                        ProfileIcon { visible: line.prof !== null; x: 45; y: 12; profileId: line.prof ? line.prof.guid : "" }
                        Text {
                            x: line.prof !== null ? 71 : 47
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 220
                            elide: Text.ElideRight
                            text: root.labelOf(line.modelData)
                            color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14; font.italic: line.italic
                        }
                        Row {
                            anchors.right: parent.right; anchors.rightMargin: 12; y: 8
                            spacing: 5
                            component Tool: Rectangle {
                                id: tool
                                property string icon
                                property bool danger: false
                                signal clicked
                                width: 24; height: 24; radius: 4
                                color: danger ? "#b22222" : Theme.dark ? "#353535" : "#f0f0f0"
                                FluentSymbol { anchors.centerIn: parent; name: tool.icon; size: 16; variant: "regular"; color: tool.danger ? "white" : Theme.text }
                                MouseArea { anchors.fill: parent; onClicked: tool.clicked() }
                            }
                            Tool { visible: line.isFolder; icon: "edit"; onClicked: root.open(root.path.concat([line.index])) }
                            Tool { icon: "arrow_up"; onClicked: root.move(line.index, -1) }
                            Tool { icon: "arrow_down"; onClicked: root.move(line.index, 1) }
                            Tool { icon: "delete"; danger: true; onClicked: root.remove(line.index) }
                        }
                    }
                }
            }
        }
    }
    Row {
        spacing: 10
        Rectangle {
            width: 38 + moveText.implicitWidth + 16; height: 32; radius: 4
            color: root.checkedCount > 0 ? "#353535" : "#313131"
            FluentSymbol { x: 12; anchors.verticalCenter: parent.verticalCenter; name: "folder_arrow_right"; size: 16; variant: "regular"; color: root.checkedCount > 0 ? Theme.text : "#7c7c7c" }
            Text { id: moveText; x: 38; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Move selected entries to folder..."); color: root.checkedCount > 0 ? Theme.text : "#7c7c7c"; font.family: Theme.fontFamily; font.pixelSize: 14 }
            MouseArea { anchors.fill: parent; enabled: root.checkedCount > 0; onClicked: moveDialog.open() }
        }
        Rectangle {
            width: 38 + deleteText.implicitWidth + 16; height: 32; radius: 4
            color: root.checkedCount > 0 ? "#b22222" : "#313131"
            FluentSymbol { x: 12; anchors.verticalCenter: parent.verticalCenter; name: "delete"; size: 16; variant: "regular"; color: root.checkedCount > 0 ? "white" : "#7c7c7c" }
            Text { id: deleteText; x: 38; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Delete selected entries"); color: root.checkedCount > 0 ? "white" : "#7c7c7c"; font.family: Theme.fontFamily; font.pixelSize: 14 }
            MouseArea { anchors.fill: parent; enabled: root.checkedCount > 0; onClicked: root.removeChecked() }
        }
    }

    // --- the folder's own properties ---------------------------------------------------------
    Item { visible: root.inFolder; width: 1; height: visible ? 42 : 0 }
    Text {
        visible: root.inFolder; height: visible ? implicitHeight : 0
        text: qsTr("Current Folder Properties")
        color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14; font.weight: Font.DemiBold
    }
    SettingCard {
        id: nameCard
        visible: root.inFolder
        width: root.width; height: visible ? (nameOpen ? 124 : 62) : 0
        property bool nameOpen: false
        readonly property string current: root.inFolder ? (root.folderChain[root.folderChain.length - 1].name || "") : ""
        title: qsTr("Folder Name")
        Row {
            spacing: 8
            Text { anchors.verticalCenter: parent.verticalCenter; visible: !nameCard.nameOpen; text: nameCard.current; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: 12 }
            Rectangle {
                width: 32; height: 32; radius: 4; color: Theme.choice
                Glyph { anchors.centerIn: parent; name: "chevronDown"; rotation: nameCard.nameOpen ? 180 : 0; color: Theme.text }
                MouseArea { anchors.fill: parent; onClicked: nameCard.nameOpen = !nameCard.nameOpen }
            }
        }
        FieldBox {
            parent: nameCard
            visible: nameCard.nameOpen
            x: 16; y: 70
            text: nameCard.current
            boxWidth: 248
            onEdited: function(t) { root.edit(function(list, folder) { if (folder) folder.name = t }) }
        }
    }
    SettingCard {
        visible: root.inFolder; height: visible ? implicitHeight : 0
        width: root.width
        title: qsTr("Allow inlining")
        description: qsTr("When enabled, if the folder only has a single entry, the entry will show directly and no folder will be rendered.")
        Row {
            spacing: 12
            Text { anchors.verticalCenter: parent.verticalCenter; text: inlineToggle.checked ? qsTr("On") : qsTr("Off"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
            ToggleSwitch {
                id: inlineToggle
                anchors.verticalCenter: parent.verticalCenter
                checked: root.inFolder && root.folderChain[root.folderChain.length - 1].inline === "auto"
                onToggled: function(v) { root.edit(function(list, folder) { if (folder) folder.inline = v ? "auto" : "never" }) }
            }
        }
    }
    SettingCard {
        visible: root.inFolder; height: visible ? implicitHeight : 0
        width: root.width
        title: qsTr("Allow empty")
        description: qsTr("When enabled, if the folder has no entries, it will still be displayed. Otherwise, the folder will not be rendered.")
        Row {
            spacing: 12
            Text { anchors.verticalCenter: parent.verticalCenter; text: emptyToggle.checked ? qsTr("On") : qsTr("Off"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
            ToggleSwitch {
                id: emptyToggle
                anchors.verticalCenter: parent.verticalCenter
                checked: root.inFolder && root.folderChain[root.folderChain.length - 1].allowEmpty === true
                onToggled: function(v) { root.edit(function(list, folder) { if (folder) folder.allowEmpty = v }) }
            }
        }
    }

    // --- add an entry -------------------------------------------------------------------------
    Item { width: 1; height: 42 }
    Text { text: qsTr("Add Entry"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14; font.weight: Font.DemiBold }

    component PlusButton: Rectangle {
        id: plus
        property bool active: true
        signal clicked
        width: 32; height: 32; radius: 4
        color: Theme.choice; opacity: active ? 1 : 0.4
        Glyph { anchors.centerIn: parent; name: "plus"; color: Theme.text }
        MouseArea { anchors.fill: parent; enabled: plus.active; onClicked: plus.clicked() }
    }

    SettingCard {
        width: root.width
        icon: "window_console"
        title: qsTr("Profile")
        property string chosen: ""
        Row {
            spacing: 8
            ChoiceBox {
                id: profileChoice
                boxWidth: 248
                options: ProfileManager.profiles.filter(function(p) { return !p.hidden }).map(function(p) { return { v: p.guid, l: p.name } })
                value: parent.parent.parent.chosen.length > 0 ? parent.parent.parent.chosen : (options.length > 0 ? options[0].v : "")
                onChosen: function(v) { parent.parent.parent.chosen = v }
            }
            PlusButton { onClicked: { const p = root.profileOf(profileChoice.value); if (p) root.add({ type: "profile", profile: p.name }) } }
        }
    }
    SettingCard {
        id: sepCard
        width: root.width
        icon: " "
        title: qsTr("Separator")
        Column {   // the "=" icon of the original, drawn
            parent: sepCard
            x: 16; y: 25; spacing: 4
            Rectangle { width: 18; height: 2; radius: 1; color: Theme.text }
            Rectangle { width: 18; height: 2; radius: 1; color: Theme.text }
        }
        Row { PlusButton { onClicked: root.add({ type: "separator" }) } }
    }
    SettingCard {
        width: root.width
        icon: "folder"
        title: qsTr("Folder")
        Row {
            spacing: 8
            FieldBox { id: folderName; boxWidth: 248; placeholder: qsTr("Folder name") }
            PlusButton { active: folderName.typed.length > 0; onClicked: { root.add({ type: "folder", name: folderName.typed, entries: [] }); folderName.clear() } }
        }
    }
    // Profile matcher: an expander with three regular expressions.
    Rectangle {
        id: matcher
        width: root.width; radius: 4; color: Theme.card; border.width: 1; border.color: Theme.cardBorder
        property bool open: false
        height: open ? 62 + 12 + 24 + 3 * 66 + 52 : 62
        FluentSymbol { x: 16; y: 21; name: "person_swap"; size: 20; variant: "regular"; color: Theme.text }
        Column {
            x: 50; y: 12; width: parent.width - 120; spacing: 2
            Text { text: qsTr("Profile matcher"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
            Text { width: parent.width; text: qsTr("Add a group of profiles that match at least one of the defined regular expression properties"); color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: 12; elide: Text.ElideRight }
        }
        Rectangle {
            x: parent.width - 49; y: 15; width: 32; height: 32; radius: 4; color: matcher.open ? Theme.choice : "transparent"
            Glyph { anchors.centerIn: parent; name: "chevronDown"; rotation: matcher.open ? 180 : 0; color: Theme.text }
            MouseArea { anchors.fill: parent; onClicked: matcher.open = !matcher.open }
        }
        MouseArea { anchors.fill: parent; anchors.rightMargin: 60; height: 62; onClicked: matcher.open = !matcher.open }
        Column {
            visible: matcher.open
            x: 16; y: 74; width: parent.width - 32; spacing: 0
            Text { text: qsTr("Learn more about regular expressions"); color: Theme.accentLight; font.family: Theme.fontFamily; font.pixelSize: 14; height: 36
                   verticalAlignment: Text.AlignVCenter; leftPadding: 0
                   MouseArea { anchors.fill: parent; onClicked: Launcher.openUrl("https://learn.microsoft.com/windows/terminal/customize-settings/new-tab-menu") } }
            Repeater {
                model: [ { k: "name", t: qsTr("Profile name (regular expression)") },
                         { k: "source", t: qsTr("Profile source (regular expression)") },
                         { k: "commandline", t: qsTr("Commandline (regular expression)") } ]
                delegate: Column {
                    required property var modelData
                    spacing: 4; width: parent.width; bottomPadding: 6
                    Text { text: modelData.t; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                    FieldBox { id: rx; boxWidth: parent.width; Component.onCompleted: matcher.fields[modelData.k] = rx }
                }
            }
            Rectangle {
                width: addMatcherText.width + 52; height: 32; radius: 4; color: Theme.dark ? "#353535" : "#f0f0f0"
                Glyph { x: 12; anchors.verticalCenter: parent.verticalCenter; name: "plus"; color: Theme.text }
                Text { id: addMatcherText; x: 38; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Add profile matcher"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                MouseArea { anchors.fill: parent; onClicked: matcher.commit() }
            }
        }
        property var fields: ({})
        function commit() {
            const e = { type: "matchProfiles" }
            for (const k of ["name", "source", "commandline"]) {
                const f = fields[k]
                if (f && f.typed.length > 0) e[k] = f.typed
            }
            if (!e.name && !e.source && !e.commandline) return
            root.add(e)
            for (const k in fields) fields[k].clear()
            open = false
        }
    }
    SettingCard {
        width: root.width
        icon: "people_community"
        title: qsTr("Remaining profiles")
        description: qsTr("There can only be one \"remaining profiles\" entry")
        Row { PlusButton { active: !root.hasRemaining; onClicked: root.add({ type: "remainingProfiles" }) } }
    }

    // --- "Mover para a pasta" --------------------------------------------------------------
    Item {
        id: moveDialog
        parent: Overlay.overlay
        anchors.fill: parent
        visible: false
        z: 20000
        property int target: 0
        property var targets: []
        function open() { targets = root.folderTargets(); target = 0; visible = true }
        function close() { visible = false }
        Rectangle { anchors.fill: parent; color: "#66000000" }
        MouseArea { anchors.fill: parent }
        Rectangle {
            width: 320; height: Math.min(parent.height - 40, 150 + moveDialog.targets.length * 32 + 80)
            anchors.centerIn: parent
            radius: 8; color: Theme.dialogBody; border.width: 1; border.color: "#050505"; clip: true
            Text { x: 25; y: 26; text: qsTr("Move to folder"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 20; font.weight: Font.DemiBold }
            Column {
                x: 25; y: 76; width: parent.width - 50
                Repeater {
                    model: moveDialog.targets
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: parent.width; height: 32; radius: 4
                        color: moveDialog.target === index ? Theme.flyoutHover : "transparent"
                        Text { x: 12 + modelData.depth * 18; anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                        MouseArea { anchors.fill: parent; onClicked: moveDialog.target = index }
                    }
                }
            }
            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                height: 80; color: Theme.dialogFooter
                Row {
                    x: 25; anchors.verticalCenter: parent.verticalCenter; spacing: 9
                    Rectangle {
                        width: 131; height: 32; radius: 4; color: Theme.accentLight
                        Text { anchors.centerIn: parent; text: qsTr("OK"); color: Theme.accentText; font.family: Theme.fontFamily; font.pixelSize: 14 }
                        MouseArea { anchors.fill: parent; onClicked: { root.moveChecked(moveDialog.targets[moveDialog.target].path); moveDialog.close() } }
                    }
                    Rectangle {
                        width: 131; height: 32; radius: 4; color: Theme.dialogButton
                        Text { anchors.centerIn: parent; text: qsTr("Cancel"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                        MouseArea { anchors.fill: parent; onClicked: moveDialog.close() }
                    }
                }
            }
        }
    }
}
