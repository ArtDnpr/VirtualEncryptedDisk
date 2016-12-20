#pragma once

#include "Disk.h"

enum MessageCode
{
    MessageMountDisk,
    MessageUnmountDisk,
    MessageDiskInfo
};

struct MountMessage
{
    Disk diskInfo;
    size_t diskSectorSize;
    std::string password;
    size_t diskOffset;
};

struct UnmountMessage
{
    char diskLetter;
};

struct DiskInfoMessage
{
    std::vector<Disk> disks;
};

bool SendData(MessageCode messageCode, const void* data, void* receivedData = nullptr);
