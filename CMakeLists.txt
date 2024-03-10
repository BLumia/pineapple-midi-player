cmake_minimum_required(VERSION 3.9.5)

project(pineapple-midi-player)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 "6.6" REQUIRED COMPONENTS Widgets LinguistTools)
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

    # vendored libs
    libs/tsf.h
    libs/tml.h
    libs/dr_wav.h
    libs/stb_vorbis.c
    libs/implements.c
)

if (WIN32)
    target_sources(${EXE_NAME} PRIVATE assets/pineapple-midi-player.rc)
endif ()

target_include_directories(${EXE_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/libs)
target_link_libraries(${EXE_NAME} PRIVATE Qt6::Widgets PortAudio::PortAudio)
