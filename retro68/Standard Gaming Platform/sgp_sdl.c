/* JA2 PPC: SDL platform layer under the Standard Gaming Platform (see
 * sgp_sdl.h). This file deliberately does not include the SGP's Types.h:
 * the Mac and SDL headers define types of the same names. */

#include <SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* The port tools pull in QuickDraw; they come before the Win32 subset,
   whose ShowCursor/ShowWindow macros would clash with the Toolbox. */
#include "../ja2_autotest.h"
#include "../ja2_options.h"
#include "../ja2_prof.h"
#include "sgp_sdl.h"
#include "sgp_win32.h"

#define SGP_SCREEN_WIDTH 640
#define SGP_SCREEN_HEIGHT 480

/* Shared with the autotest (ja2_autotest.c), which pushes SDL events into
   this window and saves its surface. */
SDL_Window *gWindow;

/* Owned by the original SGP. */
extern unsigned char gfProgramIsRunning; /* BOOLEAN */
extern unsigned int guiCurrentScreen; /* gameloop.c */
extern void (*gpSGPDDPresent)(const unsigned short *pixels, unsigned int pitch,
                              unsigned int width, unsigned int height);

static int gLeftDown;
static int gRightDown;
static int gRightFromControlClick;

void DebugLog(const char *format, ...)
{
#if defined(JA2_DEBUG_BUILD)
    char line[512];
    va_list arguments;
    FILE *file;

    va_start(arguments, format);
    vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    /* Opened per line so the log survives a crash. */
    file = fopen("JA2PPC-Debug.log", "a");
    if (file != NULL) {
        fprintf(file, "%7lu ", (unsigned long)SDL_GetTicks());
        fputs(line, file);
        fputc('\n', file);
        fclose(file);
    }
#else
    (void)format;
#endif
}

/* ------------------------------------------------------------------ */
/* Mouse                                                               */

static void ClampToClip(LONG *x, LONG *y)
{
    RECT clip;
    if (*x < 0)
        *x = 0;
    if (*y < 0)
        *y = 0;
    if (*x >= SGP_SCREEN_WIDTH)
        *x = SGP_SCREEN_WIDTH - 1;
    if (*y >= SGP_SCREEN_HEIGHT)
        *y = SGP_SCREEN_HEIGHT - 1;
    /* ClipCursor keeps the Windows pointer inside the rectangle, right and
       bottom exclusive. */
    if (SGPGetCursorClip(&clip)) {
        if (*x < clip.left)
            *x = clip.left;
        if (*y < clip.top)
            *y = clip.top;
        if (*x >= clip.right)
            *x = clip.right - 1;
        if (*y >= clip.bottom)
            *y = clip.bottom - 1;
    }
}

static void MousePosition(LONG *x, LONG *y)
{
    int mouseX, mouseY;
    JA2AutotestGetMouseState(&mouseX, &mouseY);
    *x = mouseX;
    *y = mouseY;
    ClampToClip(x, y);
}

static void CursorPos(LONG *x, LONG *y)
{
    MousePosition(x, y);
}

static void WarpMouse(LONG x, LONG y)
{
    if (gWindow)
        SDL_WarpMouseInWindow(gWindow, (int)x, (int)y);
}

static void MouseButton(const SDL_MouseButtonEvent *button, int down)
{
    LONG x = button->x, y = button->y;
    int right = button->button == SDL_BUTTON_RIGHT;

    ClampToClip(&x, &y);
    /* One-button Mac mice and the PowerBook trackpad: Control-click is the
       right button, as for contextual menus in Mac OS. */
    if (button->button == SDL_BUTTON_LEFT) {
        if (down && (SDL_GetModState() & KMOD_CTRL)) {
            gRightFromControlClick = 1;
            right = 1;
        } else if (!down && gRightFromControlClick) {
            gRightFromControlClick = 0;
            right = 1;
        }
    } else if (!right) {
        return;
    }
    if (right) {
        if (down == gRightDown)
            return;
        gRightDown = down;
        SGPCallMouseHook(down ? WM_RBUTTONDOWN : WM_RBUTTONUP, x, y);
    } else {
        if (down == gLeftDown)
            return;
        gLeftDown = down;
        SGPCallMouseHook(down ? WM_LBUTTONDOWN : WM_LBUTTONUP, x, y);
    }
}

/* ------------------------------------------------------------------ */
/* Keyboard                                                            */

typedef struct {
    SDL_Keycode key;
    unsigned char virtualKey;
    unsigned char scanCode; /* PC set 1, as Windows reports it */
    unsigned char extended;
} SGPKeyMap;

/* Windows virtual-key codes and scan codes of a German keyboard (the
   letters and digits are mapped below by character). The scan codes matter
   for the numeric keypad: input.c's KeyChange tells keypad keys from the
   separate navigation keys by them. */
static const SGPKeyMap gKeyMap[] = {
    {SDLK_BACKSPACE, 8, 14, 0},
    {SDLK_TAB, 9, 15, 0},
    {SDLK_RETURN, 13, 28, 0},
    {SDLK_KP_ENTER, 13, 28, 1},
    {SDLK_LSHIFT, 16, 42, 0},
    {SDLK_RSHIFT, 16, 54, 0},
    {SDLK_LCTRL, 17, 29, 0},
    {SDLK_RCTRL, 17, 29, 1},
    {SDLK_LALT, 18, 56, 0},
    {SDLK_RALT, 18, 56, 1},
    {SDLK_PAUSE, 19, 69, 0},
    {SDLK_CAPSLOCK, 20, 58, 0},
    {SDLK_ESCAPE, 27, 1, 0},
    {SDLK_SPACE, 32, 57, 0},
    {SDLK_PAGEUP, 33, 73, 1},
    {SDLK_PAGEDOWN, 34, 81, 1},
    {SDLK_END, 35, 79, 1},
    {SDLK_HOME, 36, 71, 1},
    {SDLK_LEFT, 37, 75, 1},
    {SDLK_UP, 38, 72, 1},
    {SDLK_RIGHT, 39, 77, 1},
    {SDLK_DOWN, 40, 80, 1},
    {SDLK_PRINTSCREEN, 44, 55, 1},
    {SDLK_INSERT, 45, 82, 1},
    {SDLK_DELETE, 46, 83, 1},
    {SDLK_KP_0, 96, 82, 0},
    {SDLK_KP_1, 97, 79, 0},
    {SDLK_KP_2, 98, 80, 0},
    {SDLK_KP_3, 99, 81, 0},
    {SDLK_KP_4, 100, 75, 0},
    {SDLK_KP_5, 101, 76, 0},
    {SDLK_KP_6, 102, 77, 0},
    {SDLK_KP_7, 103, 71, 0},
    {SDLK_KP_8, 104, 72, 0},
    {SDLK_KP_9, 105, 73, 0},
    {SDLK_KP_MULTIPLY, 106, 55, 0},
    {SDLK_KP_PLUS, 107, 78, 0},
    {SDLK_KP_MINUS, 109, 74, 0},
    {SDLK_KP_PERIOD, 110, 83, 0},
    {SDLK_KP_DIVIDE, 111, 53, 1},
    {SDLK_F1, 112, 59, 0},
    {SDLK_F2, 113, 60, 0},
    {SDLK_F3, 114, 61, 0},
    {SDLK_F4, 115, 62, 0},
    {SDLK_F5, 116, 63, 0},
    {SDLK_F6, 117, 64, 0},
    {SDLK_F7, 118, 65, 0},
    {SDLK_F8, 119, 66, 0},
    {SDLK_F9, 120, 67, 0},
    {SDLK_F10, 121, 68, 0},
    {SDLK_F11, 122, 87, 0},
    {SDLK_F12, 123, 88, 0},
    {SDLK_NUMLOCKCLEAR, 144, 69, 0},
    {SDLK_SCROLLLOCK, 145, 70, 0},
    /* German layout: VK_OEM_* by key position. */
    {0xFC /* ü */, 186, 26, 0},
    {SDLK_PLUS, 187, 27, 0},
    {SDLK_COMMA, 188, 51, 0},
    {SDLK_MINUS, 189, 53, 0},
    {SDLK_PERIOD, 190, 52, 0},
    {SDLK_HASH, 191, 43, 0},
    {0xF6 /* ö */, 192, 39, 0},
    {0xDF /* ß */, 219, 12, 0},
    {SDLK_CARET, 220, 41, 0},
    {0xB4 /* ´ */, 221, 13, 0},
    {0xE4 /* ä */, 222, 40, 0},
    {SDLK_LESS, 226, 86, 0},
};

static const unsigned char gLetterScanCodes[26] = {
    30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, 50,
    49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 44, 21,
};
static const unsigned char gDigitScanCodes[10] = {11, 2, 3, 4, 5, 6, 7, 8, 9, 10};

static int VirtualKey(SDL_Keycode key, DWORD *virtualKey, DWORD *scanCode, DWORD *extended)
{
    size_t index;
    *extended = 0;
    if (key >= SDLK_a && key <= SDLK_z) {
        *virtualKey = (DWORD)('A' + (key - SDLK_a));
        *scanCode = gLetterScanCodes[key - SDLK_a];
        return 1;
    }
    if (key >= SDLK_0 && key <= SDLK_9) {
        *virtualKey = (DWORD)key;
        *scanCode = gDigitScanCodes[key - SDLK_0];
        return 1;
    }
    for (index = 0; index < sizeof(gKeyMap) / sizeof(gKeyMap[0]); ++index) {
        if (gKeyMap[index].key == key) {
            *virtualKey = gKeyMap[index].virtualKey;
            *scanCode = gKeyMap[index].scanCode;
            *extended = gKeyMap[index].extended;
            return 1;
        }
    }
    return 0;
}

static void Key(const SDL_KeyboardEvent *event, int down)
{
    DWORD virtualKey, scanCode, extended, keyData;

    if (down && event->keysym.sym == SDLK_q && (event->keysym.mod & KMOD_GUI)) {
        DebugLog("SGP: Command-Q, quitting");
        gfProgramIsRunning = FALSE;
        return;
    }
    if (!VirtualKey(event->keysym.sym, &virtualKey, &scanCode, &extended)) {
        DebugLog("SGP: no Windows key for SDL key %d", (int)event->keysym.sym);
        return;
    }
    /* WM_KEYDOWN/WM_KEYUP lParam layout. */
    keyData = 1 | (scanCode << 16) | (extended << 24);
    if (!down)
        keyData |= 0xC0000000UL;
    else if (event->repeat)
        keyData |= 0x40000000UL;
    SGPCallKeyboardHook(virtualKey, keyData);
}

/* ------------------------------------------------------------------ */
/* Events                                                              */

static void HandleEvent(const SDL_Event *event)
{
    switch (event->type) {
    case SDL_QUIT:
        DebugLog("SGP: window close requested");
        gfProgramIsRunning = FALSE;
        break;
    case SDL_KEYDOWN:
        Key(&event->key, 1);
        break;
    case SDL_KEYUP:
        Key(&event->key, 0);
        break;
    case SDL_MOUSEMOTION: {
        LONG x = event->motion.x, y = event->motion.y;
        ClampToClip(&x, &y);
        SGPCallMouseHook(WM_MOUSEMOVE, x, y);
        break;
    }
    case SDL_MOUSEBUTTONDOWN:
        MouseButton(&event->button, 1);
        break;
    case SDL_MOUSEBUTTONUP:
        MouseButton(&event->button, 0);
        break;
    case SDL_MOUSEWHEEL: {
        LONG x, y;
        MousePosition(&x, &y);
        PostMessage((HWND)1, WM_MOUSEWHEEL, (WPARAM)((event->wheel.y * WHEEL_DELTA) << 16),
                    (LPARAM)((y << 16) | (x & 0xffff)));
        break;
    }
    default:
        break;
    }
}

static void PumpEvents(BOOL wait)
{
    SDL_Event event;
    JA2_PROF_BEGIN(JA2_PROF_EVENTS);
    if (wait && SDL_WaitEventTimeout(&event, 100))
        HandleEvent(&event);
    while (SDL_PollEvent(&event))
        HandleEvent(&event);
    JA2_PROF_END(JA2_PROF_EVENTS);
}

/* ------------------------------------------------------------------ */
/* Presentation                                                        */

static void Present(const unsigned short *pixels, unsigned int pitch, unsigned int width,
                    unsigned int height)
{
    SDL_Surface *frame, *window;

    window = SDL_GetWindowSurface(gWindow);
    if (window == NULL) {
        DebugLog("SGP present: SDL_GetWindowSurface failed: %s", SDL_GetError());
        return;
    }
    frame = SDL_CreateRGBSurfaceWithFormatFrom((void *)pixels, (int)width, (int)height, 16,
                                               (int)pitch, SDL_PIXELFORMAT_RGB555);
    if (frame == NULL) {
        DebugLog("SGP present: cannot wrap the primary surface: %s", SDL_GetError());
        return;
    }
    /* The only conversion out of RGB555, into the display's format. */
    JA2_PROF_BEGIN(JA2_PROF_BLIT);
    if (SDL_BlitSurface(frame, NULL, window, NULL) != 0)
        DebugLog("SGP present: SDL_BlitSurface failed: %s", SDL_GetError());
    JA2_PROF_END(JA2_PROF_BLIT);
    SDL_FreeSurface(frame);
    SGPPlatformPresentWindowSurface("SGP primary surface");
}

void SGPPlatformPresentWindowSurface(const char *name)
{
    JA2_PROF_BEGIN(JA2_PROF_PRESENT);
    if (SDL_UpdateWindowSurface(gWindow) != 0)
        DebugLog("SGP present: %s: SDL_UpdateWindowSurface failed: %s", name, SDL_GetError());
    JA2_PROF_END(JA2_PROF_PRESENT);
}

/* ------------------------------------------------------------------ */

int SGPPlatformInitialize(void)
{
    SDL_Surface *surface;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        return 0;
    DebugLog("JA2 PPC startup: original game on the Mac SGP (language %s)",
#if defined(JA2_LANGUAGE_GERMAN)
             "german"
#else
             "english"
#endif
    );
    JA2OptionsLoad();
    JA2ProfInit();
    gWindow = SDL_CreateWindow("JA2 PPC", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               SGP_SCREEN_WIDTH, SGP_SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (gWindow == NULL) {
        DebugLog("SGP: SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 0;
    }
    surface = SDL_GetWindowSurface(gWindow);
    if (surface != NULL)
        DebugLog("SGP: window surface format %s",
                 SDL_GetPixelFormatName(surface->format->format));
    /* The SGP draws its own mouse cursor into the frame (video.c). */
    SDL_ShowCursor(SDL_DISABLE);

    gpSGPDDPresent = Present;
    gpSGPGetCursorPos = CursorPos;
    gpSGPPumpEvents = PumpEvents;
    gpSGPWarpMouse = WarpMouse;
    JA2AutotestInit();
    return 1;
}

void SGPPlatformFrameBegin(void)
{
    JA2ProfFrameBegin();
}

void SGPPlatformFrameEnd(void)
{
    static unsigned int lastScreen = 0xffffffff;
    static unsigned long frames;

    ++frames;
    if (guiCurrentScreen != lastScreen) {
        DebugLog("SGP: frame %lu, screen %u -> %u", frames, lastScreen, guiCurrentScreen);
        lastScreen = guiCurrentScreen;
    }
    JA2ProfFrameEnd();
    JA2AutotestFrame();
    if (JA2ProfShouldQuit()) {
        DebugLog("Profiler: frame limit reached; quitting");
        gfProgramIsRunning = FALSE;
    }
    if (JA2AutotestShouldQuit())
        gfProgramIsRunning = FALSE;
}
