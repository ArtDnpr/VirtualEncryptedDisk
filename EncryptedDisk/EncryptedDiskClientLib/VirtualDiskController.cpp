#include "stdafx.h"
#include "VirtualDiskController.h"
#include "IVirtualDiskControllerObserver.h"
#include "Transport.h"

namespace
{
    const size_t cEncrypteBlockSize = 512;

    const size_t cVerificationStringSize = 16;
    char cVerification[] = "Verification";

    struct DiskInfo
    {
        uint64_t diskSizeInBytes;
        char verification[cVerificationStringSize];
    };
    const size_t cDiskInfoSize = sizeof(DiskInfo::diskSizeInBytes) + sizeof(DiskInfo::verification);

    std::vector<unsigned char> GetDiskInfoIv(size_t ivSize)
    {
        std::vector <unsigned char> iv(ivSize);
        memset(iv.data(), 0xFF, iv.size());

        return iv;
    }

    size_t GetAlignedDiskInfoSize(size_t blockSize)
    {
        return cDiskInfoSize + (cDiskInfoSize % blockSize);
    }

    bool WriteDiskInfo(const DiskInfo& diskInfo, AesKey *pAesKey, std::ofstream &fout)
    {
        std::vector <unsigned char> serializedDiskInfo(GetAlignedDiskInfoSize(pAesKey->blockSize), 0x00);

        memcpy(serializedDiskInfo.data(), &diskInfo.diskSizeInBytes, sizeof(diskInfo.diskSizeInBytes));
        const size_t offset = sizeof(diskInfo.diskSizeInBytes);
        memcpy(serializedDiskInfo.data() + offset, diskInfo.verification, sizeof(diskInfo.verification));

        std::vector <unsigned char> iv = GetDiskInfoIv(pAesKey->blockSize);

        std::vector<unsigned char> encryptedDiskInfo;
        encryptedDiskInfo.resize(GetCryptedBlockSize(pAesKey, serializedDiskInfo.size(), true));

        size_t cryptedDiskInfoSize = EncrypDecryptData(pAesKey, serializedDiskInfo.data(), serializedDiskInfo.size(), encryptedDiskInfo.data(), encryptedDiskInfo.size(), true, iv.data());
        encryptedDiskInfo.resize(cryptedDiskInfoSize);

        fout.write(reinterpret_cast<const char*>(encryptedDiskInfo.data()), encryptedDiskInfo.size());

        return true;
    }

    DiskInfo ReadDiskInfo(AesKey *pAesKey, std::ifstream &fin)
    {
        const int64_t currentPos = fin.tellg();
        fin.seekg(-static_cast<int64_t>(GetAlignedDiskInfoSize(pAesKey->blockSize)), std::ios::end);

        DiskInfo diskInfo = {};

        std::vector <unsigned char> encryptedDiskInfo;
        encryptedDiskInfo.resize(GetCryptedBlockSize(pAesKey, GetAlignedDiskInfoSize(pAesKey->blockSize), true));

        fin.read(reinterpret_cast<char*>(encryptedDiskInfo.data()), encryptedDiskInfo.size());

        std::vector <unsigned char> serializedDiskInfo;
        serializedDiskInfo.resize(GetCryptedBlockSize(pAesKey, encryptedDiskInfo.size(), false));

        std::vector <unsigned char> iv = GetDiskInfoIv(pAesKey->blockSize);

        const size_t decryptedDiskInfoSize = EncrypDecryptData(pAesKey, encryptedDiskInfo.data(), encryptedDiskInfo.size(), serializedDiskInfo.data(), serializedDiskInfo.size(), false, iv.data());
        if (!decryptedDiskInfoSize)
        {
            return diskInfo;
        }
        serializedDiskInfo.resize(decryptedDiskInfoSize);

        memcpy(&diskInfo.diskSizeInBytes, serializedDiskInfo.data(), sizeof(diskInfo.diskSizeInBytes));
        const size_t offset = sizeof(diskInfo.diskSizeInBytes);
        memcpy(diskInfo.verification, serializedDiskInfo.data() + offset, sizeof(diskInfo.verification));

        fin.seekg(currentPos);

        return diskInfo;
    }
}

VirtualDiskController::VirtualDiskController()
{}

const std::vector<Disk>& VirtualDiskController::getDisks()
{
    DiskInfoMessage disksInfo;
    if (!SendData(MessageDiskInfo, nullptr, &disksInfo))
    {
        throw std::runtime_error("Unable to communicate with driver");
    }

    m_disks = std::move(disksInfo.disks);

    return m_disks;
}

void VirtualDiskController::createNewDisk(const std::wstring &fullDiskPath, const uint64_t diskSize, const std::string &password,
                                          IVirtualDiskControllerObserver* observer, const std::atomic<bool>& canceled)
{
    std::ifstream fin (fullDiskPath);

    if (fin.is_open())
    {
        observer->virtualDiskControllerDidFailDiskCreation("The disk file is already exists.");
        return;
    }
    fin.close();

    std::vector<unsigned char> encryptedData;
    std::ofstream fout(fullDiskPath, std::ios::out | std::ios::binary);
    AesKey key;
    unsigned char emptyBlock[cEncrypteBlockSize] = { 0 };
    DiskInfo diskInfo;
    memset(&diskInfo, 0, sizeof(diskInfo));

    const uint64_t diskSizeInBytes = diskSize * 1024 * 1024;
    diskInfo.diskSizeInBytes = diskSizeInBytes;
    memcpy(diskInfo.verification, cVerification, std::min(sizeof(cVerification), cVerificationStringSize));
    const uint64_t blocksCount = diskSizeInBytes / cEncrypteBlockSize;

    MakeAesKey(&key, password.c_str());

    encryptedData.resize(GetCryptedBlockSize(&key, cEncrypteBlockSize, true));

    std::vector<unsigned char> iv(key.blockSize);

    size_t size = 0;
    for (uint64_t i = 0; i < blocksCount && !canceled; i++)
    {
        size+= encryptedData.size();
        PrepareIv(&key, i, iv.data());
        EncrypDecryptData(&key, emptyBlock, cEncrypteBlockSize, encryptedData.data(), encryptedData.size(), true, iv.data());
        fout.write(reinterpret_cast<const char*>(encryptedData.data()), encryptedData.size());

        if (observer)
        {
            observer->virtualDiskControllerDidEncryptDisk((i + 1) * cEncrypteBlockSize, diskSizeInBytes);
        }
    }

    WriteDiskInfo(diskInfo, &key, fout);

    if (observer)
    {
        observer->virtualDiskControllerDidFinishCreation(canceled);
    }
}

const Disk& VirtualDiskController::mountDisk(const std::wstring &fullDiskPath, char diskLetter, const std::string &password)
{
    std::ifstream fin (fullDiskPath);
    if (!fin.is_open())
    {
        throw std::runtime_error("Invalid disk path");
    }
    AesKey key;
    MakeAesKey(&key, password.c_str());
    DiskInfo diskInfo = ReadDiskInfo(&key, fin);
    if (strncmp(diskInfo.verification, cVerification, cVerificationStringSize))
    {
        throw std::runtime_error("Invalid password");
    }

    fin.seekg(0, fin.end);
    uint64_t currentDiskSize = fin.tellg();
    fin.seekg(0, fin.beg);

    if(currentDiskSize != diskInfo.diskSizeInBytes + GetAlignedDiskInfoSize(key.blockSize))
    {
        throw std::runtime_error("Invalid disk size");
    }

    MountMessage mount;
    mount.diskInfo.fullDiskPath = fullDiskPath;
    mount.diskInfo.letter = diskLetter;
    mount.diskInfo.size = diskInfo.diskSizeInBytes;
    mount.diskSectorSize = cEncrypteBlockSize;
    mount.password = password;
    mount.diskOffset = GetAlignedDiskInfoSize(key.blockSize);

    fin.close();

    if (!SendData(MessageMountDisk, &mount))
    {
        throw std::runtime_error("Unable to mount disk");
    }

    m_disks.push_back(std::move(mount.diskInfo));

    return m_disks.back();
}


void VirtualDiskController::unmountDisk(const char diskLetter)
{
    for (size_t i = 0; i < m_disks.size(); i++)
    {
        if (m_disks[i].letter == diskLetter)
        {
            UnmountMessage unmount;
            unmount.diskLetter = m_disks[i].letter;
            if (!SendData(MessageUnmountDisk, &unmount))
            {
                throw std::runtime_error("Unable to unmount disk");
            }

            m_disks.erase(m_disks.begin() + i);

            return;
        }
    }
}

void VirtualDiskController::unmountAllDisks()
{
    while(!m_disks.empty())
    {
        unmountDisk(m_disks.back().letter);
        Sleep(500); //for driver purposes
    }
}

