cmake_minimum_required(VERSION 3.9.5)

project(pineapple-midi-player
    VERSION 1.1.0
    HOMEPAGE_URL "https://github.com/BLumia/pineapple-midi-player"
)

# This is optional. Arch Linux's portaudio package doesn't have CMake support (for now).
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

include(GNUInstallDirs)

option(CONAN2_STATIC_QT_BUG "Workaround conan 2 static Qt issue, see conan-io/conan-center-index#23045" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 "6.4" REQUIRED COMPONENTS Widgets LinguistTools)
find_package(PortAudio REQUIRED)

if(NOT TARGET PortAudio::PortAudio AND TARGET portaudio)
    # the current latest released tag (19.7.0) doesn't provide the PortAudio::portaudio target
    add_library(PortAudio::PortAudio ALIAS portaudio)
endif()

set(EXE_NAME pmidi)

add_executable(${EXE_NAME} WIN32)

qt_add_resources(${EXE_NAME} resources.qrc
    PREFIX "/icons"
    FILES ./assets/icons/app-icon.svg
)

target_sources(${EXE_NAME}
PRIVATE
    app/main.cpp
    app/player.h
    app/player.cpp
    app/mainwindow.cpp
    app/mainwindow.h
    app/mainwindow.ui
    app/seekableslider.cpp
    app/seekableslider.h
    app/settings.cpp
    app/settings.h

    # vendored libs
    libs/tsf.h
    libs/tml.h
    libs/opl.h
    libs/dr_wav.h
    libs/stb_vorbis.c
    libs/implements.c
)

target_include_directories(${EXE_NAME} PRIVATE app)

qt_add_translations(${EXE_NAME}
TS_FILES
    translations/pineapple-midi-player.ts
    translations/pineapple-midi-player.zh_CN.ts
)

target_compile_definitions(${EXE_NAME} PRIVATE
    PMIDI_VERSION_STRING="${CMAKE_PROJECT_VERSION}"
)

if (WIN32)
    target_sources(${EXE_NAME} PRIVATE assets/pineapple-midi-player.rc)

    # Extra build settings
    target_compile_definitions(${EXE_NAME} PRIVATE
        FLAG_PORTABLE_MODE_SUPPORT=1
    )
endif ()

target_include_directories(${EXE_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/libs)
target_link_libraries(${EXE_NAME} PRIVATE Qt6::Widgets PortAudio::PortAudio)

if (CONAN2_STATIC_QT_BUG)
    target_link_libraries(${EXE_NAME} PRIVATE Qt6::QWindowsIntegrationPlugin)
    target_compile_definitions(${EXE_NAME} PRIVATE CONAN2_STATIC_QT_BUG)
endif ()

# Install -------------------------------------

set(INSTALL_TARGETS_DEFAULT_ARGS
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Devel
)

install(
    TARGETS ${EXE_NAME}
    ${INSTALL_TARGETS_DEFAULT_ARGS}
)

if (UNIX)
    if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX /usr)
    endif ()

    # install icon
    install(
        FILES assets/icons/app-icon.svg
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
        RENAME net.blumia.pineapple-midi-player.svg
    )

    # install shortcut
    install(
        FILES dist/net.blumia.pineapple-midi-player.desktop
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
    )

    # install app metadata file for appstream (and some other stuff using this metadata like snapcraft)
    install(
        FILES dist/appstream/net.blumia.pineapple-midi-player.metainfo.xml
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo"
    )
endif ()

set(CPACK_GENERATOR "TBZ2")
set(CPACK_PACKAGE_NAME "pineapple-midi-player")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A simple SoundFont MIDI Player")
set(CPACK_PACKAGE_VENDOR "Gary Wang")
set(CPACK_PACKAGE_CONTACT "https://github.com/BLumia/pineapple-midi-player/issues/")
if(WIN32)
    # ...
elseif (APPLE)
    # ...
elseif (UNIX)
    set(CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "timgm6mb-soundfont")
endif()

include(CPack)
