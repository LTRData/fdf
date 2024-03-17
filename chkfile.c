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
	static char File1Buffer[16384];
	static char File2Buffer[16384];
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

BOOL GetFileCheckSum(HANDLE hFile, PULARGE_INTEGER lpChecksum0,
	PULARGE_INTEGER lpChecksum1, volatile BOOL *bBreak)
{
	static ULARGE_INTEGER buffer[131072];  // 1 MB buffer
	static DWORD dwReadSize = sizeof buffer;
	static DWORD dwAlignMask;
	static DWORD dwBufferIdx;
	int i = 0;
	LARGE_INTEGER endPosition = { 0 };

	lpChecksum0->QuadPart = 0;
	lpChecksum1->QuadPart = 0;

	for (; ; i++)
	{
		if (*bBreak || !ReadFile(hFile, buffer, sizeof buffer, &dwReadSize, NULL))
			return FALSE;

#ifndef NO_LOOP_SLEEPS
		Sleep(0);
#endif

		if (dwReadSize == 0)
			return TRUE;

		// If read size not 8 byte aligned...
		if (dwReadSize & 7)
		{
			ZeroMemory(((LPBYTE)buffer) + dwReadSize, 8 - (dwReadSize & 7));
			dwReadSize += 8 - (dwReadSize & 7);
		}

		dwReadSize >>= 3;

		for (dwBufferIdx = 0; dwBufferIdx < dwReadSize; dwBufferIdx++)
		{
			lpChecksum0->QuadPart ^= buffer[dwBufferIdx].QuadPart;
			lpChecksum1->QuadPart += buffer[dwBufferIdx].QuadPart +
				lpChecksum0->QuadPart + dwBufferIdx + 1;
		}

		if (i > 0)
		{
#ifdef _DEBUG
			printf("CHECK: %I64X-%I64X\n",
				buffer[dwBufferIdx].QuadPart,
				lpChecksum0->QuadPart);
#endif

			return TRUE;
		}

		endPosition.QuadPart = -(SSIZE_T)(sizeof buffer);

		if (!SetFilePointerEx(hFile, endPosition, NULL, FILE_END))
			if (GetLastError() == ERROR_NEGATIVE_SEEK)
				return TRUE;
			else
				return FALSE;
	}
}
