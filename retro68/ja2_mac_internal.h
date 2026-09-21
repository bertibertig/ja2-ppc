#ifndef JA2_MAC_INTERNAL_H
#define JA2_MAC_INTERNAL_H

#include <Quickdraw.h>
#include <QDOffscreen.h>
#include <SDL.h>

#define JA2_SCREEN_WIDTH 640
#define JA2_SCREEN_HEIGHT 480

enum {
    MENU_SCREEN_MAIN,
    MENU_SCREEN_NEW_GAME,
    MENU_SCREEN_DIFFICULTY,
    MENU_SCREEN_INTRO,
    MENU_SCREEN_LOAD_GAME,
    MENU_SCREEN_OPTIONS,
    MENU_SCREEN_CREDITS,
    MENU_SCREEN_LAPTOP
};

enum {
    NEW_GAME_NOT_STARTED,
    NEW_GAME_INITIALIZING,
    NEW_GAME_READY
};

/* Pack 8-bit RGB components into the fixed SDL_PIXELFORMAT_RGB555 (xRGB1555)
   format used for every game surface, per AGENTS.md's "Rendering Backend
   (SDL2)" color depth policy. Every offscreen composition surface (menu,
   credits, cursor) is created with this format; only the final blit onto
   SDL_GetWindowSurface() may cross into the real display format. */
#define JA2_RGB555(r, g, b) \
    ((Uint16)((((r) >> 3) << 10) | (((g) >> 3) << 5) | ((b) >> 3)))

#define JA2_COLOR_WHITE  JA2_RGB555(255, 255, 255)
#define JA2_COLOR_BLACK  JA2_RGB555(0, 0, 0)
#define JA2_COLOR_RED    JA2_RGB555(255, 32, 32)
#define JA2_COLOR_ACCENT JA2_RGB555(217, 155, 69)

extern SDL_Window *gWindow;
extern SDL_Surface *gMenuSurface;
extern Boolean gActionScreenNeedsRender;
extern short gMenuScreen;

extern short gNewGameState;
extern short gNewGameDifficulty;
extern long gNewGameStartingCash;
extern short gNewGameOpeningEmails;
extern short gNewGameSectorX;
extern short gNewGameSectorY;
extern Boolean gNewGameGameStarted;
extern short gNewGameScreenCount;
extern short gNewGamePendingScreen;

void DebugLog(const char *format, ...);
Boolean RenderIndexedBackground(SDL_Surface *destination, const char *archiveName,
                                const char *fileName);
void DrawCString(SDL_Surface *destination, short x, short y, const char *text,
                 Uint16 color);
void FillRectRGB555(SDL_Surface *surface, short left, short top, short right,
                    short bottom, Uint16 color);
void FrameRectRGB555(SDL_Surface *surface, short left, short top, short right,
                     short bottom, Uint16 color);
void DrawLineRGB555(SDL_Surface *surface, short x0, short y0, short x1,
                    short y1, Uint16 color);
void RestoreMainMenu(void);
void JA2BlitMenuBackground(void);
void JA2BlitSurfaceToWindow(SDL_Surface *surface, const char *name);
Boolean JA2PresentVideoWorld(GWorldPtr world);
void JA2PPCIntroPoll(void);
void JA2PPCIntroSkip(void);
void JA2PPCIntroShutdown(void);
void JA2PPCVideoShutdown(void);
Boolean JA2RenderInterfaceButton(SDL_Surface *destination, short objectIndex,
                                 short x, short y);
Boolean JA2InstallNormalCursor(void);

Boolean JA2InitializeNewGame(void);
void JA2DrawDifficultyScreen(void);
void JA2HandleDifficultyClick(short localX, short localY);

#endif
