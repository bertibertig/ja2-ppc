#include "ja2_mac_internal.h"
#include "ja2_game_init_options.h"
#include "ja2/Build/Intro.h"

static short gDifficultySelection = 0;
static Boolean gDifficultySciFi = 1;
static Boolean gDifficultyExtraWeapons = 0;
static Boolean gDifficultyIronMan = 0;
static Boolean gDifficultyBackgroundLoaded = 0;

static void DarkenDifficultyPanel(void)
{
    SDL_Rect panel;
    int x;
    int y;

    if (gMenuSurface == NULL) {
        return;
    }
    panel.x = 48;
    panel.y = 55;
    panel.w = 544;
    panel.h = 323;
    if (SDL_MUSTLOCK(gMenuSurface)) {
        SDL_LockSurface(gMenuSurface);
    }
    for (y = panel.y; y < panel.y + panel.h; ++y) {
        Uint16 *row = (Uint16 *)((Uint8 *)gMenuSurface->pixels +
                                 (long)y * gMenuSurface->pitch);
        for (x = panel.x; x < panel.x + panel.w; ++x) {
            Uint16 pixel = row[x];
            row[x] = (Uint16)((pixel & 0x7bde) >> 1);
        }
    }
    if (SDL_MUSTLOCK(gMenuSurface)) {
        SDL_UnlockSurface(gMenuSurface);
    }
}

static void DrawDifficultyCheckBox(short x, short y, Boolean checked)
{
    FillRectRGB555(gMenuSurface, x, y, (short)(x + 29), (short)(y + 29),
                   JA2_RGB555(24, 24, 24));
    FrameRectRGB555(gMenuSurface, x, y, (short)(x + 29), (short)(y + 29),
                    JA2_COLOR_WHITE);
    if (checked) {
        DrawLineRGB555(gMenuSurface, (short)(x + 2), (short)(y + 14),
                       (short)(x + 10), (short)(y + 23), JA2_COLOR_RED);
        DrawLineRGB555(gMenuSurface, (short)(x + 10), (short)(y + 23),
                       (short)(x + 28), (short)(y + 2), JA2_COLOR_RED);
    }
}

static void DrawDifficultyButtons(void)
{
    if (!JA2RenderInterfaceButton(gMenuSurface, 0, 141, 418)) {
        DebugLog("Difficulty: PreferencesButtons normal frame unavailable");
    }
    if (!JA2RenderInterfaceButton(gMenuSurface, 1, 379, 418)) {
        DebugLog("Difficulty: PreferencesButtons cancel frame unavailable");
    }
    DrawCString(gMenuSurface, 188, 442, "Ok", JA2_COLOR_WHITE);
    DrawCString(gMenuSurface, 400, 442, "Abbrechen", JA2_COLOR_WHITE);
}

void JA2ResetDifficultyScreen(void)
{
    gDifficultyBackgroundLoaded = 0;
}

void JA2DrawDifficultyScreen(void)
{
    if (gMenuSurface == NULL) {
        return;
    }
    DebugLog("Difficulty: rendering SDL2 RGB555 surface");

    if (!gDifficultyBackgroundLoaded) {
        gDifficultyBackgroundLoaded = RenderIndexedBackground(
            gMenuSurface, "Interface.slf", "OptionsScreenBackground.sti");
        if (gDifficultyBackgroundLoaded) {
            DarkenDifficultyPanel();
        } else {
            SDL_FillRect(gMenuSurface, NULL, JA2_COLOR_BLACK);
            DebugLog("Difficulty: background unavailable; using solid fallback");
        }
    }

    DrawCString(gMenuSurface, 248, 80, "GRUNDEINSTELLUNGEN", JA2_COLOR_WHITE);

    DrawCString(gMenuSurface, 80, 126, "Schwierigkeitsgrad", JA2_COLOR_WHITE);
    DrawCString(gMenuSurface, 100, 152, "Einsteiger", JA2_COLOR_WHITE);
    DrawCString(gMenuSurface, 100, 187, "Profi", JA2_COLOR_WHITE);
    DrawCString(gMenuSurface, 100, 222, "Alter Hase", JA2_COLOR_WHITE);
    DrawDifficultyCheckBox(235, 141, gDifficultySelection == 0);
    DrawDifficultyCheckBox(235, 176, gDifficultySelection == 1);
    DrawDifficultyCheckBox(235, 211, gDifficultySelection == 2);

    DrawCString(gMenuSurface, 350, 126, "Waffen", JA2_COLOR_WHITE);
    DrawCString(gMenuSurface, 370, 152, "Normal", JA2_COLOR_WHITE);
    DrawCString(gMenuSurface, 370, 187, "Zusaetzliche Waffen", JA2_COLOR_WHITE);
    DrawDifficultyCheckBox(505, 141, !gDifficultyExtraWeapons);
    DrawDifficultyCheckBox(505, 176, gDifficultyExtraWeapons);

    DrawCString(gMenuSurface, 80, 286, "Extraschwer", JA2_COLOR_WHITE);
    DrawCString(gMenuSurface, 100, 322, "Ohne Zeitlimit", JA2_COLOR_WHITE);
    DrawCString(gMenuSurface, 100, 357, "Ironman", JA2_COLOR_WHITE);
    DrawCString(gMenuSurface, 100, 378, "(Speichern im Kampf nicht moeglich)", JA2_COLOR_WHITE);
    DrawDifficultyCheckBox(235, 311, !gDifficultyIronMan);
    DrawDifficultyCheckBox(235, 346, gDifficultyIronMan);

    DrawCString(gMenuSurface, 350, 286, "Spielmodus", JA2_COLOR_WHITE);
    DrawCString(gMenuSurface, 370, 322, "Realistisch", JA2_COLOR_WHITE);
    DrawCString(gMenuSurface, 370, 357, "SciFi", JA2_COLOR_WHITE);
    DrawDifficultyCheckBox(505, 311, !gDifficultySciFi);
    DrawDifficultyCheckBox(505, 346, gDifficultySciFi);

    DrawDifficultyButtons();
    JA2BlitMenuBackground();
}

static Boolean DifficultyOptionAt(short x, short y)
{
    if (x >= 210 && x < 300 && y >= 130 && y < 245) {
        if (y < 165) gDifficultySelection = 0;
        else if (y < 200) gDifficultySelection = 1;
        else gDifficultySelection = 2;
        return 1;
    }
    if (x >= 480 && x < 570 && y >= 130 && y < 210) {
        gDifficultyExtraWeapons = y >= 165;
        return 1;
    }
    if (x >= 210 && x < 300 && y >= 300 && y < 390) {
        gDifficultyIronMan = y >= 335;
        return 1;
    }
    if (x >= 480 && x < 570 && y >= 300 && y < 390) {
        gDifficultySciFi = y >= 335;
        return 1;
    }
    return 0;
}

void JA2HandleDifficultyClick(short localX, short localY)
{
    DebugLog("Difficulty: click local=%d,%d", localX, localY);
    if (DifficultyOptionAt(localX, localY)) {
        DebugLog("Difficulty: selection changed difficulty=%d extraWeapons=%d ironMan=%d sciFi=%d",
                 gDifficultySelection, gDifficultyExtraWeapons,
                 gDifficultyIronMan, gDifficultySciFi);
        JA2DrawDifficultyScreen();
        return;
    }
    if (localX >= 141 && localX < 261 && localY >= 418 && localY < 454) {
        gNewGameDifficulty = gDifficultySelection;
        DebugLog("Difficulty: OK difficulty=%d extraWeapons=%d ironMan=%d sciFi=%d",
                 gDifficultySelection, gDifficultyExtraWeapons,
                 gDifficultyIronMan, gDifficultySciFi);
        gMenuScreen = MENU_SCREEN_INTRO;
        if (!JA2PPCIntroStart()) {
            DebugLog("Intro: unavailable; falling back to New Game initialization");
            gMenuScreen = MENU_SCREEN_NEW_GAME;
            if (!JA2InitializeNewGame()) {
                DebugLog("New Game: initialization failed after intro fallback");
            } else {
                DebugLog("New Game: intro fallback reached menuScreen=%d state=%d",
                         gMenuScreen, gNewGameState);
            }
        } else {
            if (JA2PPCIntroIsActive()) {
                DebugLog("Intro: started successfully; waiting for completion before New Game "
                         "menuScreen=%d", gMenuScreen);
            } else {
                DebugLog("Intro: completed synchronously; New Game already reached "
                         "menuScreen=%d", gMenuScreen);
            }
        }
        gActionScreenNeedsRender = 1;
        return;
    }
    if (localX >= 379 && localX < 499 && localY >= 418 && localY < 454) {
        DebugLog("Difficulty: cancelled; returning to main menu");
        JA2ResetDifficultyScreen();
        RestoreMainMenu();
    }
}
