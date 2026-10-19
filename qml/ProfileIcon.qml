import QtQuick
import Win11Terminal

// The icon of a profile: bash, sh, dash or a console window.
Item {
    id: root
    property string profileId
    property color color: Theme.dark ? "#dcdcdc" : "#303030"
    implicitWidth: 16
    implicitHeight: 16

    readonly property var profile: ProfileManager.profile(profileId)
    readonly property string iconName: {
        if (!profile || !profile.name) return "window_console"
        const n = profile.name.toLowerCase()
        if (n.indexOf("bash") >= 0) return "bash"
        if (n === "sh") return "sh"
        if (n.indexOf("dash") >= 0) return "dash"
        if (profile.icon && profile.icon.length > 0 && profile.icon !== "terminal") return profile.icon
        return "window_console"
    }

    FluentSymbol {
        anchors.centerIn: parent
        name: root.iconName
        size: Math.min(root.width, root.height)
        variant: "regular"
        color: root.color
    }
}
