import QtQuick
import QtQuick.Controls
import QtQuick.Window
import Win11Terminal

// The window of Windows Terminal: no system frame (the tab bar is the title bar),
// tabs, and the pages below. Moving and resizing go through the window manager
// (startSystemMove / startSystemResize), as in the Explorer.
ApplicationWindow {
    id: win

    required property string program
    required property var arguments
    required property string workingDirectory
    required property string profileName
    required property bool startMaximized
    required property bool startFullscreen
    // A tab handed over by another window ("Move tab to new window"): this window takes it in
    // instead of opening a first tab. The page keeps its panes and shells, nothing restarts.
    property var adopt: null
    property var adoptInfo: null
    // A window this program opened itself (a new window, a tab moved out): closed, it is destroyed.
    property bool secondary: false

    // 120x30 cells at 100% (8px padding, 17px for the scrollbar): the original's default.
    width: 1115
    height: Theme.barHeight + 586 + 2
    minimumWidth: 300
    minimumHeight: 160
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    title: currentPage ? currentPage.title : qsTr("Terminal")

    property bool closeConfirmed: false
    onClosing: function(close) {
        // confirmCloseAllTabs: closing a window with several tabs asks first.
        if (!closeConfirmed && tabs.count > 1 && AppSettings.globals.confirmCloseAllTabs !== false) {
            close.accepted = false
            confirmClose.open()
            return
        }
        // A closed window is only hidden: its tabs (and their programs) go now, and a window of
        // our own goes as well.
        shutdownAll()
        if (secondary) Qt.callLater(function() { win.destroy() })
    }
    function shutdownAll() {
        const list = pageList
        pageList = []
        tabs.clear()
        for (const page of list) {
            if (page.shutdown) page.shutdown()
            page.destroy()
        }
    }

    readonly property bool maximized: visibility === Window.Maximized
    readonly property bool fullscreen: visibility === Window.FullScreen
    property int currentIndex: 0
    // The pages, in the tabs' order (the children of `pages` keep creation order).
    property var pageList: []
    readonly property Item currentPage: currentIndex >= 0 && currentIndex < pageList.length ? pageList[currentIndex] : null
    readonly property color terminalBackground: currentPage ? currentPage.background : "#0c0c0c"

    ListModel { id: tabs }

    Connections {
        target: LanguageManager
        function onLanguageChanged() {
            win.refreshTitles()
        }
    }

    Component { id: pageComponent; TabPage { anchors.fill: parent; windowActive: win.active } }

    function newTab(profileId, cwd, program, programArguments) {
        const id = profileId && profileId.length > 0 ? profileId : ProfileManager.defaultProfileId
        const page = pageComponent.createObject(pages, {
            profileId: id, workingDirectory: cwd !== undefined ? cwd : win.workingDirectory,
            program: program !== undefined ? program : "", programArguments: programArguments !== undefined ? programArguments : [] })
        page.host = win
        // Through `page.host`, so the page can move to another window and keep these connections.
        page.closeRequested.connect(function() { const w = page.host; const i = w.pageList.indexOf(page); if (i >= 0) w.closeTab(i) })
        page.bell.connect(function(style, sound) { page.host.ring(style, sound) })
        page.titleChanged.connect(function() { page.host.refreshTitles() })
        pageList = pageList.concat([page])
        tabs.append({ title: page.title, profileId: id, kind: "terminal", tabColor: "", custom: "" })
        selectTab(tabs.count - 1)
    }
    function refreshTitles() {
        for (let i = 0; i < pageList.length; ++i)
            tabs.setProperty(i, "title", tabs.get(i).custom.length > 0 ? tabs.get(i).custom : pageList[i].title)
    }
    function selectTab(i) {
        if (i < 0 || i >= tabs.count) return
        currentIndex = i
        for (let k = 0; k < pageList.length; ++k)
            pageList[k].current = (k === i)
    }
    function closeTab(i) {
        if (i < 0 || i >= tabs.count) return
        const page = pageList[i]
        const list = pageList.slice()
        list.splice(i, 1)
        pageList = list
        tabs.remove(i)
        if (page.shutdown) page.shutdown()
        page.destroy()
        if (tabs.count === 0) {
            closeConfirmed = true
            close()       // the last window closing ends the program
            return
        }
        // The neighbour to the left takes over (the first tab when the first closed).
        selectTab(i > currentIndex ? currentIndex : Math.max(0, Math.min(currentIndex - (i <= currentIndex ? 1 : 0), tabs.count - 1)))
    }
    function moveTab(from, to) {
        if (from === to || from < 0 || to < 0 || from >= tabs.count || to >= tabs.count) return
        const list = pageList.slice()
        list.splice(to, 0, list.splice(from, 1)[0])
        pageList = list
        tabs.move(from, to, 1)
        currentIndex = currentIndex === from ? to
                     : (from < currentIndex && to >= currentIndex) ? currentIndex - 1
                     : (from > currentIndex && to <= currentIndex) ? currentIndex + 1 : currentIndex
    }
    // Takes a page (with its tab's colour and name) from another window.
    function adoptPage(page, info) {
        page.parent = pages
        page.anchors.fill = pages
        page.host = win
        page.windowActive = Qt.binding(function() { return win.active })
        pageList = pageList.concat([page])
        tabs.append({ title: page.title, profileId: page.profileId, kind: "terminal",
                      tabColor: info ? info.tabColor : "", custom: info ? info.custom : "" })
        refreshTitles()
        selectTab(tabs.count - 1)
    }
    // Gives a terminal tab to a new window of this program: the page and its shells move as they are.
    function moveTabToNewWindow(i) {
        if (i < 0 || i >= tabs.count || tabs.get(i).kind !== "terminal") return
        const comp = Qt.createComponent(Qt.resolvedUrl("Main.qml"))
        if (comp.status !== Component.Ready) return
        const page = pageList[i]
        const info = { tabColor: tabs.get(i).tabColor, custom: tabs.get(i).custom }
        const other = comp.createObject(null, { program: "", arguments: [], workingDirectory: "", profileName: "",
                                                startMaximized: false, startFullscreen: false, adopt: page, adoptInfo: info,
                                                secondary: true })
        if (!other) return
        const list = pageList.slice()
        list.splice(i, 1)
        pageList = list
        tabs.remove(i)
        if (tabs.count === 0) {
            closeConfirmed = true
            close()
        } else {
            selectTab(Math.max(0, Math.min(i - 1, tabs.count - 1)))
        }
    }
    // A new window in this same process (one Qt, one QML engine, one glyph cache for all of them).
    function openNewWindow(directory) {
        const comp = Qt.createComponent(Qt.resolvedUrl("Main.qml"))
        if (comp.status !== Component.Ready) return
        comp.createObject(null, { program: "", arguments: [], workingDirectory: directory || "", profileName: "",
                                  startMaximized: false, startFullscreen: false, secondary: true })
    }
    // A paste the settings warn about (warning.multiLinePaste / warning.largePaste) asks first.
    function confirmPaste(view, text, multiLine, large) {
        pasteDialog.view = view
        pasteDialog.text = text
        pasteDialog.body = multiLine
            ? qsTr("You are about to paste text that contains multiple lines. If you paste this text into your shell, it may result in the unexpected execution of commands. Do you wish to continue?")
            : qsTr("You are about to paste text that is longer than 5 KiB. Do you wish to continue?")
        pasteDialog.open()
    }
    // settings.json: a typo is reported (the defaults still apply) and never written over.
    property string reportedSettingsError: ""
    function reportSettingsError() {
        if (AppSettings.error.length > 0 && AppSettings.error !== reportedSettingsError)
            toast(qsTr("settings.json could not be loaded; the defaults are in use") + ": " + AppSettings.error)
        reportedSettingsError = AppSettings.error
    }
    Connections {
        target: AppSettings
        function onChanged() { win.reportSettingsError() }
        function onSaveFailed(reason) { if (win.active) win.toast(qsTr("Settings were not saved") + ": " + reason) }
    }
    function setTabColor(i, c) { tabs.setProperty(i, "tabColor", c) }
    function renameTab(i, text) { tabs.setProperty(i, "custom", text.trim()); refreshTitles() }
    function closeOthers(i) {
        for (let k = tabs.count - 1; k >= 0; --k) if (k !== i) closeTab(k)
    }
    function closeToRight(i) {
        for (let k = tabs.count - 1; k > i; --k) closeTab(k)
    }
    property string toastText: ""
    function toast(text) { toastText = text; toastTimer.restart() }
    function exportTab(i) {
        const page = pageList[i]
        if (!page || !page.activeLeaf) return
        const stamp = Qt.formatDateTime(new Date(), "yyyy-MM-dd hh.mm.ss")
        const path = Launcher.documentsFolder() + "/Terminal " + stamp + ".txt"
        toast(page.activeLeaf.view.exportText(path) ? qsTr("Successfully exported terminal content") + ": " + path
                                                    : qsTr("Failed to export terminal content"))
    }
    function openTabMenu(i, x, y) {
        tabMenu.tabIndex = i
        tabMenu.x = x + strip.x + bar.x
        tabMenu.y = y + bar.y
        tabMenu.open()
    }
    // bellStyle: "audible" (the default), "window", "taskbar", "all", "none"; or a list of those.
    function ring(style, sound) {
        const list = style === undefined || style === "" ? ["audible"] : (typeof style === "string" ? [style] : style)
        const all = list.indexOf("all") >= 0
        if (all || list.indexOf("audible") >= 0) Launcher.playBell(sound || "")
        if ((all || list.indexOf("window") >= 0 || list.indexOf("taskbar") >= 0) && !active) alert(0)
    }
    function toggleMaximized() {
        visibility = maximized ? Window.Windowed : Window.Maximized
    }
    function toggleFullscreen() {
        visibility = fullscreen ? Window.Windowed : Window.FullScreen
    }
    readonly property int tabCount: tabs.count
    function duplicateTab(i) {
        const page = pageList[i]
        if (!page || !page.activeLeaf) return
        const leaf = page.activeLeaf
        if (!leaf) return
        newTab(leaf.profileId, leaf.session.currentDirectory() || leaf.workingDirectory, leaf.program, leaf.programArguments)
    }
    function palette() { paletteLoader.active = true; return paletteLoader.item }
    function togglePalette() { palette().toggle() }
    function openAbout() { aboutLoader.active = true; aboutLoader.item.open() }
    function openDropdown() { flyout.x = strip.buttonsX + strip.x + bar.x; flyout.y = strip.barHeight + bar.y; flyout.open() }
    Component { id: settingsComponent; SettingsPage { anchors.fill: parent; windowActive: win.active } }
    function openSettings(pageId) {
        // One settings tab: a second request brings it up.
        for (let i = 0; i < tabs.count; ++i) {
            if (tabs.get(i).kind === "settings") {
                selectTab(i)
                if (pageId) pageList[i].open(pageId)
                return
            }
        }
        const page = settingsComponent.createObject(pages)
        page.closeRequested.connect(function() { const i = win.pageList.indexOf(page); if (i >= 0) win.closeTab(i) })
        page.titleChanged.connect(function() { win.refreshTitles() })
        pageList = pageList.concat([page])
        tabs.append({ title: page.title, profileId: "", kind: "settings", tabColor: "", custom: "" })
        selectTab(tabs.count - 1)
    }
    // The window that holds initialCols x initialRows cells (120x30 at 100%: 1115x628, as measured).
    function fitToInitialSize() {
        const leaf = currentPage && currentPage.activeLeaf ? currentPage.activeLeaf : null
        if (!leaf || maximized || fullscreen || leaf.view.cellWidth <= 0) return
        const cols = parseInt(AppSettings.globals.initialCols) || 120
        const rows = parseInt(AppSettings.globals.initialRows) || 30
        const pad = String(leaf.profile.padding).split(",").map(function(x) { return parseInt(x) || 0 })
        const l = pad.length >= 4 ? pad[0] : pad[0], t = pad.length >= 4 ? pad[1] : (pad.length === 2 ? pad[1] : pad[0])
        const r = pad.length >= 4 ? pad[2] : pad[0], b = pad.length >= 4 ? pad[3] : t
        width = Math.round(cols * leaf.view.cellWidth + l + r + 17 + 2)
        height = Math.round(rows * leaf.view.cellHeight + t + b + Theme.barHeight + 2)
        if (Screen.width > width && Screen.height > height) {
            win.x = Math.max(0, Math.round((Screen.width - width) / 2))
            win.y = Math.max(0, Math.round((Screen.height - height) / 2))
        }
    }
    // The terminal's context menu. `pos` is in the pane's coordinates; `byKeyboard` hangs it from the
    // cursor or the selection, like the Menu key does in the original.
    function openContextMenu(leaf, pos, byKeyboard) {
        if (!leaf || !currentPage) return
        const p = leaf.view.mapToItem(Overlay.overlay, pos.x, byKeyboard ? pos.y - 29 : pos.y)
        termMenu.hasSelection = leaf.view.hasSelection
        termMenu.profileName = leaf.profile.name
        termMenu.profiles = ProfileManager.profiles.filter(function(q) { return !q.hidden })
        termMenu.openAt(p.x, p.y, byKeyboard)
    }
    function searchWeb(text) {
        const template = AppSettings.globals.searchWebDefaultQueryUrl || "https://www.bing.com/search?q=%22%s%22"
        Launcher.openUrl(template.replace("%s", encodeURIComponent(text)))
    }
    function focusActive() { if (currentPage) currentPage.focusTerminal() }

    // Where the maximize button is, for the window manager (see WindowProps).
    WindowProps {
        window: win
        blurBehind: currentPage ? currentPage.acrylic && currentPage.opacityValue < 1 : false
        maxButton: Qt.rect(win.width - 2 * Theme.captionWidth - (win.maximized ? 0 : 1), win.maximized ? 0 : 1,
                           Theme.captionWidth, bar.height - 1)
    }
    KeyRouter {
        window: win
        // The key came to this window: its actions act here (not on the last one made active).
        handler: function(id, args, inTerminal) { Actions.window = win; return Actions.run(id, args, inTerminal) }
    }

    onActiveChanged: if (active) Actions.window = win

    Component.onCompleted: {
        Actions.window = win
        reportSettingsError()
        if (win.adopt) {
            win.adoptPage(win.adopt, win.adoptInfo)
            Qt.callLater(fitToInitialSize)
            return
        }
        // The command line names a program for the first tab; otherwise the default profile.
        let guid = ""
        for (const p of ProfileManager.profiles)
            if (win.profileName.length > 0 && (p.guid === win.profileName || p.name.toLowerCase() === win.profileName.toLowerCase())) guid = p.guid
        newTab(guid, win.workingDirectory, win.program, win.arguments)
        Qt.callLater(fitToInitialSize)
        if (win.startFullscreen) visibility = Window.FullScreen
        else if (win.startMaximized || AppSettings.globals.launchMode === "maximized") visibility = Window.Maximized
        else if (AppSettings.globals.launchMode === "fullscreen") visibility = Window.FullScreen
    }

    // ------------------------------------------------------------------ frame
    Rectangle {
        id: frame
        anchors.fill: parent
        // Opaque behind the panes, unless the profile is translucent: then the panes paint their own.
        color: currentPage && currentPage.opacityValue < 1 ? "transparent" : win.terminalBackground
        radius: win.maximized || win.fullscreen ? 0 : Theme.windowRadius
        clip: true

        // The title bar: tabs, "+ ⌄", and the caption buttons.
        Rectangle {
            id: bar
            visible: !win.fullscreen
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: win.maximized ? 0 : 1; anchors.rightMargin: win.maximized ? 0 : 1; anchors.topMargin: win.maximized ? 0 : 1
            height: win.maximized ? 31 : Theme.barHeight
            color: win.active ? Theme.barActive : Theme.barInactive

            // The bar's line, under everything.
            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                height: 1
                color: Theme.barLine
            }
            // Dragging the empty bar moves the window (the move starts with the first
            // movement, so a double click still gets through); a double click maximizes.
            DragHandler {
                target: null
                acceptedButtons: Qt.LeftButton
                onActiveChanged: if (active) win.startSystemMove()
            }
            TapHandler {
                acceptedButtons: Qt.LeftButton
                onDoubleTapped: win.toggleMaximized()
            }
            TabStrip {
                id: strip
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width - captions.width
                model: tabs
                currentIndex: win.currentIndex
                terminalBackground: win.terminalBackground
                windowActive: win.active
                compact: win.maximized
                menuOpen: flyout.visible
                onSelected: function(i) { win.selectTab(i) }
                onCloseRequested: function(i) { win.closeTab(i) }
                onMoveRequested: function(from, to) { win.moveTab(from, to) }
                onContextRequested: function(i, x, y) { win.openTabMenu(i, x, y) }
                onRenamed: function(i, text) { win.renameTab(i, text); win.focusActive() }
                onNewTabRequested: win.newTab("")
                onMenuRequested: function(x, y) { flyout.x = x + strip.x + bar.x; flyout.y = y + bar.y; flyout.open() }
            }
            Row {
                id: captions
                anchors.right: parent.right
                anchors.top: parent.top
                CaptionButton { glyph: "minimize"; barHeight: bar.height; windowActive: win.active; onActivated: win.showMinimized() }
                CaptionButton { glyph: win.maximized ? "restore" : "maximize"; barHeight: bar.height; windowActive: win.active; onActivated: win.toggleMaximized() }
                CaptionButton { glyph: "close"; danger: true; barHeight: bar.height; windowActive: win.active; onActivated: win.close() }
            }
        }

        // The pages.
        Item {
            id: pages
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: win.fullscreen ? parent.top : bar.bottom
            anchors.leftMargin: win.maximized ? 0 : 1; anchors.rightMargin: win.maximized ? 0 : 1; anchors.bottomMargin: win.maximized ? 0 : 1
        }

        // The system's 1px border.
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            radius: parent.radius
            border.width: win.maximized ? 0 : 1
            border.color: Theme.windowBorder
        }
    }

    MenuPanel {
        id: tabMenu
        parent: Overlay.overlay
        property int tabIndex: 0
        readonly property bool terminalTab: tabs.count > tabIndex && tabs.get(tabIndex).kind === "terminal"
        items: [
            { label: qsTr("Change tab color"), icon: "color", action: function() { colorFlyout.x = tabMenu.x; colorFlyout.y = tabMenu.y; colorFlyout.tabIndex = tabMenu.tabIndex; colorFlyout.open() } },
            { label: qsTr("Rename tab"), icon: "rename", action: function() { strip.startRename(tabMenu.tabIndex) } },
            { label: qsTr("Duplicate tab"), icon: "copy", enabled: terminalTab, action: function() { win.duplicateTab(tabMenu.tabIndex) } },
            { label: qsTr("Split tab"), icon: "split_horizontal", enabled: terminalTab, action: function() { win.selectTab(tabMenu.tabIndex); win.currentPage.splitActive("auto") } },
            { label: qsTr("Move tab"), submenu: [
                { label: qsTr("Move tab to new window"), icon: "panel_separate_window", enabled: terminalTab, action: function() { win.moveTabToNewWindow(tabMenu.tabIndex) } },
                { label: qsTr("Move right"), icon: "arrow_right", enabled: tabMenu.tabIndex < tabs.count - 1, action: function() { win.moveTab(tabMenu.tabIndex, tabMenu.tabIndex + 1) } },
                { label: qsTr("Move left"), icon: "arrow_left", enabled: tabMenu.tabIndex > 0, action: function() { win.moveTab(tabMenu.tabIndex, tabMenu.tabIndex - 1) } } ] },
            { label: qsTr("Export text"), icon: "save", enabled: terminalTab, action: function() { win.exportTab(tabMenu.tabIndex) } },
            { label: qsTr("Find"), icon: "search", enabled: terminalTab, action: function() { win.selectTab(tabMenu.tabIndex); Qt.callLater(function() { win.currentPage.activeLeaf.openSearch() }) } },
            { label: qsTr("Close"), separator: true, submenu: [
                { label: qsTr("Close tabs to the right"), enabled: tabMenu.tabIndex < tabs.count - 1, action: function() { win.closeToRight(tabMenu.tabIndex) } },
                { label: qsTr("Close other tabs"), enabled: tabs.count > 1, action: function() { win.closeOthers(tabMenu.tabIndex) } } ] },
            { label: qsTr("Close tab"), icon: "dismiss", action: function() { win.closeTab(tabMenu.tabIndex) } }
        ]
        onClosed: if (!colorFlyout.opened) win.focusActive()
    }
    ContextMenu {
        id: termMenu
        parent: Overlay.overlay
        function view() { return win.currentPage ? win.currentPage.activeLeaf.view : null }
        onCopyChosen: view().copy()
        onPasteChosen: view().paste()
        onFindChosen: Qt.callLater(function() { win.currentPage.activeLeaf.openSearch() })
        onDuplicateTabChosen: win.duplicateTab(win.currentIndex)
        onSplitChosen: function(guid) { win.currentPage.splitActive("auto", guid) }
        onSearchWebChosen: win.searchWeb(view().selectedText())
        onCloseTabChosen: win.closeTab(win.currentIndex)
        onClosed: win.focusActive()
    }
    TabColorFlyout {
        id: colorFlyout
        parent: Overlay.overlay
        property int tabIndex: 0
        onPicked: function(c) { win.setTabColor(tabIndex, c) }
        onClosed: win.focusActive()
    }
    // A short message at the bottom (the export's result).
    Rectangle {
        parent: Overlay.overlay
        visible: win.toastText.length > 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 24
        width: Math.min(parent.width - 40, toastLabel.implicitWidth + 32); height: 40; radius: 6
        color: Theme.flyout; border.width: 1; border.color: Theme.flyoutBorder
        z: 30000
        Text { id: toastLabel; anchors.centerIn: parent; width: parent.width - 24; elide: Text.ElideMiddle; text: win.toastText; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
        Timer { id: toastTimer; interval: 5000; onTriggered: win.toastText = "" }
    }

    ProfileFlyout {
        id: flyout
        parent: Overlay.overlay
        defaultProfileId: ProfileManager.defaultProfileId
        onProfileChosen: function(guid) { win.newTab(guid) }
        onActionChosen: function(id) { Qt.callLater(function() { Actions.run(id, {}, true) }) }
        onSettingsChosen: win.openSettings()
        onPaletteChosen: Qt.callLater(function() { win.palette().open() })
        onAboutChosen: Qt.callLater(function() { win.openAbout() })
        onClosed: if (win.currentPage) win.currentPage.focusTerminal()
    }

    // Made on first use (most sessions never open it), then kept.
    Loader {
        id: paletteLoader
        parent: Overlay.overlay
        anchors.fill: parent
        anchors.topMargin: win.maximized ? 31 : Theme.barHeight + 1
        z: 20000
        active: false
        sourceComponent: CommandPalette {
            anchors.fill: parent
            onClosed: win.focusActive()
        }
    }
    ConfirmDialog {
        id: confirmClose
        parent: Overlay.overlay
        anchors.fill: parent
        z: 20002
        title: qsTr("Do you want to close all tabs?")
        primaryText: qsTr("Close all")
        secondaryText: qsTr("Cancel")
        onAccepted: { win.closeConfirmed = true; win.close() }
        onRejected: win.focusActive()
    }
    ConfirmDialog {
        id: pasteDialog
        property var view: null
        property string text
        parent: Overlay.overlay
        anchors.fill: parent
        z: 20002
        title: qsTr("Warning")
        primaryText: qsTr("Paste anyway")
        secondaryText: qsTr("Cancel")
        onAccepted: { if (view) view.pasteText(text); text = ""; win.focusActive() }
        onRejected: { text = ""; win.focusActive() }
    }
    Loader {
        id: aboutLoader
        parent: Overlay.overlay
        anchors.fill: parent
        z: 20001
        active: false
        sourceComponent: AboutDialog {
            anchors.fill: parent
            onClosed: win.focusActive()
        }
    }

    // ---------------------------------------------------------- resize handles
    Repeater {
        model: [
            { e: Qt.LeftEdge, c: Qt.SizeHorCursor }, { e: Qt.RightEdge, c: Qt.SizeHorCursor },
            { e: Qt.TopEdge, c: Qt.SizeVerCursor }, { e: Qt.BottomEdge, c: Qt.SizeVerCursor },
            { e: Qt.LeftEdge | Qt.TopEdge, c: Qt.SizeFDiagCursor },
            { e: Qt.RightEdge | Qt.BottomEdge, c: Qt.SizeFDiagCursor },
            { e: Qt.RightEdge | Qt.TopEdge, c: Qt.SizeBDiagCursor },
            { e: Qt.LeftEdge | Qt.BottomEdge, c: Qt.SizeBDiagCursor }
        ]
        MouseArea {
            required property var modelData
            readonly property int edges: modelData.e
            readonly property bool atLeft: (edges & Qt.LeftEdge) !== 0
            readonly property bool atRight: (edges & Qt.RightEdge) !== 0
            readonly property bool atTop: (edges & Qt.TopEdge) !== 0
            readonly property bool atBottom: (edges & Qt.BottomEdge) !== 0
            readonly property bool corner: (atLeft || atRight) && (atTop || atBottom)
            enabled: !win.maximized && !win.fullscreen
            visible: enabled
            cursorShape: modelData.c
            width: corner ? 10 : (atLeft || atRight ? 5 : win.width)
            height: corner ? 10 : (atTop || atBottom ? 5 : win.height)
            x: atLeft ? 0 : (atRight ? win.width - width : 0)
            y: atTop ? 0 : (atBottom ? win.height - height : 0)
            z: 10000
            onPressed: win.startSystemResize(edges)
        }
    }
}
