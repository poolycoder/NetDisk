cmake_minimum_required(VERSION 3.16)
project(NetDiskQtClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 QUIET COMPONENTS Widgets Network)
if (Qt6_FOUND)
    set(QT_PACKAGE Qt6)
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets Network)
    set(QT_PACKAGE Qt5)
endif()

add_executable(netdisk_qt_client
    main.cpp protocol.h protocol.cpp
    networkclient.h networkclient.cpp
    transferworker.h transferworker.cpp
    mainwindow.h mainwindow.cpp
)

target_link_libraries(netdisk_qt_client PRIVATE
    ${QT_PACKAGE}::Widgets
    ${QT_PACKAGE}::Network
)

if (MSVC)
    target_compile_options(netdisk_qt_client PRIVATE /utf-8)
endif()

if (WIN32)
    target_compile_definitions(netdisk_qt_client PRIVATE NOMINMAX)
endif()
