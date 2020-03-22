#define WIN32_LEAN_AND_MEAN
#include <winstrct.h>
#include <wfind.h>
#include <wconsole.h>

#if defined(_DLL) && defined(_M_IX86)
#include <minwcrtlib.h>
#pragma comment(lib, "minwcrt.lib")
#else
#include <stdlib.h>
#endif
#include <conio.h>
#include <process.h>
#include <string.h>

#include "fdftable.hpp"
#include "lnk.h"

#ifndef FILE_ATTRIBUTE_RECALL_ON_OPEN
#define FILE_ATTRIBUTE_RECALL_ON_OPEN                    0x00040000  
#endif
#ifndef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS             0x00400000  
#endif
#define FILE_OFFLINE_ATTRIBUTES (FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_RECALL_ON_OPEN | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS | FILE_ATTRIBUTE_VIRTUAL)

char cCurrentPath[MAX_PATH_MAX_LENGTH] = "";
char *szExcludeStrings = NULL;
DWORD dwExcludeStrings = 0;
char **patterns;

DWORD dwSkipSize = 0;

QWORD qwDupSize = 0;
QWORD qwSavedSize = 0;
QWORD qwDupFiles = 0;
QWORD qwFiles = 0;
QWORD qwDeletedFiles = 0;

bool bDisplayLinks = true;
bool bDisplayEqual = true;
bool bVerbose = true;
bool bQuiet = false;
bool bDeleteEqual = false;
bool bForceDelete = false;
bool bLinkEqual = false;
bool bRecurse = false;
DWORD dwSkipFilesWithAttributes = FILE_ATTRIBUTE_DIRECTORY;
bool bNoFollowJunctions = false;
volatile bool bBreak = false;
bool bSplitLinks = false;

FileRecordTableClass *FileRecordTable = NULL;

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        bBreak = true;
        return true;
    default:
        return false;
    }
}

bool ExcludedString()
{
    DWORD dwExcludeString = dwExcludeStrings;
    char *szExcludeString = szExcludeStrings;
    char *szPathEnd = cCurrentPath + strlen(cCurrentPath);

    do
    {
        SIZE_T dwExclStrLen = strlen(szExcludeString);

        for (char *szPathPtr = cCurrentPath;
        (DWORD)(szPathEnd - szPathPtr) >= dwExclStrLen;
            szPathPtr++)
            if (_strnicmp(szPathPtr, szExcludeString, dwExclStrLen) == 0)
                return true;

        szExcludeString += dwExclStrLen + 1;
    } while (--dwExcludeString);

    return false;
}

void
chkfile(LPCSTR szFilePath, const WIN32_FIND_DATA *file)
{
    if (bVerbose)
    {
        printf("\r%.79s", szFilePath);
        clreol();
        fputc('\r', stdout);
    }

    FILE_RECORD *fr = FileRecordTable->Find(szFilePath);
    if (fr != NULL)
    {
        qwFiles++;

        if (bSplitLinks)
        {
            DWORD dwFileAttrs = GetFileAttributes(szFilePath);
            SetFileAttributes(szFilePath, FILE_ATTRIBUTE_NORMAL);
            if (!DeleteFile(szFilePath))
            {
                win_perror(szFilePath);
                SetFileAttributes(szFilePath, dwFileAttrs);
                fprintf(stderr, "Cannot unlink '%s'\n", szFilePath);
                clreol();
                puts("");
                return;
            }

            SetFileAttributes(fr->szFilePath, dwFileAttrs);
            if (!CopyFile(fr->szFilePath, szFilePath, TRUE))
            {
                win_perror(szFilePath);
                fprintf(stderr, "Cannot copy '%s' to '%s'\n", fr->szFilePath,
                    szFilePath);
                clreol();
                puts("");
                return;
            }

            printf("'%s' copied to '%s'\n", fr->szFilePath, szFilePath);
        }
        else if (bDeleteEqual & bLinkEqual)
        {
            printf("'%s' <=> '%s': ", szFilePath, fr->szFilePath);

            DWORD dwFileAttrs = 0;
            if (bForceDelete)
            {
                dwFileAttrs = GetFileAttributes(fr->szFilePath);
                SetFileAttributes(szFilePath, FILE_ATTRIBUTE_NORMAL);
            }

            if (DeleteFile(szFilePath))
            {
                if (bDisplayEqual)
                    printf("Deleted ok.");

                qwDeletedFiles++;
            }
            else
            {
                win_perror(szFilePath);
                fprintf(stderr, "Cannot delete '%s'\n", szFilePath);
            }

            clreol();
            puts("");

            if (bForceDelete)
                SetFileAttributes(fr->szFilePath, dwFileAttrs);
        }
        else if (bDisplayLinks)
        {
            printf("'%s' <=> '%s'", szFilePath, fr->szFilePath);
            clreol();
            puts("");
        }

        return;
    }

    LARGE_INTEGER FileSize;
    FileSize.HighPart = file->nFileSizeHigh;
    FileSize.LowPart = file->nFileSizeLow;

    if (dwSkipSize > 0)
    {
        if (FileSize.QuadPart > dwSkipSize)
            fr = FileRecordTable->FindEqualOrAdd(szFilePath, dwSkipSize);
        else
            fr = NULL;
    }
    else
        fr = FileRecordTable->FindEqualOrAdd(szFilePath, 0);

    if (fr == NULL)
    {
        qwFiles++;
        return;
    }

    if (strcmp(szFilePath, fr->szFilePath) == 0)
        return;

    qwFiles++;
    qwDupFiles++;

    if (bDisplayEqual)
    {
        printf("'%s' contains same as '%s' (%.4g %s). ",
            szFilePath, fr->szFilePath,
            TO_h(FileSize.QuadPart), TO_p(FileSize.QuadPart));
        clreol();
    }

    if (bDeleteEqual)
    {
        DWORD dwFileAttrs = 0;
        if (bForceDelete)
        {
            dwFileAttrs = GetFileAttributes(szFilePath);
            SetFileAttributes(szFilePath, FILE_ATTRIBUTE_NORMAL);
        }

        if (!DeleteFile(szFilePath))
        {
            win_perror(szFilePath);
            if (bForceDelete)
                SetFileAttributes(szFilePath, dwFileAttrs);
            fprintf(stderr, "Cannot delete '%s'\n", szFilePath);
            qwDupSize += file->nFileSizeLow;
            qwDupSize += (QWORD)file->nFileSizeHigh << 32;
            return;
        }

        qwDeletedFiles++;

        if (bDisplayEqual)
            puts("Deleted ok.");

        qwSavedSize += file->nFileSizeLow;
        qwSavedSize += (QWORD)file->nFileSizeHigh << 32;
        return;
    }

    if (bLinkEqual)
    {
        DWORD dwFileAttrs = 0;
        if (bForceDelete)
        {
            dwFileAttrs = GetFileAttributes(szFilePath);
            SetFileAttributes(szFilePath, FILE_ATTRIBUTE_NORMAL);
        }

        if (!CreateHardLinkToFile(fr->szFilePath, szFilePath, TRUE))
            if (!CreateHardLinkToFile(szFilePath, fr->szFilePath, TRUE))
            {
                win_perror(szFilePath);
                if (bForceDelete)
                    SetFileAttributes(szFilePath, dwFileAttrs);
                fprintf(stderr, "Cannot link '%s' to '%s'.\n",
                    szFilePath, fr->szFilePath);
                qwDupSize += file->nFileSizeLow;
                qwDupSize += (QWORD)file->nFileSizeHigh << 32;
                return;
            }

        qwDeletedFiles++;

        if (bDisplayEqual)
            puts("Hard linked ok.");

        qwSavedSize += file->nFileSizeLow;
        qwSavedSize += (QWORD)file->nFileSizeHigh << 32;
        return;
    }

    if (bDisplayEqual)
        puts("");

    qwDupSize += file->nFileSizeLow;
    qwDupSize += (QWORD)file->nFileSizeHigh << 32;
    return;
}

bool
dosubdir(LPSTR pCurrentPathPtr)
{
    if (bRecurse)
    {
        if (bVerbose)
        {
            printf("\r%.79s", cCurrentPath);
            clreol();
            fputc('\r', stdout);
        }

        pCurrentPathPtr[0] = '*';
        pCurrentPathPtr[1] = 0;

        WFilteredFileFinder dirfound(cCurrentPath,
            bNoFollowJunctions ?
            FILE_ATTRIBUTE_REPARSE_POINT : 0,
            FILE_ATTRIBUTE_DIRECTORY);
        if (dirfound)
            do
            {
#ifndef NO_LOOP_SLEEPS
                Sleep(0);
#endif
                if (bBreak)
                    return false;

                if (sizeof(cCurrentPath) - (pCurrentPathPtr - cCurrentPath) <
                    strlen(dirfound.cFileName) + 2)
                {
                    fprintf(stderr, "Skipping too long name: '%s'\n",
                        dirfound.cFileName);
                    continue;
                }

                strcpy(pCurrentPathPtr, dirfound.cFileName);

                if ((strcmp(dirfound.cFileName, ".") == 0) ||
                    (strcmp(dirfound.cFileName, "..") == 0))
                    continue;

                if (sizeof(cCurrentPath) - strlen(pCurrentPathPtr) -
                    (pCurrentPathPtr - cCurrentPath) < 4)
                {
                    fprintf(stderr, "Skipping too long path: '%s'\n",
                        cCurrentPath);
                    continue;
                }

                strcat(pCurrentPathPtr, "\\");

                if (dwExcludeStrings && ExcludedString())
                    continue;

                if (!dosubdir(pCurrentPathPtr + strlen(pCurrentPathPtr)))
                    return false;

                continue;
            } while (dirfound.Next());
    }

    char **argv = patterns;
    while ((++argv)[0])
    {
#ifndef NO_LOOP_SLEEPS
        Sleep(0);
#endif

        if (bBreak)
            return false;

        if (sizeof(cCurrentPath) - (pCurrentPathPtr - cCurrentPath) -
            strlen(argv[0]) < 1)
        {
            fprintf(stderr, "Skipping too long path: '%s'\n", cCurrentPath);
            continue;
        }

        strcpy(pCurrentPathPtr, argv[0]);

        WFilteredFileFinder found(cCurrentPath, dwSkipFilesWithAttributes);
        if (!found)
            continue;

        do
        {
#ifndef NO_LOOP_SLEEPS
            Sleep(0);
#endif

            if (bBreak)
                return false;

            if (sizeof(cCurrentPath) - (pCurrentPathPtr - cCurrentPath) <
                strlen(found.cFileName) + 2)
            {
                fprintf(stderr, "Skipping too long name: '%s'\n",
                    found.cFileName);
                continue;
            }

            strcpy(pCurrentPathPtr, found.cFileName);

            if (dwExcludeStrings && ExcludedString())
                continue;

            if (found.nFileSizeHigh | found.nFileSizeLow)
                chkfile(cCurrentPath, &found);
        } while (found.Next());
    }

    return true;
}

__declspec(noreturn) void
usage()
{
    fputs("Utility to check for duplicate files.\r\n"
        "\n"
        "Version 1.0.0. Copyright (C) Olof Lagerkvist 2004 - 2019.\r\n"
        "This program is open source freeware.\r\n"
        "http://www.ltr-data.se    olof@ltr-data.se\r\n"
        "\n"
        "Usage:\r\n"
        "fdf [-cdfhjlqrs] [-n:LEN] [-x:string[,string2 ...]] [filepattern ...]\r\n"
        "\n"
        "If no filepatterns are given, all files are processed.\r\n"
        "\n"
        "-c      Split existing hard links by copying them into separate files.\r\n"
        "-d      Delete duplicate files.\r\n"
        "-f      Used with -d to force deletion of read-only files or with -l to\r\n"
        "        overwrite read-only files with hard links to files with same contents.\r\n"
        "-h      Do not display information about hard links.\r\n"
        "-j      In combination with -r, do not follow junctions or other reparse\r\n"
        "        points.\r\n"
        "-l      Hard link duplicate files. This saves disk space but means that updates\r\n"
        "        to one of these files will affect all of them.\r\n"
        "        In combination with -d, this switch also deletes links between files\r\n"
        "        leaving only one link to each found file.\r\n"
        "-n:LEN  Skip LEN bytes from the beginning of files when comparing. This can\r\n"
        "        e.g. be used to find similar files where only some header bytes\r\n"
        "        differs. All files smaller than or equal to the LEN size are treated as\r\n"
        "        not equal. LEN may be suffixed with K or M to specify kilobytes or\r\n"
        "        megabytes, k or m to specify 1,000 bytes or 1,000,000 bytes. The\r\n"
        "        argument must be smaller than 2 GB.\r\n"
        "-q      Do not display which file or directory is currently scanned.\r\n"
        "-qq     Same as -q but also skips error messages.\r\n"
        "-o      Skips offline files.\r\n"
        "-r      Recurse into subdirectories.\r\n"
        "-s      Do not display information about duplicate files found.\r\n"
        "-x      Exclude paths and filenames containing given strings.\r\n",
        stderr);

    exit(1);
}

int
main(int argc, char **argv)
{
    // Nice argument parse loop :)
    while (argv[1] ? argv[1][0] ? (argv[1][0] | 0x02) == '/' : false : false)
    {
        while ((++argv[1])[0])
            switch (argv[1][0] | 0x20)
            {
            case 'h':
                bDisplayLinks = false;
                break;
            case 's':
                bDisplayEqual = false;
                break;
            case 'd':
                bDeleteEqual = true;
                break;
            case 'f':
                bForceDelete = true;
                break;
            case 'l':
                bLinkEqual = true;
                break;
            case 'n':
                if (argv[1][1] == ':')
                {
                    char *szSizeSuffix = NULL;
                    dwSkipSize = strtoul(argv[1] + 2, &szSizeSuffix, 0);
                    if (szSizeSuffix == argv[1] + 2)
                    {
                        fprintf(stderr,
                            "Invalid size: %s\r\n", szSizeSuffix);
                        return -1;
                    }

                    if (szSizeSuffix[0] ? szSizeSuffix[1] != 0 : false)
                    {
                        fprintf(stderr,
                            "Invalid size suffix: %s\r\n", szSizeSuffix);
                        return -1;
                    }

                    switch (szSizeSuffix[0])
                    {
                    case 'M':
                        dwSkipSize <<= 10;
                    case 'K':
                        dwSkipSize <<= 10;
                        break;
                    case 'm':
                        dwSkipSize *= 1000;
                    case 'k':
                        dwSkipSize *= 1000;
                    case 0:
                        break;
                    default:
                        fprintf(stderr,
                            "Invalid size suffix: %c\r\n", szSizeSuffix[0]);
                        return -1;
                    }

                    argv[1] += strlen(argv[1]) - 1;
                }
                else
                {
                    fputs("Option -n requires an argument.\r\n", stderr);
                    return -1;
                }
                break;
            case 'q':
                if (!bVerbose)
                    bQuiet = true;
                bVerbose = false;
                break;
            case 'o':
                dwSkipFilesWithAttributes |= FILE_OFFLINE_ATTRIBUTES;
                break;
            case 'r':
                bRecurse = true;
                break;
            case 'j':
                bNoFollowJunctions = true;
                break;
            case 'c':
                bSplitLinks = true;
                break;
            case 'x':
                if (dwExcludeStrings ||
                    argv[1][1] != ':' ||
                    argv[1][2] == 0)
                    usage();

                szExcludeStrings = strtok(argv[1] + 2, ",");
                dwExcludeStrings = 1;
                
                while (strtok(NULL, ","))
                    dwExcludeStrings++;
                
                argv[1] += strlen(argv[1]) - 1;
                break;
            default:
                usage();
            }

        --argc;
        ++argv;
    }

    if (bForceDelete & !(bDeleteEqual | bLinkEqual))
        usage();

    char *fakeargv[3];
    if (argv[1] == NULL)
    {
        fakeargv[1] = "*";
        fakeargv[2] = NULL;
        patterns = fakeargv;
    }
    else
        patterns = argv;

    while ((++argv)[0] != NULL)
        CharToOem(argv[0], argv[0]);

    SetFileApisToOEM();

    FileRecordTable = new FileRecordTableClass;
    if (FileRecordTable == NULL)
    {
        perror("Memory allocation error");
        return -1;
    }

    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    SetConsoleCtrlHandler(ConsoleCtrlHandler, true);

    EnableBackupPrivileges();

    if (!dosubdir(cCurrentPath))
        printf("\r* Break *");

    clreol();
    puts("");

    fflush(stdout);
    fflush(stderr);

    if (qwFiles)
        printf("%.f file%s scanned.\n", (double)qwFiles, qwFiles == 1 ? "" : "s");
    else
        puts("No files found.");

    if (qwSavedSize)
    {
        printf("%.f file%s %s saved %.4g %s.\n",
            (double)qwDeletedFiles, qwDeletedFiles == 1 ? "" : "s",
            bDeleteEqual ? "removed" : "linked with duplicate", TO_h(qwSavedSize),
            TO_p(qwSavedSize));

        if (qwDupSize)
            printf("%.f duplicate file%s left occupying %.4g %s.\n",
                (double)(qwDupFiles - qwDeletedFiles),
                (qwDupFiles - qwDeletedFiles) == 1 ? "" : "s", TO_h(qwDupSize),
                TO_p(qwDupSize));
    }
    else
    {
        if (qwDupSize)
            printf("%.f duplicate file%s occupying %.4g %s.\n", (double)qwDupFiles,
                qwDupFiles == 1 ? "" : "s", TO_h(qwDupSize), TO_p(qwDupSize));
        else
            puts("No duplicate files found.");
    }

    return 0;
}
