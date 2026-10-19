cmake_minimum_required(VERSION 3.21)

project(rivet VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Concurrent
    Svg
)

qt_standard_project_setup()

set(RIVET_SOURCES
    src/main.cpp

    src/core/Paths.cpp
    src/core/IniFile.cpp
    src/core/Config.cpp
    src/core/Utils.cpp
    src/core/Http.cpp
    src/core/Steam.cpp
    src/core/ProtonManager.cpp
    src/core/FixParser.cpp
    src/core/IconExtractor.cpp
    src/core/RarExtractor.cpp
    src/core/DesktopEntry.cpp
    src/core/CommandBuilder.cpp
    src/core/Runner.cpp
    src/core/Installer.cpp

    src/ui/Theme.cpp
    src/ui/Icons.cpp
    src/ui/GameCard.cpp
    src/ui/FlowLayout.cpp
    src/ui/BannerWidget.cpp
    src/ui/MainWindow.cpp
    src/ui/GameDetailPage.cpp
    src/ui/AddGameWizard.cpp
    src/ui/GameSettingsDialog.cpp
    src/ui/LauncherSettingsDialog.cpp
    src/ui/EnvDialog.cpp
    src/ui/LogWindow.cpp
    src/ui/DownloadGamesDialog.cpp
    src/ui/RemoveGameDialog.cpp
    src/ui/Toast.cpp
)

qt_add_executable(rivet ${RIVET_SOURCES})

qt_add_resources(rivet "rivet_assets"
    PREFIX "/"
    FILES
        assets/Newtonsoft.Json.dll
        assets/ftpPath32.dll
        assets/ftpPath64.dll
        assets/rivet.svg
)

target_include_directories(rivet PRIVATE src src/core src/ui)

target_link_libraries(rivet PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    Qt6::Svg
)

target_compile_options(rivet PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wno-unused-parameter>
)

install(TARGETS rivet RUNTIME DESTINATION bin)
configure_file(packaging/rivet.desktop.in "${CMAKE_CURRENT_BINARY_DIR}/rivet.desktop" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/rivet.desktop" DESTINATION share/applications)
install(FILES assets/rivet.svg DESTINATION share/icons/hicolor/scalable/apps RENAME rivet.svg)
install(FILES assets/rivet-256.png DESTINATION share/icons/hicolor/256x256/apps RENAME rivet.png)

include(CTest)

option(RIVET_BUILD_TESTS "Build the headless core tests" OFF)
if (RIVET_BUILD_TESTS)
    add_executable(rivet_addgame_test
        tests/addgame_smoke.cpp
        src/core/Paths.cpp
        src/core/IniFile.cpp
        src/core/Config.cpp
        src/core/Utils.cpp
        src/core/Http.cpp
        src/core/Steam.cpp
        src/core/ProtonManager.cpp
        src/core/FixParser.cpp
        src/core/IconExtractor.cpp
        src/core/RarExtractor.cpp
        src/core/DesktopEntry.cpp
        src/core/Installer.cpp
        src/core/CommandBuilder.cpp
    )
    target_include_directories(rivet_addgame_test PRIVATE src src/core)
    target_link_libraries(rivet_addgame_test PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
    )
    add_test(NAME addgame_pipeline COMMAND rivet_addgame_test)
endif()
