TEMPLATE = lib
CONFIG += staticlib

INCLUDEPATH = \
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.10240.0/ucrt"

SOURCES += \
    CryptoUtils.cpp

HEADERS  += \
    targetver.h \
    CryptoUtils.h \
    stdafx.h
