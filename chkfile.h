#ifndef EXTERN_C
#ifdef _cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif
#endif

enum FILE_COMPARE_STATUS
{
  FILE_COMPARE_EQUAL,
  FILE_COMPARE_FILE1_FAILED,
  FILE_COMPARE_FILE2_FAILED,
  FILE_COMPARE_FILE1_LONGER,
  FILE_COMPARE_FILE2_LONGER,
  FILE_COMPARE_NOT_EQUAL
};

EXTERN_C BOOL GetFileCheckSum(HANDLE hFile, DWORD dwSkupSize, PULARGE_INTEGER lpChecksum,
    volatile BOOL *bBreak);

EXTERN_C INT CompareFiles(LPCWSTR szFile1, LPCWSTR szFile2, DWORD dwSkipSize, volatile BOOL *bBreak);

#ifdef _M_IX86

__forceinline
BOOL
SetFilePointerExInternal(
    HANDLE hFile,
    LARGE_INTEGER liDistanceToMove,
    PLARGE_INTEGER lpNewFilePointer,
    DWORD dwMoveMethod
)
{
    LONG high = liDistanceToMove.HighPart;
    DWORD result = SetFilePointer(hFile, liDistanceToMove.LowPart, &high, dwMoveMethod);

    if (result == INVALID_SET_FILE_POINTER
        && GetLastError() != NO_ERROR)
    {
        return FALSE;
    }

    if (lpNewFilePointer != NULL)
    {
        lpNewFilePointer->LowPart = result;
        lpNewFilePointer->HighPart = high;
    }

    return TRUE;
}

#define SetFilePointerEx SetFilePointerExInternal

#endif
