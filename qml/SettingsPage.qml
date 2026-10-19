import QtQuick
import QtQuick.Controls
import Win11Terminal

// The settings page, a tab of its own like in Windows Terminal: a navigation pane on
// the left, the page in a 998px column, and Save / Discard changes at the bottom.
// Layout and colours are measured on the VM (NOTES.md section 5, refs/04 to 18).
Rectangle {
    id: root

    property string page: "startup"
    onPageChanged: { if (flick) flick.contentY = 0 }
    property bool current: false
    property bool windowActive: true
    readonly property color background: "#0c0c0c"   // the tab's colour: the terminal's, as in the original
    readonly property real opacityValue: 1
    readonly property bool acrylic: false
    readonly property string title: qsTr("Settings")
    property bool navCollapsed: false
    readonly property real navWidth: navCollapsed ? 48 : 320
    signal closeRequested
    visible: current
    color: Theme.settingsBackground

    function focusTerminal() { root.forceActiveFocus() }
    function open(pageId) { page = pageId }

    SettingsStore { id: store }

    // ---- the pages: rows described as data -------------------------------------------------
    readonly property var navItems: {
        const _ = LanguageManager.currentLanguage
        return [
            { id: "startup", label: qsTr("Startup"), icon: "rocket" },
            { id: "interaction", label: qsTr("Interaction"), icon: "cursor" },
            { id: "appearance", label: qsTr("Appearance"), icon: "paint_brush" },
            { id: "colors", label: qsTr("Color schemes"), icon: "color" },
            { id: "compat", label: qsTr("Compatibility"), icon: "wrench" },
            { id: "actions", label: qsTr("Actions"), icon: "keyboard" },
            { id: "newtab", label: qsTr("New tab menu"), icon: "apps_list_detail" },
            { id: "about", label: qsTr("About"), icon: "info" }
        ]
    }
    function schemeChoices() {
        return AppSettings.schemes.map(function(s) { return { v: s.name, l: s.name } })
    }
    function profileChoices() {
        return ProfileManager.profiles.map(function(p) { return { v: p.guid, l: p.name } })
    }
    function profileNavIcon(p) {
        if (!p) return "window_console"
        const name = (p.name || "").toLowerCase()
        if (name.indexOf("bash") >= 0) return "bash"
        if (name === "sh") return "sh"
        if (name.indexOf("dash") >= 0) return "dash"
        if (p.icon && p.icon.length > 0 && p.icon !== "terminal") return p.icon
        return "window_console"
    }
    function profileRows(scope, isDefaults) {
        const rows = []
        if (isDefaults) rows.push({ type: "info", text: qsTr("The settings defined here will be applied to all profiles unless a profile's own settings override them.") })
        if (!isDefaults) {
            rows.push({ type: "field", scope: scope, key: "name", title: qsTr("Name"), desc: qsTr("The name of the profile that appears in the dropdown.") })
            rows.push({ type: "field", scope: scope, key: "commandline", title: qsTr("Command line"), desc: qsTr("Executable used in the profile."), width: 360 })
        }
        rows.push({ type: "field", scope: scope, key: "startingDirectory", title: qsTr("Starting directory"), desc: qsTr("The directory the profile starts in when it is loaded."), width: 360 })
        rows.push({ type: "field", scope: scope, key: "tabTitle", title: qsTr("Tab title"), desc: qsTr("Replaces the profile name as the title to pass to the shell on startup."), width: 248 })
        if (!isDefaults) rows.push({ type: "toggle", scope: scope, key: "hidden", title: qsTr("Hide profile from dropdown"),
                                     desc: qsTr("If enabled, the profile will not appear in the list of profiles. This can be used to hide default profiles and dynamically generated profiles, while leaving them in your settings file.") })
        rows.push({ type: "section", text: qsTr("Additional settings") })
        const base = isDefaults ? "defaults" : "profile:" + scope
        rows.push({ type: "link", title: qsTr("Appearance"), page: base + ":appearance" })
        rows.push({ type: "link", title: qsTr("Terminal emulation"), page: base + ":emulation" })
        rows.push({ type: "link", title: qsTr("Advanced"), page: base + ":advanced" })
        if (!isDefaults) rows.push({ type: "danger", title: "", button: qsTr("Delete profile"), action: "delete", guid: scope })
        return rows
    }
    // Appearance, in the order and with the texts measured on the VM .
    function appearanceRows(scope) {
        return [
            { type: "preview", scope: scope },
            { type: "section", text: qsTr("Text"), first: true },
            { type: "choice", scope: scope, key: "colorScheme", title: qsTr("Color scheme"), desc: qsTr("Expand to override the foreground, background, and selection background."), options: schemeChoices() },
            { type: "field", scope: scope, key: "fontFace", title: qsTr("Font face"), desc: qsTr("You can use multiple fonts by separating them with an ASCII comma."), width: 248 },
            { type: "number", scope: scope, key: "fontSize", title: qsTr("Font size"), desc: qsTr("Size of the font in points."), min: 1, max: 128, def: 12 },
            { type: "decimal", scope: scope, key: "lineHeight", min: 0, max: 10, def: 1.2, title: qsTr("Line height"), desc: qsTr("Override the line height of the terminal. Measured as a multiple of the font size. The default value depends on your font and is typically around 1.2.") },
            { type: "decimal", scope: scope, key: "cellWidth", min: 0, max: 10, def: 0.6, title: qsTr("Cell width"), desc: qsTr("Override the cell width of the terminal. Measured as a multiple of the font size. The default value depends on your font and is typically around 0.6.") },
            { type: "choice", scope: scope, key: "fontWeight", def: "normal", title: qsTr("Font weight"), desc: qsTr("Sets the weight (lightness or heaviness of the strokes) for the given font."), options: [
                { v: "thin", l: qsTr("Thin") }, { v: "extra-light", l: qsTr("Extra-Light") }, { v: "light", l: qsTr("Light") }, { v: "semi-light", l: qsTr("Semi-Light") },
                { v: "normal", l: qsTr("Normal") }, { v: "medium", l: qsTr("Medium") }, { v: "semi-bold", l: qsTr("Semi-Bold") }, { v: "bold", l: qsTr("Bold") },
                { v: "extra-bold", l: qsTr("Extra-Bold") }, { v: "black", l: qsTr("Black") }, { v: "extra-black", l: qsTr("Extra-Black") }, { v: "custom", l: qsTr("Custom") }] },
            { type: "fontAxes", scope: scope, key: "fontAxes", title: qsTr("Variable font axes"), desc: qsTr("Add or remove font axes for the given font.") },
            { type: "fontFeatures", scope: scope, key: "fontFeatures", title: qsTr("Font features"), desc: qsTr("Add or remove font features for the given font.") },
            { type: "toggle", scope: scope, key: "builtinGlyphs", def: true, title: qsTr("Builtin Glyphs"), desc: qsTr("When enabled, the terminal draws custom glyphs for block element and box drawing characters instead of using the font.") },
            { type: "toggle", scope: scope, key: "useColorEmoji", def: true, title: qsTr("Full-color Emoji"), desc: qsTr("When enabled, Emoji are displayed in full color.") },
            { type: "toggle", scope: scope, key: "experimental.retroTerminalEffect", title: qsTr("Retro terminal effects"), desc: qsTr("Show \"retro\" terminal effects such as glowing text and scan lines.") },
            { type: "choice", scope: scope, key: "adjustIndistinguishableColors", def: "indexed", title: qsTr("Automatically adjust lightness of indistinguishable text"),
              desc: qsTr("Automatically brightens or darkens text to make it more visible. Even when enabled, this adjustment will only occur when a combination of foreground and background colors would result in poor contrast."), options: [
                { v: "never", l: qsTr("Never") }, { v: "indexed", l: qsTr("Only for colors in the color scheme") }, { v: "always", l: qsTr("Always") }] },
            { type: "section", text: qsTr("Cursor") },
            { type: "choice", scope: scope, key: "cursorShape", title: qsTr("Cursor shape"), options: [
                { v: "vintage", l: qsTr("Vintage ( ▃ )") }, { v: "bar", l: qsTr("Bar ( ┃ )") }, { v: "underscore", l: qsTr("Underscore ( ▁ )") },
                { v: "doubleUnderscore", l: qsTr("Double underscore ( ‗ )") }, { v: "filledBox", l: qsTr("Filled box ( █ )") }, { v: "emptyBox", l: qsTr("Empty box ( ▯ )") }] },
            { type: "cursorColor", scope: scope, title: qsTr("Cursor color"), desc: qsTr("Overrides the cursor color from the color scheme.") },
            { type: "section", text: qsTr("Background image") },
            { type: "backgroundImage", scope: scope, title: qsTr("Background image path"), desc: qsTr("File location of the image used in the background of the window.") },
            { type: "slider", scope: scope, key: "backgroundImageOpacity", title: qsTr("Background image opacity"), desc: qsTr("Sets the transparency of the background image."), min: 0, max: 100, def: 50 },
            { type: "choice", scope: scope, key: "backgroundImageStretchMode", def: "uniformToFill", title: qsTr("Background image stretch mode"), desc: qsTr("Defines how the background image is resized to fill the window."), options: [
                { v: "uniformToFill", l: qsTr("Uniform to fill") },
                { v: "uniform", l: qsTr("Uniform") },
                { v: "fill", l: qsTr("Fill") },
                { v: "none", l: qsTr("None") }] },
            { type: "section", text: qsTr("Text Formatting") },
            { type: "choice", scope: scope, key: "intenseTextStyle", def: "bright", title: qsTr("Intense text style"), options: [
                { v: "none", l: qsTr("None") }, { v: "bold", l: qsTr("Bold font") }, { v: "bright", l: qsTr("Bright colors") }, { v: "all", l: qsTr("Bold font with bright colors") }] },
            { type: "section", text: qsTr("Transparency") },
            { type: "slider", scope: scope, key: "opacity", title: qsTr("Background opacity"), desc: qsTr("Sets the transparency of the window for the profile."), min: 0, max: 100, def: 100 },
            { type: "toggle", scope: scope, key: "useAcrylic", title: qsTr("Enable acrylic material"), desc: qsTr("Applies a translucent texture to the background of the window.") },
            { type: "section", text: qsTr("Window") },
            { type: "padding", scope: scope, title: qsTr("Padding"), desc: qsTr("Sets the padding around the text within the window.") },
            { type: "choice", scope: scope, key: "scrollbarState", def: "visible", title: qsTr("Scrollbar visibility"), options: [
                { v: "visible", l: qsTr("Visible") }, { v: "hidden", l: qsTr("Hidden") }, { v: "always", l: qsTr("Always") }] }
        ]
    }
    function emulationRows(scope) {
        return [
            { type: "choice", scope: scope, key: "closeOnExit", title: qsTr("Profile termination behavior"), def: "automatic", options: [
                { v: "never", l: qsTr("Never close automatically") }, { v: "automatic", l: qsTr("Close only when process exits successfully") },
                { v: "always", l: qsTr("Close when process exits, fails, or crashes") }] },
            { type: "toggle", scope: scope, key: "snapOnInput", title: qsTr("Scroll to input when typing"), def: true },
            { type: "toggle", scope: scope, key: "suppressApplicationTitle", title: qsTr("Suppress title changes") }
        ]
    }
    // Advanced, measured on the VM : the VM page has no section headings.
    function advancedRows(scope) {
        return [
            { type: "toggle", scope: scope, key: "snapOnInput", title: qsTr("Scroll to input when typing"), def: true },
            { type: "number", scope: scope, key: "historySize", title: qsTr("History size"), desc: qsTr("The number of lines above the ones displayed in the window you can scroll back to."), min: 0, max: 999999, def: 9001 },
            { type: "choice", scope: scope, key: "closeOnExit", title: qsTr("Profile termination behavior"), def: "automatic", options: [
                { v: "automatic", l: qsTr("Automatic") }, { v: "always", l: qsTr("Close when process exits, fails, or crashes") },
                { v: "graceful", l: qsTr("Close only when process exits successfully") }, { v: "never", l: qsTr("Never close automatically") }] },
            { type: "bell", scope: scope, title: qsTr("Bell notification style"), desc: qsTr("Controls what happens when the application emits a BEL character.") },
            { type: "bellSound", scope: scope, title: qsTr("Bell sound"), desc: qsTr("Controls what sound is played when the application emits a BEL character. \"Bell notification style\" must include \"Audible\".") },
            { type: "toggle", scope: "g", key: "experimental.rightClickContextMenu", def: true, title: qsTr("Display a menu on right-click"),
              desc: qsTr("When enabled, the Terminal will display a menu on right-click. When disabled, right-clicking will copy the selected text (or paste if there's no selection).") }
        ]
    }
    function schemeRows(name) {
        const rows = [ { type: "schemeActions", name: name } ]
        const colors = [["foreground", qsTr("Foreground")], ["background", qsTr("Background")], ["cursorColor", qsTr("Cursor color")],
                        ["selectionBackground", qsTr("Selection background")],
                        ["black", qsTr("Black")], ["red", qsTr("Red")], ["green", qsTr("Green")], ["yellow", qsTr("Yellow")],
                        ["blue", qsTr("Blue")], ["purple", qsTr("Purple")], ["cyan", qsTr("Cyan")], ["white", qsTr("White")],
                        ["brightBlack", qsTr("Bright black")], ["brightRed", qsTr("Bright red")], ["brightGreen", qsTr("Bright green")],
                        ["brightYellow", qsTr("Bright yellow")], ["brightBlue", qsTr("Bright blue")], ["brightPurple", qsTr("Bright purple")],
                        ["brightCyan", qsTr("Bright cyan")], ["brightWhite", qsTr("Bright white")]]
        for (const c of colors) rows.push({ type: "color", scheme: name, key: c[0], title: c[1] })
        return rows
    }
    function rowsFor(id) {
        switch (id) {
        case "startup": return [
            { type: "choice", scope: "g", key: "defaultProfile", title: qsTr("Default profile"),
              desc: qsTr("Profile that opens when you click the ' + ' icon or by typing the new tab key binding."), options: profileChoices(), profile: true },
            { type: "size", title: qsTr("Launch size"), desc: qsTr("The number of rows and columns displayed in the window upon first load. Measured in characters.") } ]
        case "interaction": return [
            { type: "toggle", scope: "g", key: "copyOnSelect", title: qsTr("Automatically copy selection to clipboard") },
            { type: "toggle", scope: "g", key: "trimBlockSelection", title: qsTr("Remove trailing white-space in rectangular selection"), def: true },
            { type: "toggle", scope: "g", key: "trimPaste", title: qsTr("Remove trailing white-space when pasting"), def: true },
            { type: "field", scope: "g", key: "wordDelimiters", title: qsTr("Word delimiters"), desc: qsTr("These symbols will be used when you double-click text in the terminal or activate \"Mark mode\"."), width: 300 },
            { type: "toggle", scope: "g", key: "snapToGridOnResize", title: qsTr("Snap window resizing to character grid"), desc: qsTr("When disabled, the window will resize smoothly."), def: true },
            { type: "toggle", scope: "g", key: "focusFollowMouse", title: qsTr("Automatically focus pane on mouse hover") },
            { type: "toggle", scope: "g", key: "experimental.scrollToZoom", title: qsTr("Adjust terminal font size by scrolling while holding the Ctrl key"), def: true },
            { type: "toggle", scope: "g", key: "experimental.scrollToChangeOpacity", title: qsTr("Adjust terminal opacity by scrolling while holding the Ctrl and Shift keys"), def: true },
            { type: "toggle", scope: "g", key: "experimental.rightClickContextMenu", title: qsTr("Display a menu on right-click"),
              desc: qsTr("When enabled, the Terminal will display a menu on right-click. When disabled, right-clicking will copy the selected text (or paste if there's no selection)."), def: true },
            { type: "toggle", scope: "g", key: "experimental.detectURLs", title: qsTr("Automatically detect URLs and make them clickable"), def: true } ]
        case "appearance": return [
            { type: "language", title: qsTr("Language"), desc: qsTr("Select the display language for the application interface.") },
            { type: "toggle", scope: "g", key: "alwaysShowTabs", title: qsTr("Always show tabs"), def: true },
            { type: "toggle", scope: "g", key: "disableAnimations", title: qsTr("Pane animations"), invert: true, def: false },
            { type: "toggle", scope: "g", key: "alwaysOnTop", title: qsTr("Always on top"), desc: qsTr("Terminal will always be the topmost window on the desktop.") },
            { type: "choice", scope: "g", key: "tabWidthMode", title: qsTr("Tab width mode"), desc: qsTr("Compact will shrink inactive tabs to the size of the icon."),
              options: [{ v: "equal", l: qsTr("Equal") }, { v: "titleLength", l: qsTr("Title length") }, { v: "compact", l: qsTr("Compact") }], def: "equal" } ]
        case "compat": return [
            { type: "danger", title: qsTr("Clear cache"), desc: qsTr("The cache stores data related to saved sessions and automatically generated profiles."), button: qsTr("Clear"), action: "clearCache" },
            { type: "danger", title: qsTr("Reset to default settings"), button: qsTr("Restore"), action: "reset" } ]
        case "about": return [
            { type: "aboutCard", title: qsTr("Win11-Like Terminal for Linux"), desc: qsTr("A standalone, modern Windows 11-style terminal emulator built for Linux Mint and other distributions."), tag: "v0.9.0" },
            { type: "aboutCard", title: qsTr("Version and Environment"), desc: qsTr("Application version: %1\nQt Runtime: %2\nPlatform: Linux (%3)").arg("0.9.0").arg(AppSettings.qtVersion).arg(Qt.platform.os) },
            { type: "aboutCard", title: qsTr("Source Code & Repository"), desc: "https://github.com/Grekto-dev/win11-like-terminal", link: "https://github.com/Grekto-dev/win11-like-terminal" },
            { type: "aboutCard", title: qsTr("License"), desc: qsTr("Open source under the MIT License. Includes Cascadia Mono (SIL OFL), libvterm (MIT), and Fluent UI System Icons (MIT).") }
        ]
        case "defaults:appearance": return appearanceRows("d")
        case "defaults:emulation": return emulationRows("d")
        case "defaults:advanced": return advancedRows("d")
        case "add": return [
            { type: "action", title: qsTr("New empty profile"), button: qsTr("New empty profile"), action: "newProfile" },
            { type: "duplicate", title: qsTr("Duplicate a profile") } ]
        }
        if (id.indexOf("scheme:") === 0) return schemeRows(id.slice(7))
        const m = /^profile:(.+?)(?::(appearance|emulation|advanced))?$/.exec(id)
        if (m) return m[2] === "appearance" ? appearanceRows(m[1]) : m[2] === "emulation" ? emulationRows(m[1])
                     : m[2] === "advanced" ? advancedRows(m[1]) : profileRows(m[1], false)
        return []
    }
    function titleFor(id) {
        for (const n of navItems) if (n.id === id) return n.label
        if (id === "defaults") return qsTr("Defaults")
        if (id === "defaults:appearance") return qsTr("Appearance")
        if (id === "defaults:emulation") return qsTr("Terminal emulation")
        if (id === "defaults:advanced") return qsTr("Advanced")
        if (id === "add") return qsTr("Add a new profile")
        if (id.indexOf("scheme:") === 0) return id.slice(7)
        const m = /^profile:(.+?)(?::(appearance|emulation|advanced))?$/.exec(id)
        if (m) {
            if (m[2] === "appearance") return qsTr("Appearance")
            if (m[2] === "emulation") return qsTr("Terminal emulation")
            if (m[2] === "advanced") return qsTr("Advanced")
            const p = store.profileOf(m[1]); return p ? p.name : ""
        }
        return ""
    }
    function parentOf(id) {
        if (id.indexOf("scheme:") === 0) return "colors"
        for (const suffix of [":appearance", ":emulation", ":advanced"])
            if (id.endsWith(suffix)) return id.slice(0, -suffix.length)
        return ""
    }
    readonly property bool isActionsPage: page === "actions"
    readonly property bool isColorsPage: page === "colors"
    readonly property bool isSchemePage: page.indexOf("scheme:") === 0

    // ---- navigation ----------------------------------------------------------------------------
    Item {
        id: nav
        width: root.navWidth
        anchors.top: parent.top; anchors.bottom: parent.bottom
        clip: true

        Item {   // the hamburger
            x: 4; y: 6; width: 40; height: 36
            Glyph { anchors.centerIn: parent; name: "menu"; color: Theme.text }
            MouseArea { anchors.fill: parent; onClicked: root.navCollapsed = !root.navCollapsed }
        }
        component NavItem: Item {
            id: item
            property string label
            property string icon
            property string badge
            property string pageId
            property bool selectedItem: root.page === pageId || root.page.indexOf(pageId + ":") === 0
            property Item glyph
            x: 4; width: root.navWidth - 8; height: 36
            Rectangle {
                anchors.fill: parent; radius: 4
                color: item.selectedItem ? Theme.navSelected : nmouse.containsMouse ? Theme.navHover : "transparent"
            }
            Rectangle { visible: item.selectedItem; x: 0; y: 12; width: 3; height: 12; radius: 1.5; color: Theme.accentLight }
            FluentSymbol { x: 12; anchors.verticalCenter: parent.verticalCenter; name: item.icon; size: 16; variant: "regular"; color: Theme.text; visible: item.icon.length > 0 }
            Text {
                x: 44; anchors.verticalCenter: parent.verticalCenter
                visible: !root.navCollapsed
                text: item.label; color: Theme.text
                font.family: Theme.fontFamily; font.pixelSize: 14
            }
            Rectangle {
                visible: item.badge.length > 0 && !root.navCollapsed
                anchors.right: parent.right; anchors.rightMargin: 14; anchors.verticalCenter: parent.verticalCenter
                width: badgeText.width + 12; height: 16; radius: 8; color: Theme.accentLight
                Text { id: badgeText; anchors.centerIn: parent; text: item.badge; color: Theme.accentText; font.family: Theme.fontFamily; font.pixelSize: 10 }
            }
            MouseArea { id: nmouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.open(item.pageId) }
        }
        Flickable {
            id: navFlick
            y: 47
            width: parent.width
            height: parent.height - 47 - 56
            contentHeight: navColumn.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { active: false; width: 0 }
        Column {
            id: navColumn
            spacing: 4
            Repeater {
                model: root.navItems
                NavItem { required property var modelData; pageId: modelData.id; label: modelData.label; icon: modelData.icon; badge: modelData.badge || "" }
            }
            Item {
                width: 1; height: 36
                Text { x: 15; anchors.verticalCenter: parent.verticalCenter; visible: !root.navCollapsed; text: qsTr("Profiles"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14; font.weight: Font.DemiBold }
            }
            NavItem { pageId: "defaults"; label: qsTr("Defaults"); icon: "layer" }
            Repeater {
                model: ProfileManager.profiles
                NavItem { required property var modelData; pageId: "profile:" + modelData.guid; label: modelData.name; icon: root.profileNavIcon(modelData) }
            }
            NavItem { pageId: "add"; label: qsTr("Add a new profile"); icon: "add" }
        }
        }
        NavItem {
            y: parent.height - 41; pageId: "json"; label: qsTr("Open JSON file"); icon: "settings"; selectedItem: false
            MouseArea { anchors.fill: parent; onClicked: Launcher.openFile(AppSettings.filePath, true) }
        }
    }

    // ---- the page ---------------------------------------------------------------------------------
    Item {
        id: area
        x: root.navWidth
        width: parent.width - x
        anchors.top: parent.top
        anchors.bottom: footer.top
        readonly property real column: Math.min(998, width - 34)

        Flickable {
            id: flick
            anchors.fill: parent
            contentWidth: width
            contentHeight: body.height + 40
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { active: true }

            Column {
                id: body
                x: (parent.width - area.column) / 2
                y: 7
                width: area.column
                spacing: 6

                Row {
                    visible: root.page !== "newtab"
                    spacing: 8
                    Item {   // back to the profile from its appearance page
                        visible: root.parentOf(root.page).length > 0
                        width: 32; height: 32
                        Glyph { anchors.centerIn: parent; name: "chevronDown"; rotation: 90; color: Theme.text }
                        MouseArea { anchors.fill: parent; onClicked: root.open(root.parentOf(root.page)) }
                    }
                    Text {
                        text: (LanguageManager.currentLanguage, root.titleFor(root.page))
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        height: 32
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Loader {
                    active: root.page === "newtab"
                    width: body.width
                    sourceComponent: NewTabMenuPage { width: body.width }
                    onLoaded: item.store = store
                }
                Repeater {
                    model: (root.isActionsPage || root.isColorsPage || root.page === "newtab") ? [] : (LanguageManager.currentLanguage, root.rowsFor(root.page))
                    delegate: Loader {
                        id: rowLoader
                        required property var modelData
                        width: body.width
                        enabled: modelData.disabled !== true
                        opacity: modelData.disabled === true ? 0.45 : 1
                        sourceComponent: modelData.type === "toggle" ? toggleRow : modelData.type === "choice" ? choiceRow
                            : modelData.type === "language" ? languageRow
                            : modelData.type === "backgroundImage" ? backgroundImageRow
                            : modelData.type === "bellSound" ? bellSoundRow
                            : modelData.type === "aboutCard" ? aboutCardRow
                            : modelData.type === "field" ? fieldRow : modelData.type === "number" ? numberRow
                            : modelData.type === "size" ? sizeRow : modelData.type === "section" ? sectionRow
                            : modelData.type === "preview" ? previewRow : modelData.type === "decimal" ? decimalRow
                            : modelData.type === "slider" ? sliderRow : modelData.type === "padding" ? paddingRow
                            : modelData.type === "bell" ? bellRow : modelData.type === "cursorColor" ? cursorColorRow
                            : modelData.type === "expander" ? expanderRow
                            : modelData.type === "fontAxes" ? fontAxesRow : modelData.type === "fontFeatures" ? fontFeaturesRow
                            : modelData.type === "link" ? linkRow : modelData.type === "danger" ? dangerRow
                            : modelData.type === "color" ? colorRow : modelData.type === "schemeActions" ? schemeActionsRow : modelData.type === "action" ? actionRow : modelData.type === "duplicate" ? duplicateRow : infoRow
                    }
                }

                Rectangle {   // "Adicionar novo": a copy of Campbell to edit
                    visible: root.isColorsPage
                    width: addText.width + 60; height: 32; radius: 4; color: Theme.card
                    border.width: 1; border.color: Theme.cardBorder
                    FluentSymbol { x: 12; anchors.verticalCenter: parent.verticalCenter; name: "add"; size: 16; variant: "regular"; color: Theme.text }
                    Text { id: addText; x: 40; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Add new"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                    MouseArea { anchors.fill: parent; onClicked: {
                        let name = "Campbell (" + qsTr("copy") + ")", n = 2
                        while (AppSettings.scheme(name).name === name) name = "Campbell (" + qsTr("copy") + " " + (n++) + ")"
                        const base = AppSettings.scheme("Campbell")
                        const copy = {}
                        for (const k in base) copy[k] = base[k]
                        copy.name = name
                        AppSettings.applyChanges({ schemes: (function() { const o = {}; o[name] = copy; return o })() })
                        root.open("scheme:" + name)
                    } }
                }
                // Color schemes: a card per scheme, the default marked.
                Repeater {
                    model: root.isColorsPage ? AppSettings.schemes : []
                    delegate: Item {
                        id: schemeItem
                        required property var modelData
                        readonly property bool isDefault: store.get("d", "colorScheme", "Campbell") === modelData.name
                        width: body.width; height: 62
                        Rectangle {
                            x: 7; y: 0
                            width: swatch.width + nameText.width + 60 + (schemeItem.isDefault ? 70 : 0); height: 56
                            radius: 4
                            color: schemeItem.modelData.background
                            Grid {
                                id: swatch
                                x: 12; anchors.verticalCenter: parent.verticalCenter
                                columns: 8; spacing: 0
                                Repeater {
                                    model: ["black","red","green","yellow","blue","purple","cyan","white","brightBlack","brightRed","brightGreen","brightYellow","brightBlue","brightPurple","brightCyan","brightWhite"]
                                    Rectangle { required property string modelData; width: 14; height: 14; color: schemeItem.modelData[modelData] || "black" }
                                }
                            }
                            Text {
                                id: nameText
                                x: swatch.x + swatch.width + 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: schemeItem.modelData.name
                                color: schemeItem.modelData.foreground
                                font.family: "Cascadia Mono"; font.pixelSize: 14; font.bold: true
                            }
                            Rectangle {
                                visible: schemeItem.isDefault
                                x: nameText.x + nameText.width + 12
                                anchors.verticalCenter: parent.verticalCenter
                                width: 58; height: 28; radius: 4; color: "transparent"; border.width: 1; border.color: "#8a8a8a"
                                Text { anchors.centerIn: parent; text: qsTr("default"); color: "white"; font.family: Theme.fontFamily; font.pixelSize: 14 }
                            }
                            MouseArea { anchors.fill: parent; onClicked: root.open("scheme:" + schemeItem.modelData.name) }
                        }
                    }
                }

                // Actions: every key binding, by name.
                Repeater {
                    model: root.isActionsPage ? root.actionRows() : []
                    delegate: Item {
                        required property var modelData
                        width: 410; height: 40
                        Text { x: 4; anchors.verticalCenter: parent.verticalCenter; text: modelData.name; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                        Rectangle {
                            anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                            width: chip.width + 8; height: 22; radius: 4; color: "transparent"; border.width: 1; border.color: Theme.chipBorder
                            Text { id: chip; anchors.centerIn: parent; text: modelData.keys; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: 12 }
                        }
                    }
                }
            }
        }
    }

    function actionRows() {
        const rows = []
        for (const b of AppSettings.keybindings) {
            const e = Actions.entry(b.id)
            if (b.id && e) rows.push({ name: e.name, keys: b.keys })
        }
        rows.sort(function(a, b) { return a.name.localeCompare(b.name) || a.keys.localeCompare(b.keys) })
        return rows
    }

    // ---- row types --------------------------------------------------------------------------------
    Component { id: infoRow
        Text { text: modelData.text; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: 12; wrapMode: Text.Wrap; width: body.width } }
    // An expandable card: the header is a clickable card with the current value and a chevron, the body opens below it.
    component ExpanderCard: Column {
        id: ex
        property string title
        property string description
        property string valueText
        property color swatch: "transparent"
        property bool expanded: false
        default property alias body: bodyItem.data
        spacing: 1
        SettingCard {
            width: ex.width; title: ex.title; description: ex.description; clickable: true
            onClicked: ex.expanded = !ex.expanded
            Row {
                spacing: 12
                Rectangle { visible: ex.swatch.a > 0; anchors.verticalCenter: parent.verticalCenter; width: 20; height: 20; radius: 2; color: ex.swatch; border.width: 1; border.color: Theme.cardBorder }
                Text { anchors.verticalCenter: parent.verticalCenter; text: ex.valueText; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                Glyph { anchors.verticalCenter: parent.verticalCenter; name: "chevronDown"; rotation: ex.expanded ? 180 : 0; color: Theme.text }
            }
        }
        Rectangle {
            visible: ex.expanded; width: ex.width; height: bodyItem.childrenRect.height + 32; radius: 4
            color: Theme.card; border.width: 1; border.color: Theme.cardBorder
            Item { id: bodyItem; x: 16; y: 16; width: parent.width - 32; height: childrenRect.height }
        }
    }
    component SmallCheck: Item {
        id: chk
        property string label
        property bool checked: false
        signal toggled(bool value)
        width: boxRow.width; height: 32
        Row {
            id: boxRow; spacing: 8; anchors.verticalCenter: parent.verticalCenter
            Rectangle {
                width: 20; height: 20; radius: 4; anchors.verticalCenter: parent.verticalCenter
                color: chk.checked ? Theme.accentLight : "transparent"; border.width: chk.checked ? 0 : 1; border.color: "#9a9a9a"
                Text { visible: chk.checked; anchors.centerIn: parent; text: "✓"; color: Theme.accentText; font.pixelSize: 14 }
            }
            Text { anchors.verticalCenter: parent.verticalCenter; text: chk.label; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
        }
        MouseArea { anchors.fill: parent; onClicked: chk.toggled(!chk.checked) }
    }
    component SmallButton: Item {
        id: smallButton
        property string label
        signal clicked
        width: buttonText.width + 24; height: 30
        Rectangle { anchors.fill: parent; radius: 4; color: Theme.accentLight }
        Text { id: buttonText; anchors.centerIn: parent; text: smallButton.label; color: Theme.accentText; font.family: Theme.fontFamily; font.pixelSize: 13 }
        MouseArea { anchors.fill: parent; onClicked: smallButton.clicked() }
    }
    Component { id: previewRow
        Rectangle {
            readonly property var sc: { store.rev; return AppSettings.scheme(store.get(modelData.scope, "colorScheme", "Campbell")) }
            readonly property string fam: "Cascadia Mono"
            width: 400; height: 180; radius: 4; color: sc.background || "#0c0c0c"; border.width: 1; border.color: Theme.cardBorder
            Column {
                x: 10; y: 6
                component Line: Row { property alias parts: rep.model; Repeater { id: rep; Text { required property var modelData; text: modelData[0]; color: modelData[1]; font.family: "Cascadia Mono"; font.pixelSize: 16; textFormat: Text.PlainText } } }
                Line { parts: [["Terminal", sc.foreground || "#cccccc"]] }
                Line { parts: [["C:\\> ", sc.foreground || "#cccccc"], ["git", sc.yellow || "#c19c00"], [" diff -w", sc.foreground || "#cccccc"]] }
                Line { parts: [["diff --git a/win b/win", sc.foreground || "#cccccc"]] }
                Line { parts: [["@@ -1 +1 @@", sc.cyan || "#3a96dd"]] }
                Line { parts: [["-    Legacy console", sc.red || "#c50f1f"]] }
                Line { parts: [["+    Terminal!", sc.green || "#13a10e"]] }
                Line { parts: [["C:\\> ", sc.foreground || "#cccccc"], ["Write-Host", sc.yellow || "#c19c00"], [" \"🏷!\"", sc.foreground || "#cccccc"]] }
            }
        } }
    Component { id: decimalRow
        SettingCard {
            title: modelData.title; description: modelData.desc || ""
            FieldBox { boxWidth: 120; text: String(store.get(modelData.scope, modelData.key, modelData.def)).replace(".", ",")
                onEdited: function(t) {
                    const n = parseFloat(t.replace(",", "."))
                    if (isNaN(n) || !isFinite(n)) return
                    store.set(modelData.scope, modelData.key, Math.max(modelData.min, Math.min(modelData.max, n)))
                } }
        } }
    Component { id: sliderRow
        SettingCard {
            id: card
            readonly property real value: Number(store.get(modelData.scope, modelData.key, modelData.def))
            title: modelData.title; description: modelData.desc || ""
            Row {
                spacing: 12
                Item {
                    id: track; width: 190; height: 32
                    Rectangle { x: 0; anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 4; radius: 2; color: "#9a9a9a" }
                    Rectangle { x: 0; anchors.verticalCenter: parent.verticalCenter; width: (parent.width - 20) * (card.value - modelData.min) / (modelData.max - modelData.min) + 10; height: 4; radius: 2; color: Theme.accentLight }
                    Rectangle { x: (parent.width - 20) * (card.value - modelData.min) / (modelData.max - modelData.min); anchors.verticalCenter: parent.verticalCenter; width: 20; height: 20; radius: 10; color: "#454545"; border.width: 1; border.color: "#6a6a6a"
                        Rectangle { anchors.centerIn: parent; width: 12; height: 12; radius: 6; color: Theme.accentLight } }
                    MouseArea {
                        anchors.fill: parent
                        function setFrom(x) { store.set(modelData.scope, modelData.key, Math.round(modelData.min + Math.max(0, Math.min(1, (x - 10) / (track.width - 20))) * (modelData.max - modelData.min))) }
                        onPressed: function(m) { setFrom(m.x) }
                        onPositionChanged: function(m) { if (pressed) setFrom(m.x) }
                    }
                }
                Text { anchors.verticalCenter: parent.verticalCenter; width: 40; text: card.value + "%"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
            }
        } }
    Component { id: expanderRow
        ExpanderCard { title: modelData.title; description: modelData.desc || ""; valueText: modelData.value || "" } }
    Component { id: fontAxesRow
        ExpanderCard {
            id: axes
            readonly property var values: {
                store.rev
                const v = store.get(modelData.scope, modelData.key, ({}))
                return v && typeof v === "object" && !Array.isArray(v) ? v : ({})
            }
            readonly property var tags: Object.keys(values).sort()
            property string message: ""
            title: modelData.title; description: modelData.desc || ""
            valueText: tags.length ? String(tags.length) : qsTr("None")
            function validTag(tag) {
                if (tag.length !== 4) return false
                for (let i = 0; i < tag.length; ++i) {
                    const code = tag.charCodeAt(i)
                    if (code < 0x20 || code > 0x7e) return false
                }
                return true
            }
            function put(tag, text) {
                const n = Number(text)
                if (!validTag(tag)) { message = qsTr("Use a 4-character OpenType tag."); return }
                if (!isFinite(n)) { message = qsTr("Enter a numeric value."); return }
                const next = Object.assign({}, values); next[tag] = n
                store.set(modelData.scope, modelData.key, next); message = ""
            }
            function remove(tag) {
                const next = Object.assign({}, values); delete next[tag]
                store.set(modelData.scope, modelData.key, next); message = ""
            }
            Column {
                spacing: 8
                Repeater {
                    model: axes.tags
                    delegate: Row {
                        required property string modelData
                        spacing: 8
                        Text { width: 48; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                        FieldBox { boxWidth: 82; text: String(axes.values[modelData]); onEdited: function(t) { axes.put(modelData, t) } }
                        SmallButton { label: qsTr("Remove"); onClicked: axes.remove(modelData) }
                    }
                }
                Row {
                    spacing: 8
                    FieldBox { id: axisTag; boxWidth: 82; placeholder: qsTr("Tag") }
                    FieldBox { id: axisValue; boxWidth: 82; placeholder: qsTr("Value") }
                    SmallButton { label: qsTr("Add"); onClicked: axes.put(axisTag.typed.trim(), axisValue.typed) }
                }
                Text { visible: axes.message.length > 0; text: axes.message; color: Theme.danger; font.family: Theme.fontFamily; font.pixelSize: 12 }
            }
        } }
    Component { id: fontFeaturesRow
        ExpanderCard {
            id: features
            readonly property var values: {
                store.rev
                const v = store.get(modelData.scope, modelData.key, ({}))
                return v && typeof v === "object" && !Array.isArray(v) ? v : ({})
            }
            readonly property var tags: Object.keys(values).sort()
            property string message: ""
            title: modelData.title; description: modelData.desc || ""
            valueText: tags.length ? String(tags.length) : qsTr("None")
            function validTag(tag) {
                if (tag.length !== 4) return false
                for (let i = 0; i < tag.length; ++i) {
                    const code = tag.charCodeAt(i)
                    if (code < 0x20 || code > 0x7e) return false
                }
                return true
            }
            function put(tag, text) {
                const n = Number(text)
                if (!validTag(tag)) { message = qsTr("Use a 4-character OpenType tag."); return }
                if (!isFinite(n) || Math.floor(n) !== n || n < 0) { message = qsTr("Enter a numeric value."); return }
                const next = Object.assign({}, values); next[tag] = n
                store.set(modelData.scope, modelData.key, next); message = ""
            }
            function remove(tag) {
                const next = Object.assign({}, values); delete next[tag]
                store.set(modelData.scope, modelData.key, next); message = ""
            }
            Column {
                spacing: 8
                Repeater {
                    model: features.tags
                    delegate: Row {
                        required property string modelData
                        spacing: 8
                        Text { width: 48; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                        FieldBox { boxWidth: 82; text: String(features.values[modelData]); onEdited: function(t) { features.put(modelData, t) } }
                        SmallButton { label: qsTr("Remove"); onClicked: features.remove(modelData) }
                    }
                }
                Row {
                    spacing: 8
                    FieldBox { id: featureTag; boxWidth: 82; placeholder: qsTr("Tag") }
                    FieldBox { id: featureValue; boxWidth: 82; placeholder: qsTr("Value") }
                    SmallButton { label: qsTr("Add"); onClicked: features.put(featureTag.typed.trim(), featureValue.typed) }
                }
                Text { visible: features.message.length > 0; text: features.message; color: Theme.danger; font.family: Theme.fontFamily; font.pixelSize: 12 }
            }
        } }
    Component { id: paddingRow
        ExpanderCard {
            id: pad
            // "all", "horizontal, vertical" or "left, top, right, bottom" (as the original reads it).
            readonly property var v: {
                const parts = String(store.get(modelData.scope, "padding", "8, 8, 8, 8")).split(",").map(function(x) { return parseFloat(x) })
                const n = function(x) { return isNaN(x) ? 0 : x }
                if (parts.length === 1) return [n(parts[0]), n(parts[0]), n(parts[0]), n(parts[0])]
                if (parts.length === 2) return [n(parts[0]), n(parts[1]), n(parts[0]), n(parts[1])]
                return [n(parts[0]), n(parts[1]), n(parts[2]), n(parts[3])]
            }
            title: modelData.title; description: modelData.desc || ""; valueText: v.join(", ")
            function put(i, t) { const n = parseFloat(t); if (isNaN(n)) return; const w = v.slice(); w[i] = Math.max(0, n); store.set(modelData.scope, "padding", w.join(", ")) }
            Item {
                width: 222; height: 114
                Rectangle { anchors.fill: parent; radius: 4; color: "transparent"; border.width: 1; border.color: Theme.chipBorder }
                FieldBox { boxWidth: 64; x: 79; y: 8; numeric: true; text: String(pad.v[1]); onEdited: function(t) { pad.put(1, t) } }
                FieldBox { boxWidth: 64; x: 8; y: 41; numeric: true; text: String(pad.v[0]); onEdited: function(t) { pad.put(0, t) } }
                FieldBox { boxWidth: 64; x: 150; y: 41; numeric: true; text: String(pad.v[2]); onEdited: function(t) { pad.put(2, t) } }
                FieldBox { boxWidth: 64; x: 79; y: 74; numeric: true; text: String(pad.v[3]); onEdited: function(t) { pad.put(3, t) } }
            }
        } }
    Component { id: bellRow
        ExpanderCard {
            id: bell
            // bellStyle is "audible", "window", "taskbar", "all", "none", or a list of those.
            readonly property var set: {
                const raw = store.get(modelData.scope, "bellStyle", "audible")
                const list = typeof raw === "string" ? [raw] : raw
                if (list.indexOf("all") >= 0) return ["audible", "window", "taskbar"]
                return list.filter(function(x) { return x === "audible" || x === "window" || x === "taskbar" })
            }
            function flip(name, on) {
                const w = bell.set.filter(function(x) { return x !== name })
                if (on) w.push(name)
                store.set(modelData.scope, "bellStyle", w.length === 0 ? "none" : w.length === 3 ? "all" : w.length === 1 ? w[0] : w)
            }
            title: modelData.title; description: modelData.desc || ""
            valueText: set.length === 0 ? qsTr("None") : set.length === 3 ? qsTr("All")
                       : set.map(function(x) { return x === "audible" ? qsTr("Audible") : x === "window" ? qsTr("Flash window") : qsTr("Flash taskbar") }).join(", ")
            Column {
                SmallCheck { label: qsTr("Audible"); checked: bell.set.indexOf("audible") >= 0; onToggled: function(v) { bell.flip("audible", v) } }
                SmallCheck { label: qsTr("Flash window"); checked: bell.set.indexOf("window") >= 0; onToggled: function(v) { bell.flip("window", v) } }
                SmallCheck { label: qsTr("Flash taskbar"); checked: bell.set.indexOf("taskbar") >= 0; onToggled: function(v) { bell.flip("taskbar", v) } }
            }
        } }
    Component { id: cursorColorRow
        ExpanderCard {
            id: cc
            readonly property string value: { const c = store.get(modelData.scope, "cursorColor", ""); return c ? String(c).toUpperCase() : "" }
            readonly property var sc: { store.rev; return AppSettings.scheme(store.get(modelData.scope, "colorScheme", "Campbell")) }
            title: modelData.title; description: modelData.desc || ""
            valueText: value.length ? value : String(sc.cursorColor || "#FFFFFF").toUpperCase()
            swatch: value.length ? value : (sc.cursorColor || "#FFFFFF")
            Row {
                spacing: 8
                FieldBox { boxWidth: 120; text: cc.valueText; onEdited: function(t) { const c = Qt.color(t); if (c.valid) store.set(modelData.scope, "cursorColor", String(c).toUpperCase()) } }
                Rectangle {
                    width: useText.width + 28; height: 32; radius: 4; color: Theme.accentLight
                    Text { id: useText; anchors.centerIn: parent; text: qsTr("Use color scheme"); color: Theme.accentText; font.family: Theme.fontFamily; font.pixelSize: 14 }
                    MouseArea { anchors.fill: parent; onClicked: store.set(modelData.scope, "cursorColor", null) }
                }
            }
        } }
    Component { id: sectionRow
        Text { topPadding: modelData.first ? 14 : 22; bottomPadding: 4; text: modelData.text; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14; font.weight: Font.DemiBold } }
    Component { id: toggleRow
        SettingCard {
            readonly property bool on: { const v = store.get(modelData.scope, modelData.key, modelData.def === true); return modelData.invert ? !v : v }
            title: modelData.title; description: modelData.desc || ""
            Row {
                spacing: 12
                Text { anchors.verticalCenter: parent.verticalCenter; text: on ? qsTr("On") : qsTr("Off"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                ToggleSwitch { anchors.verticalCenter: parent.verticalCenter; checked: on
                    onToggled: function(v) { store.set(modelData.scope, modelData.key, modelData.invert ? !v : v) } }
            }
        } }
    Component { id: choiceRow
        SettingCard {
            readonly property var currentValue: store.get(modelData.scope, modelData.key, modelData.def)
            readonly property var choices: {
                const base = modelData.profile ? root.profileChoices() : (modelData.options || [])
                if (modelData.key !== "fontWeight" || (typeof currentValue !== "number" && currentValue !== "custom")) return base
                const result = base.slice()
                const number = typeof currentValue === "number" ? currentValue : 400
                result.unshift({ v: number, l: qsTr("Custom (%1)").arg(number) })
                return result
            }
            title: modelData.title; description: modelData.desc || ""
            Row {
                spacing: modelData.key === "fontWeight" ? 8 : 0
                ChoiceBox {
                    boxWidth: modelData.key === "fontWeight" ? 160 : 248
                    options: choices
                    value: modelData.profile ? (function() { const w = store.get("g", "defaultProfile", ""); return w.length ? w : ProfileManager.defaultProfileId })()
                                             : currentValue
                    onChosen: function(v) {
                        store.set(modelData.scope, modelData.key, modelData.key === "fontWeight" && v === "custom" ? 400 : v)
                    }
                }
                FieldBox {
                    visible: modelData.key === "fontWeight" && (typeof currentValue === "number" || currentValue === "custom")
                    boxWidth: 70; numeric: true
                    text: String(currentValue === "custom" ? 400 : currentValue)
                    onEdited: function(t) {
                        const n = parseInt(t)
                        if (!isNaN(n)) store.set(modelData.scope, "fontWeight", Math.max(100, Math.min(990, n)))
                    }
                }
            }
        } }
    Component { id: languageRow
        SettingCard {
            title: modelData.title
            description: modelData.desc || ""
            ChoiceBox {
                options: LanguageManager.availableLanguages
                value: LanguageManager.currentLanguage
                onChosen: function(v) {
                    LanguageManager.setLanguage(v)
                }
            }
        } }
    Component { id: backgroundImageRow
        SettingCard {
            title: modelData.title; description: modelData.desc || ""
            readonly property string currentPath: String(store.get(modelData.scope, "backgroundImage", ""))
            Row {
                spacing: 8
                FieldBox {
                    id: bgPathBox
                    boxWidth: 260
                    text: currentPath
                    placeholder: qsTr("Image path...")
                    onEdited: function(t) { store.set(modelData.scope, "backgroundImage", t.trim()) }
                }
                Rectangle {
                    width: browseText.width + 24; height: 32; radius: 4; color: Theme.accentLight
                    Text { id: browseText; anchors.centerIn: parent; text: qsTr("Browse..."); color: Theme.accentText; font.family: Theme.fontFamily; font.pixelSize: 13 }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            const chosen = Launcher.chooseFile(qsTr("Select Background Image"), "Images (*.png *.jpg *.jpeg *.bmp *.webp *.svg) | *.png *.jpg *.jpeg *.bmp *.webp *.svg")
                            if (chosen && chosen.length > 0) {
                                store.set(modelData.scope, "backgroundImage", chosen)
                            }
                        }
                    }
                }
                Rectangle {
                    visible: currentPath.length > 0
                    width: 32; height: 32; radius: 4; color: Theme.card; border.width: 1; border.color: Theme.cardBorder
                    Text { anchors.centerIn: parent; text: "✕"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 13 }
                    MouseArea { anchors.fill: parent; onClicked: store.set(modelData.scope, "backgroundImage", "") }
                }
            }
        } }
    Component { id: bellSoundRow
        SettingCard {
            title: modelData.title; description: modelData.desc || ""
            readonly property string currentSound: String(store.get(modelData.scope, "bellSound", ""))
            Row {
                spacing: 8
                FieldBox {
                    id: soundBox
                    boxWidth: 220
                    text: currentSound
                    placeholder: qsTr("Default system sound")
                    onEdited: function(t) { store.set(modelData.scope, "bellSound", t.trim()) }
                }
                Rectangle {
                    width: soundBrowseText.width + 24; height: 32; radius: 4; color: Theme.accentLight
                    Text { id: soundBrowseText; anchors.centerIn: parent; text: qsTr("Browse..."); color: Theme.accentText; font.family: Theme.fontFamily; font.pixelSize: 13 }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            const chosen = Launcher.chooseFile(qsTr("Select Bell Sound"), "Audio Files (*.oga *.wav *.ogg *.mp3 *.flac) | *.oga *.wav *.ogg *.mp3 *.flac")
                            if (chosen && chosen.length > 0) {
                                store.set(modelData.scope, "bellSound", chosen)
                            }
                        }
                    }
                }
                Rectangle {
                    width: soundTestText.width + 24; height: 32; radius: 4; color: Theme.card; border.width: 1; border.color: Theme.cardBorder
                    Text { id: soundTestText; anchors.centerIn: parent; text: qsTr("Test"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 13 }
                    MouseArea { anchors.fill: parent; onClicked: Launcher.playBell(currentSound) }
                }
                Rectangle {
                    visible: currentSound.length > 0
                    width: 32; height: 32; radius: 4; color: Theme.card; border.width: 1; border.color: Theme.cardBorder
                    Text { anchors.centerIn: parent; text: "✕"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 13 }
                    MouseArea { anchors.fill: parent; onClicked: store.set(modelData.scope, "bellSound", "") }
                }
            }
        } }
    Component { id: aboutCardRow
        SettingCard {
            width: parent ? parent.width : implicitWidth
            title: modelData.title
            description: modelData.desc || ""
            clickable: modelData.link !== undefined
            onClicked: if (modelData.link) Launcher.openUrl(modelData.link)

            Row {
                visible: modelData.tag !== undefined
                spacing: 8
                Rectangle {
                    width: verText.width + 20; height: 28; radius: 14; color: Theme.accentLight
                    Text { id: verText; anchors.centerIn: parent; text: modelData.tag || ""; color: Theme.accentText; font.family: Theme.fontFamily; font.pixelSize: 13; font.bold: true }
                }
            }
            Glyph {
                visible: modelData.link !== undefined
                name: "chevronDown"; rotation: -90; color: Theme.text
            }
        } }
    Component { id: fieldRow
        SettingCard {
            title: modelData.title; description: modelData.desc || ""
            FieldBox { boxWidth: modelData.width || 248
                text: String(store.get(modelData.scope, modelData.key, ""))
                onEdited: function(t) { store.set(modelData.scope, modelData.key, t) } }
        } }
    Component { id: numberRow
        SettingCard {
            title: modelData.title; description: modelData.desc || ""
            FieldBox { boxWidth: 120; numeric: true
                text: String(store.get(modelData.scope, modelData.key, modelData.def))
                onEdited: function(t) { const n = parseInt(t); if (!isNaN(n)) store.set(modelData.scope, modelData.key, Math.max(modelData.min, Math.min(modelData.max, n))) } }
        } }
    Component { id: sizeRow
        SettingCard {
            title: modelData.title; description: modelData.desc || ""
            Row {
                spacing: 8
                FieldBox { boxWidth: 70; numeric: true; text: String(store.get("g", "initialCols", 120)); onEdited: function(t) { const n = parseInt(t); if (n > 0) store.set("g", "initialCols", n) } }
                Text { anchors.verticalCenter: parent.verticalCenter; text: "×"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                FieldBox { boxWidth: 70; numeric: true; text: String(store.get("g", "initialRows", 30)); onEdited: function(t) { const n = parseInt(t); if (n > 0) store.set("g", "initialRows", n) } }
            }
        } }
    Component { id: linkRow
        SettingCard {
            title: modelData.title; clickable: true; implicitHeight: 62
            onClicked: root.open(modelData.page)
            Glyph { name: "chevronDown"; rotation: -90; color: Theme.text }
        } }
    Component { id: dangerRow
        SettingCard {
            title: modelData.title; description: modelData.desc || ""
            Rectangle {
                width: dangerText.width + 28; height: 32; radius: 4; color: Theme.danger
                Text { id: dangerText; anchors.centerIn: parent; text: modelData.button; color: "white"; font.family: Theme.fontFamily; font.pixelSize: 14 }
                MouseArea { anchors.fill: parent; onClicked: {
                    if (modelData.action === "reset") { AppSettings.resetToDefaults(); store.discard() }
                    else if (modelData.action === "clearCache") Launcher.clearCache()
                    else if (modelData.action === "delete") { ProfileManager.deleteProfile(modelData.guid); root.open("startup") }
                } }
            }
        } }

    Component { id: actionRow
        SettingCard {
            title: ""; implicitHeight: 62
            Rectangle {
                width: actionText.width + 28; height: 32; radius: 4; color: Theme.accentLight
                Text { id: actionText; anchors.centerIn: parent; text: modelData.button; color: Theme.accentText; font.family: Theme.fontFamily; font.pixelSize: 14 }
                MouseArea { anchors.fill: parent; onClicked: root.open("profile:" + ProfileManager.createProfile("")) }
            }
        } }
    Component { id: colorRow
        SettingCard {
            title: modelData.title
            Row {
                spacing: 10
                Rectangle { width: 32; height: 32; radius: 4; color: store.schemeValue(modelData.scheme, modelData.key, modelData.key === "selectionBackground" ? "#FFFFFF" : "#000000"); border.width: 1; border.color: Theme.cardBorder }
                FieldBox { boxWidth: 120; text: String(store.schemeValue(modelData.scheme, modelData.key, modelData.key === "selectionBackground" ? "#FFFFFF" : "#000000")).toUpperCase()
                           onEdited: function(t) { const c = Qt.color(t); if (c.valid) store.setScheme(modelData.scheme, modelData.key, String(c).toUpperCase()) } }
            }
        } }
    Component { id: schemeActionsRow
        Row {
            spacing: 8
            readonly property bool isDefault: store.get("d", "colorScheme", "Campbell") === modelData.name
            Rectangle {
                width: setText.width + 28; height: 32; radius: 4; color: isDefault ? Theme.card : Theme.accentLight
                border.width: isDefault ? 1 : 0; border.color: Theme.cardBorder
                Text { id: setText; anchors.centerIn: parent; text: isDefault ? qsTr("default") : qsTr("Set as default")
                       color: isDefault ? Theme.text : Theme.accentText; font.family: Theme.fontFamily; font.pixelSize: 14 }
                MouseArea { anchors.fill: parent; onClicked: store.set("d", "colorScheme", modelData.name) }
            }
            Rectangle {
                width: delText.width + 28; height: 32; radius: 4; color: Theme.danger
                Text { id: delText; anchors.centerIn: parent; text: qsTr("Delete color scheme"); color: "white"; font.family: Theme.fontFamily; font.pixelSize: 14 }
                MouseArea { anchors.fill: parent; onClicked: { AppSettings.applyChanges({ schemes: (function() { const o = {}; o[modelData.name] = null; return o })() }); root.open("colors") } }
            }
        } }
    property string duplicateSource: ""
    Component { id: duplicateRow
        SettingCard {
            title: modelData.title
            Row {
                spacing: 8
                ChoiceBox { options: root.profileChoices(); value: root.duplicateSource.length ? root.duplicateSource : ProfileManager.defaultProfileId
                            onChosen: function(v) { root.duplicateSource = v } }
                Rectangle {
                    width: dupText.width + 28; height: 32; radius: 4; color: Theme.accentLight
                    Text { id: dupText; anchors.centerIn: parent; text: qsTr("Duplicate"); color: Theme.accentText; font.family: Theme.fontFamily; font.pixelSize: 14 }
                    MouseArea { anchors.fill: parent; onClicked: root.open("profile:" + ProfileManager.createProfile(root.duplicateSource.length ? root.duplicateSource : ProfileManager.defaultProfileId)) }
                }
            }
        } }

    // ---- the footer ------------------------------------------------------------------------------------
    Item {
        id: footer
        x: root.navWidth
        width: parent.width - x
        height: 56
        anchors.bottom: parent.bottom
        Rectangle { width: parent.width; height: 1; color: Theme.footerLine }
        Row {
            anchors.right: parent.right; anchors.rightMargin: 33
            y: 13
            spacing: 9
            Rectangle {
                width: 61; height: 30; radius: 4
                // Light theme (measured): hover #1875C5, pressed #3284CC; the dark states are still to measure.
                color: Theme.dark ? Theme.accentLight : saveArea.pressed ? "#3284cc" : saveArea.containsMouse ? "#1875c5" : Theme.accentLight
                opacity: store.dirty ? 1 : 0.85
                Text { anchors.centerIn: parent; text: qsTr("Save"); color: Theme.accentText; font.family: Theme.fontFamily; font.pixelSize: 14 }
                MouseArea { id: saveArea; anchors.fill: parent; hoverEnabled: true; onClicked: store.save() }
            }
            Rectangle {
                width: 148; height: 30; radius: 4
                // Light theme (measured): #FDFDFD, #F9F9F9 under the pointer and pressed.
                color: Theme.dark ? Theme.navSelected : (discardArea.containsMouse || discardArea.pressed) ? "#f9f9f9" : "#fdfdfd"
                Text { anchors.centerIn: parent; text: qsTr("Discard changes"); color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: 14 }
                MouseArea { id: discardArea; anchors.fill: parent; hoverEnabled: true; onClicked: store.discard() }
            }
        }
    }
}
