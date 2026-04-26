QT          += core gui widgets network websockets
CONFIG      += c++11
HEADERS     +=  qterminalgrabber.h \
    mainwindow.h
SOURCES     += main.cpp \
    mainwindow.cpp


linux:!android {
    SOURCES += qterminalgrabber_linux.cpp
}

FORMS += \
    mainwindow.ui

DISTFILES += \
    index.html
