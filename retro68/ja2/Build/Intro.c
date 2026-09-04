#include "Intro.h"

#include "Utils/Cinematics.h"
#include "../../ja2_mac_internal.h"
#include "../../ja2_music.h"

static const char *gIntroVideoNames[] = {
    "REBEL_CR.SMK", "OMERTA.SMK", "PRAGUE_CR.SMK", "PRAGUE.SMK"
};
static const short gIntroVideoCount =
    sizeof(gIntroVideoNames) / sizeof(gIntroVideoNames[0]);
static JA2PPCVideo *gIntroVideo;
static short gIntroVideoIndex;
static Boolean gIntroActive;
static Boolean gIntroCursorHidden;

static Boolean OpenNextIntroVideo(void)
{
    if (gIntroVideoIndex >= gIntroVideoCount) return 0;
    gIntroVideo = JA2PPCVideoOpen("Intro.slf", gIntroVideoNames[gIntroVideoIndex]);
    if (gIntroVideo == NULL) {
        DebugLog("Intro: unable to open video %d entry=%s", gIntroVideoIndex,
                 gIntroVideoNames[gIntroVideoIndex]);
        return 0;
    }
    DebugLog("Intro: playing video %d/%d entry=%s", gIntroVideoIndex + 1,
             gIntroVideoCount, gIntroVideoNames[gIntroVideoIndex]);
    return 1;
}

Boolean JA2PPCIntroStart(void)
{
    if (gIntroActive) return 1;
    DebugLog("Intro: beginning sequence with %d videos", gIntroVideoCount);
    JA2MenuMusicStop();
    /* QuickTime presents directly to the window. Release the menu cache before
       movie import so this path remains viable in a 32 MB application zone. */
    if (gMenuWorld != NULL) {
        DisposeGWorld(gMenuWorld);
        gMenuWorld = NULL;
        DebugLog("Intro: released menu GWorld before video playback");
    }
    if (!JA2PPCVideoInitialize(gWindow, NULL)) {
        DebugLog("Intro: video backend unavailable; sequence not started");
        return 0;
    }
    InitCursor();
    HideCursor();
    gIntroCursorHidden = 1;
    gIntroVideoIndex = 0;
    if (!OpenNextIntroVideo()) {
        if (gIntroCursorHidden) {
            ShowCursor();
            gIntroCursorHidden = 0;
        }
        return 0;
    }
    gIntroActive = 1;
    return 1;
}

void JA2PPCIntroPoll(void)
{
    if (!gIntroActive || gIntroVideo == NULL) return;
    if (JA2PPCVideoPoll(gIntroVideo)) return;
    JA2PPCVideoClose(gIntroVideo);
    gIntroVideo = NULL;
    ++gIntroVideoIndex;
    if (OpenNextIntroVideo()) return;

    DebugLog("Intro: sequence complete; returning to New Game initialization");
    gIntroActive = 0;
    if (gIntroCursorHidden) {
        ShowCursor();
        gIntroCursorHidden = 0;
    }
    gMenuScreen = MENU_SCREEN_NEW_GAME;
    JA2InitializeNewGame();
    gActionScreenNeedsRender = 1;
}

void JA2PPCIntroUpdate(void)
{
    JA2PPCVideoUpdate(gIntroVideo);
}

void JA2PPCIntroSkip(void)
{
    if (!gIntroActive || gIntroVideo == NULL) return;
    DebugLog("Intro: skipping video %d/%d", gIntroVideoIndex + 1,
             gIntroVideoCount);
    JA2PPCVideoSkip(gIntroVideo);
}

void JA2PPCIntroShutdown(void)
{
    if (gIntroVideo != NULL) {
        JA2PPCVideoClose(gIntroVideo);
        gIntroVideo = NULL;
    }
    if (gIntroCursorHidden) {
        ShowCursor();
        gIntroCursorHidden = 0;
    }
    gIntroActive = 0;
}
