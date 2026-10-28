#ifndef JA2_PPC_CINEMATICS_H
#define JA2_PPC_CINEMATICS_H

#include <Quickdraw.h>

typedef struct JA2PPCVideo JA2PPCVideo;

Boolean JA2PPCVideoInitialize(void);
JA2PPCVideo *JA2PPCVideoOpen(const char *archiveName, const char *entryName);
Boolean JA2PPCVideoPoll(JA2PPCVideo *video);
void JA2PPCVideoUpdate(JA2PPCVideo *video);
Boolean JA2PPCVideoDone(JA2PPCVideo *video);
void JA2PPCVideoSkip(JA2PPCVideo *video);
void JA2PPCVideoClose(JA2PPCVideo *video);
void JA2PPCVideoShutdown(void);

#endif
