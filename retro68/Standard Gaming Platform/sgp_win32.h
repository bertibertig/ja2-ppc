#ifndef SGP_WIN32_H
#define SGP_WIN32_H

/* JA2 PPC: the subset of the Win32 API that the original Standard Gaming
 * Platform files call, implemented on stdio plus the Mac File Manager
 * (sgp_win32.c). The original SGP sources include this header instead of
 * <windows.h> under #ifdef JA2_PPC, so their logic stays unchanged.
 *
 * Paths keep their Windows form in game code ("Data\\Laptop.slf",
 * "..\\SavedGames\\*.sav"). sgp_win32.c maps them below a virtual root:
 * on the Mac the folder the application runs in (HFS paths ":Data:..."),
 * in host tests the directory named by the SGP_ROOT environment variable.
 * HFS compares names case-insensitively, as Windows does.
 *
 * Without macintosh defined (host tests) the same file builds on POSIX.
 */

#include <stddef.h>
#include <stdarg.h>

typedef void *HANDLE;
typedef void *HWND;
typedef void *HINSTANCE;
/* Win32's DWORD/LONG are 32 bits; the original code passes UINT32/INT32
   pointers where the API takes LPDWORD/PLONG, so use the same types. */
typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef int BOOL;
typedef int LONG;
typedef char *LPSTR;
typedef const char *LPCSTR;
typedef void *LPVOID;
typedef unsigned int UINT;
typedef DWORD *LPDWORD;
typedef LONG *PLONG;
typedef int INT;
typedef char CHAR;
typedef unsigned char *LPBYTE;
typedef void *HKEY;

typedef struct {
    LONG x;
    LONG y;
} POINT, *LPPOINT;

typedef struct {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME;

#define MAX_PATH 260
#define _MAX_PATH MAX_PATH

typedef struct {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    CHAR cFileName[MAX_PATH];
    CHAR cAlternateFileName[14];
} WIN32_FIND_DATA;

typedef struct {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    DWORD dwTotalPhys;
    DWORD dwAvailPhys;
    DWORD dwTotalPageFile;
    DWORD dwAvailPageFile;
    DWORD dwTotalVirtual;
    DWORD dwAvailVirtual;
} MEMORYSTATUS;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define WINAPI
#define _cdecl
#define __cdecl

#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)
#define INVALID_FILE_SIZE ((DWORD)0xffffffffUL)
#define HFILE_ERROR (-1)

#define GENERIC_READ 0x80000000UL
#define GENERIC_WRITE 0x40000000UL
#define FILE_SHARE_READ 0x00000001UL
#define FILE_SHARE_WRITE 0x00000002UL

#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5

#define FILE_ATTRIBUTE_READONLY 0x00000001UL
#define FILE_ATTRIBUTE_HIDDEN 0x00000002UL
#define FILE_ATTRIBUTE_SYSTEM 0x00000004UL
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010UL
#define FILE_ATTRIBUTE_ARCHIVE 0x00000020UL
#define FILE_ATTRIBUTE_NORMAL 0x00000080UL
#define FILE_ATTRIBUTE_TEMPORARY 0x00000100UL
#define FILE_ATTRIBUTE_COMPRESSED 0x00000800UL
#define FILE_ATTRIBUTE_OFFLINE 0x00001000UL
#define FILE_FLAG_RANDOM_ACCESS 0x10000000UL
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000UL
#define FILE_FLAG_DELETE_ON_CLOSE 0x04000000UL

#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2

#define FORMAT_MESSAGE_FROM_SYSTEM 0x00001000UL

#define ERROR_SUCCESS 0
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_ACCESS_DENIED 5
#define ERROR_INVALID_HANDLE 6
#define ERROR_NO_MORE_FILES 18
#define ERROR_FILE_EXISTS 80
#define ERROR_ALREADY_EXISTS 183

#define MB_OK 0

/* The Mac shell hides the system cursor itself (SDL) and owns the only
   window, so these window calls do nothing. */
#define ShowCursor( fShow ) ( (void)( fShow ), 0 )
#define ShowWindow( hWindow, iCommand ) ( (void)( hWindow ), (void)( iCommand ), 0 )
#define SW_HIDE 0
#define MB_TASKMODAL 0x00002000L
#define MB_ICONERROR 0x00000010L

/* The Mac build is single-threaded (no refresh thread), so the Mutex
   Manager's critical sections are no-ops. */
typedef struct
{
    int iUnused;
} CRITICAL_SECTION;
#define InitializeCriticalSection( p ) ( (void)( p ) )
#define DeleteCriticalSection( p ) ( (void)( p ) )
#define EnterCriticalSection( p ) ( (void)( p ) )
#define LeaveCriticalSection( p ) ( (void)( p ) )

#ifdef __cplusplus
extern "C" {
#endif

HANDLE CreateFile(LPCSTR name, DWORD access, DWORD share, void *security,
                  DWORD creation, DWORD flags, HANDLE templateFile);
BOOL ReadFile(HANDLE file, void *buffer, DWORD count, LPDWORD bytesRead, void *overlapped);
BOOL WriteFile(HANDLE file, const void *buffer, DWORD count, LPDWORD bytesWritten,
               void *overlapped);
DWORD SetFilePointer(HANDLE file, LONG distance, PLONG distanceHigh, DWORD method);
DWORD GetFileSize(HANDLE file, LPDWORD sizeHigh);
BOOL GetFileTime(HANDLE file, FILETIME *creation, FILETIME *access, FILETIME *write);
BOOL CloseHandle(HANDLE handle);

BOOL DeleteFile(LPCSTR name);
BOOL CopyFile(LPCSTR source, LPCSTR destination, BOOL failIfExists);
BOOL MoveFile(LPCSTR source, LPCSTR destination);
DWORD GetFileAttributes(LPCSTR name);
BOOL SetFileAttributes(LPCSTR name, DWORD attributes);
BOOL CreateDirectory(LPCSTR name, void *security);
BOOL RemoveDirectory(LPCSTR name);
BOOL SetCurrentDirectory(LPCSTR name);
DWORD GetCurrentDirectory(DWORD length, LPSTR buffer);
char *_getcwd(char *buffer, int length);
DWORD GetModuleFileName(HINSTANCE module, LPSTR buffer, DWORD length);

HANDLE FindFirstFile(LPCSTR pattern, WIN32_FIND_DATA *data);
BOOL FindNextFile(HANDLE find, WIN32_FIND_DATA *data);
BOOL FindClose(HANDLE find);

BOOL FileTimeToLocalFileTime(const FILETIME *fileTime, FILETIME *localTime);
LONG CompareFileTime(const FILETIME *first, const FILETIME *second);
BOOL GetDiskFreeSpace(LPCSTR root, LPDWORD sectorsPerCluster, LPDWORD bytesPerSector,
                      LPDWORD freeClusters, LPDWORD totalClusters);

DWORD GetEnvironmentVariable(LPCSTR name, LPSTR buffer, DWORD size);
BOOL SetEnvironmentVariable(LPCSTR name, LPCSTR value);
LPSTR CharLower(LPSTR text);

DWORD GetLastError(void);
DWORD FormatMessage(DWORD flags, const void *source, DWORD messageId, DWORD languageId,
                    LPSTR buffer, DWORD size, va_list *arguments);
void OutputDebugString(LPCSTR text);
int MessageBox(HWND window, LPCSTR text, LPCSTR caption, UINT type);
DWORD GetTickCount(void);
/* Mouse position in window coordinates; the shell installs the source. */
typedef void (*SGPCursorPosProc)(LONG *x, LONG *y);
extern SGPCursorPosProc gpSGPGetCursorPos;
BOOL GetCursorPos(LPPOINT point);
void GlobalMemoryStatus(MEMORYSTATUS *status);

int _stricmp(const char *left, const char *right);
int _strnicmp(const char *left, const char *right, size_t count);
char *_strupr(char *text);
char *_strlwr(char *text);
void _splitpath(const char *path, char *drive, char *directory, char *name, char *extension);

#define stricmp _stricmp
#define strnicmp _strnicmp
#define strcmpi _stricmp
#define _strcmpi _stricmp
#define strupr _strupr
#define strlwr _strlwr

/* Port helper: translates a Windows-style game path into a host path
   (HFS on the Mac). Returns 0 when the path cannot be represented. */
int SGPTranslatePath(LPCSTR windowsPath, char *output, size_t outputSize);

#ifdef __cplusplus
}
#endif

#endif
