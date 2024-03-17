#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

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

WCHAR cCurrentPath[MAX_PATH_MAX_LENGTH] = L"";
LPWSTR szExcludeStrings = NULL;
DWORD dwExcludeStrings = 0;
LPWSTR *patterns;

DWORD dwSkipSize = 0;

DWORDLONG qwDupSize = 0;
DWORDLONG qwSavedSize = 0;
DWORDLONG qwDupFiles = 0;
DWORDLONG qwFiles = 0;
DWORDLONG qwDeletedFiles = 0;

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
volatile BOOL bBreak = FALSE;
bool bSplitLinks = false;

FileRecordTableClass *FileRecordTable = NULL;

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        bBreak = TRUE;
        return true;
    default:
        return false;
    }
}

bool ExcludedString()
{
    DWORD dwExcludeString = dwExcludeStrings;
    LPWSTR szExcludeString = szExcludeStrings;
    LPWSTR szPathEnd = cCurrentPath + wcslen(cCurrentPath);

    do
    {
        SIZE_T dwExclStrLen = wcslen(szExcludeString);

        for (LPWSTR szPathPtr = cCurrentPath;
        (DWORD)(szPathEnd - szPathPtr) >= dwExclStrLen;
            szPathPtr++)
            if (_wcsnicmp(szPathPtr, szExcludeString, dwExclStrLen) == 0)
                return true;

        szExcludeString += dwExclStrLen + 1;
    } while (--dwExcludeString);

    return false;
}

void
chkfile(LPCWSTR szFilePath, const WIN32_FIND_DATA *file)
{
    if (bVerbose)
    {
        wprintf(L"\r%.79s", szFilePath);
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
                fwprintf(stderr, L"Cannot unlink '%s'\n", szFilePath);
                clreol();
                puts("");
                return;
            }

            SetFileAttributes(fr->szFilePath, dwFileAttrs);
            if (!CopyFile(fr->szFilePath, szFilePath, TRUE))
            {
                win_perror(szFilePath);
                fwprintf(stderr, L"Cannot copy '%s' to '%s'\n", fr->szFilePath,
                    szFilePath);
                clreol();
                puts("");
                return;
            }

            wprintf(L"'%s' copied to '%s'\n", fr->szFilePath, szFilePath);
        }
        else if (bDeleteEqual && bLinkEqual)
        {
            wprintf(L"'%s' <=> '%s': ", szFilePath, fr->szFilePath);

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
                fwprintf(stderr, L"Cannot delete '%s'\n", szFilePath);
            }

            clreol();
            puts("");

            if (bForceDelete)
                SetFileAttributes(fr->szFilePath, dwFileAttrs);
        }
        else if (bDisplayLinks)
        {
            wprintf(L"'%s' <=> '%s'", szFilePath, fr->szFilePath);
            clreol();
            puts("");
        }

        return;
    }

    LARGE_INTEGER FileSize = { 0 };
    FileSize.HighPart = file->nFileSizeHigh;
    FileSize.LowPart = file->nFileSizeLow;

    if (dwSkipSize > 0)
    {
        if (FileSize.QuadPart > dwSkipSize)
            fr = FileRecordTable->FindEqualOrAdd(szFilePath, dwSkipSize, &bBreak);
        else
            fr = NULL;
    }
    else
        fr = FileRecordTable->FindEqualOrAdd(szFilePath, 0, &bBreak);

    if (fr == NULL)
    {
        qwFiles++;
        return;
    }

    if (wcscmp(szFilePath, fr->szFilePath) == 0)
        return;

    qwFiles++;
    qwDupFiles++;

    if (bDisplayEqual)
    {
        wprintf(L"'%s' contains same as '%s' (%.4g %s). ",
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
            fwprintf(stderr, L"Cannot delete '%s'\n", szFilePath);
            qwDupSize += file->nFileSizeLow;
            qwDupSize += (DWORDLONG)file->nFileSizeHigh << 32;
            return;
        }

        qwDeletedFiles++;

        if (bDisplayEqual)
            puts("Deleted ok.");

        qwSavedSize += file->nFileSizeLow;
        qwSavedSize += (DWORDLONG)file->nFileSizeHigh << 32;
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
                fwprintf(stderr, L"Cannot link '%s' to '%s'.\n",
                    szFilePath, fr->szFilePath);
                qwDupSize += file->nFileSizeLow;
                qwDupSize += (DWORDLONG)file->nFileSizeHigh << 32;
                return;
            }

        qwDeletedFiles++;

        if (bDisplayEqual)
            puts("Hard linked ok.");

        qwSavedSize += file->nFileSizeLow;
        qwSavedSize += (DWORDLONG)file->nFileSizeHigh << 32;
        return;
    }

    if (bDisplayEqual)
        puts("");

    qwDupSize += file->nFileSizeLow;
    qwDupSize += (DWORDLONG)file->nFileSizeHigh << 32;
    return;
}

bool
dosubdir(LPWSTR pCurrentPathPtr)
{
    if (bRecurse)
    {
        if (bVerbose)
        {
            wprintf(L"\r%.79s", cCurrentPath);
            clreol();
#ifdef _DEBUG
            fputc('\n', stdout);
#else
            fputc('\r', stdout);
#endif
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

                if (_countof(cCurrentPath) - (pCurrentPathPtr - cCurrentPath) <
                    wcslen(dirfound.cFileName) + 2)
                {
                    fwprintf(stderr, L"Skipping too long name: '%s'\n",
                        dirfound.cFileName);

                    continue;
                }

                wcscpy(pCurrentPathPtr, dirfound.cFileName);

                if ((wcscmp(dirfound.cFileName, L".") == 0) ||
                    (wcscmp(dirfound.cFileName, L"..") == 0))
                    continue;

                if (_countof(cCurrentPath) - wcslen(pCurrentPathPtr) -
                    (pCurrentPathPtr - cCurrentPath) < 4)
                {
                    fwprintf(stderr, L"Skipping too long path: '%s'\n",
                        cCurrentPath);

                    continue;
                }

                wcscat(pCurrentPathPtr, L"\\");

                if (dwExcludeStrings && ExcludedString())
                    continue;

                if (!dosubdir(pCurrentPathPtr + wcslen(pCurrentPathPtr)))
                    return false;

                continue;
            } while (dirfound.Next());
    }

    LPWSTR *argv = patterns;
    while ((++argv)[0])
    {
#ifndef NO_LOOP_SLEEPS
        Sleep(0);
#endif

        if (bBreak)
            return false;

        if (_countof(cCurrentPath) - (pCurrentPathPtr - cCurrentPath) -
            wcslen(argv[0]) < 1)
        {
            fwprintf(stderr, L"Skipping too long path: '%s'\n", cCurrentPath);
            continue;
        }

        wcscpy(pCurrentPathPtr, argv[0]);

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

            if (_countof(cCurrentPath) - (pCurrentPathPtr - cCurrentPath) <
                wcslen(found.cFileName) + 2)
            {
                fwprintf(stderr, L"Skipping too long name: '%s'\n",
                    found.cFileName);

                continue;
            }

            wcscpy(pCurrentPathPtr, found.cFileName);

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
wmain(int argc, LPWSTR *argv)
{
    // Nice argument parse loop :)
    while (argv[1] != NULL && argv[1][0] != 0 && (argv[1][0] | 0x02) == '/')
    {
        while ((++argv[1])[0] != 0)
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
                    LPWSTR szSizeSuffix = NULL;
                    dwSkipSize = wcstoul(argv[1] + 2, &szSizeSuffix, 0);
                    if (szSizeSuffix == argv[1] + 2)
                    {
                        fwprintf(stderr,
                            L"Invalid size: %s\r\n", szSizeSuffix);

                        return -1;
                    }

                    if (szSizeSuffix[0] ? szSizeSuffix[1] != 0 : false)
                    {
                        fwprintf(stderr,
                            L"Invalid size suffix: %s\r\n", szSizeSuffix);

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

                    argv[1] += wcslen(argv[1]) - 1;
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

                szExcludeStrings = wcstok(argv[1] + 2, L",");
                dwExcludeStrings = 1;
                
                while (strtok(NULL, ","))
                    dwExcludeStrings++;
                
                argv[1] += wcslen(argv[1]) - 1;
                break;
            default:
                usage();
            }

        --argc;
        ++argv;
    }

    if (bForceDelete && !(bDeleteEqual || bLinkEqual))
        usage();

    LPWSTR fakeargv[3] = { 0 };
    if (argv[1] == NULL)
    {
        fakeargv[1] = L"*";
        fakeargv[2] = NULL;
        patterns = fakeargv;
    }
    else
        patterns = argv;

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
            TO_pA(qwSavedSize));

        if (qwDupSize)
            printf("%.f duplicate file%s left occupying %.4g %s.\n",
                (double)(qwDupFiles - qwDeletedFiles),
                (qwDupFiles - qwDeletedFiles) == 1 ? "" : "s", TO_h(qwDupSize),
                TO_pA(qwDupSize));
    }
    else
    {
        if (qwDupSize)
            printf("%.f duplicate file%s occupying %.4g %s.\n", (double)qwDupFiles,
                qwDupFiles == 1 ? "" : "s", TO_h(qwDupSize), TO_pA(qwDupSize));
        else
            puts("No duplicate files found.");
    }

    return 0;
}
