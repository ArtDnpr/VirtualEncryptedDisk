#pragma once

#include <CommonStructs.h>

int FileDiskMount(POPEN_FILE_INFORMATION OpenFileInformation);
int FileDiskUmount(char DriveLetter);
int FileDiskStatus(PDISK_INFO myDisk_info);
