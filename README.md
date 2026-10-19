# 🪟 Win11-Like Terminal for Linux

A modern, high-fidelity Windows 11-style Terminal emulator for Linux Mint and other Linux distributions, built with Qt 6 and QML on top of a fast, custom-tuned terminal emulation core.

![Platform](https://img.shields.io/badge/Platform-Linux%20Mint%20%7C%20Ubuntu%20%7C%20Debian%20%7C%20Arch%20%7C%20Fedora-blue)
![Version](https://img.shields.io/badge/Version-0.9.0-green)
![Qt](https://img.shields.io/badge/Qt-6.4+-brightgreen)
![License](https://img.shields.io/badge/License-GPL--3.0%20%2F%20MIT-blue)

---

## ✨ Features

- **📑 Windows 11 Tabbed Interface**: Integrated tab bar in the title area with fluent animations, tab coloring flyouts, tab renaming, drag-and-drop reordering, and duplicate/split actions.
- **🪟 Split Panes**: Divide any tab horizontally or vertically (`Alt+Shift+Plus` / `Alt+Shift+Minus` / `Alt+Shift+D`). Resize interactively and navigate between panes with `Alt+Arrow Keys`.
- **🖼️ Custom Background Images**: Set custom wallpaper images for any terminal profile with customizable opacity (slider) and stretch modes (`Uniform to fill`, `Uniform`, `Fill`, `None`) plus native file picker dialog.
- **📺 Retro Terminal CRT Effects**: Authentic CRT monitor scanlines, screen glass curvature vignette, phosphor glow, and subtle ambient pulse.
- **🔔 Customizable Bell Sound**: Pick custom audio files (`.oga`, `.wav`, `.ogg`, `.mp3`, `.flac`) or use system defaults with built-in instant preview sound test.
- **🎨 Color Schemes & Visual Styles**: Bundled schemes (Campbell, Campbell PowerShell, Vintage, One Half Dark, One Half Light, Solarized, and Tango) plus an interactive **Color Scheme Editor** built into the Settings page.
- **⚙️ Integrated Settings Tab**: Complete settings experience mirroring Windows Terminal:
  - Startup behaviors, default profile, and initial window size.
  - Interaction settings (copy on select, mouse wheel scrolling, paste warnings).
  - Appearance (font family, font size, cell spacing, line height, background opacity, and acrylic blur).
  - Dedicated **About** page reporting version `0.9.0`, system environment, and repository links.
  - Profiles manager (auto-detects Bash, Zsh, Fish, PowerShell, and custom shells).
  - New tab menu customizer and command palette actions.
- **🔍 Search & Command Palette**: Instant buffer search (`Ctrl+Shift+F`) with match highlighting and a fuzzy command palette (`Ctrl+Shift+P`) for quick actions.
- **⚡ High-Performance Emulation**:
  - Accelerated glyph atlas rendering with Cascadia Mono.
  - Custom patched `libvterm` ring buffer for zero-lag high-throughput scrolling.
  - Full support for 24-bit TrueColor, 256 colors, bracketed paste, mouse reporting (SGR 1006), and Unicode emoji.
- **🧩 100% Autonomous & Self-Contained**:
  - No external private framework dependencies.
  - Bundled Fluent UI system icons (embedded as scalable SVGs).
  - Bundled Cascadia Mono font family.
- **🌐 Multi-Language & Live Switcher**: In-app language selector in Settings > Appearance that switches the entire interface immediately on the fly (English default, with Brazilian Portuguese `pt-BR`) and persists your preference across sessions.

---

## 🚀 Installation & Build

### 1. Install Build Dependencies

#### Linux Mint / Ubuntu / Debian:
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    qt6-base-dev \
    qt6-declarative-dev \
    qt6-shadertools-dev \
    qt6-tools-dev \
    qt6-l10n-tools \
    libqt6svg6-dev \
    libvterm-dev \
    libgl1-mesa-dev \
    libxcb1-dev \
    libxkbcommon-dev \
    qml6-module-qtquick \
    qml6-module-qtquick-window \
    qml6-module-qtquick-controls \
    qml6-module-qtquick-templates \
    qml6-module-qtquick-shapes \
    qml6-module-qtquick-layouts \
    qml6-module-qtqml-workerscript \
    zenity
```

#### Fedora:
```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    ninja-build \
    qt6-qtbase-devel \
    qt6-qtdeclarative-devel \
    qt6-qtshadertools-devel \
    qt6-qtsvg-devel \
    qt6-linguist \
    libxcb-devel
```

#### Arch Linux:
```bash
sudo pacman -S --needed \
    base-devel \
    cmake \
    ninja \
    qt6-base \
    qt6-declarative \
    qt6-shadertools \
    qt6-svg \
    qt6-tools \
    libxcb
```

### Quick Install (Linux Mint / Ubuntu / Debian)

Run the included automated install script:
```bash
git clone https://github.com/Grekto-dev/win11-like-terminal.git
cd win11-like-terminal
./scripts/install-local.sh
```
This will automatically install build dependencies, build the release binary, install to `/usr/bin/win11terminal`, and register the desktop icon.

---

### Manual Build from Source

```bash
# Clone the repository
git clone https://github.com/Grekto-dev/win11-like-terminal.git
cd win11-like-terminal

# Configure with CMake and Ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build build
```

---

### 3. Run or Install

#### Run directly from build folder:
```bash
./build/win11terminal
```

#### Install to user directory (`~/.local`):
```bash
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$HOME/.local

cmake --build build
cmake --install build

# Update desktop and icon databases
update-desktop-database ~/.local/share/applications/ 2>/dev/null || true
gtk-update-icon-cache ~/.local/share/icons/hicolor/ 2>/dev/null || true
```

Once installed, **Terminal** will appear in your application menu (under System / Terminal Emulators) and can be launched by running `win11terminal`.

---

## ⌨️ Useful Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+Shift+T` | Open new tab (default profile) |
| `Ctrl+Shift+W` | Close current pane or tab |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Switch to next / previous tab |
| `Alt+Shift+D` | Split pane automatically |
| `Alt+Shift+Plus (+)` | Split pane horizontally |
| `Alt+Shift+Minus (-)` | Split pane vertically |
| `Alt+Left/Right/Up/Down` | Move focus between split panes |
| `Ctrl+Shift+P` | Open Command Palette |
| `Ctrl+Shift+F` | Open Search bar |
| `Ctrl+Shift+,` | Open Settings tab |
| `Ctrl+Shift+1...9` | Open profile by index |
| `Ctrl++` / `Ctrl+-` / `Ctrl+0` | Zoom in / Zoom out / Reset font size |
| `Ctrl+Shift+C` | Copy selected text |
| `Ctrl+Shift+V` | Paste from clipboard |

---

## ⚙️ Configuration

Win11-Like Terminal follows standard Windows Terminal configuration semantics:
- Configuration file location: `~/.config/win11terminal/settings.json`
- Supports live hot-reload upon saving changes in `settings.json`.
- Settings can also be adjusted through the built-in graphical Settings interface (`Ctrl+Shift+,`).

---

## 📄 License

- Application code is licensed under the **GNU General Public License v3.0 (GPL-3.0)** or compatible licenses.
- Embedded `libvterm` is licensed under the **MIT License** (by Paul Evans).
- Bundled Fluent UI System Icons are licensed under the **MIT License** (by Microsoft Corporation).
- Bundled Cascadia Mono font is licensed under the **SIL Open Font License (OFL)**.
