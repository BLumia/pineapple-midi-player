cmake_minimum_required(VERSION 3.9.5)

project(pineapple-midi-player)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 "6.6" REQUIRED COMPONENTS Widgets LinguistTools)
find_package(PkgConfig REQUIRED)

pkg_check_modules(PortAudio REQUIRED portaudiocpp IMPORTED_TARGET)

set(EXE_NAME pmidi)

add_executable(${EXE_NAME})

qt_add_resources(${EXE_NAME} resources.qrc
    PREFIX "/icons"
    FILES ./dist/app-icon.svg
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
    libs/stb_vorbis.c
)

target_include_directories(${EXE_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/libs)
target_link_libraries(${EXE_NAME} PRIVATE Qt6::Widgets PkgConfig::PortAudio)
