cmake_minimum_required(VERSION 3.16)
project(obs-plugin-manager VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Automatische Codegenerierung für Qt (MOC, RCC, UIC)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# -----------------------------------------------------------------------------
# Externe Abhängigkeiten: Qt 6
# -----------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# -----------------------------------------------------------------------------
# Externe Abhängigkeiten: OBS Studio (libobs und obs-frontend-api)
# -----------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)

# -----------------------------------------------------------------------------
# Quellcodedateien
# -----------------------------------------------------------------------------
set(PLUGIN_SOURCES
    src/plugin-main.cpp
    src/core/github_client.hpp
    src/core/github_client.cpp
    src/core/installer.hpp
    src/core/installer.cpp
    src/ui/manager_dialog.hpp
    src/ui/manager_dialog.cpp
    3rdparty/miniz.h
    3rdparty/miniz.c
    resources.qrc
)

# Erstelle OBS-Plugin als Shared/Module-Bibliothek
add_library(obs-plugin-manager MODULE ${PLUGIN_SOURCES})

# Include-Verzeichnisse
target_include_directories(obs-plugin-manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
)

# Compiler-Definitionen
target_compile_definitions(obs-plugin-manager PRIVATE
    _CRT_SECURE_NO_WARNINGS
)

# Bibliotheken verlinken
target_link_libraries(obs-plugin-manager PRIVATE
    OBS::libobs
    OBS::obs-frontend-api
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# Plattformspezifische Einstellungen
if(WIN32)
    target_link_libraries(obs-plugin-manager PRIVATE ws2_32)
endif()

# Namenskonvention für OBS-Plugins (ohne "lib"-Präfix unter Unix/macOS)
set_target_properties(obs-plugin-manager PROPERTIES
    PREFIX ""
    OUTPUT_NAME "obs-plugin-manager"
)

# -----------------------------------------------------------------------------
# Installations-Regeln
# -----------------------------------------------------------------------------
if(WIN32)
    install(TARGETS obs-plugin-manager
        RUNTIME DESTINATION "obs-plugins/64bit"
        LIBRARY DESTINATION "obs-plugins/64bit"
    )
    install(DIRECTORY src/data/ DESTINATION "data/obs-plugins/obs-plugin-manager"
        FILES_MATCHING PATTERN "*.json"
    )
elseif(APPLE)
    install(TARGETS obs-plugin-manager
        LIBRARY DESTINATION "obs-plugins"
    )
    install(DIRECTORY src/data/ DESTINATION "data/obs-plugins/obs-plugin-manager"
        FILES_MATCHING PATTERN "*.json"
    )
else()
    include(GNUInstallDirs)
    install(TARGETS obs-plugin-manager
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins"
    )
    install(DIRECTORY src/data/ DESTINATION "${CMAKE_INSTALL_DATADIR}/obs/obs-plugins/obs-plugin-manager"
        FILES_MATCHING PATTERN "*.json"
    )
endif()
