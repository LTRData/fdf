#define WIN32_LEAN_AND_MEAN
#include <winstrct.h>
#include <wfind.h>

#include "chkfile.h"

#pragma comment(lib, "minwcrt")

int main(int, char **argv)
{
  SetFileApisToOEM();

  while ((++argv)[0])
    {
      CharToOem(argv[0], argv[0]);

      WFileFinder ff(argv[0]);
      if (!ff)
	{
	  win_perror(argv[0]);
	  return 1;
	}

      do
	{
	  if (ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	    continue;
	  
	  HANDLE h = CreateFile(ff.cFileName, GENERIC_READ,
				FILE_SHARE_READ|FILE_SHARE_DELETE, NULL,
				OPEN_EXISTING, 0, NULL);
	  
	  if (h == INVALID_HANDLE_VALUE)
	    {
	      win_perror(ff.cFileName);
	      continue;
	    }

	  ULARGE_INTEGER Checksum0;
	  ULARGE_INTEGER Checksum1;
	  if (!GetFileCheckSum(h, &Checksum0, &Checksum1))
	    {
	      win_perror(ff.cFileName);
	      continue;
	    }

	  printf("%s - %p%p-%p%p\n", ff.cFileName,
		 Checksum0.HighPart, Checksum0.LowPart,
		 Checksum1.HighPart, Checksum1.LowPart);

	  CloseHandle(h);
	}
      while (ff.Next());
    }      

  return 0;
}

