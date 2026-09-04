#include "ja2_mac_internal.h"
#include "ja2_game_init_options.h"

static short gDifficultySelection = 0;
static Boolean gDifficultySciFi = 1;
static Boolean gDifficultyExtraWeapons = 0;
static Boolean gDifficultyIronMan = 0;
static Boolean gDifficultyBackgroundLoaded = 0;
static Boolean gDifficultyPanelDarkened = 0;
static Boolean gDifficultyControlsDrawn = 0;
static Boolean gDifficultyButtonsDrawn = 0;


void JA2ResetDifficultyScreen(void)
{
    gDifficultyBackgroundLoaded = 0;
    gDifficultyPanelDarkened = 0;
    gDifficultyControlsDrawn = 0;
    gDifficultyButtonsDrawn = 0;
    InitCursor();
}

static void RestoreDifficultyCheckBoxes(void)
{
    /* Infinite Mac corrupts the destination when CopyBits targets clipped
       window regions. DrawDifficultyCheckBox clears its own backing instead. */
    DebugLog("Difficulty: checkbox backing redraw selected; no window CopyBits");
}

static void DarkenDifficultyPanel(const Rect *panel)
{
    PixMapHandle pixMap;
    unsigned char *base;
    long rowBytes;
    short x;
    short y;

    pixMap = gMenuWorld == NULL ? NULL : GetGWorldPixMap(gMenuWorld);
    if (pixMap == NULL || !LockPixels(pixMap)) {
        DebugLog("Difficulty: unable to lock cached PixMap for panel shading");
        return;
    }
    base = (unsigned char *)GetPixBaseAddr(pixMap);
    rowBytes = (**pixMap).rowBytes & 0x3fff;
    for (y = panel->top; y < panel->bottom; ++y) {
        unsigned char *pixel = base +
            (long)(y - (**pixMap).bounds.top) * rowBytes +
            (long)(panel->left - (**pixMap).bounds.left) * 4;
        for (x = panel->left; x < panel->right; ++x) {
            pixel[1] = (unsigned char)(pixel[1] >> 1);
            pixel[2] = (unsigned char)(pixel[2] >> 1);
            pixel[3] = (unsigned char)(pixel[3] >> 1);
            pixel += 4;
        }
    }
    UnlockPixels(pixMap);
}

static void DrawDifficultyCheckBox(short x, short y, Boolean checked)
{
    Rect box;
    RGBColor white = { 0xffff, 0xffff, 0xffff };
    RGBColor red = { 0xffff, 0x2020, 0x2020 };
    RGBColor backing = { 0x1800, 0x1800, 0x1800 };

    SetRect(&box, x, y, (short)(x + 29), (short)(y + 29));
    RGBForeColor(&backing);
    PaintRect(&box);
    RGBForeColor(&white);
    FrameRect(&box);
    FrameRect(&box);
    if (checked) {
        RGBForeColor(&red);
        MoveTo((short)(x + 2), (short)(y + 14));
        LineTo((short)(x + 10), (short)(y + 23));
        LineTo((short)(x + 28), (short)(y + 2));
    }
}

static void DrawDifficultyButtons(void)
{
    RGBColor white = { 0xffff, 0xffff, 0xffff };

    if (gDifficultyButtonsDrawn) return;
    if (!JA2RenderInterfaceButton(0, 141, 418)) {
        DebugLog("Difficulty: PreferencesButtons normal frame unavailable");
    }
    if (!JA2RenderInterfaceButton(1, 379, 418)) {
        DebugLog("Difficulty: PreferencesButtons cancel frame unavailable");
    }
    RGBForeColor(&white);
    DrawCString(188, 442, "Ok");
    DrawCString(400, 442, "Abbrechen");
    gDifficultyButtonsDrawn = 1;
}

void JA2DrawDifficultyScreen(void)
{
    Rect shade;
    RGBColor white = { 0xffff, 0xffff, 0xffff };
    RGBColor black = { 0x0000, 0x0000, 0x0000 };
    RGBColor buttonFill = { 0x5140, 0x4930, 0x2700 };
    RGBColor buttonBorder = { 0xb000, 0xa000, 0x7000 };

    JA2InstallNormalCursor();
    gMenuRenderDirectToWindow = 1;
    JA2LogDifficultyRenderState("entry");
    if (!gDifficultyBackgroundLoaded) {
        /* Decode the expensive STCI image once, like the main-menu cache. */
        gMenuRenderDirectToWindow = 0;
        gDifficultyBackgroundLoaded = RenderIndexedBackground(
            "Interface.slf", "OptionsScreenBackground.sti");
        gDifficultyPanelDarkened = 0;
        gMenuRenderDirectToWindow = 1;
        JA2LogDifficultyRenderState("after STCI cache decode");
        if (!gDifficultyBackgroundLoaded) {
            DebugLog("Difficulty: background unavailable; using solid fallback");
            SetPort(gWindow);
            RGBForeColor(&black);
            PaintRect(&gWindow->portRect);
        }
    }
    if (gDifficultyBackgroundLoaded) {
        if (!gDifficultyPanelDarkened) {
            SetRect(&shade, 48, 55, 592, 378);
            DarkenDifficultyPanel(&shade);
            gDifficultyPanelDarkened = 1;
            JA2LogDifficultyRenderState("after cached panel shade");
        }
        if (!gDifficultyControlsDrawn) {
            JA2BlitMenuBackground();
            gDifficultyControlsDrawn = 1;
            JA2LogDifficultyRenderState("after initial cache blit");
        } else {
            RestoreDifficultyCheckBoxes();
            JA2LogDifficultyRenderState("after checkbox cache restore");
        }
    }

    /* The cache is presentation-only. Keep all text and controls in the
       color window so QuickDraw cannot modify it between option selections. */
    SetGWorld((CGrafPtr)gWindow, GetMainDevice());
    JA2LogDifficultyRenderState("before controls");
    TextFont(systemFont);
    TextSize(16);
    TextMode(srcOr);
    RGBForeColor(&white);
    DrawCString(248, 80, "GRUNDEINSTELLUNGEN");

    DrawCString(80, 126, "Schwierigkeitsgrad");
    DrawCString(100, 152, "Einsteiger");
    DrawCString(100, 187, "Profi");
    DrawCString(100, 222, "Alter Hase");
    DrawDifficultyCheckBox(235, 141, gDifficultySelection == 0);
    DrawDifficultyCheckBox(235, 176, gDifficultySelection == 1);
    DrawDifficultyCheckBox(235, 211, gDifficultySelection == 2);

    RGBForeColor(&white);
    DrawCString(350, 126, "Waffen");
    DrawCString(370, 152, "Normal");
    DrawCString(370, 187, "Zusaetzliche Waffen");
    DrawDifficultyCheckBox(505, 141, !gDifficultyExtraWeapons);
    DrawDifficultyCheckBox(505, 176, gDifficultyExtraWeapons);

    RGBForeColor(&white);
    DrawCString(80, 286, "Extraschwer");
    DrawCString(100, 322, "Ohne Zeitlimit");
    DrawCString(100, 357, "Ironman");
    DrawCString(100, 378, "(Speichern im Kampf nicht moeglich)");
    DrawDifficultyCheckBox(235, 311, !gDifficultyIronMan);
    DrawDifficultyCheckBox(235, 346, gDifficultyIronMan);

    RGBForeColor(&white);
    DrawCString(350, 286, "Spielmodus");
    DrawCString(370, 322, "Realistisch");
    DrawCString(370, 357, "SciFi");
    DrawDifficultyCheckBox(505, 311, !gDifficultySciFi);
    DrawDifficultyCheckBox(505, 346, gDifficultySciFi);

    (void)buttonFill;
    (void)buttonBorder;
    DrawDifficultyButtons();
    TextMode(srcCopy);
    gMenuRenderDirectToWindow = 0;
    JA2LogDifficultyRenderState("after controls");
    ShowCursor();
}

static Boolean DifficultyOptionAt(Point point)
{
    if (point.h >= 210 && point.h < 300 && point.v >= 130 && point.v < 245) {
        if (point.v < 165) gDifficultySelection = 0;
        else if (point.v < 200) gDifficultySelection = 1;
        else gDifficultySelection = 2;
        return 1;
    }
    if (point.h >= 480 && point.h < 570 && point.v >= 130 && point.v < 210) {
        gDifficultyExtraWeapons = point.v >= 165;
        return 1;
    }
    if (point.h >= 210 && point.h < 300 && point.v >= 300 && point.v < 390) {
        gDifficultyIronMan = point.v >= 335;
        return 1;
    }
    if (point.h >= 480 && point.h < 570 && point.v >= 300 && point.v < 390) {
        gDifficultySciFi = point.v >= 335;
        return 1;
    }
    return 0;
}

void JA2HandleDifficultyClick(Point point)
{
    if (DifficultyOptionAt(point)) {
        DebugLog("Difficulty: selection changed difficulty=%d extraWeapons=%d ironMan=%d sciFi=%d",
                 gDifficultySelection, gDifficultyExtraWeapons,
                 gDifficultyIronMan, gDifficultySciFi);
        JA2DrawDifficultyScreen();
        return;
    }
    if (point.h >= 141 && point.h < 261 && point.v >= 418 && point.v < 454) {
        gNewGameDifficulty = gDifficultySelection;
        DebugLog("Difficulty: OK difficulty=%d extraWeapons=%d ironMan=%d sciFi=%d",
                 gDifficultySelection, gDifficultyExtraWeapons,
                 gDifficultyIronMan, gDifficultySciFi);
        gMenuScreen = MENU_SCREEN_NEW_GAME;
        if (!JA2InitializeNewGame()) {
            DebugLog("New Game: initialization failed after difficulty confirmation");
        }
        gActionScreenNeedsRender = 1;
        return;
    }
    if (point.h >= 379 && point.h < 499 && point.v >= 418 && point.v < 454) {
        DebugLog("Difficulty: cancelled; returning to main menu");
        gDifficultyBackgroundLoaded = 0;
        RestoreMainMenu();
    }
}
