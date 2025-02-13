#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <ntdll.h>
#include "chkfile.h"

#pragma comment(lib, "ntdll.lib")

INT
CompareFiles(LPCWSTR szFile1, LPCWSTR szFile2, DWORD dwSkipSize, volatile BOOL *bBreak)
{
	HANDLE hFile1;
	HANDLE hFile2;
	static char File1Buffer[1 << 20];
	static char File2Buffer[1 << 20];
	DWORD dwFile1ReadSize = 0;
	DWORD dwFile2ReadSize = 0;

	hFile1 = CreateFile(szFile1, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile1 == INVALID_HANDLE_VALUE)
		return FILE_COMPARE_FILE1_FAILED;

	if (dwSkipSize > 0)
	{
		LARGE_INTEGER liSkipSize = { 0 };
		liSkipSize.QuadPart = dwSkipSize;

		if (!SetFilePointerEx(hFile1, liSkipSize, NULL, FILE_BEGIN))
			return FILE_COMPARE_FILE1_FAILED;
	}

	hFile2 = CreateFile(szFile2, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile2 == INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFile1);
		return FILE_COMPARE_FILE2_FAILED;
	}

	if (dwSkipSize > 0)
	{
		LARGE_INTEGER liSkipSize = { 0 };
		liSkipSize.QuadPart = dwSkipSize;

		if (!SetFilePointerEx(hFile2, liSkipSize, NULL, FILE_BEGIN))
		{
			CloseHandle(hFile1);
			return FILE_COMPARE_FILE1_FAILED;
		}
	}

	for (;;)
	{
		if (*bBreak)
			return FILE_COMPARE_NOT_EQUAL;

#ifndef NO_LOOP_SLEEPS
		Sleep(0);
#endif

		if (!ReadFile(hFile1, File1Buffer, sizeof File1Buffer, &dwFile1ReadSize,
			NULL))
		{
			CloseHandle(hFile1);
			CloseHandle(hFile2);
			return FILE_COMPARE_FILE1_FAILED;
		}

		if (!ReadFile(hFile2, File2Buffer, sizeof File2Buffer, &dwFile2ReadSize,
			NULL))
		{
			CloseHandle(hFile1);
			CloseHandle(hFile2);
			return FILE_COMPARE_FILE2_FAILED;
		}

		if (dwFile1ReadSize > dwFile2ReadSize)
		{
			CloseHandle(hFile1);
			CloseHandle(hFile2);
			return FILE_COMPARE_FILE1_LONGER;
		}
		else if (dwFile1ReadSize < dwFile2ReadSize)
		{
			CloseHandle(hFile1);
			CloseHandle(hFile2);
			return FILE_COMPARE_FILE2_LONGER;
		}
		else if (dwFile1ReadSize == 0)
		{
			CloseHandle(hFile1);
			CloseHandle(hFile2);
			return FILE_COMPARE_EQUAL;
		}

		if (RtlCompareMemory(File1Buffer, File2Buffer, dwFile1ReadSize) != dwFile1ReadSize)
		{
			CloseHandle(hFile1);
			CloseHandle(hFile2);
			return FILE_COMPARE_NOT_EQUAL;
		}
	}
}

BOOL GetFileCheckSum(HANDLE hFile, DWORD dwStartPos, PULARGE_INTEGER lpChecksum,
	volatile BOOL *bBreak)
{
	static ULARGE_INTEGER buffer[16384];  // 128 KB buffer (8 * 16384 bytes)

	lpChecksum->QuadPart = 0x811C9DC5811C9DC5;

	LARGE_INTEGER liStartPos = { dwStartPos };

	if (liStartPos.QuadPart != 0 &&
		!SetFilePointerEx(hFile, liStartPos, NULL, FILE_BEGIN))
	{
		return FALSE;
	}

	for (int i = 0; ; i++)
	{
		DWORD dwReadSize;

		if (*bBreak ||
			!ReadFile(hFile, buffer, sizeof buffer, &dwReadSize, NULL))
		{
			return FALSE;
		}

		if (dwReadSize == 0)
		{
			break;
		}

		// If read size not 8 byte aligned...
		if (dwReadSize & 7)
		{
			ZeroMemory(((LPBYTE)buffer) + dwReadSize, 8 - (dwReadSize & 7));
			dwReadSize += 8 - (dwReadSize & 7);
		}

		int iReadBlocks = dwReadSize / sizeof(*buffer);

		for (int iBufferIdx = 0; iBufferIdx < iReadBlocks && !*bBreak; iBufferIdx++)
		{
			lpChecksum->QuadPart ^= buffer[iBufferIdx].QuadPart;
			lpChecksum->QuadPart *= 0x1000193010001CF;
		}

		if (*bBreak || i >= 1)
		{
			break;
		}

		if (dwReadSize < sizeof buffer)
		{
			break;
		}

		LARGE_INTEGER endPosition = { 0 };

		endPosition.QuadPart = -(SSIZE_T)(sizeof buffer);

		if (!SetFilePointerEx(hFile, endPosition, &endPosition, FILE_END))
		{
			if (GetLastError() == ERROR_NEGATIVE_SEEK)
			{
				break;
			}
			else
			{
				return FALSE;
			}
		}

		if (endPosition.QuadPart < liStartPos.QuadPart + (DWORD)sizeof buffer)
		{
			endPosition.QuadPart = liStartPos.QuadPart + (DWORD)sizeof buffer;

			if (!SetFilePointerEx(hFile, endPosition, NULL, FILE_BEGIN))
			{
				return FALSE;
			}
		}
	}

#ifdef _DEBUG
	printf("CHECK: %I64X\n", lpChecksum->QuadPart);
#endif

	return TRUE;
}
