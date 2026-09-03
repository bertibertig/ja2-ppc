#include "ja2_slf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JA2_SLF_HEADER_SIZE 532
#define JA2_SLF_ENTRY_SIZE 280

struct JA2SLFArchive {
    FILE *file;
    unsigned long entryCount;
    long directoryOffset;
};

static unsigned long ReadLE32(const unsigned char *bytes)
{
    return ((unsigned long)bytes[0]) |
           ((unsigned long)bytes[1] << 8) |
           ((unsigned long)bytes[2] << 16) |
           ((unsigned long)bytes[3] << 24);
}

static int NamesEqual(const char *left, const char *right)
{
    while (*left != 0 && *right != 0) {
        char leftChar = *left++;
        char rightChar = *right++;
        if (leftChar >= 'a' && leftChar <= 'z') {
            leftChar = (char)(leftChar - 'a' + 'A');
        }
        if (rightChar >= 'a' && rightChar <= 'z') {
            rightChar = (char)(rightChar - 'a' + 'A');
        }
        if (leftChar != rightChar) {
            return 0;
        }
    }
    return *left == 0 && *right == 0;
}

JA2SLFArchive *JA2SLFOpen(const char *archiveName)
{
    JA2SLFArchive *archive;
    unsigned char header[JA2_SLF_HEADER_SIZE];
    long archiveSize;

    archive = (JA2SLFArchive *)malloc(sizeof(JA2SLFArchive));
    if (archive == NULL) {
        return NULL;
    }
    archive->file = NULL;

    {
        char path[256];
        sprintf(path, ":Data:%s", archiveName);
        archive->file = fopen(path, "rb");
        if (archive->file == NULL) {
            sprintf(path, "Data/%s", archiveName);
            archive->file = fopen(path, "rb");
        }
    }
    if (archive->file == NULL ||
        fread(header, 1, sizeof(header), archive->file) != sizeof(header) ||
        fseek(archive->file, 0, SEEK_END) != 0) {
        JA2SLFClose(archive);
        return NULL;
    }

    archive->entryCount = ReadLE32(&header[512]);
    archiveSize = ftell(archive->file);
    archive->directoryOffset = archiveSize -
                               (long)(archive->entryCount * JA2_SLF_ENTRY_SIZE);
    if (archive->entryCount == 0 || archive->entryCount > 65535 ||
        archive->directoryOffset < JA2_SLF_HEADER_SIZE ||
        fseek(archive->file, archive->directoryOffset, SEEK_SET) != 0) {
        JA2SLFClose(archive);
        return NULL;
    }
    return archive;
}

void JA2SLFClose(JA2SLFArchive *archive)
{
    if (archive == NULL) {
        return;
    }
    if (archive->file != NULL) {
        fclose(archive->file);
    }
    free(archive);
}

unsigned long JA2SLFEntryCount(const JA2SLFArchive *archive)
{
    return archive == NULL ? 0 : archive->entryCount;
}

int JA2SLFFind(JA2SLFArchive *archive, const char *fileName,
               unsigned long *fileOffset, unsigned long *fileLength)
{
    unsigned char entry[JA2_SLF_ENTRY_SIZE];
    char entryName[257];
    unsigned long entries;

    if (archive == NULL || archive->file == NULL) {
        return 0;
    }
    if (fseek(archive->file, archive->directoryOffset, SEEK_SET) != 0) {
        return 0;
    }
    entries = archive->entryCount;
    while (entries-- != 0) {
        unsigned short index;
        if (fread(entry, 1, sizeof(entry), archive->file) != sizeof(entry)) {
            return 0;
        }
        for (index = 0; index < 256 && entry[index] != 0; ++index) {
            entryName[index] = (char)entry[index];
        }
        entryName[index] = 0;
        if (entry[264] == 0 && NamesEqual(entryName, fileName)) {
            if (fileOffset != NULL) {
                *fileOffset = ReadLE32(&entry[256]);
            }
            if (fileLength != NULL) {
                *fileLength = ReadLE32(&entry[260]);
            }
            return 1;
        }
    }
    return 0;
}

int JA2SLFRead(JA2SLFArchive *archive, unsigned long fileOffset,
               void *buffer, unsigned long length)
{
    if (archive == NULL || archive->file == NULL || buffer == NULL ||
        fseek(archive->file, (long)fileOffset, SEEK_SET) != 0) {
        return 0;
    }
    return fread(buffer, 1, length, archive->file) == length;
}
