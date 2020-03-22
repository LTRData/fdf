#define WIN32_LEAN_AND_MEAN
#include <winstrct.h>

#pragma comment(lib, "minwcrt")

#include "lnk.h"

int main(int argc, char **argv)
{
  EnableBackupPrivileges();

  if (argc < 3)
    {
      puts("Usage: lnktst existing new");
      return 1;
    }

  if (HardLink(argv[1], argv[2]))
    puts("OK.");
  else
    win_perror();
}
