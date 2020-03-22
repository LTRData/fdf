#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "chkfile.h"

INT
CompareFiles(LPCSTR szFile1, LPCSTR szFile2, DWORD dwSkipSize)
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
    if (!SetFilePointer(hFile1, dwSkipSize, NULL, FILE_BEGIN))
      return FILE_COMPARE_FILE1_FAILED;

  hFile2 = CreateFile(szFile2, GENERIC_READ, FILE_SHARE_READ, NULL,
		      OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
  if (hFile2 == INVALID_HANDLE_VALUE)
    {
      CloseHandle(hFile1);
      return FILE_COMPARE_FILE2_FAILED;
    }

  if (dwSkipSize > 0)
    if (!SetFilePointer(hFile2, dwSkipSize, NULL, FILE_BEGIN))
      {
	CloseHandle(hFile1);
	return FILE_COMPARE_FILE1_FAILED;
      }

  for (;;)
    {
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

      if (!CompareMemory(File1Buffer, File2Buffer, dwFile1ReadSize >> 3))
	{
	  CloseHandle(hFile1);
	  CloseHandle(hFile2);
	  return FILE_COMPARE_NOT_EQUAL;
	}
    }
}

BOOL CompareMemory(LPVOID lpMem1, LPVOID lpMem2, DWORD dw64BitBlocks)
{
  DWORD dwIndex = 0;
  DWORDLONG *Mem1 = lpMem1;
  DWORDLONG *Mem2 = lpMem2;
  while (dwIndex < dw64BitBlocks)
    {
      if (Mem1[dwIndex] != Mem2[dwIndex])
	return FALSE;

      dwIndex++;
    }

  return TRUE;
}

BOOL GetFileCheckSum(HANDLE hFile, PULARGE_INTEGER lpChecksum0,
		     PULARGE_INTEGER lpChecksum1)
{
  static ULARGE_INTEGER buffer[2048];
  static DWORD dwReadSize = sizeof buffer;
  static DWORD dwAlignMask;
  static DWORD dwBufferIdx;

  lpChecksum0->QuadPart = 0;
  lpChecksum1->QuadPart = 0;

  while (ReadFile(hFile, buffer, sizeof buffer, &dwReadSize, NULL))
    {
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

#ifdef _DEBUG
	  printf("CHECK: %p%p-%p%p\n",
		 buffer[dwBufferIdx].HighPart, buffer[dwBufferIdx].LowPart,
		 lpChecksum0->HighPart, lpChecksum0->LowPart);
#endif
	}
    }

  return FALSE;
}
