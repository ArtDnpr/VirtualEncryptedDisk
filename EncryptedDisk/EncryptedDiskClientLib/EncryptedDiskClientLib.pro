TEMPLATE = lib
CONFIG += staticlib

DEFINES -= UNICODE

INCLUDEPATH = \
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.10240.0/ucrt"

INCLUDEPATH += \
    ../../Commons/include

SOURCES += \
    Transport.cpp\
    VirtualDiskController.cpp \
    DriverCommunication.cpp

HEADERS  += \
    targetver.h \
    Transport.h \
    Disk.h \
    VirtualDiskController.h \
    IVirtualDiskControllerObserver.h \
    stdafx.h \
    DriverCommunication.h
