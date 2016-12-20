#include "stdafx.h"
#include "Transport.h"
#include "DriverCommunication.h"

namespace
{
    std::string ConvertWstringToString(const std::wstring& wStr)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;

        return converter.to_bytes(wStr);
    }

    std::wstring ConvertStringToWString(const std::string& str)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;

        return converter.from_bytes(str);
    }

    const std::string cAdditionalSymbolsForDriverMount = "\\??\\";

    OPEN_FILE_INFORMATION ConvertMountMessageToOpenStruct(const MountMessage* mountMessage)
    {
        OPEN_FILE_INFORMATION openInfo;
        memset(&openInfo, 0 , sizeof(openInfo));

        openInfo.FileSize.QuadPart = mountMessage->diskInfo.size;
        openInfo.SectorSize.QuadPart = mountMessage->diskSectorSize;
        //Putting disk info in the beginning of the disk file and sending its offset to driver is implemented in client side.
        //But driver doesn not support disks that begins at non-zero offset.
        openInfo.DiskOffset = 0; // static_cast<USHORT>( mountMessage->diskOffset);
        memcpy(&openInfo.Password, mountMessage->password.c_str(), static_cast<USHORT>(mountMessage->password.length()));
        openInfo.PasswordLength = static_cast<USHORT>(mountMessage->password.length());
        openInfo.DriveLetter = mountMessage->diskInfo.letter;
        const std::string& diskPath = cAdditionalSymbolsForDriverMount + ConvertWstringToString(mountMessage->diskInfo.fullDiskPath);
        openInfo.FileNameLength = static_cast<USHORT>(diskPath.length());
        memcpy(&openInfo.FileName, diskPath.c_str(), openInfo.FileNameLength);

        return openInfo;
    }

    void MakeDiskInfo(const DISK_INFO* disksInfo, DiskInfoMessage* diskInfoMessage)
    {
        for (USHORT i = 0; i < disksInfo->DiskCount; i++)
        {
            const OPEN_FILE_INFORMATION& fileInfo = disksInfo->disks[i];
            Disk disk;
            disk.size = fileInfo.FileSize.QuadPart;
            disk.letter = fileInfo.DriveLetter;
            disk.fullDiskPath = ConvertStringToWString(std::string(fileInfo.FileName, fileInfo.FileName + fileInfo.FileNameLength));

            diskInfoMessage->disks.emplace_back(std::move(disk));
        }
    }
}

bool SendData(MessageCode messageCode, const void* data, void* receivedData)
{
    switch (messageCode)
    {
    case MessageMountDisk:
    {
        OPEN_FILE_INFORMATION info = ConvertMountMessageToOpenStruct(reinterpret_cast<const MountMessage*>(data));
        return !FileDiskMount(&info);
    }

    case MessageUnmountDisk:
    {
        return !FileDiskUmount(reinterpret_cast<const UnmountMessage*>(data)->diskLetter);
    }

    case MessageDiskInfo:
    {
        return true;        // This feature is not supported at the moment
        DISK_INFO disksInfo;
        memset(&disksInfo, 0 , sizeof(disksInfo));
        if (FileDiskStatus(&disksInfo))
        {
            return false;
        }

        MakeDiskInfo(&disksInfo, reinterpret_cast<DiskInfoMessage*>(receivedData));
        return true;
    }

    default:
        break;
    }

    return false;
}
