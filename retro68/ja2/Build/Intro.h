#ifndef JA2_PPC_INTRO_H
#define JA2_PPC_INTRO_H

#include <Quickdraw.h>

Boolean JA2PPCIntroStart(void);
Boolean JA2PPCIntroIsActive(void);
void JA2PPCIntroPoll(void);
void JA2PPCIntroUpdate(void);
void JA2PPCIntroSkip(void);
void JA2PPCIntroShutdown(void);

#endif
