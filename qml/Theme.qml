pragma Singleton
import QtQuick
import Win11Terminal

// Colours and metrics of Windows Terminal 1.24 (dark theme), measured on the VM:
// see NOTES.md, sections 2, 3 and 4. Values without a comment come from there.
QtObject {
    readonly property string fontFamily: SystemFont.family
    // The application theme (settings.json "theme"): "light", or dark for "dark" and "system".
    readonly property bool dark: AppSettings.globals.theme !== "light"
    // What a hover or an inactive tab mixes into the colour underneath.
    readonly property color mixTowards: "white"

    // Window chrome
    readonly property color barActive: dark ? "#2e2e2e" : "#e8e8e8"
    readonly property color barInactive: dark ? "#333333" : "#ffffff"
    readonly property color barLine: dark ? "#292929" : "#dadada"
    readonly property color windowBorder: dark ? Qt.rgba(1, 1, 1, 0.19) : Qt.rgba(0, 0, 0, 0.30)
    readonly property color text: dark ? "#ffffff" : "#1a1a1a"
    readonly property color textDisabled: dark ? "#5d5d5d" : "#a0a0a0"
    readonly property color closeHover: "#c42b1c"
    readonly property color closePressed: "#b22b1f"
    readonly property color captionHover: dark ? "#3a3a3a" : "#e0e0e0"
    readonly property color captionPressed: dark ? "#313131" : "#d0d0d0"
    // A hover is the colour underneath with a little white over it.
    readonly property color hoverOverlay: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.06)
    readonly property color pressedOverlay: dark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.04)
    readonly property real tabHoverMix: dark ? 0.0535 : 0.362   // inactive tab under the pointer: terminal background + 5.35% white (dark theme) or 36.2% white (light: #0C0C0C gives #646464, measured)
    readonly property color splitDivider: dark ? "#3d3d3d" : "#c4c4c4"
    readonly property color tabDivider: dark ? "#3f3f3f" : "#c4c4c4"
    readonly property color splitButtonHover: dark ? "#3b3b3b" : "#d8d8d8"
    readonly property color splitButtonPressed: dark ? "#484848" : "#c7c7c7"
    readonly property color accent: "#0078d4"
    readonly property color accentLight: dark ? "#4cc2ff" : "#0067c0"  // buttons, focus marks (dark theme)
    readonly property color accentText: dark ? "#000000" : "#ffffff"

    // Flyouts and dialogs
    readonly property color flyout: dark ? "#2c2c2c" : "#f9f9f9"
    readonly property color flyoutBorder: dark ? "#0a0a0a" : "#dadada"
    readonly property color flyoutHover: dark ? "#383838" : "#f0f0f0"
    readonly property color flyoutSeparator: dark ? "#3d3d3d" : "#e5e5e5"
    readonly property color textSecondary: dark ? "#c5c5c5" : "#585858"
    // The settings page
    readonly property color settingsBackground: dark ? "#282828" : "#f9f9f9"
    readonly property color card: dark ? "#333333" : "#fdfdfd"
    readonly property color cardHover: dark ? "#383838" : "#f6f6f6"
    readonly property color cardBorder: dark ? "#2f2f2f" : "#eaeaea"
    readonly property color choice: dark ? "#3f3f3f" : "#fefefe"
    readonly property color choiceHover: dark ? "#454545" : "#fbfbfb"
    readonly property color navSelected: dark ? "#353535" : "#f0f0f0"
    readonly property color navHover: dark ? "#303030" : "#f0f0f0"
    readonly property color footerLine: dark ? "#1f1f1f" : "#e5e5e5"
    readonly property color danger: "#c42b1c"
    readonly property color textPlaceholder: dark ? "#9d9d9d" : "#6b6b6b"
    readonly property color chipBorder: dark ? "#5a5a5a" : "#c4c4c4"
    readonly property color fieldBackground: dark ? "#222222" : "#ffffff"
    readonly property color dialogBody: dark ? "#2b2b2b" : "#f3f3f3"
    readonly property color dialogFooter: dark ? "#202020" : "#e5e5e5"
    readonly property color dialogButton: dark ? "#2d2d2d" : "#fbfbfb"

    // Metrics (pixels at 100%)
    readonly property int barHeight: 40
    readonly property int tabTop: 8
    readonly property int tabHeight: 32
    readonly property int tabRadius: 8
    readonly property int tabMaxWidth: 240
    readonly property int tabMinWidth: 100
    readonly property int barMarginLeft: 8
    readonly property int captionWidth: 46
    readonly property int splitButtonWidth: 31
    readonly property int splitButtonHeight: 24
    readonly property int dragReserve: 48        // room left for grabbing the window when the tabs fill the bar
    readonly property int windowRadius: 8

    // Relative luminance (0 black .. 1 white), to pick dark or light text on a coloured tab.
    function luminance(c) {
        const col = Qt.color(c)
        const f = function(v) { return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4) }
        return 0.2126 * f(col.r) + 0.7152 * f(col.g) + 0.0722 * f(col.b)
    }
    function mix(base, over, amount) {
        return Qt.rgba(base.r + (over.r - base.r) * amount,
                       base.g + (over.g - base.g) * amount,
                       base.b + (over.b - base.b) * amount, 1)
    }
}
