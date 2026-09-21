#include "ja2_mac_internal.h"
#include "ja2_new_game_init.h"

short gNewGameState = NEW_GAME_NOT_STARTED;
short gNewGameDifficulty = 1;
long gNewGameStartingCash;
short gNewGameOpeningEmails;
short gNewGameSectorX;
short gNewGameSectorY;
Boolean gNewGameGameStarted;
short gNewGameScreenCount;
short gNewGamePendingScreen;

Boolean JA2InitializeNewGame(void)
{
    DebugLog("New Game: InitNewGame(FALSE) PPC implementation entered "
             "menuScreen=%d difficulty=%d", gMenuScreen, gNewGameDifficulty);
    gNewGameState = NEW_GAME_INITIALIZING;
    gNewGameGameStarted = 0;
    gNewGameOpeningEmails = 0;
    gNewGameStartingCash = 0;
    gNewGameScreenCount = 0;
    gNewGamePendingScreen = MENU_SCREEN_MAIN;

    /* InitNewGame(FALSE) only enters this branch when gubScreenCount is zero
       in the original Strategic/Game Init.c. The laptop and strategic
       implementations are not in the PPC tree yet, so retain the boundary
       explicitly instead of claiming that either subsystem was initialized. */
    DebugLog("New Game: phase 1, initializing laptop and strategic state");
    DebugLog("New Game: InitLaptopAndLaptopScreens pending PPC migration");
    DebugLog("New Game: InitStrategicLayer pending PPC migration");

    DebugLog("New Game: phase 2, setting laptop new-game flag and helicopter seats");
    DebugLog("New Game: SetLaptopNewGameFlag pending PPC migration");
    DebugLog("New Game: ResetHeliSeats pending PPC migration");

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
    gNewGameScreenCount = 1;
    gNewGamePendingScreen = MENU_SCREEN_LAPTOP;
    gMenuScreen = gNewGamePendingScreen;
    DebugLog("New Game: gTacticalStatus.fHasAGameBeenStarted=TRUE (PPC state)");
    DebugLog("New Game: SetLaptopExitScreen(INIT_SCREEN)");
    DebugLog("New Game: SetPendingNewScreen(LAPTOP_SCREEN)");
    DebugLog("New Game: gubScreenCount=1");
    DebugLog("New Game: completed state=%d gameStarted=%d difficulty=%d "
             "cash=%ld emails=%d sector=%d,%d screen=%d", gNewGameState,
             gNewGameGameStarted, gNewGameDifficulty, gNewGameStartingCash,
             gNewGameOpeningEmails, gNewGameSectorX, gNewGameSectorY,
             gMenuScreen);
    return 1;
}
