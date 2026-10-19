pragma Singleton
import QtQuick

// The actions of Windows Terminal that a key or the command palette can run. The
// ids are the original's (Terminal.OpenNewTab...), so settings.json keybindings
// carry over. `english` is what the palette shows under the translated name.
QtObject {
    id: root

    property var window   // the Main window, which owns tabs, panes and dialogs

    readonly property var catalog: [
        { id: "Terminal.OpenAbout", name: qsTr("Open about dialog"), english: "Open about dialog" },
        { id: "Terminal.OpenNewTabDropdown", name: qsTr("Open new tab dropdown"), english: "Open new tab dropdown" },
        { id: "Terminal.OpenSettingsUI", name: qsTr("Open settings"), english: "Open settings" },
        { id: "Terminal.OpenSettingsFile", name: qsTr("Open settings file (JSON)"), english: "Open settings file (JSON)" },
        { id: "Terminal.OpenDefaultSettingsFile", name: qsTr("Open default settings file (JSON)"), english: "Open default settings file (JSON)" },
        { id: "Terminal.OpenSettingsDirectory", name: qsTr("Open the settings file directory"), english: "Open the settings file directory" },
        { id: "Terminal.OpenCwd", name: qsTr("Open current working directory"), english: "Open current working directory" },
        { id: "Terminal.ToggleCommandPalette", name: qsTr("Toggle command palette"), english: "Toggle command palette" },
        { id: "Terminal.OpenNewTab", name: qsTr("New tab"), english: "New tab" },
        { id: "Terminal.OpenNewWindow", name: qsTr("New window"), english: "New window" },
        { id: "Terminal.MoveTabToNewWindow", name: qsTr("Move tab to a new window"), english: "Move tab to a new window" },
        { id: "Terminal.DuplicateTab", name: qsTr("Duplicate tab"), english: "Duplicate tab" },
        { id: "Terminal.CloseTab", name: qsTr("Close tab"), english: "Close tab" },
        { id: "Terminal.CloseWindow", name: qsTr("Close window"), english: "Close window" },
        { id: "Terminal.NextTab", name: qsTr("Next tab"), english: "Next tab" },
        { id: "Terminal.PrevTab", name: qsTr("Previous tab"), english: "Previous tab" },
        { id: "Terminal.SwitchToLastTab", name: qsTr("Switch to the last tab"), english: "Switch to the last tab" },
        { id: "Terminal.DuplicatePaneRight", name: qsTr("Split pane right"), english: "Split pane right" },
        { id: "Terminal.DuplicatePaneDown", name: qsTr("Split pane down"), english: "Split pane down" },
        { id: "Terminal.DuplicatePaneAuto", name: qsTr("Duplicate pane"), english: "Duplicate pane" },
        { id: "Terminal.ClosePane", name: qsTr("Close pane"), english: "Close pane" },
        { id: "Terminal.MoveFocusLeft", name: qsTr("Move focus left"), english: "Move focus left" },
        { id: "Terminal.MoveFocusRight", name: qsTr("Move focus right"), english: "Move focus right" },
        { id: "Terminal.MoveFocusUp", name: qsTr("Move focus up"), english: "Move focus up" },
        { id: "Terminal.MoveFocusDown", name: qsTr("Move focus down"), english: "Move focus down" },
        { id: "Terminal.MoveFocusPrevious", name: qsTr("Move focus to the last used pane"), english: "Move focus to the last used pane" },
        { id: "Terminal.ResizePaneLeft", name: qsTr("Resize pane left"), english: "Resize pane left" },
        { id: "Terminal.ResizePaneRight", name: qsTr("Resize pane right"), english: "Resize pane right" },
        { id: "Terminal.ResizePaneUp", name: qsTr("Resize pane up"), english: "Resize pane up" },
        { id: "Terminal.ResizePaneDown", name: qsTr("Resize pane down"), english: "Resize pane down" },
        { id: "Terminal.CopyToClipboard", name: qsTr("Copy text"), english: "Copy text" },
        { id: "Terminal.PasteFromClipboard", name: qsTr("Paste"), english: "Paste" },
        { id: "Terminal.SelectAll", name: qsTr("Select all text"), english: "Select all text" },
        { id: "Terminal.ToggleMarkMode", name: qsTr("Toggle mark mode"), english: "Toggle mark mode" },
        { id: "Terminal.FindText", name: qsTr("Find"), english: "Find" },
        { id: "Terminal.ShowContextMenu", name: qsTr("Show context menu"), english: "Show context menu" },
        { id: "Terminal.ScrollUp", name: qsTr("Scroll up"), english: "Scroll up" },
        { id: "Terminal.ScrollDown", name: qsTr("Scroll down"), english: "Scroll down" },
        { id: "Terminal.ScrollUpPage", name: qsTr("Scroll up one page"), english: "Scroll up one page" },
        { id: "Terminal.ScrollDownPage", name: qsTr("Scroll down one page"), english: "Scroll down one page" },
        { id: "Terminal.ScrollToTop", name: qsTr("Scroll to the top of history"), english: "Scroll to the top of history" },
        { id: "Terminal.ScrollToBottom", name: qsTr("Scroll to the bottom of history"), english: "Scroll to the bottom of history" },
        { id: "Terminal.ClearBuffer", name: qsTr("Clear buffer"), english: "Clear buffer" },
        { id: "Terminal.IncreaseFontSize", name: qsTr("Increase font size"), english: "Increase font size" },
        { id: "Terminal.DecreaseFontSize", name: qsTr("Decrease font size"), english: "Decrease font size" },
        { id: "Terminal.ResetFontSize", name: qsTr("Reset font size"), english: "Reset font size" },
        { id: "Terminal.ToggleFullscreen", name: qsTr("Toggle fullscreen"), english: "Toggle fullscreen" },
        { id: "Terminal.RestartConnection", name: qsTr("Restart connection"), english: "Restart connection" }
    ]

    function entry(id) {
        for (const e of catalog) if (e.id === id) return e
        return null
    }

    // The action ids that work whichever item has the focus (the search box, the palette).
    readonly property var global: ["Terminal.OpenNewTab", "Terminal.OpenNewWindow", "Terminal.NextTab", "Terminal.PrevTab",
        "Terminal.CloseTab", "Terminal.CloseWindow", "Terminal.ToggleCommandPalette", "Terminal.OpenSettingsUI",
        "Terminal.ToggleFullscreen", "Terminal.OpenNewTabDropdown", "Terminal.OpenAbout"]

    function view() {
        const p = window ? window.currentPage : null
        return p && p.activeLeaf ? p.activeLeaf.view : null
    }

    /** Runs an action; false when it did not apply (so the key goes on to the terminal). */
    function run(id, args, inTerminal) {
        const w = window
        if (!w || !w.currentPage) return false
        if (inTerminal === false && global.indexOf(id) < 0) return false
        const page = w.currentPage
        const v = view()
        const paneIds = ["Terminal.DuplicatePaneRight", "Terminal.DuplicatePaneDown", "Terminal.DuplicatePaneAuto", "Terminal.ClosePane",
            "Terminal.MoveFocusLeft", "Terminal.MoveFocusRight", "Terminal.MoveFocusUp", "Terminal.MoveFocusDown", "Terminal.MoveFocusPrevious",
            "Terminal.ResizePaneLeft", "Terminal.ResizePaneRight", "Terminal.ResizePaneUp", "Terminal.ResizePaneDown"]
        if (paneIds.indexOf(id) >= 0 && !page.activeLeaf) return false
        let m
        if ((m = /^Terminal\.SwitchToTab(\d+)$/.exec(id))) { w.selectTab(parseInt(m[1])); return true }
        if ((m = /^Terminal\.OpenNewTabProfile(\d+)$/.exec(id))) {
            const list = ProfileManager.profiles.filter(function(p) { return !p.hidden })
            const i = parseInt(m[1])
            if (i < list.length) w.newTab(list[i].guid)
            return true
        }
        switch (id) {
        case "Terminal.OpenNewTab": w.newTab(args && args.profile ? args.profile : ""); return true
        case "Terminal.OpenNewWindow": w.openNewWindow(page.activeLeaf ? page.activeLeaf.session.currentDirectory() : ""); return true
        case "Terminal.DuplicateTab": w.duplicateTab(w.currentIndex); return true
        case "Terminal.MoveTabToNewWindow": w.moveTabToNewWindow(w.currentIndex); return true
        case "Terminal.CloseTab": w.closeTab(w.currentIndex); return true
        case "Terminal.CloseWindow": w.close(); return true
        case "Terminal.NextTab": w.selectTab((w.currentIndex + 1) % w.tabCount); return true
        case "Terminal.PrevTab": w.selectTab((w.currentIndex + w.tabCount - 1) % w.tabCount); return true
        case "Terminal.SwitchToLastTab": w.selectTab(w.tabCount - 1); return true
        case "Terminal.DuplicatePaneRight": page.splitActive("right"); return true
        case "Terminal.DuplicatePaneDown": page.splitActive("down"); return true
        case "Terminal.DuplicatePaneAuto": page.splitActive("auto"); return true
        case "Terminal.ClosePane": page.closeActive(); return true
        case "Terminal.MoveFocusLeft": page.moveFocus("left"); return true
        case "Terminal.MoveFocusRight": page.moveFocus("right"); return true
        case "Terminal.MoveFocusUp": page.moveFocus("up"); return true
        case "Terminal.MoveFocusDown": page.moveFocus("down"); return true
        case "Terminal.MoveFocusPrevious": page.moveFocusRelative(-1); return true
        case "Terminal.ResizePaneLeft": page.resizeActive("left", 0.05); return true
        case "Terminal.ResizePaneRight": page.resizeActive("right", 0.05); return true
        case "Terminal.ResizePaneUp": page.resizeActive("up", 0.05); return true
        case "Terminal.ResizePaneDown": page.resizeActive("down", 0.05); return true
        case "Terminal.ToggleCommandPalette": w.togglePalette(); return true
        case "Terminal.OpenAbout": w.openAbout(); return true
        case "Terminal.OpenNewTabDropdown": w.openDropdown(); return true
        case "Terminal.OpenSettingsUI": w.openSettings(); return true
        case "Terminal.OpenSettingsFile": Launcher.openFile(AppSettings.filePath, true); return true
        case "Terminal.OpenDefaultSettingsFile": Launcher.openDefaultsFile(); return true
        case "Terminal.OpenSettingsDirectory": Launcher.openFolder(AppSettings.filePath.replace(/\/[^\/]*$/, "")); return true
        case "Terminal.OpenCwd": Launcher.openFolder(page.activeLeaf ? page.activeLeaf.session.currentDirectory() : ""); return true
        case "Terminal.ToggleFullscreen": w.toggleFullscreen(); return true
        }
        if (!v) return false
        switch (id) {
        case "Terminal.CopyToClipboard": return v.copy(args && args.keepSelection === true)   // false: nothing selected
        case "Terminal.PasteFromClipboard": v.paste(); return true
        case "Terminal.SelectAll": v.selectAll(); return true
        case "Terminal.ToggleMarkMode": v.toggleMarkMode(); return true
        case "Terminal.FindText": page.activeLeaf.openSearch(); return true
        case "Terminal.ShowContextMenu": w.openContextMenu(page.activeLeaf, v.contextAnchor(), true); return true
        case "Terminal.ScrollUp": v.scrollLines(args && args.rowsToScroll ? args.rowsToScroll : 1); return true
        case "Terminal.ScrollDown": v.scrollLines(-(args && args.rowsToScroll ? args.rowsToScroll : 1)); return true
        case "Terminal.ScrollUpPage": v.scrollPages(1); return true
        case "Terminal.ScrollDownPage": v.scrollPages(-1); return true
        case "Terminal.ScrollToTop": v.scrollToTop(); return true
        case "Terminal.ScrollToBottom": v.scrollToBottom(); return true
        case "Terminal.ClearBuffer": v.clearBuffer(); return true
        case "Terminal.IncreaseFontSize": v.fontSize = Math.min(128, v.fontSize + 1); return true
        case "Terminal.DecreaseFontSize": v.fontSize = Math.max(1, v.fontSize - 1); return true
        case "Terminal.ResetFontSize": v.fontSize = page.activeLeaf.profile.fontSize; return true
        case "Terminal.RestartConnection": page.activeLeaf.session.restart(); return true
        }
        return false
    }
}
