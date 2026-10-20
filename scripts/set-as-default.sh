#!/usr/bin/env bash
# set-as-default.sh
# Configures win11terminal as the default terminal emulator across Linux environments
# (Debian/Ubuntu/Mint alternatives, Cinnamon/GNOME/MATE desktop settings, and modern xdg-terminal-exec / GLib).

set -e

DESKTOP_ID="org.pedro.win11terminal.desktop"
BIN_PATH="$(which win11terminal 2>/dev/null || echo "/usr/bin/win11terminal")"

if [ ! -x "$BIN_PATH" ]; then
    echo "❌ win11terminal binary not found in PATH or /usr/bin/win11terminal."
    echo "   Please install win11terminal first."
    exit 1
fi

echo "=========================================================="
echo " Setting win11terminal as default terminal emulator"
echo "=========================================================="

# 1. Debian / Ubuntu / Linux Mint alternatives (x-terminal-emulator)
if command -v update-alternatives >/dev/null 2>&1; then
    echo "--> [1/4] Configuring Debian/Ubuntu update-alternatives..."
    if ! update-alternatives --list x-terminal-emulator 2>/dev/null | grep -q "$BIN_PATH"; then
        sudo update-alternatives --install /usr/bin/x-terminal-emulator x-terminal-emulator "$BIN_PATH" 50
    fi
    sudo update-alternatives --set x-terminal-emulator "$BIN_PATH"
    echo "    ✓ x-terminal-emulator points to $BIN_PATH"
fi

# 2. Desktop Environment GSettings (Cinnamon, GNOME, MATE)
if command -v gsettings >/dev/null 2>&1; then
    echo "--> [2/4] Configuring Desktop Environment preferences (gsettings)..."

    # Cinnamon
    if gsettings list-schemas | grep -q "org.cinnamon.desktop.default-applications.terminal"; then
        gsettings set org.cinnamon.desktop.default-applications.terminal exec 'win11terminal'
        gsettings set org.cinnamon.desktop.default-applications.terminal exec-arg '-e'
        echo "    ✓ Cinnamon default terminal set to win11terminal"
    fi

    # GNOME
    if gsettings list-schemas | grep -q "org.gnome.desktop.default-applications.terminal"; then
        gsettings set org.gnome.desktop.default-applications.terminal exec 'win11terminal'
        gsettings set org.gnome.desktop.default-applications.terminal exec-arg '-e'
        echo "    ✓ GNOME default terminal set to win11terminal"
    fi

    # MATE
    if gsettings list-schemas | grep -q "org.mate.applications-terminal"; then
        gsettings set org.mate.applications-terminal exec 'win11terminal'
        gsettings set org.mate.applications-terminal exec-arg '-e'
        echo "    ✓ MATE default terminal set to win11terminal"
    fi
fi

# 3. xdg-terminal-exec (Freedesktop modern specification & GLib/GIO)
echo "--> [3/4] Configuring modern xdg-terminal-exec specification..."
if ! command -v xdg-terminal-exec >/dev/null 2>&1; then
    echo "    ℹ xdg-terminal-exec is not installed. Installing it ensures full GLib/GIO and desktop compatibility..."
    if command -v apt-get >/dev/null 2>&1; then
        sudo apt-get install -y xdg-terminal-exec || true
    elif command -v dnf >/dev/null 2>&1; then
        sudo dnf install -y xdg-terminal-exec || true
    elif command -v pacman >/dev/null 2>&1; then
        sudo pacman -S --noconfirm xdg-terminal-exec || true
    fi
fi

# Locate desktop file
SRC_DESKTOP=""
for candidate in \
    "/usr/share/applications/$DESKTOP_ID" \
    "/usr/local/share/applications/$DESKTOP_ID" \
    "$HOME/.local/share/applications/$DESKTOP_ID"; do
    if [ -f "$candidate" ]; then
        SRC_DESKTOP="$candidate"
        break
    fi
done

# Setup xdg-terminals entry
XDG_TERMINALS_USER="$HOME/.local/share/xdg-terminals"
mkdir -p "$XDG_TERMINALS_USER"
if [ -n "$SRC_DESKTOP" ]; then
    cp -f "$SRC_DESKTOP" "$XDG_TERMINALS_USER/$DESKTOP_ID"
else
    cat <<EOF > "$XDG_TERMINALS_USER/$DESKTOP_ID"
[Desktop Entry]
Type=Application
Name=Terminal
Exec=win11terminal
Terminal=false
Categories=System;TerminalEmulator;
EOF
fi

# Ensure X-ExecArg=-e is declared so xdg-terminal-exec knows how to run commands
if ! grep -q "^X-ExecArg=" "$XDG_TERMINALS_USER/$DESKTOP_ID"; then
    echo "X-ExecArg=-e" >> "$XDG_TERMINALS_USER/$DESKTOP_ID"
fi

# Set as default for xdg-terminal-exec (supports legacy list & directory configs)
mkdir -p "$HOME/.config/xdg-terminals.list.d"
echo "$DESKTOP_ID" > "$HOME/.config/xdg-terminals.list"
echo "$DESKTOP_ID" > "$HOME/.config/xdg-terminals.list.d/default.list"
echo "    ✓ xdg-terminal-exec configured with $DESKTOP_ID (X-ExecArg=-e)"

# 4. System-wide xdg-terminals (if root / sudo available)
if [ -w /usr/share ]; then
    sudo mkdir -p /usr/share/xdg-terminals /etc/xdg/xdg-terminals.list.d 2>/dev/null || true
    if [ -d /usr/share/xdg-terminals ]; then
        sudo cp -f "$XDG_TERMINALS_USER/$DESKTOP_ID" /usr/share/xdg-terminals/ 2>/dev/null || true
        sudo bash -c "echo '$DESKTOP_ID' > /etc/xdg/xdg-terminals.list" 2>/dev/null || true
        sudo bash -c "echo '$DESKTOP_ID' > /etc/xdg/xdg-terminals.list.d/default.list" 2>/dev/null || true
    fi
fi

echo "=========================================================="
echo " ✓ win11terminal is now your default terminal emulator!"
echo "=========================================================="
