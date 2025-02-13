#define WIN32_LEAN_AND_MEAN
#include <winstrct.h>
#include <wfind.h>

#include "chkfile.h"

#if defined(_DLL) && defined(_M_IX86)
#pragma comment(lib, "minwcrt.lib")
#endif

int main(int, char **argv)
{
    char buf[MAX_PATH * 2];

    while ((++argv)[0])
    {
        if (strlen(argv[0]) >= sizeof buf)
        {
            fprintf(stderr, "Path too long: %s\n", argv[0]);
            continue;
        }

        strcpy(buf, argv[0]);
        char *filepart = strchr(buf, '\\');
        if (filepart == NULL)
            filepart = strchr(buf, '/');
        if (filepart == NULL)
            filepart = buf;
        else
            ++filepart;

        WFilteredFileFinder ff(argv[0], FILE_ATTRIBUTE_DIRECTORY);
        if (!ff)
        {
            win_perror(argv[0]);
            continue;
        }

        do
        {
            strncpy(filepart, ff.cFileName, sizeof(buf) - (filepart - buf));
            buf[sizeof(buf) - 1] = 0;
            HANDLE h = CreateFile(buf, GENERIC_READ, FILE_SHARE_READ, NULL,
                OPEN_EXISTING, 0, NULL);

            if (h == INVALID_HANDLE_VALUE)
            {
                win_perror(buf);
                continue;
            }

            ULARGE_INTEGER Checksum;
            BOOL Break = FALSE;
            if (!GetFileCheckSum(h, 0, &Checksum, &Break))
            {
                win_perror(buf);
                continue;
            }

            printf("%s - %.4X%.4X\n", buf,
                Checksum.HighPart, Checksum.LowPart);

            CloseHandle(h);
        } while (ff.Next());
    }

    return 0;
}

