# Fallback Findobs-frontend-api.cmake module
# 1. Versuche zuerst den offiziellen Config-Modus (obs-frontend-apiConfig.cmake)
find_package(obs-frontend-api QUIET CONFIG)

if(TARGET OBS::obs-frontend-api)
    set(obs-frontend-api_FOUND TRUE)
    return()
endif()

# 2. Falls nicht im Config-Modus gefunden, suche manuell nach Headern und Bibliotheken
find_path(OBS_FRONTEND_API_INCLUDE_DIR
    NAMES obs-frontend-api.h
    HINTS
        ${CMAKE_CURRENT_SOURCE_DIR}/obs-sdk
        ${CMAKE_CURRENT_SOURCE_DIR}/../obs-sdk
        ${OBS_FRONTEND_API_DIR}
        ${OBS_STUDIO_DIR}
        $ENV{OBS_FRONTEND_API_DIR}
        $ENV{OBS_STUDIO_DIR}
        "C:/Program Files/obs-studio"
    PATH_SUFFIXES
        include
        UI/obs-frontend-api
        obs-frontend-api
        include/obs-frontend-api
)

find_library(OBS_FRONTEND_API_LIBRARY
    NAMES obs-frontend-api
    HINTS
        ${CMAKE_CURRENT_SOURCE_DIR}/obs-sdk
        ${CMAKE_CURRENT_SOURCE_DIR}/../obs-sdk
        ${OBS_FRONTEND_API_DIR}
        ${OBS_STUDIO_DIR}
        $ENV{OBS_FRONTEND_API_DIR}
        $ENV{OBS_STUDIO_DIR}
        "C:/Program Files/obs-studio"
    PATH_SUFFIXES
        lib
        lib/x64
        bin
        bin/64bit
        build/UI/obs-frontend-api/Release
        build/UI/obs-frontend-api
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(obs-frontend-api
    DEFAULT_MSG
    OBS_FRONTEND_API_LIBRARY
    OBS_FRONTEND_API_INCLUDE_DIR
)

if(obs-frontend-api_FOUND AND NOT TARGET OBS::obs-frontend-api)
    add_library(OBS::obs-frontend-api UNKNOWN IMPORTED)
    set_target_properties(OBS::obs-frontend-api PROPERTIES
        IMPORTED_LOCATION "${OBS_FRONTEND_API_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OBS_FRONTEND_API_INCLUDE_DIR}"
    )
endif()
