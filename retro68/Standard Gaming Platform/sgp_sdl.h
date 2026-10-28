#ifndef SGP_SDL_H
#define SGP_SDL_H

/* JA2 PPC: the platform layer under the Standard Gaming Platform. It owns
 * the SDL window and is the only SGP file that talks to SDL:
 *   - SDL events become the Win32 hook calls and window messages the
 *     original input.c and sgp.c expect (sgp_win32_msg.c);
 *   - every frame the software DirectDraw flips is drawn into the window;
 *   - GetCursorPos and SimulateMouseMovement read and move the SDL mouse.
 * There is no original counterpart: on x86 Windows itself did this. */

#ifdef __cplusplus
extern "C" {
#endif

/* Opens the window and installs the hooks; called by main() in sgp.c
   before the original WinMain runs. Returns 0 on failure. */
int SGPPlatformInitialize(void);

/* Around each GameLoop() call in sgp.c: profiler frame, autotest step,
   quit requests from the profiler or the autotest. */
void SGPPlatformFrameBegin(void);
void SGPPlatformFrameEnd(void);

/* Presents the window surface as drawn by the caller (movie playback
   writes it directly), without converting the SGP frame buffer. */
void SGPPlatformPresentWindowSurface(const char *name);

#ifdef __cplusplus
}
#endif

#endif
