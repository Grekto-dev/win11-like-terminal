qt_add_executable(tst_vtcore tst_vtcore.cpp)
target_link_libraries(tst_vtcore PRIVATE Qt6::Test terminalcore)
add_test(NAME vtcore COMMAND tst_vtcore)

qt_add_executable(tst_pty tst_pty.cpp)
target_link_libraries(tst_pty PRIVATE Qt6::Test terminalcore)
add_test(NAME pty COMMAND tst_pty)

qt_add_executable(bench_vtcore bench_vtcore.cpp)
target_link_libraries(bench_vtcore PRIVATE Qt6::Test terminalcore)

qt_add_executable(tst_builtinglyphs tst_builtinglyphs.cpp ../src/view/builtinglyphs.cpp ../src/view/builtinglyphs.h)
target_include_directories(tst_builtinglyphs PRIVATE ../src/view)
target_link_libraries(tst_builtinglyphs PRIVATE Qt6::Test Qt6::Gui)
add_test(NAME builtinglyphs COMMAND tst_builtinglyphs)

qt_add_executable(tst_glyphatlas tst_glyphatlas.cpp ../src/view/glyphatlas.cpp ../src/view/glyphatlas.h
    ../src/view/builtinglyphs.cpp ../src/view/builtinglyphs.h)
target_include_directories(tst_glyphatlas PRIVATE ../src/view)
target_link_libraries(tst_glyphatlas PRIVATE Qt6::Test Qt6::Gui)
add_test(NAME glyphatlas COMMAND tst_glyphatlas)

qt_add_executable(tst_colorscheme tst_colorscheme.cpp ../src/view/colorscheme.cpp ../src/view/colorscheme.h ../src/vt/cell.h)
target_include_directories(tst_colorscheme PRIVATE ../src ../src/view)
target_link_libraries(tst_colorscheme PRIVATE Qt6::Test Qt6::Gui)
add_test(NAME colorscheme COMMAND tst_colorscheme)

if(NOT WIN11TERMINAL_CORE_ONLY)
    qt_add_executable(tst_settings tst_settings.cpp ../src/app/appsettings.cpp ../src/app/appsettings.h ../src/app/newtabmenu.cpp ../src/app/newtabmenu.h)
    target_include_directories(tst_settings PRIVATE ../src/app)
    target_link_libraries(tst_settings PRIVATE Qt6::Test Qt6::Qml)
    target_compile_definitions(tst_settings PRIVATE WIN11TERMINAL_TESTING WIN11TERMINAL_SOURCE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data")
    add_test(NAME settings COMMAND tst_settings)
endif()
