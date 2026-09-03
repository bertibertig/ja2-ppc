#ifndef JA2_SLF_H
#define JA2_SLF_H

typedef struct JA2SLFArchive JA2SLFArchive;

JA2SLFArchive *JA2SLFOpen(const char *archiveName);
void JA2SLFClose(JA2SLFArchive *archive);
unsigned long JA2SLFEntryCount(const JA2SLFArchive *archive);
int JA2SLFFind(JA2SLFArchive *archive, const char *fileName,
               unsigned long *fileOffset, unsigned long *fileLength);
int JA2SLFRead(JA2SLFArchive *archive, unsigned long fileOffset,
               void *buffer, unsigned long length);

#endif
