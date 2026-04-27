QT          += core gui widgets network websockets
CONFIG      += c++11
HEADERS     +=  qterminalgrabber.h mainwindow.h
SOURCES     += main.cpp  mainwindow.cpp
qtHaveModule(httpserver) : QT += httpserver


windows {
    INCLUDEPATH     += $${PWD}/winpty/include
    QMAKE_LIBDIR    += $${PWD}/winpty/x64/lib
    LIBS            += -lwinpty
    SOURCES         += qterminalgrabber_windows.cpp
}

linux:!android {
    SOURCES += qterminalgrabber_linux.cpp
}

osx {
    SOURCES += qterminalgrabber_osx.cpp
}


FORMS       += mainwindow.ui
DISTFILES   += index.html

RESOURCES += \
    res.qrc
