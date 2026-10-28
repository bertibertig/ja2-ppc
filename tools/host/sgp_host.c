/* Headless host platform layer for the JA2 PPC port (debugging aid).
 *
 * Replaces Standard Gaming Platform/sgp_sdl.c when tools/host_run.sh
 * builds the same original sources as a 32-bit Linux program: no window,
 * no sound, frames are written as PPM files on request. Little-endian
 * like the x86 original, so it finds logic errors, not byte-order ones.
 *
 * Environment:
 *   SGP_ROOT        bundle directory with Data/ and Ja2.ini (sgp_win32.c)
 *   SGP_HOST_FRAMES quit after this many game loops (default 600; one
 *                   loop takes about 10 ms)
 *   SGP_HOST_SCRIPT "<frame> <command> [args];..." with commands
 *                   shot <name> | move <x> <y> | click <x> <y> [right] |
 *                   key <vk>    (vk = Windows virtual-key code)
 *   SGP_HOST_OUT    directory for <name>.ppm (default .)
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sgp_sdl.h"
#include "sgp_win32.h"

extern unsigned char gfProgramIsRunning;
extern unsigned int guiCurrentScreen;
extern void (*gpSGPDDPresent)(const unsigned short *pixels, unsigned int pitch,
                              unsigned int width, unsigned int height);

static unsigned long gFrame;
static unsigned long gMaxFrames = 600;
static LONG gMouseX = 320, gMouseY = 240;
static unsigned short gLastFrame[640 * 480];
static char gScript[4096];

void DebugLog(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    fprintf(stderr, "[%5lu] ", gFrame);
    vfprintf(stderr, format, arguments);
    fputc('\n', stderr);
    va_end(arguments);
}

/* Autotest/options/profiler entry points the SGP files reference. */
void JA2AutotestGetMouseState(int *x, int *y) { *x = (int)gMouseX; *y = (int)gMouseY; }

static void Present(const unsigned short *pixels, unsigned int pitch, unsigned int width, unsigned int height)
{
    unsigned int row;
    for (row = 0; row < height && row < 480; ++row)
        memcpy(gLastFrame + row * 640, (const char *)pixels + row * pitch, 2 * (width < 640 ? width : 640));
}

void SGPPlatformPresentWindowSurface(const char *name) { (void)name; }

static void Shot(const char *name)
{
    char path[512];
    const char *dir = getenv("SGP_HOST_OUT");
    FILE *file;
    int index;
    snprintf(path, sizeof(path), "%s/%s.ppm", dir ? dir : ".", name);
    file = fopen(path, "wb");
    if (!file)
        return;
    fprintf(file, "P6\n640 480\n255\n");
    for (index = 0; index < 640 * 480; ++index) {
        unsigned short p = gLastFrame[index];
        unsigned char rgb[3];
        rgb[0] = (unsigned char)(((p >> 10) & 31) * 255 / 31);
        rgb[1] = (unsigned char)(((p >> 5) & 31) * 255 / 31);
        rgb[2] = (unsigned char)((p & 31) * 255 / 31);
        fwrite(rgb, 1, 3, file);
    }
    fclose(file);
    DebugLog("host: shot %s (screen %u)", path, guiCurrentScreen);
}

static void CursorPos(LONG *x, LONG *y) { *x = gMouseX; *y = gMouseY; }
static void Warp(LONG x, LONG y) { gMouseX = x; gMouseY = y; }

static void RunScript(void)
{
    char *entry = gScript;
    while (*entry) {
        char *next = strchr(entry, ';');
        char command[32] = "", a[64] = "", b[32] = "", c[32] = "";
        unsigned long frame;
        if (next)
            *next = 0;
        if (sscanf(entry, "%lu %31s %63s %31s %31s", &frame, command, a, b, c) >= 2 && frame == gFrame) {
            if (!strcmp(command, "shot")) {
                Shot(a);
            } else if (!strcmp(command, "move")) {
                gMouseX = atol(a); gMouseY = atol(b);
                SGPCallMouseHook(WM_MOUSEMOVE, gMouseX, gMouseY);
            } else if (!strcmp(command, "click")) {
                int right = !strcmp(c, "right");
                gMouseX = atol(a); gMouseY = atol(b);
                SGPCallMouseHook(WM_MOUSEMOVE, gMouseX, gMouseY);
                SGPCallMouseHook(right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN, gMouseX, gMouseY);
                SGPCallMouseHook(right ? WM_RBUTTONUP : WM_LBUTTONUP, gMouseX, gMouseY);
            } else if (!strcmp(command, "key")) {
                DWORD vk = (DWORD)atol(a);
                SGPCallKeyboardHook(vk, 1);
                SGPCallKeyboardHook(vk, 0xC0000001UL);
            }
        }
        if (!next)
            break;
        *next = ';';
        entry = next + 1;
    }
}

static void Pump(BOOL wait) { (void)wait; }

int SGPPlatformInitialize(void)
{
    const char *frames = getenv("SGP_HOST_FRAMES");
    const char *script = getenv("SGP_HOST_SCRIPT");
    if (frames)
        gMaxFrames = strtoul(frames, NULL, 10);
    if (script)
        strncpy(gScript, script, sizeof(gScript) - 1);
    gpSGPDDPresent = Present;
    gpSGPGetCursorPos = CursorPos;
    gpSGPPumpEvents = Pump;
    gpSGPWarpMouse = Warp;
    return 1;
}

void SGPPlatformFrameBegin(void)
{
    RunScript();
}

void SGPPlatformFrameEnd(void)
{
    static unsigned int lastScreen = 0xffffffff;
    if (guiCurrentScreen != lastScreen) {
        DebugLog("host: screen %u -> %u", lastScreen, guiCurrentScreen);
        lastScreen = guiCurrentScreen;
    }
    /* About 100 frames per second, so frame numbers in scripts map to
       game time (the menus wait on the JA2 clock). */
    usleep(10000);
    if (++gFrame >= gMaxFrames)
        gfProgramIsRunning = 0;
}

int main(int argc, char *argv[])
{
    extern int SDL_main(int, char **);
    return SDL_main(argc, argv);
}
