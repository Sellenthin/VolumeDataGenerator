#-------------------------------------------------
#
# Project created by QtCreator 2017-06-22T11:34:55
#
#-------------------------------------------------

QMAKE_CXXFLAGS += -std=c++17

LIBS += -lGL -lOpenCL

QMAKE_CXXFLAGS += -fopenmp
QMAKE_LFLAGS +=  -fopenmp

QT       += core gui concurrent opengl

greaterThan(QT_MAJOR_VERSION, 5): QT += widgets

TARGET = RaycastLight
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
    src/generateSource/datagenerator.cpp \
    src/generateSource/frequency.cpp \
    src/generateSource/getSetData.cpp \
    src/generateSource/Perlin.cpp \
    src/renderSource/colorutils.cpp \
    src/renderSource/colorwheel.cpp \
    src/renderSource/datrawreader.cpp \
    src/renderSource/hoverpoints.cpp \
    src/renderSource/main.cpp \
    src/renderSource/mainwindow.cpp \
    src/renderSource/openclglutilities.cpp \
    src/renderSource/openclutilities.cpp \
    src/renderSource/transferfunctionwidget.cpp \
    src/renderSource/volumerendercl.cpp \
    src/renderSource/volumerenderwidget.cpp \


HEADERS += \
    header/generateHeader/datagenerator.hpp \
    header/generateHeader/exprtk.hpp \
    header/generateHeader/frequency.hpp \
    header/generateHeader/getSetData.hpp \
    header/generateHeader/json.hpp \
    header/generateHeader/Perlin.hpp \
    header/generateHeader/rapidxml.hpp \
    header/generateHeader/vec3.hpp \
    header/renderHeader/colorutils.h \
    header/renderHeader/colorwheel.h \
    header/renderHeader/datrawreader.h \
    header/renderHeader/hoverpoints.h \
    header/renderHeader/mainwindow.h \
    header/renderHeader/openclglutilities.h \
    header/renderHeader/openclutilities.h \
    header/renderHeader/transferfunctionwidget.h \
    header/renderHeader/volumerendercl.h \
    header/renderHeader/volumerenderwidget.h \


FORMS += \
        mainwindow.ui

#include($$PWD/inc/painting/painting.pri)

RESOURCES += \
    kernels.qrc

DISTFILES += \
    kernels/volumeraycast.cl \
    style.qss

#enable console
CONFIG += console
