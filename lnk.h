#ifdef __cplusplus
extern "C" {
#endif

BOOL
CreateHardLinkToFile(LPCWSTR lpExisting, LPCWSTR lpNew, BOOL bReplaceOk);

BOOL
CreateHardLinkToOpenFile(HANDLE hFile, LPCWSTR lpNew, BOOL bReplaceOk);

#ifdef __cplusplus
}
#endif

