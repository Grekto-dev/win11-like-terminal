import QtQuick
import Win11Terminal

// One terminal of a tab. `sides` says on which sides another pane touches it:
// there the focused pane draws its 2px accent border, inside the 4px divider.
Item {
    id: root

    property string profileId
    property string workingDirectory
    property string program
    property var programArguments: []
    property int sides: 0             // Pane.Left | Right | Top | Bottom
    property bool focusedPane: false
    property bool windowActive: true
    property int zoomDelta: 0       // Ctrl + wheel: points over the profile's size (this pane only, not saved)
    property int opacityDelta: 0    // Ctrl+Shift + wheel: percent over the profile's opacity
    // The resolved profile; it reads the lists themselves so that it follows an edit of settings.json.
    property var profile: {
        const list = ProfileManager.profiles
        const fallback = ProfileManager.defaultProfileId
        let found = null, def = null
        for (const p of list) {
            if (p.guid === profileId) found = p
            if (p.guid === fallback) def = p
        }
        return found || def || list[0]
    }
    // The profile's colour scheme; the profile's own cursorColor, when set, wins over the scheme's.
    readonly property var scheme: {
        const schemes = AppSettings.schemes
        let found = schemes.length ? schemes[0] : ({})
        for (const sc of schemes) if (sc.name === profile.colorScheme) { found = sc; break }
        if (profile.cursorColor && Qt.color(profile.cursorColor).valid) {
            const copy = {}
            for (const k in found) copy[k] = found[k]
            copy.cursorColor = profile.cursorColor
            return copy
        }
        return found
    }
    readonly property alias session: session
    readonly property alias view: view
    // tabTitle replaces the profile name; the program's own title wins unless suppressApplicationTitle.
    readonly property string baseTitle: profile.tabTitle && profile.tabTitle.length > 0 ? profile.tabTitle : profile.name
    readonly property string title: profile.suppressApplicationTitle === true ? baseTitle : (session.title.length > 0 ? session.title : baseTitle)
    readonly property color background: view.background
    readonly property real opacityValue: view.backgroundOpacity
    readonly property bool acrylic: root.profile.useAcrylic === true
    signal activated
    signal finished(int exitCode, bool close)
    signal closeRequested
    signal bell

    function focusTerminal() { view.forceActiveFocus() }
    onSchemeChanged: view.setColorScheme(scheme)
    Component.onCompleted: view.setColorScheme(scheme)
    function openSearch() { search.open() }

    TerminalSession {
        id: session
        program: root.program.length > 0 ? root.program : root.profile.program
        arguments: root.program.length > 0 ? root.programArguments : root.profile.arguments
        workingDirectory: root.workingDirectory.length > 0 ? root.workingDirectory : root.profile.startingDirectory
        historySize: root.profile.historySize
        closeOnExit: root.profile.closeOnExit || "automatic"
        onFinished: function(code, close) { root.finished(code, close) }
        onCloseRequested: root.closeRequested()
        onBell: root.bell()
    }
    TerminalView {
        id: view
        anchors.fill: parent
        session: session
        fontFamily: root.profile.fontFace
        fontSize: Math.max(1, Math.min(128, Number(root.profile.fontSize) + root.zoomDelta))
        lineHeight: root.profile.lineHeight === undefined ? 0 : Number(root.profile.lineHeight)
        cellWidthOverride: root.profile.cellWidth === undefined ? 0 : Number(root.profile.cellWidth)
        fontWeight: root.profile.fontWeight === undefined ? "normal" : root.profile.fontWeight
        fontAxes: root.profile.fontAxes || ({})
        fontFeatures: root.profile.fontFeatures || ({})
        useColorEmoji: root.profile.useColorEmoji !== false
        intenseTextStyle: root.profile.intenseTextStyle || "bright"
        adjustIndistinguishableColors: root.profile.adjustIndistinguishableColors || "indexed"
        padding: String(root.profile.padding)
        cursorShape: root.profile.cursorShape
        copyOnSelect: AppSettings.globals.copyOnSelect === true
        wordDelimiters: AppSettings.globals.wordDelimiters
        snapToGrid: AppSettings.globals.snapToGridOnResize !== false
        backgroundOpacity: Math.max(0, Math.min(100, (root.profile.opacity === undefined ? 100 : Number(root.profile.opacity)) + root.opacityDelta)) / 100
        scrollToZoom: AppSettings.globals["experimental.scrollToZoom"] !== false
        scrollToChangeOpacity: AppSettings.globals["experimental.scrollToChangeOpacity"] !== false
        // Measured on the VM : 1pt a notch; the top is a hard 128pt, but below 1pt the notches keep
        // counting (they have to be undone before the size grows again).
        onZoomRequested: function(steps) { root.zoomDelta = Math.min(root.zoomDelta + steps, 128 - Number(root.profile.fontSize)) }
        // 1% a notch, clamped to 0..100% (measured on the VM: the notches past either end are lost)
        onOpacityRequested: function(steps) {
            const base = root.profile.opacity === undefined ? 100 : Number(root.profile.opacity)
            root.opacityDelta = Math.max(-base, Math.min(100 - base, root.opacityDelta + steps))
        }
        snapOnInput: root.profile.snapOnInput !== false
        detectUrls: AppSettings.globals["experimental.detectURLs"] !== false
        onActiveFocusChanged: if (activeFocus) root.activated()
        builtinGlyphs: root.profile.builtinGlyphs !== false
        rightClickMenu: AppSettings.globals["experimental.rightClickContextMenu"] === true
        multiLinePasteWarning: AppSettings.globals["warning.multiLinePaste"]
        largePasteWarning: AppSettings.globals["warning.largePaste"] !== false
        onPasteConfirmationRequested: function(text, multiLine, large) { root.activated(); Actions.window.confirmPaste(view, text, multiLine, large) }
        onFindRequested: search.open()
        onContextMenuRequested: function(pos) { root.activated(); Actions.window.openContextMenu(root, pos, false) }
    }
    TerminalScrollBar {
        visible: root.profile.scrollbarState !== "hidden"
        alwaysShown: root.profile.scrollbarState === "always"
        view: view
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
    }
    SearchBox {
        id: search
        view: view
        x: parent.width - width - 24
        y: 7
        onClosed: view.forceActiveFocus()
    }

    // The focused pane's border, on the sides that face another pane.
    readonly property color accent: Theme.accent
    Rectangle { visible: root.focusedPane && (root.sides & 1); x: -2; width: 2; height: parent.height; color: root.accent }
    Rectangle { visible: root.focusedPane && (root.sides & 2); x: parent.width; width: 2; height: parent.height; color: root.accent }
    Rectangle { visible: root.focusedPane && (root.sides & 4); y: -2; height: 2; width: parent.width; color: root.accent }
    Rectangle { visible: root.focusedPane && (root.sides & 8); y: parent.height; height: 2; width: parent.width; color: root.accent }
}
