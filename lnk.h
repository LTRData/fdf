#ifdef __cplusplus
extern "C" {
#endif

BOOL
CreateHardLinkToFile(LPCSTR lpExisting, LPCSTR lpNew, BOOL bReplaceOk);

BOOL
CreateHardLinkToOpenFile(HANDLE hFile, LPCWSTR lpNew, BOOL bReplaceOk);

#ifdef __cplusplus
}
#endif

