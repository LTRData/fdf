#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#define WIN32_NO_STATUS
#include <winstrct.h>
#include <ntdll.h>

#include "lnk.h"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ntdll.lib")

BOOL
CreateHardLinkToOpenFile(HANDLE hFile, LPCWSTR lpTarget, BOOL bReplaceOk)
{
    UNICODE_STRING Target;
    PFILE_LINK_INFORMATION FileLinkData;
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatusBlock;

    if (!RtlDosPathNameToNtPathName_U(lpTarget, &Target, NULL, NULL))
    {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }

    FileLinkData = (PFILE_LINK_INFORMATION)halloc(Target.Length + sizeof(FILE_LINK_INFORMATION) -
        sizeof(FileLinkData->FileName));
    if (FileLinkData == NULL)
        return FALSE;

    FileLinkData->ReplaceIfExists = (BOOLEAN)bReplaceOk;
    FileLinkData->RootDirectory = NULL;
    FileLinkData->FileNameLength = Target.Length;
    memcpy(FileLinkData->FileName, Target.Buffer, Target.Length);

    Status = NtSetInformationFile(hFile, &IoStatusBlock, FileLinkData,
        Target.Length +
        sizeof(FILE_LINK_INFORMATION) -
        sizeof(FileLinkData->FileName),
        FileLinkInformation);

    RtlFreeUnicodeString(&Target);
    hfree(FileLinkData);

    if (NT_SUCCESS(Status))
        return TRUE;
    else
    {
        SetLastError(RtlNtStatusToDosError(Status));
        return FALSE;
    }
}

BOOL
CreateHardLinkToFile(LPCWSTR lpExisting, LPCWSTR lpNew, BOOL bReplaceOk)
{
    HANDLE hFile;
    DWORD dwLastError = NO_ERROR;
    BOOL bResult;

    hFile = CreateFile(lpExisting, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    bResult = CreateHardLinkToOpenFile(hFile, lpNew, bReplaceOk);

    if (!bResult)
        dwLastError = GetLastError();

    CloseHandle(hFile);

    if (!bResult)
        SetLastError(dwLastError);

    return bResult;
}
