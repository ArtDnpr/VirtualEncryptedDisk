#include "stdafx.h"
#include "CryptoUtils.h"

bool InitCBCAlg(BCRYPT_ALG_HANDLE *phAesAlg)
{

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(phAesAlg, BCRYPT_AES_ALGORITHM, NULL, 0)))
    {
        goto cleanup;
    }

    if (!BCRYPT_SUCCESS(BCryptSetProperty(*phAesAlg, BCRYPT_CHAINING_MODE, (PBYTE)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0)))
    {
        goto cleanup;
    }

    return true;

cleanup:
    if (*phAesAlg)
    {
        BCryptCloseAlgorithmProvider(*phAesAlg, 0);
    }

    return false;
}

bool HashPassword(const char * pwdToHash, unsigned char *pHashData)
{
    bool status = false;
    BCRYPT_ALG_HANDLE hHashAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    PBYTE hData = NULL;
    DWORD hLen = 0;
    DWORD tmp = 0;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hHashAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0)))
    {
        goto cleanup;
    }

    if (!BCRYPT_SUCCESS(BCryptCreateHash(hHashAlg, &hHash, NULL, NULL, NULL, 0, 0)))
    {
        goto cleanup;
    }

    if (!BCRYPT_SUCCESS(BCryptHashData(hHash, (PUCHAR)pwdToHash, (ULONG)strlen(pwdToHash), 0)))
    {
        goto cleanup;
    }

    if (!BCRYPT_SUCCESS(BCryptGetProperty(hHash, BCRYPT_HASH_LENGTH, (PBYTE)&hLen, sizeof(hLen), &tmp, 0)))
    {
        goto cleanup;
    }

    hData = (PBYTE)HeapAlloc(GetProcessHeap(), 0, hLen);
    if (!hData)
    {
        goto cleanup;
    }

    if (!BCRYPT_SUCCESS(BCryptFinishHash(hHash, hData, hLen, 0)))
    {
        goto cleanup;
    }

    memcpy(pHashData, hData, cAesKeySize256Bit);

    status = true;

cleanup:
    if (hData)
    {
        HeapFree(GetProcessHeap(), 0, hData);
    }

    if (hHash)
    {
        BCryptDestroyHash(hHash);
    }

    if (hHashAlg)
    {
        BCryptCloseAlgorithmProvider(hHashAlg, 0);
    }

    return status;
}

bool MakeAesKey(AesKey *pAesKey, const char *password)
{
    bool status = false;
    BCRYPT_ALG_HANDLE hAesAlg = NULL;
    PBYTE pHashData = NULL;

    if (!InitCBCAlg(&hAesAlg))
    {
        goto cleanup;
    }

    pHashData = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cAesKeySize256Bit);
    if (!pHashData)
    {
        goto cleanup;
    }

    if (!HashPassword(password, pHashData))
    {
        goto cleanup;
    }

    if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAesAlg, &pAesKey->hKey, NULL, NULL,	pHashData, cAesKeySize256Bit, 0)))
    {
        goto cleanup;
    }

    ULONG tmp;
    if (!BCRYPT_SUCCESS(BCryptGetProperty(hAesAlg, BCRYPT_BLOCK_LENGTH, (PUCHAR)&pAesKey->blockSize, sizeof(pAesKey->blockSize), &tmp, 0)))
    {
        goto cleanup;
    }

    status = true;

cleanup:
    if (pHashData)
    {
        HeapFree(GetProcessHeap(), 0, pHashData);
    }

    if (hAesAlg)
    {
        BCryptCloseAlgorithmProvider(hAesAlg, 0);
    }

    return status;
}

size_t GetCryptedBlockSize(AesKey *pAesKey, size_t dataSize, bool encrypt)
{
    NTSTATUS bcryptResult = 0;
    size_t cryptedBlockSize = 0;
    bcryptResult = (encrypt ? BCryptEncrypt : BCryptDecrypt)(pAesKey->hKey, NULL, (ULONG)dataSize, NULL, NULL, NULL, NULL, 0, (ULONG*)&cryptedBlockSize, 0);

    return BCRYPT_SUCCESS(bcryptResult) ? cryptedBlockSize : 0;
}

size_t EncrypDecryptData(AesKey *pAesKey, unsigned char *srcData, size_t srcDataSize, unsigned char *dstData, size_t dstDataSize, bool encrypt, unsigned char* iv)
{
    size_t writtenBytes = 0;
    if (!BCRYPT_SUCCESS((encrypt ? BCryptEncrypt : BCryptDecrypt)(pAesKey->hKey,
                                                                  srcData, (ULONG)srcDataSize, NULL, iv, pAesKey->blockSize,
                                                                  dstData, (ULONG)dstDataSize, (ULONG*)&writtenBytes, 0)))
    {
        return 0;
    }

    return writtenBytes;
}

void PrepareIv(const AesKey* key, uint64_t ivValue, unsigned char* iv)
{
    memset(iv, 0, key->blockSize);
    memcpy(iv, &ivValue, sizeof(ivValue));
}

