#include "sgp_win32.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(macintosh)
#include <Files.h>
#include <Memory.h>
#include <Script.h>
#include <Timer.h>
void DebugLog(const char *format, ...);
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#endif

#define SGP_MAX_COMPONENTS 32
#define SGP_COMPONENT_LENGTH 64
#define SGP_HOST_PATH 512
/* Same buffer as Data/*.slf reads in ja2_slf.c: Retro68's stdio otherwise
   refills 1 KB per File Manager call. */
#define SGP_STDIO_BUFFER (64 * 1024)

typedef struct {
    FILE *file;
} SGPFile;

typedef struct {
    char components[SGP_MAX_COMPONENTS][SGP_COMPONENT_LENGTH];
    int count;
} SGPPath;

static SGPPath gCurrentDirectory;
static DWORD gLastError;

/* ------------------------------------------------------------------------
   MSVC runtime rand() (see sgp_platform.h) */

static unsigned long gRandSeed = 1;

int SGPRand(void)
{
    gRandSeed = gRandSeed * 214013UL + 2531011UL;
    return (int)((gRandSeed >> 16) & 0x7fff);
}

void SGPSrand(unsigned int seed)
{
    gRandSeed = seed;
}

/* ------------------------------------------------------------------------
   Strings */

int _stricmp(const char *left, const char *right)
{
    while (*left && tolower((unsigned char)*left) == tolower((unsigned char)*right)) {
        ++left;
        ++right;
    }
    return tolower((unsigned char)*left) - tolower((unsigned char)*right);
}

int _strnicmp(const char *left, const char *right, size_t count)
{
    while (count > 0 && *left &&
           tolower((unsigned char)*left) == tolower((unsigned char)*right)) {
        ++left;
        ++right;
        --count;
    }
    return count == 0 ? 0 : tolower((unsigned char)*left) - tolower((unsigned char)*right);
}

char *_strupr(char *text)
{
    char *cursor;

    for (cursor = text; *cursor; ++cursor) {
        *cursor = (char)toupper((unsigned char)*cursor);
    }
    return text;
}

char *_strlwr(char *text)
{
    char *cursor;

    for (cursor = text; *cursor; ++cursor) {
        *cursor = (char)tolower((unsigned char)*cursor);
    }
    return text;
}

void _splitpath(const char *path, char *drive, char *directory, char *name, char *extension)
{
    const char *start = path;
    const char *slash = NULL;
    const char *dot = NULL;
    const char *cursor;

    if (path[0] && path[1] == ':') {
        if (drive) {
            drive[0] = path[0];
            drive[1] = ':';
            drive[2] = 0;
        }
        start += 2;
    } else if (drive) {
        drive[0] = 0;
    }
    for (cursor = start; *cursor; ++cursor) {
        if (*cursor == '\\' || *cursor == '/') {
            slash = cursor;
            dot = NULL;
        } else if (*cursor == '.') {
            dot = cursor;
        }
    }
    if (directory) {
        size_t length = slash ? (size_t)(slash - start + 1) : 0;

        memcpy(directory, start, length);
        directory[length] = 0;
    }
    start = slash ? slash + 1 : start;
    if (!dot) {
        dot = cursor;
    }
    if (name) {
        memcpy(name, start, (size_t)(dot - start));
        name[dot - start] = 0;
    }
    if (extension) {
        strcpy(extension, dot);
    }
}

/* ------------------------------------------------------------------------
   Paths: Windows path -> component list below the virtual root */

static int ParsePath(LPCSTR windowsPath, SGPPath *path)
{
    const char *cursor = windowsPath;

    if (cursor[0] && cursor[1] == ':') {
        /* Drive letters name the virtual root. */
        cursor += 2;
        path->count = 0;
    } else if (*cursor == '\\' || *cursor == '/') {
        path->count = 0;
    } else {
        *path = gCurrentDirectory;
    }
    while (*cursor) {
        char component[SGP_COMPONENT_LENGTH];
        size_t length = 0;

        while (*cursor == '\\' || *cursor == '/') {
            ++cursor;
        }
        while (*cursor && *cursor != '\\' && *cursor != '/') {
            if (length + 1 >= sizeof(component)) {
                return 0;
            }
            component[length++] = *cursor++;
        }
        component[length] = 0;
        if (length == 0 || strcmp(component, ".") == 0) {
            continue;
        }
        if (strcmp(component, "..") == 0) {
            if (path->count > 0) {
                --path->count;
            }
            continue;
        }
        if (path->count >= SGP_MAX_COMPONENTS) {
            return 0;
        }
        strcpy(path->components[path->count++], component);
    }
    return 1;
}

static void FormatWindowsPath(const SGPPath *path, char *output, size_t outputSize)
{
    int index;

    output[0] = 0;
    for (index = 0; index < path->count; ++index) {
        if (strlen(output) + strlen(path->components[index]) + 2 >= outputSize) {
            break;
        }
        strcat(output, "\\");
        strcat(output, path->components[index]);
    }
    if (output[0] == 0 && outputSize > 1) {
        strcpy(output, "\\");
    }
}

#if defined(macintosh)

static int HostPath(const SGPPath *path, char *output, size_t outputSize)
{
    int index;
    size_t length = 1;

    output[0] = ':';
    output[1] = 0;
    for (index = 0; index < path->count; ++index) {
        size_t part = strlen(path->components[index]);

        if (length + part + 2 >= outputSize || part > 31) {
            return 0;
        }
        if (index > 0) {
            output[length++] = ':';
        }
        memcpy(output + length, path->components[index], part);
        length += part;
        output[length] = 0;
    }
    return 1;
}

#else

static int HostPath(const SGPPath *path, char *output, size_t outputSize)
{
    const char *root = getenv("SGP_ROOT");
    int index;

    snprintf(output, outputSize, "%s", root && *root ? root : ".");
    for (index = 0; index < path->count; ++index) {
        char candidate[SGP_HOST_PATH];
        DIR *directory;
        struct dirent *entry;

        snprintf(candidate, sizeof(candidate), "%s/%s", output, path->components[index]);
        if (access(candidate, F_OK) != 0 && (directory = opendir(output)) != NULL) {
            /* Host file systems are case-sensitive; match like HFS. */
            while ((entry = readdir(directory)) != NULL) {
                if (_stricmp(entry->d_name, path->components[index]) == 0) {
                    snprintf(candidate, sizeof(candidate), "%s/%s", output, entry->d_name);
                    break;
                }
            }
            closedir(directory);
        }
        if (strlen(candidate) + 1 >= outputSize) {
            return 0;
        }
        strcpy(output, candidate);
    }
    return 1;
}

#endif

int SGPTranslatePath(LPCSTR windowsPath, char *output, size_t outputSize)
{
    SGPPath path;

    return ParsePath(windowsPath, &path) && HostPath(&path, output, outputSize);
}

FILE *SGPFOpen(const char *windowsPath, const char *mode)
{
    char hostPath[SGP_HOST_PATH];

    if (!SGPTranslatePath(windowsPath, hostPath, sizeof(hostPath))) {
        return NULL;
    }
    return fopen(hostPath, mode);
}

/* ------------------------------------------------------------------------
   Files */

static int Exists(const char *hostPath)
{
    FILE *file = fopen(hostPath, "rb");

    if (file != NULL) {
        fclose(file);
        return 1;
    }
    return 0;
}

HANDLE CreateFile(LPCSTR name, DWORD access, DWORD share, void *security,
                  DWORD creation, DWORD flags, HANDLE templateFile)
{
    char hostPath[SGP_HOST_PATH];
    const char *mode;
    SGPFile *handle;
    FILE *file;
    int exists;

    (void)share;
    (void)security;
    (void)flags;
    (void)templateFile;
    if (!SGPTranslatePath(name, hostPath, sizeof(hostPath))) {
        gLastError = ERROR_PATH_NOT_FOUND;
        return INVALID_HANDLE_VALUE;
    }
    exists = Exists(hostPath);
    switch (creation) {
    case CREATE_NEW:
        if (exists) {
            gLastError = ERROR_FILE_EXISTS;
            return INVALID_HANDLE_VALUE;
        }
        mode = "w+b";
        break;
    case CREATE_ALWAYS:
        mode = "w+b";
        break;
    case OPEN_ALWAYS:
        mode = exists ? ((access & GENERIC_WRITE) ? "r+b" : "rb") : "w+b";
        break;
    case TRUNCATE_EXISTING:
        if (!exists) {
            gLastError = ERROR_FILE_NOT_FOUND;
            return INVALID_HANDLE_VALUE;
        }
        mode = "w+b";
        break;
    case OPEN_EXISTING:
    default:
        if (!exists) {
            gLastError = ERROR_FILE_NOT_FOUND;
            return INVALID_HANDLE_VALUE;
        }
        mode = (access & GENERIC_WRITE) ? "r+b" : "rb";
        break;
    }
    file = fopen(hostPath, mode);
    if (file == NULL) {
        gLastError = ERROR_ACCESS_DENIED;
        return INVALID_HANDLE_VALUE;
    }
    setvbuf(file, NULL, _IOFBF, SGP_STDIO_BUFFER);
    handle = (SGPFile *)malloc(sizeof(*handle));
    if (handle == NULL) {
        fclose(file);
        return INVALID_HANDLE_VALUE;
    }
    handle->file = file;
    gLastError = ERROR_SUCCESS;
    return (HANDLE)handle;
}

static SGPFile *FileFromHandle(HANDLE handle)
{
    if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
        gLastError = ERROR_INVALID_HANDLE;
        return NULL;
    }
    return (SGPFile *)handle;
}

BOOL ReadFile(HANDLE handle, void *buffer, DWORD count, LPDWORD bytesRead, void *overlapped)
{
    SGPFile *file = FileFromHandle(handle);
    size_t done;

    (void)overlapped;
    if (file == NULL) {
        return FALSE;
    }
    done = fread(buffer, 1, count, file->file);
    if (bytesRead) {
        *bytesRead = (DWORD)done;
    }
    /* Like Win32, reading at end of file succeeds with fewer bytes. */
    return done == count || !ferror(file->file);
}

BOOL WriteFile(HANDLE handle, const void *buffer, DWORD count, LPDWORD bytesWritten,
               void *overlapped)
{
    SGPFile *file = FileFromHandle(handle);
    size_t done;

    (void)overlapped;
    if (file == NULL) {
        return FALSE;
    }
    done = fwrite(buffer, 1, count, file->file);
    if (bytesWritten) {
        *bytesWritten = (DWORD)done;
    }
    return done == count;
}

DWORD SetFilePointer(HANDLE handle, LONG distance, PLONG distanceHigh, DWORD method)
{
    SGPFile *file = FileFromHandle(handle);
    int whence = method == FILE_CURRENT ? SEEK_CUR : method == FILE_END ? SEEK_END : SEEK_SET;

    if (distanceHigh) {
        *distanceHigh = 0;
    }
    if (file == NULL || fseek(file->file, distance, whence) != 0) {
        return INVALID_FILE_SIZE;
    }
    return (DWORD)ftell(file->file);
}

DWORD GetFileSize(HANDLE handle, LPDWORD sizeHigh)
{
    SGPFile *file = FileFromHandle(handle);
    long position;
    long size;

    if (sizeHigh) {
        *sizeHigh = 0;
    }
    if (file == NULL) {
        return INVALID_FILE_SIZE;
    }
    position = ftell(file->file);
    fseek(file->file, 0, SEEK_END);
    size = ftell(file->file);
    fseek(file->file, position, SEEK_SET);
    return (DWORD)size;
}

BOOL CloseHandle(HANDLE handle)
{
    SGPFile *file = FileFromHandle(handle);

    if (file == NULL) {
        return FALSE;
    }
    fclose(file->file);
    free(file);
    return TRUE;
}

BOOL CopyFile(LPCSTR source, LPCSTR destination, BOOL failIfExists)
{
    char from[SGP_HOST_PATH];
    char to[SGP_HOST_PATH];
    char buffer[4096];
    FILE *input;
    FILE *output;
    size_t count;
    BOOL ok = TRUE;

    if (!SGPTranslatePath(source, from, sizeof(from)) ||
        !SGPTranslatePath(destination, to, sizeof(to))) {
        return FALSE;
    }
    if (failIfExists && Exists(to)) {
        gLastError = ERROR_FILE_EXISTS;
        return FALSE;
    }
    input = fopen(from, "rb");
    if (input == NULL) {
        gLastError = ERROR_FILE_NOT_FOUND;
        return FALSE;
    }
    output = fopen(to, "wb");
    if (output == NULL) {
        fclose(input);
        gLastError = ERROR_ACCESS_DENIED;
        return FALSE;
    }
    while ((count = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, count, output) != count) {
            ok = FALSE;
            break;
        }
    }
    fclose(input);
    fclose(output);
    return ok;
}

/* ------------------------------------------------------------------------
   Time */

/* FILETIME counts 100 ns units since 1601-01-01. */
static void SecondsToFileTime(unsigned long long secondsSince1601, FILETIME *time)
{
    unsigned long long ticks = secondsSince1601 * 10000000ULL;

    time->dwLowDateTime = (DWORD)(ticks & 0xffffffffUL);
    time->dwHighDateTime = (DWORD)(ticks >> 32);
}

BOOL FileTimeToLocalFileTime(const FILETIME *fileTime, FILETIME *localTime)
{
    /* Mac OS file dates are already local time. */
    *localTime = *fileTime;
    return TRUE;
}

LONG CompareFileTime(const FILETIME *first, const FILETIME *second)
{
    if (first->dwHighDateTime != second->dwHighDateTime) {
        return first->dwHighDateTime < second->dwHighDateTime ? -1 : 1;
    }
    if (first->dwLowDateTime != second->dwLowDateTime) {
        return first->dwLowDateTime < second->dwLowDateTime ? -1 : 1;
    }
    return 0;
}

/* ------------------------------------------------------------------------
   Directories, attributes, find: File Manager on the Mac, POSIX on hosts */

typedef struct {
    char directory[SGP_HOST_PATH];
    char pattern[SGP_COMPONENT_LENGTH];
#if defined(macintosh)
    short vRefNum;
    long dirID;
    short index;
#else
    DIR *dir;
#endif
} SGPFind;

static int WildcardMatch(const char *pattern, const char *name)
{
    if (*pattern == 0) {
        return *name == 0;
    }
    if (*pattern == '*') {
        /* "*.*" matches names without a dot too, as on Windows. */
        if (strcmp(pattern, "*.*") == 0) {
            return 1;
        }
        do {
            if (WildcardMatch(pattern + 1, name)) {
                return 1;
            }
        } while (*name++);
        return 0;
    }
    if (*name == 0) {
        return 0;
    }
    if (*pattern == '?' || tolower((unsigned char)*pattern) == tolower((unsigned char)*name)) {
        return WildcardMatch(pattern + 1, name + 1);
    }
    return 0;
}

static int SplitPattern(LPCSTR pattern, SGPPath *directory, char *filePattern)
{
    if (!ParsePath(pattern, directory) || directory->count == 0) {
        return 0;
    }
    --directory->count;
    strcpy(filePattern, directory->components[directory->count]);
    return 1;
}

#if defined(macintosh)

static void ToPascal(const char *text, Str255 pascalName)
{
    size_t length = strlen(text);

    if (length > 255) {
        length = 255;
    }
    pascalName[0] = (unsigned char)length;
    memcpy(pascalName + 1, text, length);
}

static OSErr MakeSpec(const char *hostPath, FSSpec *spec)
{
    Str255 pascalName;

    ToPascal(hostPath, pascalName);
    return FSMakeFSSpec(0, 0, pascalName, spec);
}

static OSErr CatInfo(const FSSpec *spec, CInfoPBRec *info)
{
    Str255 name;

    memcpy(name, spec->name, (size_t)spec->name[0] + 1);
    memset(info, 0, sizeof(*info));
    info->hFileInfo.ioNamePtr = name;
    info->hFileInfo.ioVRefNum = spec->vRefNum;
    info->hFileInfo.ioDirID = spec->parID;
    return PBGetCatInfoSync(info);
}

static void FillFindData(const CInfoPBRec *info, const char *name, WIN32_FIND_DATA *data)
{
    Boolean isDirectory = (info->hFileInfo.ioFlAttrib & ioDirMask) != 0;
    unsigned long modified = isDirectory ? info->dirInfo.ioDrMdDat : info->hFileInfo.ioFlMdDat;

    memset(data, 0, sizeof(*data));
    data->dwFileAttributes = isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    if (!isDirectory && (info->hFileInfo.ioFlAttrib & 0x01)) {
        data->dwFileAttributes = FILE_ATTRIBUTE_READONLY;
    }
    data->nFileSizeLow = isDirectory ? 0 : (DWORD)info->hFileInfo.ioFlLgLen;
    /* Mac dates count seconds since 1904-01-01; 1601 -> 1904 is 9561628800 s. */
    SecondsToFileTime((unsigned long long)modified + 9561628800ULL, &data->ftLastWriteTime);
    data->ftCreationTime = data->ftLastWriteTime;
    data->ftLastAccessTime = data->ftLastWriteTime;
    strncpy(data->cFileName, name, sizeof(data->cFileName) - 1);
}

DWORD GetFileAttributes(LPCSTR name)
{
    char hostPath[SGP_HOST_PATH];
    FSSpec spec;
    CInfoPBRec info;
    WIN32_FIND_DATA data;

    if (!SGPTranslatePath(name, hostPath, sizeof(hostPath)) || MakeSpec(hostPath, &spec) != noErr ||
        CatInfo(&spec, &info) != noErr) {
        gLastError = ERROR_FILE_NOT_FOUND;
        return INVALID_FILE_SIZE;
    }
    FillFindData(&info, "", &data);
    return data.dwFileAttributes;
}

BOOL SetFileAttributes(LPCSTR name, DWORD attributes)
{
    char hostPath[SGP_HOST_PATH];
    FSSpec spec;

    if (!SGPTranslatePath(name, hostPath, sizeof(hostPath)) || MakeSpec(hostPath, &spec) != noErr) {
        return FALSE;
    }
    if (attributes & FILE_ATTRIBUTE_READONLY) {
        return FSpSetFLock(&spec) == noErr;
    }
    FSpRstFLock(&spec);
    return TRUE;
}

BOOL GetFileTime(HANDLE handle, FILETIME *creation, FILETIME *access, FILETIME *write)
{
    /* stdio handles carry no catalog information; report the current
       time so comparisons stay well defined. Save games store their own
       time stamps, the original only uses this for editor tools. */
    unsigned long now;
    FILETIME time;

    if (FileFromHandle(handle) == NULL) {
        return FALSE;
    }
    GetDateTime(&now);
    SecondsToFileTime((unsigned long long)now + 9561628800ULL, &time);
    if (creation) *creation = time;
    if (access) *access = time;
    if (write) *write = time;
    return TRUE;
}

BOOL DeleteFile(LPCSTR name)
{
    char hostPath[SGP_HOST_PATH];
    FSSpec spec;

    if (!SGPTranslatePath(name, hostPath, sizeof(hostPath)) || MakeSpec(hostPath, &spec) != noErr ||
        FSpDelete(&spec) != noErr) {
        gLastError = ERROR_FILE_NOT_FOUND;
        return FALSE;
    }
    return TRUE;
}

BOOL MoveFile(LPCSTR source, LPCSTR destination)
{
    char from[SGP_HOST_PATH];
    char to[SGP_HOST_PATH];

    if (!SGPTranslatePath(source, from, sizeof(from)) ||
        !SGPTranslatePath(destination, to, sizeof(to))) {
        return FALSE;
    }
    if (rename(from, to) == 0) {
        return TRUE;
    }
    return CopyFile(source, destination, TRUE) && DeleteFile(source);
}

BOOL CreateDirectory(LPCSTR name, void *security)
{
    char hostPath[SGP_HOST_PATH];
    FSSpec spec;
    long newDirID;
    OSErr error;

    (void)security;
    if (!SGPTranslatePath(name, hostPath, sizeof(hostPath))) {
        return FALSE;
    }
    error = MakeSpec(hostPath, &spec);
    if (error == noErr) {
        gLastError = ERROR_ALREADY_EXISTS;
        return FALSE;
    }
    return error == fnfErr && FSpDirCreate(&spec, smSystemScript, &newDirID) == noErr;
}

BOOL RemoveDirectory(LPCSTR name)
{
    return DeleteFile(name);
}

HANDLE FindFirstFile(LPCSTR pattern, WIN32_FIND_DATA *data)
{
    SGPPath directory;
    SGPFind *find;
    FSSpec spec;
    CInfoPBRec info;

    find = (SGPFind *)calloc(1, sizeof(*find));
    if (find == NULL) {
        return INVALID_HANDLE_VALUE;
    }
    if (!SplitPattern(pattern, &directory, find->pattern) ||
        !HostPath(&directory, find->directory, sizeof(find->directory))) {
        free(find);
        gLastError = ERROR_PATH_NOT_FOUND;
        return INVALID_HANDLE_VALUE;
    }
    if (directory.count == 0) {
        /* The virtual root is the application's default directory. */
        WDPBRec working;

        memset(&working, 0, sizeof(working));
        if (PBHGetVolSync(&working) != noErr) {
            free(find);
            return INVALID_HANDLE_VALUE;
        }
        find->vRefNum = working.ioWDVRefNum;
        find->dirID = working.ioWDDirID;
    } else if (MakeSpec(find->directory, &spec) != noErr || CatInfo(&spec, &info) != noErr ||
               (info.hFileInfo.ioFlAttrib & ioDirMask) == 0) {
        free(find);
        gLastError = ERROR_PATH_NOT_FOUND;
        return INVALID_HANDLE_VALUE;
    } else {
        find->vRefNum = spec.vRefNum;
        find->dirID = info.dirInfo.ioDrDirID;
    }
    if (!FindNextFile((HANDLE)find, data)) {
        free(find);
        gLastError = ERROR_FILE_NOT_FOUND;
        return INVALID_HANDLE_VALUE;
    }
    return (HANDLE)find;
}

BOOL FindNextFile(HANDLE handle, WIN32_FIND_DATA *data)
{
    SGPFind *find = (SGPFind *)handle;

    if (find == NULL || handle == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    for (;;) {
        CInfoPBRec info;
        Str255 name;
        char cName[256];

        memset(&info, 0, sizeof(info));
        name[0] = 0;
        info.hFileInfo.ioNamePtr = name;
        info.hFileInfo.ioVRefNum = find->vRefNum;
        info.hFileInfo.ioDirID = find->dirID;
        info.hFileInfo.ioFDirIndex = ++find->index;
        if (PBGetCatInfoSync(&info) != noErr) {
            gLastError = ERROR_NO_MORE_FILES;
            return FALSE;
        }
        memcpy(cName, name + 1, name[0]);
        cName[name[0]] = 0;
        if (WildcardMatch(find->pattern, cName)) {
            FillFindData(&info, cName, data);
            return TRUE;
        }
    }
}

BOOL FindClose(HANDLE handle)
{
    if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    free(handle);
    return TRUE;
}

BOOL GetDiskFreeSpace(LPCSTR root, LPDWORD sectorsPerCluster, LPDWORD bytesPerSector,
                      LPDWORD freeClusters, LPDWORD totalClusters)
{
    HParamBlockRec volume;

    (void)root;
    memset(&volume, 0, sizeof(volume));
    volume.volumeParam.ioVolIndex = 0;
    if (PBHGetVInfoSync(&volume) != noErr) {
        return FALSE;
    }
    if (sectorsPerCluster) *sectorsPerCluster = 1;
    if (bytesPerSector) *bytesPerSector = (DWORD)volume.volumeParam.ioVAlBlkSiz;
    if (freeClusters) *freeClusters = (DWORD)volume.volumeParam.ioVFrBlk;
    if (totalClusters) *totalClusters = (DWORD)volume.volumeParam.ioVNmAlBlks;
    return TRUE;
}

DWORD GetTickCount(void)
{
    UnsignedWide now;

    Microseconds(&now);
    return (DWORD)((((unsigned long long)now.hi << 32) | now.lo) / 1000ULL);
}

void GlobalMemoryStatus(MEMORYSTATUS *status)
{
    memset(status, 0, sizeof(*status));
    status->dwLength = sizeof(*status);
    status->dwAvailPhys = (DWORD)FreeMem();
    status->dwTotalPhys = status->dwAvailPhys;
    status->dwAvailVirtual = status->dwAvailPhys;
    status->dwTotalVirtual = status->dwAvailPhys;
}

void OutputDebugString(LPCSTR text)
{
    DebugLog("SGP: %s", text);
}

int MessageBox(HWND window, LPCSTR text, LPCSTR caption, UINT type)
{
    (void)window;
    (void)type;
    DebugLog("SGP MessageBox: %s: %s", caption ? caption : "", text ? text : "");
    return 1;
}

#else /* POSIX host build for tests */

static void FillFindDataPosix(const char *path, const char *name, WIN32_FIND_DATA *data)
{
    struct stat info;

    memset(data, 0, sizeof(*data));
    if (stat(path, &info) == 0) {
        data->dwFileAttributes = S_ISDIR(info.st_mode) ? FILE_ATTRIBUTE_DIRECTORY
                                                       : FILE_ATTRIBUTE_NORMAL;
        data->nFileSizeLow = S_ISDIR(info.st_mode) ? 0 : (DWORD)info.st_size;
        /* 1601 -> 1970 is 11644473600 s. */
        SecondsToFileTime((unsigned long long)info.st_mtime + 11644473600ULL,
                          &data->ftLastWriteTime);
    }
    strncpy(data->cFileName, name, sizeof(data->cFileName) - 1);
}

DWORD GetFileAttributes(LPCSTR name)
{
    char hostPath[SGP_HOST_PATH];
    WIN32_FIND_DATA data;

    if (!SGPTranslatePath(name, hostPath, sizeof(hostPath)) || access(hostPath, F_OK) != 0) {
        gLastError = ERROR_FILE_NOT_FOUND;
        return INVALID_FILE_SIZE;
    }
    FillFindDataPosix(hostPath, "", &data);
    return data.dwFileAttributes;
}

BOOL SetFileAttributes(LPCSTR name, DWORD attributes)
{
    (void)name;
    (void)attributes;
    return TRUE;
}

BOOL GetFileTime(HANDLE handle, FILETIME *creation, FILETIME *access, FILETIME *write)
{
    FILETIME stamp;

    if (FileFromHandle(handle) == NULL) {
        return FALSE;
    }
    SecondsToFileTime((unsigned long long)time(NULL) + 11644473600ULL, &stamp);
    if (creation) *creation = stamp;
    if (access) *access = stamp;
    if (write) *write = stamp;
    return TRUE;
}

BOOL DeleteFile(LPCSTR name)
{
    char hostPath[SGP_HOST_PATH];

    return SGPTranslatePath(name, hostPath, sizeof(hostPath)) && remove(hostPath) == 0;
}

BOOL MoveFile(LPCSTR source, LPCSTR destination)
{
    char from[SGP_HOST_PATH];
    char to[SGP_HOST_PATH];

    return SGPTranslatePath(source, from, sizeof(from)) &&
           SGPTranslatePath(destination, to, sizeof(to)) && rename(from, to) == 0;
}

BOOL CreateDirectory(LPCSTR name, void *security)
{
    char hostPath[SGP_HOST_PATH];

    (void)security;
    return SGPTranslatePath(name, hostPath, sizeof(hostPath)) && mkdir(hostPath, 0777) == 0;
}

BOOL RemoveDirectory(LPCSTR name)
{
    char hostPath[SGP_HOST_PATH];

    return SGPTranslatePath(name, hostPath, sizeof(hostPath)) && rmdir(hostPath) == 0;
}

HANDLE FindFirstFile(LPCSTR pattern, WIN32_FIND_DATA *data)
{
    SGPPath directory;
    SGPFind *find = (SGPFind *)calloc(1, sizeof(*find));

    if (find == NULL) {
        return INVALID_HANDLE_VALUE;
    }
    if (!SplitPattern(pattern, &directory, find->pattern) ||
        !HostPath(&directory, find->directory, sizeof(find->directory)) ||
        (find->dir = opendir(find->directory)) == NULL) {
        free(find);
        gLastError = ERROR_PATH_NOT_FOUND;
        return INVALID_HANDLE_VALUE;
    }
    if (!FindNextFile((HANDLE)find, data)) {
        closedir(find->dir);
        free(find);
        gLastError = ERROR_FILE_NOT_FOUND;
        return INVALID_HANDLE_VALUE;
    }
    return (HANDLE)find;
}

BOOL FindNextFile(HANDLE handle, WIN32_FIND_DATA *data)
{
    SGPFind *find = (SGPFind *)handle;
    struct dirent *entry;

    if (find == NULL || handle == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    while ((entry = readdir(find->dir)) != NULL) {
        if (WildcardMatch(find->pattern, entry->d_name)) {
            char path[SGP_HOST_PATH];

            snprintf(path, sizeof(path), "%s/%s", find->directory, entry->d_name);
            FillFindDataPosix(path, entry->d_name, data);
            return TRUE;
        }
    }
    gLastError = ERROR_NO_MORE_FILES;
    return FALSE;
}

BOOL FindClose(HANDLE handle)
{
    SGPFind *find = (SGPFind *)handle;

    if (find == NULL || handle == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    closedir(find->dir);
    free(find);
    return TRUE;
}

BOOL GetDiskFreeSpace(LPCSTR root, LPDWORD sectorsPerCluster, LPDWORD bytesPerSector,
                      LPDWORD freeClusters, LPDWORD totalClusters)
{
    (void)root;
    if (sectorsPerCluster) *sectorsPerCluster = 1;
    if (bytesPerSector) *bytesPerSector = 4096;
    if (freeClusters) *freeClusters = 262144;
    if (totalClusters) *totalClusters = 262144;
    return TRUE;
}

DWORD GetTickCount(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (DWORD)(now.tv_sec * 1000UL + now.tv_nsec / 1000000UL);
}

void GlobalMemoryStatus(MEMORYSTATUS *status)
{
    memset(status, 0, sizeof(*status));
    status->dwLength = sizeof(*status);
    status->dwTotalPhys = status->dwAvailPhys = 64UL * 1024UL * 1024UL;
    status->dwTotalVirtual = status->dwAvailVirtual = status->dwTotalPhys;
}

void OutputDebugString(LPCSTR text)
{
    fprintf(stderr, "SGP: %s\n", text);
}

int MessageBox(HWND window, LPCSTR text, LPCSTR caption, UINT type)
{
    (void)window;
    (void)type;
    fprintf(stderr, "SGP MessageBox: %s: %s\n", caption ? caption : "", text ? text : "");
    return 1;
}

#endif

/* ------------------------------------------------------------------------
   Mouse */

SGPCursorPosProc gpSGPGetCursorPos;

BOOL GetCursorPos(LPPOINT point)
{
    point->x = 0;
    point->y = 0;
    if (gpSGPGetCursorPos) {
        gpSGPGetCursorPos(&point->x, &point->y);
    }
    return TRUE;
}

/* ------------------------------------------------------------------------
   Process state */

BOOL SetCurrentDirectory(LPCSTR name)
{
    SGPPath previous = gCurrentDirectory;
    SGPPath path;
    DWORD attributes;

    if (!ParsePath(name, &path)) {
        return FALSE;
    }
    gCurrentDirectory = path;
    attributes = GetFileAttributes(".");
    if (attributes == INVALID_FILE_SIZE || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        gCurrentDirectory = previous;
        gLastError = ERROR_PATH_NOT_FOUND;
        return FALSE;
    }
    return TRUE;
}

DWORD GetCurrentDirectory(DWORD length, LPSTR buffer)
{
    char path[SGP_HOST_PATH];

    FormatWindowsPath(&gCurrentDirectory, path, sizeof(path));
    if (strlen(path) + 1 > length) {
        return (DWORD)strlen(path) + 1;
    }
    strcpy(buffer, path);
    return (DWORD)strlen(path);
}

char *_getcwd(char *buffer, int length)
{
    return GetCurrentDirectory((DWORD)length, buffer) < (DWORD)length ? buffer : NULL;
}

DWORD GetModuleFileName(HINSTANCE module, LPSTR buffer, DWORD length)
{
    /* The application sits in the virtual root. */
    static const char name[] = "\\JA2PPC";

    (void)module;
    if (length < sizeof(name)) {
        return 0;
    }
    strcpy(buffer, name);
    return (DWORD)strlen(name);
}

/* Mac OS has no process environment; the original only used it to add
   a directory to PATH for DLL loading. */
DWORD GetEnvironmentVariable(LPCSTR name, LPSTR buffer, DWORD size)
{
    (void)name;
    if (buffer && size) {
        buffer[0] = 0;
    }
    return 0;
}

BOOL SetEnvironmentVariable(LPCSTR name, LPCSTR value)
{
    (void)name;
    (void)value;
    return TRUE;
}

LPSTR CharLower(LPSTR text)
{
    return _strlwr(text);
}

DWORD GetLastError(void)
{
    return gLastError;
}

DWORD FormatMessage(DWORD flags, const void *source, DWORD messageId, DWORD languageId,
                    LPSTR buffer, DWORD size, va_list *arguments)
{
    (void)flags;
    (void)source;
    (void)languageId;
    (void)arguments;
    if (buffer == NULL || size == 0) {
        return 0;
    }
    snprintf(buffer, size, "SGP file error %lu", (unsigned long)messageId);
    return (DWORD)strlen(buffer);
}
