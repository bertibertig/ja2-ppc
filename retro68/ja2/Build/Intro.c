#include "Intro.h"

#include "Utils/Cinematics.h"
#include "../../ja2_mac_internal.h"
#include "../../ja2_music.h"
#include "../../ja2_options.h"

static const char *gIntroVideoNames[] = {
    "REBEL_CR.SMK", "OMERTA.SMK", "PRAGUE_CR.SMK", "PRAGUE.SMK"
};
static const short gIntroVideoCount =
    sizeof(gIntroVideoNames) / sizeof(gIntroVideoNames[0]);
static JA2PPCVideo *gIntroVideo;
static short gIntroVideoIndex;
static Boolean gIntroActive;

static void CompleteIntroToNewGame(const char *reason)
{
    Boolean initialized;

    DebugLog("Intro: sequence complete; returning to New Game initialization "
             "reason=%s previousScreen=%d", reason, gMenuScreen);
    gIntroActive = 0;
    gMenuScreen = MENU_SCREEN_NEW_GAME;
    DebugLog("Intro: entering New Game init menuScreen=%d difficulty=%d",
             gMenuScreen, gNewGameDifficulty);
    initialized = JA2InitializeNewGame();
    DebugLog("Intro: New Game init returned=%d state=%d gameStarted=%d "
             "screenCount=%d pendingScreen=%d menuScreen=%d",
             initialized, gNewGameState, gNewGameGameStarted,
             gNewGameScreenCount, gNewGamePendingScreen, gMenuScreen);
    if (!initialized) {
        DebugLog("Intro: New Game init FAILED after %s", reason);
    } else if (gMenuScreen != MENU_SCREEN_LAPTOP) {
        DebugLog("Intro: New Game init did not reach Laptop screen actual=%d",
                 gMenuScreen);
    } else {
        DebugLog("Intro: New Game init reached Laptop screen successfully");
    }
    gActionScreenNeedsRender = 1;
    DebugLog("Intro: requested Laptop screen render actionNeedsRender=%d",
             gActionScreenNeedsRender);
}

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
    if (gIntroActive) {
        DebugLog("Intro: start requested while already active index=%d", gIntroVideoIndex);
        return 1;
    }
    DebugLog("Intro: beginning sequence with %d videos menuScreen=%d difficulty=%d",
             gIntroVideoCount, gMenuScreen, gNewGameDifficulty);
    JA2MenuMusicStop();
    if (JA2OptionsSkipIntros()) {
        DebugLog("Intro: skipped by options.cfg; no video backend started");
        CompleteIntroToNewGame("options.cfg");
        return 1;
    }
    if (!JA2PPCVideoInitialize(gWindow, NULL)) {
        DebugLog("Intro: video backend unavailable; sequence not started");
        return 0;
    }
    gIntroVideoIndex = 0;
    if (!OpenNextIntroVideo()) {
        DebugLog("Intro: first video could not be opened; sequence not started");
        return 0;
    }
    gIntroActive = 1;
    DebugLog("Intro: sequence active index=%d menuScreen=%d", gIntroVideoIndex,
             gMenuScreen);
    return 1;
}

Boolean JA2PPCIntroIsActive(void)
{
    return gIntroActive;
}

void JA2PPCIntroPoll(void)
{
    if (!gIntroActive || gIntroVideo == NULL) return;
    if (JA2PPCVideoPoll(gIntroVideo)) return;
    DebugLog("Intro: video complete index=%d/%d", gIntroVideoIndex + 1,
             gIntroVideoCount);
    JA2PPCVideoClose(gIntroVideo);
    gIntroVideo = NULL;
    ++gIntroVideoIndex;
    if (OpenNextIntroVideo()) return;

    gIntroActive = 0;
    CompleteIntroToNewGame("video-sequence");
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
    gIntroActive = 0;
}
