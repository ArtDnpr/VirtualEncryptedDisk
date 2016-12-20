#include "stdafx.h"
#include "DriverCommunication.h"

#define DEVICE_BASE_NAME    _T("\\FileDisk")
#define DEVICE_DIR_NAME     _T("\\Device")      DEVICE_BASE_NAME
#define DEVICE_NAME_PREFIX  DEVICE_DIR_NAME

int n = 1;

int FileDiskMount(POPEN_FILE_INFORMATION  pok)
{
    char    VolumeName[] = "\\\\.\\ :";
    char    DriveName[] = " :\\";
    char    DeviceName[255];
    HANDLE  Device;
    DWORD   BytesReturned;

    VolumeName[4] = pok->DriveLetter;
    DriveName[0] = pok->DriveLetter;

    Device = CreateFile(
                "\\\\.\\MyDevice",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_NO_BUFFERING,
                NULL
                );

    if (Device == INVALID_HANDLE_VALUE)
    {
        return -1;
    }

    if (!DeviceIoControl(
                Device,
                IOCTL_FILE_DISK_OPEN_FILE,
                pok,
                sizeof(OPEN_FILE_INFORMATION),
                NULL,
                0,
                &BytesReturned,
                NULL))
    {
        CloseHandle(Device);
        return -1;
    }

    sprintf(DeviceName, DEVICE_NAME_PREFIX "%u", n);
    if (!DefineDosDevice(DDD_RAW_TARGET_PATH, &VolumeName[4], DeviceName))
    {
        printf("\n!Define  \n");
    }
    CloseHandle(Device);

    SHChangeNotify(SHCNE_DRIVEADD, SHCNF_PATH, DriveName, NULL);
    n++;

    return 0;
}

int FileDiskUmount(char DriveLetter)
{
    char    VolumeName[] = "\\\\.\\ :";
    char    DriveName[] = " :\\";
    HANDLE  Device;
    DWORD   BytesReturned;

    VolumeName[4] = DriveLetter;
    DriveName[0] = DriveLetter;

    Device = CreateFile(
                VolumeName,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_NO_BUFFERING,
                NULL
                );

    if (Device == INVALID_HANDLE_VALUE)
    {
        return -1;
    }

    if (!DeviceIoControl(
                Device,
                IOCTL_FILE_DISK_CLOSE_FILE,
                NULL,
                0,
                NULL,
                0,
                &BytesReturned,
                NULL
                ))
    {
        CloseHandle(Device);
        return -1;
    }

    CloseHandle(Device);

    if (!DefineDosDevice(DDD_REMOVE_DEFINITION,&VolumeName[4],NULL))
    {
        return -1;
    }

    SHChangeNotify(SHCNE_DRIVEREMOVED, SHCNF_PATH, DriveName, NULL);
    n--;

    return 0;
}

int FileDiskStatus(PDISK_INFO myDisk_info)
{
    HANDLE		Device;
    DWORD		BytesReturned;

    Device = CreateFile(
                "\\\\.\\MyDevice",
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_NO_BUFFERING,
                NULL
                );

    if (Device == INVALID_HANDLE_VALUE)
    {
        return -1;
    }

    if (!DeviceIoControl(
                Device,
                IOCTL_FILE_DISK_QUERY_FILE,
                NULL,
                0,
                myDisk_info,
                sizeof(DISK_INFO),
                &BytesReturned,
                NULL
                ))
    {
        CloseHandle(Device);
        return -1;
    }

    CloseHandle(Device);

    return 0;
}
