QT       += core gui \

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    widget.cpp

HEADERS += \
    widget.h

FORMS += \
    widget.ui

RESOURCES += \
    src.qrc

RC_ICON = lion8.ico

include(Utils/Utils.pri)

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# add ffmpeg lib

win32: LIBS += -L$$PWD/3rdparty/ffmpeg/lib/ -lavcodec \
                -L$$PWD/3rdparty/ffmpeg/lib/ -lavdevice \
                -L$$PWD/3rdparty/ffmpeg/lib/ -lavfilter \
                -L$$PWD/3rdparty/ffmpeg/lib/ -lavformat \
                -L$$PWD/3rdparty/ffmpeg/lib/ -lavutil \
                -L$$PWD/3rdparty/ffmpeg/lib/ -lpostproc \
                -L$$PWD/3rdparty/ffmpeg/lib/ -lswresample \
                -L$$PWD/3rdparty/ffmpeg/lib/ -lswscale

INCLUDEPATH += $$PWD/3rdparty/ffmpeg/include
DEPENDPATH += $$PWD/3rdparty/ffmpeg/include

# add sdl lib

win32: LIBS += -L$$PWD/3rdparty/SDL2/lib/x64/ -lSDL2

INCLUDEPATH += $$PWD/3rdparty/SDL2/include
DEPENDPATH += $$PWD/3rdparty/SDL2/include



## add qslog lib
win32: LIBS += -L$$PWD/3rdparty/QsLog/bin/ -lQsLog2

INCLUDEPATH += $$PWD/3rdparty/QsLog/include
DEPENDPATH += $$PWD/3rdparty/QsLog/include

## add eigen
INCLUDEPATH += $$PWD/3rdparty/eigen-3.4.0
