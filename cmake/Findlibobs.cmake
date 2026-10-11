# Fallback Findlibobs.cmake module
# 1. Versuche zuerst den offiziellen Config-Modus (libobsConfig.cmake)
find_package(libobs QUIET CONFIG)

if(TARGET OBS::libobs)
    set(libobs_FOUND TRUE)
    return()
endif()

# 2. Falls nicht im Config-Modus gefunden, suche manuell nach Headern und Bibliotheken
find_path(LIBOBS_INCLUDE_DIR
    NAMES obs.h obs-module.h
    HINTS
        ${CMAKE_CURRENT_SOURCE_DIR}/obs-sdk
        ${CMAKE_CURRENT_SOURCE_DIR}/../obs-sdk
        ${LIBOBS_DIR}
        ${OBS_STUDIO_DIR}
        $ENV{LIBOBS_DIR}
        $ENV{OBS_STUDIO_DIR}
        "C:/Program Files/obs-studio"
    PATH_SUFFIXES
        include
        libobs
        obs
        include/obs
        deps/libobs
)

find_library(LIBOBS_LIBRARY
    NAMES obs libobs
    HINTS
        ${CMAKE_CURRENT_SOURCE_DIR}/obs-sdk
        ${CMAKE_CURRENT_SOURCE_DIR}/../obs-sdk
        ${LIBOBS_DIR}
        ${OBS_STUDIO_DIR}
        $ENV{LIBOBS_DIR}
        $ENV{OBS_STUDIO_DIR}
        "C:/Program Files/obs-studio"
    PATH_SUFFIXES
        lib
        lib/x64
        bin
        bin/64bit
        build/libobs/Release
        build/libobs
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libobs
    DEFAULT_MSG
    LIBOBS_LIBRARY
    LIBOBS_INCLUDE_DIR
)

if(libobs_FOUND AND NOT TARGET OBS::libobs)
    add_library(OBS::libobs UNKNOWN IMPORTED)
    set_target_properties(OBS::libobs PROPERTIES
        IMPORTED_LOCATION "${LIBOBS_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBOBS_INCLUDE_DIR}"
    )
endif()
