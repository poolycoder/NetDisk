QT += widgets network
CONFIG += c++11 thread
msvc {
    QMAKE_CXXFLAGS += /utf-8
}
TEMPLATE = app
TARGET = netdisk_qt_client

SOURCES += main.cpp protocol.cpp networkclient.cpp transferworker.cpp mainwindow.cpp
HEADERS += protocol.h networkclient.h transferworker.h mainwindow.h
