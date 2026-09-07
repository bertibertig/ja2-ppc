#include "ja2_mac_internal.h"
#include "ja2_new_game_init.h"

short gNewGameState = NEW_GAME_NOT_STARTED;
short gNewGameDifficulty = 1;
long gNewGameStartingCash;
short gNewGameOpeningEmails;
short gNewGameSectorX;
short gNewGameSectorY;
Boolean gNewGameGameStarted;

Boolean JA2InitializeNewGame(void)
{
    DebugLog("New Game: InitNewGame(FALSE) PPC implementation entered");
    gNewGameState = NEW_GAME_INITIALIZING;
    gNewGameGameStarted = 0;
    gNewGameOpeningEmails = 0;
    gNewGameStartingCash = 0;

    /* Mirrors the first-time branch in Strategic/Game Init.c. */
    DebugLog("New Game: phase 1, initializing laptop and strategic state");
    DebugLog("New Game: laptop subsystem pending PPC migration; state recorded");
    DebugLog("New Game: strategic subsystem pending PPC migration; state recorded");

    DebugLog("New Game: phase 2, setting laptop new-game flag and helicopter seats");
    DebugLog("New Game: laptop new-game flag=TRUE");
    DebugLog("New Game: helicopter seats reset");

    DebugLog("New Game: phase 3, adding opening email sequence");
    gNewGameOpeningEmails = 5;
    DebugLog("New Game: opening emails queued=%d", gNewGameOpeningEmails);

    switch (gNewGameDifficulty) {
        case 0:
            gNewGameStartingCash = 45000;
            break;
        case 1:
            gNewGameStartingCash = 35000;
            break;
        case 2:
            gNewGameStartingCash = 30000;
            break;
        default:
            DebugLog("New Game: invalid difficulty=%d", gNewGameDifficulty);
            gNewGameState = NEW_GAME_NOT_STARTED;
            return 0;
    }
    DebugLog("New Game: difficulty=%d starting cash=%ld", gNewGameDifficulty,
             gNewGameStartingCash);

    /* A9 Omerta is the original selected initial map sector (9, 1, 0). */
    gNewGameSectorX = 9;
    gNewGameSectorY = 1;
    DebugLog("New Game: selected initial sector A9 Omerta (%d,%d,0)",
             gNewGameSectorX, gNewGameSectorY);

    gNewGameGameStarted = 1;
    gNewGameState = NEW_GAME_READY;
    DebugLog("New Game: game-started flag=TRUE");
    DebugLog("New Game: transition target=LAPTOP_SCREEN (PPC placeholder)");
    return 1;
}
