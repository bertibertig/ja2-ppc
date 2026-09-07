/*
 * Stage 2 smoke test for the SDL2 rendering backend migration
 * (retro68/JA2_PPC_SDL2_MIGRATION_PLAN.md). This is a narrowly scoped,
 * standalone platform test, not a game screen: it exists only to validate
 * that SDL2 (laulandn/sdl2macos9) initializes, opens a window, and presents
 * a fixed 16-bit RGB555 surface on real System 9 hardware before any real
 * rendering code is ported off QuickDraw.
 *
 * Policy references (AGENTS.md "Rendering Backend (SDL2)"):
 *  - Color depth is fixed at SDL_PIXELFORMAT_RGB555 for the game-side
 *    surface; the final present blit lets SDL2 convert to whatever the
 *    real SDL_GetWindowSurface() format is (16-bit on the Lombard's LCD,
 *    32-bit over VGA-out).
 *  - DrawSprocket/AGL/InputSprocket stay disabled; this opens a plain
 *    windowed SDL_Window.
 */

#include "SDL.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define SMOKE_LOG_FILE "JA2PPC-SDL2-Test-Debug.log"
#define SMOKE_SCREEN_WIDTH 640
#define SMOKE_SCREEN_HEIGHT 480
/* Observed on real hardware (PowerBook G3 Lombard): roughly 180 frames
   presented in well under the 45s LaunchAPPL test-harness timeout. 120
   is a safe, deliberately conservative budget that still proves multiple
   seconds of stable presentation while guaranteeing a clean, bounded
   shutdown log within that timeout. */
#define SMOKE_FRAME_BUDGET 120

static void SmokeLog(const char *format, ...)
{
    va_list arguments;
    char line[256];
    FILE *file;

    va_start(arguments, format);
    vsprintf(line, format, arguments);
    va_end(arguments);

    file = fopen(SMOKE_LOG_FILE, "a");
    if (file != NULL) {
        fputs(line, file);
        fputs("\n", file);
        fclose(file);
    }
}

static const char *PixelFormatName(Uint32 format)
{
    switch (format) {
        case SDL_PIXELFORMAT_RGB555: return "RGB555 (16-bit)";
        case SDL_PIXELFORMAT_RGB565: return "RGB565 (16-bit)";
        case SDL_PIXELFORMAT_RGB888: return "RGB888 (32-bit)";
        case SDL_PIXELFORMAT_RGBX8888: return "RGBX8888 (32-bit)";
        default: return "unknown";
    }
}

int SDL_main(int argc, char *argv[])
{
    SDL_Window *window;
    SDL_Surface *frame;
    SDL_Surface *windowSurface;
    SDL_Event event;
    SDL_DisplayMode desktopMode;
    int running;
    unsigned long frameCount;
    Uint8 colorPhase;

    (void)argc;
    (void)argv;

    SmokeLog("SDL2 smoke test: entry");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SmokeLog("SDL2 smoke test: SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SmokeLog("SDL2 smoke test: SDL_Init(SDL_INIT_VIDEO) ok");

    if (SDL_GetDesktopDisplayMode(0, &desktopMode) == 0) {
        SmokeLog("SDL2 smoke test: desktop display %dx%d format=%s",
                 desktopMode.w, desktopMode.h,
                 PixelFormatName(desktopMode.format));
    } else {
        SmokeLog("SDL2 smoke test: SDL_GetDesktopDisplayMode failed: %s",
                 SDL_GetError());
    }

    window = SDL_CreateWindow(
        "JA2 SDL2 Smoke Test",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        SMOKE_SCREEN_WIDTH, SMOKE_SCREEN_HEIGHT,
        0);
    if (window == NULL) {
        SmokeLog("SDL2 smoke test: SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SmokeLog("SDL2 smoke test: SDL_CreateWindow %dx%d ok",
             SMOKE_SCREEN_WIDTH, SMOKE_SCREEN_HEIGHT);

    windowSurface = SDL_GetWindowSurface(window);
    if (windowSurface != NULL) {
        SmokeLog("SDL2 smoke test: window surface format=%s",
                 PixelFormatName(windowSurface->format->format));
    } else {
        SmokeLog("SDL2 smoke test: SDL_GetWindowSurface failed: %s",
                 SDL_GetError());
    }

    /* Fixed 16-bit game-side surface, independent of the real window
       surface format detected above. */
    frame = SDL_CreateRGBSurfaceWithFormat(
        0, SMOKE_SCREEN_WIDTH, SMOKE_SCREEN_HEIGHT, 16,
        SDL_PIXELFORMAT_RGB555);
    if (frame == NULL) {
        SmokeLog("SDL2 smoke test: SDL_CreateRGBSurfaceWithFormat failed: %s",
                 SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SmokeLog("SDL2 smoke test: fixed RGB555 frame surface created");

    running = 1;
    frameCount = 0;
    colorPhase = 0;

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    SmokeLog("SDL2 smoke test: SDL_QUIT received");
                    running = 0;
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                        SmokeLog("SDL2 smoke test: window close requested");
                        running = 0;
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE ||
                        event.key.keysym.sym == SDLK_q) {
                        SmokeLog("SDL2 smoke test: quit key pressed");
                        running = 0;
                    }
                    break;
                default:
                    break;
            }
        }

        if (!running) {
            break;
        }

        /* Cycle a solid fill through the RGB555 surface so a human
           inspecting the Lombard's LCD vs. an external VGA monitor can see
           that the same fixed-depth frame presents on both. No per-pixel
           byte-order code here; SDL2's blit does the depth conversion. */
        colorPhase = (Uint8)((colorPhase + 1) & 0xFF);
        SDL_FillRect(frame, NULL,
            SDL_MapRGB(frame->format, colorPhase,
                       (Uint8)(255 - colorPhase), 128));

        windowSurface = SDL_GetWindowSurface(window);
        if (windowSurface != NULL) {
            SDL_BlitSurface(frame, NULL, windowSurface, NULL);
            SDL_UpdateWindowSurface(window);
        }

        ++frameCount;
        if ((frameCount % 60) == 0) {
            SmokeLog("SDL2 smoke test: presented %lu frames", frameCount);
        }

        /* Auto-quit after a bounded frame budget instead of waiting for a
           close-box click or SDL_QUIT: this test is also run unattended,
           without a Cam Link session or any way to send input, and it
           must still produce a clean, bounded log to download and
           inspect (AGENTS.md "Verification"). */
        if (frameCount >= SMOKE_FRAME_BUDGET) {
            SmokeLog("SDL2 smoke test: frame budget reached, auto-quit");
            running = 0;
            break;
        }

        SDL_Delay(16);
    }

    SmokeLog("SDL2 smoke test: shutting down after %lu frames", frameCount);
    SDL_FreeSurface(frame);
    SDL_DestroyWindow(window);
    SDL_Quit();
    SmokeLog("SDL2 smoke test: exit");
    return 0;
}
