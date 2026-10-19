import QtQuick
import Win11Terminal

// One tab's content: a tree of panes. A leaf is a terminal (PaneLeaf); a split
// (PaneSplit) holds two nodes. The tree is built and cut with plain objects, so
// a split or a close never restarts the programs in the panes it does not touch.
Item {
    id: root

    property string profileId
    property string workingDirectory
    property string program            // overrides the profile's command line (first pane only)
    property var programArguments: []
    property bool current: false
    property bool windowActive: true
    property var host: null           // the window the page is in (it changes when the tab moves)
    property Item activeLeaf
    property Item rootNode
    readonly property string title: activeLeaf ? activeLeaf.title : ""
    readonly property real opacityValue: activeLeaf ? activeLeaf.opacityValue : 1
    readonly property bool acrylic: activeLeaf ? activeLeaf.acrylic : false
    readonly property color background: activeLeaf ? activeLeaf.background : "#0c0c0c"
    signal closeRequested             // the last pane is gone: the tab goes
    signal bell(var style)

    visible: current

    // ------------------------------------------------------------------ tree
    Item { id: host; anchors.fill: parent }
    Component { id: leafComponent; PaneLeaf {} }
    Component { id: splitComponent; PaneSplit {} }

    function isLeaf(node) { return node && node.focusTerminal !== undefined }

    function leaves(node, out) {
        out = out || []
        if (!node) return out
        if (isLeaf(node)) out.push(node)
        else { leaves(node.first, out); leaves(node.second, out) }
        return out
    }
    function firstLeaf(node) { return isLeaf(node) ? node : firstLeaf(node.first) }

    function createLeaf(profileId, cwd, program, programArguments) {
        const leaf = leafComponent.createObject(null, {
            profileId: profileId, workingDirectory: cwd,
            program: program || "", programArguments: programArguments || [],
            windowActive: root.windowActive })
        leaf.activated.connect(function() { setActive(leaf) })
        leaf.finished.connect(function(code, close) { if (close) closeLeaf(leaf) })
        leaf.closeRequested.connect(function() { closeLeaf(leaf) })
        leaf.bell.connect(function() { root.bell(leaf.profile.bellStyle) })
        return leaf
    }

    function setActive(leaf) {
        if (activeLeaf === leaf) return
        activeLeaf = leaf
        for (const l of leaves(rootNode)) l.focusedPane = (l === leaf)
    }
    function focusTerminal() { if (activeLeaf) activeLeaf.focusTerminal() }

    function mount(node) {
        rootNode = node
        node.parent = host
        node.sides = 0
        node.anchors.fill = host
    }

    // dir: "right" | "left" | "down" | "up" | "auto"
    function splitActive(dir, profileId) {
        const leaf = activeLeaf
        if (!leaf) return
        if (dir === "auto") dir = leaf.width >= leaf.height ? "right" : "down"
        const horizontal = dir === "right" || dir === "left"
        const nl = createLeaf(profileId && profileId.length > 0 ? profileId : leaf.profileId,
                              leaf.session.currentDirectory() || leaf.workingDirectory,
                              profileId && profileId.length > 0 ? "" : leaf.program, leaf.programArguments)
        const s = splitComponent.createObject(host, { orientation: horizontal ? Qt.Horizontal : Qt.Vertical })
        const par = leaf.parent
        s.sides = leaf.sides
        if (par === host) {
            leaf.anchors.fill = undefined
            mount(s)
        } else {
            par.replaceChild(leaf, s)
        }
        if (dir === "left" || dir === "up") { s.setFirst(nl); s.setSecond(leaf) }
        else { s.setFirst(leaf); s.setSecond(nl) }
        setActive(nl)
        Qt.callLater(function() { nl.focusTerminal() })
    }

    // A pane that goes: its program hears the hangup, and the pane (its terminal, history and
    // pty) is freed now, not whenever the JavaScript collector gets to it.
    function dispose(leaf) {
        leaf.session.hangUp()
        leaf.parent = null
        leaf.destroy()
    }
    // The tab closes: every pane goes with it.
    function shutdown() {
        const all = leaves(rootNode)
        rootNode = null
        activeLeaf = null
        for (const l of all) dispose(l)
    }

    function closeLeaf(leaf) {
        if (!leaf) return
        const par = leaf.parent
        if (par === host) { closeRequested(); return }
        const sibling = par.first === leaf ? par.second : par.first
        const grand = par.parent
        const wasActive = leaf === activeLeaf
        if (grand === host) {
            mount(sibling)
        } else {
            grand.replaceChild(par, sibling)
        }
        par.destroy()
        dispose(leaf)
        if (wasActive || !activeLeaf || activeLeaf === leaf) {
            activeLeaf = null
            setActive(firstLeaf(sibling))
            Qt.callLater(function() { focusTerminal() })
        }
    }

    function closeActive() { closeLeaf(activeLeaf) }

    // ------------------------------------------------------- focus and sizes
    function rectOf(leaf) {
        const p = leaf.mapToItem(host, 0, 0)
        return { l: p.x, t: p.y, r: p.x + leaf.width, b: p.y + leaf.height, leaf: leaf }
    }
    function overlap(a0, a1, b0, b1) { return Math.max(0, Math.min(a1, b1) - Math.max(a0, b0)) }

    // dir: "left" | "right" | "up" | "down": the pane next to the active one that way.
    function moveFocus(dir) {
        if (!activeLeaf) return
        const me = rectOf(activeLeaf)
        let best = null, bestScore = -1
        for (const l of leaves(rootNode)) {
            if (l === activeLeaf) continue
            const r = rectOf(l)
            let gap, ov
            if (dir === "left")       { gap = me.l - r.r; ov = overlap(me.t, me.b, r.t, r.b) }
            else if (dir === "right") { gap = r.l - me.r; ov = overlap(me.t, me.b, r.t, r.b) }
            else if (dir === "up")    { gap = me.t - r.b; ov = overlap(me.l, me.r, r.l, r.r) }
            else                      { gap = r.t - me.b; ov = overlap(me.l, me.r, r.l, r.r) }
            if (gap < -1 || ov <= 0) continue
            const score = 1000000 - gap * 1000 + ov
            if (score > bestScore) { bestScore = score; best = l }
        }
        if (best) { setActive(best); best.focusTerminal() }
    }
    function moveFocusRelative(step) {
        const all = leaves(rootNode)
        const i = all.indexOf(activeLeaf)
        if (i < 0) return
        const next = all[(i + step + all.length) % all.length]
        setActive(next); next.focusTerminal()
    }

    // Moves the border of the active pane in `dir` by `amount` of the split it belongs to.
    function resizeActive(dir, amount) {
        const horizontalMove = dir === "left" || dir === "right"
        let node = activeLeaf
        let fallback = null
        while (node && node.parent && node.parent !== host) {
            const s = node.parent
            if ((s.orientation === Qt.Horizontal) === horizontalMove) {
                const inFirst = s.first === node
                // the border on the requested side of the pane wins; else the far one
                const near = (dir === "right" || dir === "down") ? inFirst : !inFirst
                if (near) { s.ratio = Math.max(0.05, Math.min(0.95, s.ratio + ((dir === "right" || dir === "down") ? amount : -amount))); return }
                if (!fallback) fallback = s
            }
            node = s
        }
        if (fallback) fallback.ratio = Math.max(0.05, Math.min(0.95, fallback.ratio + ((dir === "right" || dir === "down") ? amount : -amount)))
    }

    onWindowActiveChanged: { for (const l of leaves(rootNode)) l.windowActive = windowActive }
    onCurrentChanged: if (current) Qt.callLater(focusTerminal)

    Component.onCompleted: {
        const leaf = createLeaf(profileId, workingDirectory, program, programArguments)
        mount(leaf)
        setActive(leaf)
    }
}
