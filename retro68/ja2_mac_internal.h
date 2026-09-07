#ifndef JA2_MAC_INTERNAL_H
#define JA2_MAC_INTERNAL_H

#include <Quickdraw.h>
#include <Windows.h>

enum {
    MENU_SCREEN_MAIN,
    MENU_SCREEN_NEW_GAME,
    MENU_SCREEN_DIFFICULTY,
    MENU_SCREEN_INTRO,
    MENU_SCREEN_LOAD_GAME,
    MENU_SCREEN_OPTIONS,
    MENU_SCREEN_CREDITS
};

enum {
    NEW_GAME_NOT_STARTED,
    NEW_GAME_INITIALIZING,
    NEW_GAME_READY
};

extern WindowPtr gWindow;
extern GWorldPtr gMenuWorld;
extern Boolean gMenuRenderDirectToWindow;
extern Boolean gActionScreenNeedsRender;
extern short gMenuScreen;

extern short gNewGameState;
extern short gNewGameDifficulty;
extern long gNewGameStartingCash;
extern short gNewGameOpeningEmails;
extern short gNewGameSectorX;
extern short gNewGameSectorY;
extern Boolean gNewGameGameStarted;

void DebugLog(const char *format, ...);
Boolean RenderIndexedBackground(const char *archiveName, const char *fileName);
void DrawCString(short x, short y, const char *text);
void RestoreMainMenu(void);
void JA2BlitMenuBackground(void);
void JA2BlitWorldToWindow(GWorldPtr world, const char *name);
void JA2PPCIntroPoll(void);
void JA2PPCIntroUpdate(void);
void JA2PPCIntroSkip(void);
void JA2PPCIntroShutdown(void);
void JA2PPCVideoShutdown(void);
void JA2LogDifficultyRenderState(const char *stage);
Boolean JA2RenderInterfaceButton(short objectIndex, short x, short y);
Boolean JA2InstallNormalCursor(void);

Boolean JA2InitializeNewGame(void);
void JA2DrawDifficultyScreen(void);
void JA2HandleDifficultyClick(Point point);

#endif
