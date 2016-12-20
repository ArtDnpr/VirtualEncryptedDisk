#pragma once

#include "Disk.h"

class IVirtualDiskControllerObserver;

class VirtualDiskController
{
public:
    VirtualDiskController();
    void createNewDisk(const std::wstring &fullDiskPath, const uint64_t diskSize, const std::string &password,
                       IVirtualDiskControllerObserver* observer, const std::atomic<bool>& canceled);
    const Disk& mountDisk(const std::wstring &fullDiskPath, char diskLetter, const std::string &password);
    void unmountDisk(const char diskLetter);
    void unmountAllDisks();
	
    const std::vector<Disk>& getDisks();

private:
    std::vector<Disk> m_disks;
};

