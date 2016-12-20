#pragma once

class IVirtualDiskControllerObserver
{
public:
    virtual ~IVirtualDiskControllerObserver() {}
    virtual void virtualDiskControllerDidEncryptDisk(uint64_t encryptedSize, uint64_t totalSize) = 0;
    virtual void virtualDiskControllerDidFinishCreation(bool canceled) = 0;
    virtual void virtualDiskControllerDidFailDiskCreation(const std::string& error) = 0;
};

