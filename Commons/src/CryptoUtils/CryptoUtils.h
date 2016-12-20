#pragma once

const size_t cAesKeySize256Bit = 32;

struct AesKey
{
    BCRYPT_KEY_HANDLE hKey;
    ULONG blockSize;
};

bool MakeAesKey(AesKey *pAesKey, const char *password);
size_t GetCryptedBlockSize(AesKey *pAesKey, size_t dataSize, bool encrypt);
size_t EncrypDecryptData(AesKey *pAesKey, unsigned char *srcData, size_t srcDataSize, unsigned char *dstData, size_t dstDataSize, bool encrypt, unsigned char* iv = NULL);
void PrepareIv(const AesKey* key, uint64_t ivValue, unsigned char* iv);
