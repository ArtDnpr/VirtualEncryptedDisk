TEMPLATE = subdirs

SUBDIRS += \
    EncryptedDiskGui \
    EncryptedDiskClientLib

EncryptedDiskGui.file = EncryptedDiskGui/EncryptedDiskGui.pro
EncryptedDiskClientLib.file = EncryptedDiskClientLib/EncryptedDiskClientLib.pro

EncryptedDiskClientLib.depend = CryptoUtils
EncryptedDiskGui.depend = EncryptedDiskClientLib
