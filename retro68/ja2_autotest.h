#ifndef JA2_AUTOTEST_H
#define JA2_AUTOTEST_H

/* PPC-only scripted input for unattended tests on the real Mac and in
   emulators. There is no original counterpart: the x86 game was tested by
   hand. When options.cfg names a script (autotest=<file>), each frame
   replays its commands as SDL events, writes screenshots of the window
   surface as BMP files and logs results to JA2AUTOTEST.log beside the app.
   Without a script every entry point returns immediately. */

#include <Quickdraw.h>

void JA2AutotestInit(void);
void JA2AutotestFrame(void);
void JA2AutotestShutdown(void);
Boolean JA2AutotestActive(void);
Boolean JA2AutotestShouldQuit(void);

/* SDL_GetMouseState replacement: while a script has moved the mouse, the
   game sees the scripted position instead of the real pointer. */
void JA2AutotestGetMouseState(int *x, int *y);

#endif
