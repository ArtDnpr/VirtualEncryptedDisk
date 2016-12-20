#pragma once
#include <ntddk.h>
#include <bcrypt.h>
#ifndef _FILE_DISK_
#define _FILE_DISK_

#define FILE_DISK_POOL_TAG 'ksiD'

#ifndef __T
#ifdef _NTDDK_
#define __T(x)  L ## x
#else
#define __T(x)  x
#endif
#endif

#ifndef _T
#define _T(x)   __T(x)
#endif

#define DEVICE_BASE_NAME    _T("\\FileDisk")
#define DEVICE_DIR_NAME     _T("\\Device")      DEVICE_BASE_NAME
#define DEVICE_NAME_PREFIX  DEVICE_DIR_NAME
#define SYMBLINK0    _T("\\DosDevices\\Global\\MyDevice")
#define SYMBLINK1    _T("\\DosDevices\\Global\\K:")
#define DEVICE_SYMB0_NAME	DEVICE_NAME_PREFIX	SYMBLINK0
#define DEVICE_SYMB1_NAME	DEVICE_NAME_PREFIX	SYMBLINK1

#define MAX_PATH1 260
#define INVALID_NUMBER_DEV 150

typedef struct _OPEN_FILE_INFORMATION
{
	LARGE_INTEGER	FileSize;
	LARGE_INTEGER	SectorSize;
	USHORT			DiskOffset;
	CHAR			Password[16];
	USHORT			PasswordLength;
	UCHAR			DriveLetter;
	USHORT			FileNameLength;
	CHAR			FileName[260];
} OPEN_FILE_INFORMATION, *POPEN_FILE_INFORMATION;

typedef struct _DISK_INFO
{
	USHORT DiskCount;
	OPEN_FILE_INFORMATION disks[26];
} DISK_INFO, *PDISK_INFO;

#define IOCTL_FILE_DISK_OPEN_FILE   CTL_CODE(FILE_DEVICE_DISK, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_FILE_DISK_CLOSE_FILE  CTL_CODE(FILE_DEVICE_DISK, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_FILE_DISK_QUERY_FILE  CTL_CODE(FILE_DEVICE_DISK, 0x802, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_FILE_DISK_UNMOUNT_ALL  CTL_CODE(FILE_DEVICE_DISK, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS)


#pragma once
#include <bcrypt.h>

const size_t cAesKeySize256Bit = 32;

typedef struct _AesKey
{
	BCRYPT_KEY_HANDLE hKey;
	ULONG blockSize;
} AesKey, *PAesKey;

#define uint64_t unsigned long long
#define bool BOOLEAN

bool MakeAesKey(AesKey* pAesKey, const char *password);
size_t GetCryptedBlockSize(AesKey* pAesKey, size_t dataSize, bool encrypt);
size_t EncrypDecryptData(AesKey* pAesKey, unsigned char *srcData, size_t srcDataSize, unsigned char *dstData, size_t dstDataSize, bool encrypt, unsigned char* iv);
void PrepareIv(const AesKey* key, uint64_t ivValue, unsigned char* iv);

bool InitCBCAlg(BCRYPT_ALG_HANDLE *phAesAlg)
{

	if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(phAesAlg, BCRYPT_AES_ALGORITHM, NULL, 0)))
	{
		goto cleanup;
	}

	if (!BCRYPT_SUCCESS(BCryptSetProperty(*phAesAlg, BCRYPT_CHAINING_MODE, (unsigned char*)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0)))
	{
		goto cleanup;
	}

	return TRUE;

cleanup:
	if (*phAesAlg)
	{
		BCryptCloseAlgorithmProvider(*phAesAlg, 0);
	}

	return FALSE;
}

bool HashPassword(const char * pwdToHash, unsigned char *pHashData)
{
	bool status = FALSE;
	BCRYPT_ALG_HANDLE hHashAlg = NULL;
	BCRYPT_HASH_HANDLE hHash = NULL;
	unsigned char* hData = NULL;
	SIZE_T hLen = 0;
	DWORD tmp = 0;

	if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hHashAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0)))
	{
		goto cleanup;
	}

	if (!BCRYPT_SUCCESS(BCryptCreateHash(hHashAlg, &hHash, (PUCHAR)NULL, (ULONG)NULL, NULL, 0, 0)))
	{
		goto cleanup;
	}

	if (!BCRYPT_SUCCESS(BCryptHashData(hHash, (PUCHAR)pwdToHash, (ULONG)strlen(pwdToHash), 0)))
	{
		goto cleanup;
	}

	if (!BCRYPT_SUCCESS(BCryptGetProperty(hHash, BCRYPT_HASH_LENGTH, (unsigned char*)&hLen, sizeof(hLen), &tmp, 0)))
	{
		goto cleanup;
	}

	//hData = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, hLen);
	hData = (unsigned char*)MmAllocateNonCachedMemory(hLen);
	if (!hData)
	{
		goto cleanup;
	}

	if (!BCRYPT_SUCCESS(BCryptFinishHash(hHash, hData, hLen, 0)))
	{
		goto cleanup;
	}

	memcpy(pHashData, hData, cAesKeySize256Bit);

	status = TRUE;

cleanup:
	if (hData)
	{
		//HeapFree(GetProcessHeap(), 0, hData);
		MmFreeNonCachedMemory(hData, hLen);
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
	bool status = FALSE;
	BCRYPT_ALG_HANDLE hAesAlg = (BCRYPT_ALG_HANDLE)NULL;
	unsigned char* pHashData = (unsigned char*)NULL;

	if (!InitCBCAlg(&hAesAlg))
	{
		goto cleanup;
	}

	//pHashData = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, cAesKeySize256Bit);
	pHashData = (unsigned char*)MmAllocateNonCachedMemory(cAesKeySize256Bit);
	if (!pHashData)
	{
		goto cleanup;
	}

	if (!HashPassword(password, pHashData))
	{
		goto cleanup;
	}

	if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAesAlg, &pAesKey->hKey, (PUCHAR)NULL, (ULONG)NULL, pHashData, cAesKeySize256Bit, 0)))
	{
		goto cleanup;
	}

	ULONG tmp;
	if (!BCRYPT_SUCCESS(BCryptGetProperty(hAesAlg, BCRYPT_BLOCK_LENGTH, (PUCHAR)&pAesKey->blockSize, sizeof(pAesKey->blockSize), &tmp, 0)))
	{
		goto cleanup;
	}

	status = TRUE;

cleanup:
	if (pHashData)
	{
		//HeapFree(GetProcessHeap(), 0, pHashData);
		MmFreeNonCachedMemory(pHashData, cAesKeySize256Bit);
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
	bcryptResult = (encrypt ? BCryptEncrypt : BCryptDecrypt)(pAesKey->hKey, (PUCHAR)NULL, (ULONG)dataSize, NULL, (PUCHAR)NULL, (ULONG)NULL, (PUCHAR)NULL, 0, (ULONG*)&cryptedBlockSize, 0);

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

//#define IOCTL_FILE_DISK_OPEN_FILE   CTL_CODE(FILE_DEVICE_DISK, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
//#define IOCTL_FILE_DISK_CLOSE_FILE  CTL_CODE(FILE_DEVICE_DISK, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
//#define IOCTL_FILE_DISK_QUERY_FILE  CTL_CODE(FILE_DEVICE_DISK, 0x802, METHOD_BUFFERED, FILE_READ_ACCESS)
//#define IOCTL_FILE_DISK_UNMOUNT_ALL  CTL_CODE(FILE_DEVICE_DISK, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS)

//typedef struct _OPEN_FILE_INFORMATION {
//	LARGE_INTEGER   FileSize;
//	BOOLEAN         ReadOnly;
//	UCHAR			DriveLetter;
//	USHORT			FileNameLength;
//	CHAR            FileName[1];
//} OPEN_FILE_INFORMATION, *POPEN_FILE_INFORMATION;

//typedef struct _OPEN_FILE_INFORMATION
//{
//	LARGE_INTEGER FileSize;
//	UCHAR         DriveLetter;
//	USHORT        FileNameLength;
//	LARGE_INTEGER SectorSize;
//	CHAR          Password[16];
//	USHORT        DiskOffset;
//	CHAR          FileName[1];
//} OPEN_FILE_INFORMATION, *POPEN_FILE_INFORMATION;
//
//typedef struct _DISK_INFO
//{
//	USHORT DiskCount;
//	OPEN_FILE_INFORMATION disks[26];
//}DISK_INFO, *PDISK_INFO;
//
//const size_t cAesKeySize256Bit = 32;
//#define uint64_t unsigned long long
//#define bool BOOLEAN
//
//typedef struct _AesKey
//{
//	BCRYPT_KEY_HANDLE hKey;
//	ULONG blockSize;
//} AesKey, *PAesKey;
//
//bool MakeAesKey(AesKey *pAesKey, const char *password);
//size_t GetCryptedBlockSize(AesKey *pAesKey, size_t dataSize, bool encrypt);
//size_t EncrypDecryptData(AesKey *pAesKey, unsigned char *srcData, size_t srcDataSize, unsigned char *dstData, size_t dstDataSize, bool encrypt, unsigned char* iv);
//void PrepareIv(const AesKey* key, uint64_t ivValue, unsigned char* iv);

#endif