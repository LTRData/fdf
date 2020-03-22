#ifndef EXTERN_C
#ifdef _cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif
#endif

typedef int HANDLE;
typedef int SOCKET;

#define INVALID_HANDLE_VALUE (-1)
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)

typedef union _ULARGE_INTEGER
{
  unsigned long long QuadPart;
  struct
  {
    unsigned long LowPart;
    unsigned long HighPart;
  };
} ULARGE_INTEGER, *PULARGE_INTEGER, *LPULARGE_INTEGER;

typedef const char *LPCSTR;
typedef char *LPSTR;
typedef const wchar_t *LPCWSTR;
typedef wchar_t *LPWSTR;
#ifdef UNICODE
typedef LPCWSTR LPCTSTR;
typedef LPWSTR LPTSTR;
#else
typedef LPCSTR LPCTSTR;
typedef LPSTR LPTSTR;
#endif
typedef void VOID, *PVOID, *LPVOID;
typedef unsigned char BYTE, *LPBYTE;
typedef unsigned long DWORD;
typedef unsigned long long QWORD, DWORDLONG, *PDWORDLONG, *LPDWORDLONG;
typedef long long LONGLONG, *PLONGLONG, *LPLONGLONG;
typedef unsigned short WORD;
typedef short SHORT;
typedef int INT;
typedef char *LPSTR;

#ifndef _cplusplus
typedef int BOOL;
#define FALSE 0
#define TRUE 1
#endif

#define CloseHandle(h) close(h)
#define closesocket(s) close(s)
#define Sleep(n) usleep(n)
#define ZeroMemory(m,s) bzero(m,s)
#define FillMemory(m,s,f) memset(m,f,s)
#define strnicmp(a,b,n) strncasecmp(a,b,n)
#define clreol()
