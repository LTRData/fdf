#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include "chkfile.h"

int CompareFiles(const char *szFile1, const char *szFile2)
{
  int hFile1;
  int hFile2;
  static char File1Buffer[16384];
  static char File2Buffer[16384];
  ssize_t dwFile1ReadSize = 0;
  ssize_t dwFile2ReadSize = 0;

  hFile1 = open(szFile1, O_RDONLY);
  if (hFile1 == -1)
    return FILE_COMPARE_FILE1_FAILED;

  hFile2 = open(szFile2, O_RDONLY);
  if (hFile2 == -1)
    {
      close(hFile1);
      return FILE_COMPARE_FILE2_FAILED;
    }

  for (;;)
    {
#ifndef NO_LOOP_SLEEPS
      usleep(0);
#endif

      dwFile1ReadSize = read(hFile1, File1Buffer, sizeof File1Buffer);
      if (dwFile1ReadSize == -1)
	{
	  close(hFile1);
	  close(hFile2);
	  return FILE_COMPARE_FILE1_FAILED;
	}

      dwFile2ReadSize = read(hFile2, File2Buffer, sizeof File2Buffer);
      if (dwFile2ReadSize == -1)
	{
	  close(hFile1);
	  close(hFile2);
	  return FILE_COMPARE_FILE2_FAILED;
	}

      if (dwFile1ReadSize > dwFile2ReadSize)
	{
	  close(hFile1);
	  close(hFile2);
	  return FILE_COMPARE_FILE1_LONGER;
	}
      else if (dwFile1ReadSize < dwFile2ReadSize)
	{
	  close(hFile1);
	  close(hFile2);
	  return FILE_COMPARE_FILE2_LONGER;
	}
      else if (dwFile1ReadSize == 0)
	{
	  close(hFile1);
	  close(hFile2);
	  return FILE_COMPARE_EQUAL;
	}

      if (memcmp(File1Buffer, File2Buffer, dwFile1ReadSize >> 3) != 0)
	{
	  close(hFile1);
	  close(hFile2);
	  return FILE_COMPARE_NOT_EQUAL;
	}
    }
}

bool GetFileCheckSum(HANDLE hFile, PULARGE_INTEGER lpChecksum0,
		     PULARGE_INTEGER lpChecksum1)
{
  static ULARGE_INTEGER buffer[2048];
  static ssize_t dwReadSize = sizeof buffer;
  static DWORD dwAlignMask;
  static DWORD dwBufferIdx;

  lpChecksum0->QuadPart = 0;
  lpChecksum1->QuadPart = 0;

  while ((dwReadSize = read(hFile, buffer, sizeof buffer)) != -1)
    {
#ifndef NO_LOOP_SLEEPS
      usleep(0);
#endif

      if (dwReadSize == 0)
	return true;

      // If read size not 8 byte aligned...
      if (dwReadSize & 7)
	{
	  bzero(((LPBYTE)buffer) + dwReadSize, 8 - (dwReadSize & 7));
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

  return false;
}
