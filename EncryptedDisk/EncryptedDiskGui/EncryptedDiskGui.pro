
QT += core gui widgets

TARGET = EncryptedDiskGui
TEMPLATE = app

INCLUDEPATH += \
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.10240.0/ucrt"
LIBS += \
    -L"C:/Program Files (x86)/Windows Kits/10/Lib/10.0.10240.0/ucrt/$$QMAKE_HOST.arch" \
    -lbcrypt

INCLUDEPATH += \
    ../EncryptedDiskClientLib

CONFIG(debug, debug|release) {
    CONFIGURATION = debug
} else {
    CONFIGURATION = release
}

LIBS += \
    -L../../Commons/lib -lCryptoUtils \
    -L../EncryptedDiskClientLib/$$CONFIGURATION -lEncryptedDiskClientLib

SOURCES += main.cpp\
    MainWindow.cpp \
    CreateNewDiskDialog.cpp \
    MountDiskDialog.cpp

HEADERS  += MainWindow.h \
    CreateNewDiskDialog.h \
    MountDiskDialog.h

FORMS    += MainWindow.ui \
    CreateNewDiskDialog.ui \
    MountDiskDialog.ui
