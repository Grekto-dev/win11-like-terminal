cmake_minimum_required(VERSION 3.21)
project(win11terminal VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# Only the core and its tests (no QML/GUI): for a quick loop on the emulation.
option(WIN11TERMINAL_CORE_ONLY "Build only the terminal core and its tests" OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 ShaderTools LinguistTools Svg Test)
qt_standard_project_setup(REQUIRES 6.4 I18N_SOURCE_LANGUAGE en I18N_TRANSLATED_LANGUAGES pt_BR)
qt_policy(SET QTP0004 NEW)

# --- libvterm 0.3.3 (MIT, Paul Evans), vendored with patches ------------------------
# third_party/libvterm is the upstream source plus optimizations (see PATCHES.md there):
# faint (SGR 2) and overline (SGR 53) as cell attributes, and the primary screen as a
# ring buffer, so that a scrolled line no longer copies the whole screen.
add_library(vterm STATIC
    third_party/libvterm/src/encoding.c
    third_party/libvterm/src/keyboard.c
    third_party/libvterm/src/mouse.c
    third_party/libvterm/src/parser.c
    third_party/libvterm/src/pen.c
    third_party/libvterm/src/screen.c
    third_party/libvterm/src/state.c
    third_party/libvterm/src/unicode.c
    third_party/libvterm/src/vterm.c
)
target_include_directories(vterm PUBLIC third_party/libvterm/include PRIVATE third_party/libvterm/src)
set_target_properties(vterm PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
target_compile_options(vterm PRIVATE -Wno-unused-parameter -O2)

# --- The core: no windows, testable without a screen ---------------------------
qt_add_library(terminalcore STATIC
    src/pty/ptyprocess.h src/pty/ptyprocess.cpp
    src/vt/cell.h
    src/vt/historyline.h src/vt/historyline.cpp
    src/vt/vtcore.h src/vt/vtcore.cpp
)
target_include_directories(terminalcore PUBLIC src)
target_link_libraries(terminalcore PUBLIC Qt6::Core Qt6::Gui vterm util)

if(NOT WIN11TERMINAL_CORE_ONLY)
  # --- The app --------------------------------------------------------------------
  qt_add_executable(win11terminal
      src/main.cpp
  )

  set_source_files_properties(qml/Theme.qml qml/Actions.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
  qt_add_qml_module(win11terminal
      URI Win11Terminal
      VERSION 1.0
      QML_FILES
          qml/Main.qml
          qml/Theme.qml
          qml/Actions.qml
          qml/CommandPalette.qml
          qml/AboutDialog.qml
          qml/ConfirmDialog.qml
          qml/MenuPanel.qml
          qml/ContextMenu.qml
          qml/TabColorFlyout.qml
          qml/SettingsStore.qml
          qml/SettingsPage.qml
          qml/SettingCard.qml
          qml/NewTabMenuPage.qml
          qml/ToggleSwitch.qml
          qml/ChoiceBox.qml
          qml/FieldBox.qml
          qml/Glyph.qml
          qml/CaptionButton.qml
          qml/TabStrip.qml
          qml/ProfileIcon.qml
          qml/ProfileFlyout.qml
          qml/TabPage.qml
          qml/PaneLeaf.qml
          qml/PaneSplit.qml
          qml/SearchBox.qml
          qml/TerminalScrollBar.qml
      SOURCES
          src/app/appsettings.h src/app/appsettings.cpp
          src/app/newtabmenu.h src/app/newtabmenu.cpp
          src/app/profilemanager.h src/app/profilemanager.cpp
          src/app/keyrouter.h src/app/keyrouter.cpp
          src/app/launcher.h src/app/launcher.cpp
          src/app/windowprops.h src/app/windowprops.cpp
          src/view/colorscheme.h src/view/colorscheme.cpp
          src/view/glyphatlas.h src/view/glyphatlas.cpp
          src/view/builtinglyphs.h src/view/builtinglyphs.cpp
          src/view/glyphmaterial.h src/view/glyphmaterial.cpp
          src/view/terminalsession.h src/view/terminalsession.cpp
          src/view/terminalview.h src/view/terminalview.cpp
          src/view/fluentsymbol.h src/view/fluentsymbol.cpp
          src/view/systemfont.h src/view/systemfont.cpp
          src/app/languagemanager.h src/app/languagemanager.cpp
  )

  qt_add_shaders(win11terminal "shaders"
      PREFIX "/shaders"
      BATCHABLE
      BASE shaders
      FILES
          shaders/glyph.vert
          shaders/glyph.frag
  )

  target_include_directories(win11terminal PRIVATE src src/view src/app)
  target_link_libraries(win11terminal PRIVATE
      Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Svg
      terminalcore
      xcb
  )
  target_compile_definitions(win11terminal PRIVATE
      WIN11TERMINAL_SOURCE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
  )

  qt_add_resources(win11terminal "assets" PREFIX "/" FILES data/icons/org.pedro.win11terminal.svg)
  qt_add_resources(win11terminal "fluent_icons" data/icons/fluent.qrc)
  qt_add_translations(TARGETS win11terminal
      TS_FILES translations/win11terminal_pt_BR.ts
      RESOURCE_PREFIX /i18n
      LUPDATE_OPTIONS -no-obsolete
  )

  install(TARGETS win11terminal RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES data/defaults.json DESTINATION ${CMAKE_INSTALL_DATADIR}/win11terminal)
  install(FILES data/icons/org.pedro.win11terminal.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
  configure_file(data/org.pedro.win11terminal.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/org.pedro.win11terminal.desktop @ONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.pedro.win11terminal.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES data/fonts/CascadiaMono.ttf data/fonts/CascadiaMonoItalic.ttf data/fonts/OFL.txt
      DESTINATION ${CMAKE_INSTALL_DATADIR}/win11terminal/fonts)
  install(DIRECTORY data/icons/fluent/ DESTINATION ${CMAKE_INSTALL_DATADIR}/win11terminal/icons/fluent)
endif()

# --- Tests ---------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
