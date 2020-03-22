#define WIN32_LEAN_AND_MEAN
#define WIN32_NO_STATUS
#include <winstrct.h>
#include <ntdll.h>

#include "lnk.h"

#pragma comment(lib, "advapi32.lib")

enum
  {
    LnkLibNotInit,
    LnkLibInitFailed,
    LnkLibInitOk
  } LnkLibInitStatus;

typedef ULONG 
(NTAPI
 *tRtlNtStatusToDosError)
  (
   NTSTATUS status 
   );
tRtlNtStatusToDosError pRtlNtStatusToDosError = NULL;

typedef VOID 
(NTAPI 
 *tRtlFreeUnicodeString)
  ( 
   IN PUNICODE_STRING UnicodeString 
   );
tRtlFreeUnicodeString pRtlFreeUnicodeString = NULL;

typedef NTSTATUS
(NTAPI
 *tNtSetInformationFile)
  (
   IN HANDLE               FileHandle,
   OUT PIO_STATUS_BLOCK    IoStatusBlock,
   IN PVOID                FileInformation,
   IN ULONG                Length,
   IN FILE_INFORMATION_CLASS FileInformationClass
   );
tNtSetInformationFile pNtSetInformationFile = NULL;

typedef BOOLEAN 
(NTAPI 
 *tRtlDosPathNameToNtPathName_U)
  ( 
   IN PCWSTR DosName, 
   OUT PUNICODE_STRING NtName, 
   OUT PCWSTR *DosFilePath OPTIONAL, 
   OUT PUNICODE_STRING NtFilePath OPTIONAL 
   );
tRtlDosPathNameToNtPathName_U pRtlDosPathNameToNtPathName_U = NULL;

BOOL
LnkLibInit()
{
  HMODULE hModule = GetModuleHandle("NTDLL.DLL");

  LnkLibInitStatus = LnkLibInitFailed;

  if (hModule == NULL)
    return FALSE;

  pRtlNtStatusToDosError = (tRtlNtStatusToDosError)
    GetProcAddress(hModule, "RtlNtStatusToDosError");
  if (pRtlNtStatusToDosError == NULL)
    return FALSE;

  pRtlFreeUnicodeString = (tRtlFreeUnicodeString)
    GetProcAddress(hModule, "RtlFreeUnicodeString");
  if (pRtlFreeUnicodeString == NULL)
    return FALSE;

  pNtSetInformationFile = (tNtSetInformationFile)
    GetProcAddress(hModule, "NtSetInformationFile");
  if (pNtSetInformationFile == NULL)
    return FALSE;

  pRtlDosPathNameToNtPathName_U = (tRtlDosPathNameToNtPathName_U)
    GetProcAddress(hModule, "RtlDosPathNameToNtPathName_U");
  if (pRtlDosPathNameToNtPathName_U == NULL)
    return FALSE;

  LnkLibInitStatus = LnkLibInitOk;
  return TRUE;
}

BOOL
CreateHardLinkToOpenFile(HANDLE hFile, LPCWSTR lpTarget, BOOL bReplaceOk)
{
  UNICODE_STRING Target;
  PFILE_LINK_INFORMATION FileLinkData;
  NTSTATUS Status;
  IO_STATUS_BLOCK IoStatusBlock;

  if (!pRtlDosPathNameToNtPathName_U(lpTarget, &Target, NULL, NULL))
    {
      SetLastError(ERROR_PATH_NOT_FOUND);
      return FALSE;
    }

  FileLinkData = halloc(Target.Length + sizeof(FILE_LINK_INFORMATION) -
			sizeof(FileLinkData->FileName));
  if (FileLinkData == NULL)
    return FALSE;

  FileLinkData->ReplaceIfExists = (BOOLEAN) bReplaceOk;
  FileLinkData->RootDirectory = NULL;
  FileLinkData->FileNameLength = Target.Length;
  memcpy(FileLinkData->FileName, Target.Buffer, Target.Length);

  Status = pNtSetInformationFile(hFile, &IoStatusBlock, FileLinkData,
				 Target.Length +
				 sizeof(FILE_LINK_INFORMATION) -
				 sizeof(FileLinkData->FileName),
				 FileLinkInformation);

  pRtlFreeUnicodeString(&Target);
  hfree(FileLinkData);

  if (NT_SUCCESS(Status))
    return TRUE;
  else
    {
      SetLastError(pRtlNtStatusToDosError(Status));
      return FALSE;
    }
}

BOOL
CreateHardLinkToFile(LPCSTR lpExisting, LPCSTR lpNew, BOOL bReplaceOk)
{
  HANDLE hFile;
  LPWSTR wczNew;
  DWORD dwLastError = NO_ERROR;
  BOOL bResult;

  if (LnkLibInitStatus == LnkLibInitFailed)
    {
      SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
      return FALSE;
    }

  if (LnkLibInitStatus == LnkLibNotInit)
    if (!LnkLibInit())
      {
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
      }

  if (AreFileApisANSI())
    wczNew = ByteToWideAlloc(lpNew);
  else
    wczNew = OemToWideAlloc(lpNew);

  if (wczNew == NULL)
    return FALSE;

  hFile = CreateFile(lpExisting, 0,
		     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		     NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (hFile == INVALID_HANDLE_VALUE)
    {
      hfree(wczNew);
      return FALSE;
    }

  bResult = CreateHardLinkToOpenFile(hFile, wczNew, bReplaceOk);

  if (!bResult)
    dwLastError = GetLastError();

  hfree(wczNew);
  CloseHandle(hFile);

  if (!bResult)
    SetLastError(dwLastError);

  return bResult;
}
