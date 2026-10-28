/* JA2 PPC: stand-ins for the original modules that are not ported yet.
 *
 * The ported screens (main menu, credits, game options, options, message
 * boxes, fades) and InitializeJA2 call into the tactical, strategic and
 * laptop layers. Until each of those original files runs on the Mac, this
 * file provides its functions and globals with the original initial values
 * and harmless results, grouped by the original file. Every function logs
 * "JA2 PPC: <name> (<file>) is not ported yet" on its first call, so the
 * boundary shows up in JA2PPC-Debug.log. Remove a group when its file is
 * ported; the linker then reports any duplicate.
 *
 * Screens without a port show a notice and return to the main menu on any
 * key or click (UnportedScreen below). */

#ifdef PRECOMPILEDHEADERS
	#include "JA2 All.h"
#endif
#include "Game Init.h"
#include "Game Clock.h"
#include "HelpScreen.h"
#include "SaveLoadGame.h"
#include "SaveLoadScreen.h"
#include "strategic.h"
#include "strategicmap.h"
#include "Strategic Movement.h"
#include "Queen Command.h"
#include "Map Screen Interface.h"
#include "Animation Data.h"
#include "Campaign.h"
#include "Dialogue Control.h"
#include "Handle UI.h"
#include "Overhead.h"
#include "Soldier Control.h"
#include "Tactical Save.h"
#include "Vehicles.h"
#include "Ambient Control.h"
#include "Isometric Utils.h"
#include "lighting.h"
#include "overhead map.h"
#include "renderworld.h"
#include "Shade Table Util.h"
#include "SmokeEffects.h"
#include "Tactical Placement GUI.h"
#include "Tile Cache.h"
#include "worlddef.h"
#include "worldman.h"
#include "WorldDat.h"
#include "Event Manager.h"
#include "message.h"
#include "finances.h"
#include "laptop.h"
#include "Intro.h"
#include "Cheats.h"
#include "RegInst.h"
#include "soundman.h"
#include "faces.h"

void DebugLog( const char *format, ... );

static void NotPorted( BOOLEAN *pfLogged, const char *pName, const char *pFile )
{
	if ( !*pfLogged )
	{
		*pfLogged = TRUE;
		DebugLog( "JA2 PPC: %s (%s) is not ported yet", pName, pFile );
	}
}

#define NOT_PORTED( name, file ) \
	do { static BOOLEAN fLogged_ = FALSE; NotPorted( &fLogged_, name, file ); } while( 0 )


// ---------------------------------------------------------------------------
// Screens without a port: a notice, then back to the main menu.

static UINT32 UnportedScreen( const char *pName, UINT32 uiScreen )
{
	static UINT32 uiShownScreen = 0xffffffff;
	InputAtom	InputEvent;
	CHAR16		zText[ 128 ];

	if ( uiShownScreen != uiScreen )
	{
		uiShownScreen = uiScreen;
		DebugLog( "JA2 PPC: screen %s is not ported yet", pName );
		ColorFillVideoSurfaceArea( FRAME_BUFFER, 0, 0, 640, 480, 0 );
		SetFont( FONT14ARIAL );
		SetFontForeground( FONT_MCOLOR_WHITE );
		SetFontBackground( FONT_MCOLOR_BLACK );
		swprintf( zText, L"JA2 PPC: %S ist noch nicht portiert.", pName );
		mprintf( 40, 200, zText );
		mprintf( 40, 230, L"Taste oder Klick: zur\xfc""ck zum Hauptmen\xfc." );
		InvalidateScreen( );
	}

	while ( DequeueEvent( &InputEvent ) )
	{
		if ( InputEvent.usEvent == KEY_UP || InputEvent.usEvent == LEFT_BUTTON_UP || InputEvent.usEvent == RIGHT_BUTTON_UP )
		{
			uiShownScreen = 0xffffffff;
			return( MAINMENU_SCREEN );
		}
	}
	return( uiScreen );
}

#define UNPORTED_SCREEN( prefix, id, file ) \
	UINT32 prefix##Init( void ) { return( TRUE ); } \
	UINT32 prefix##Handle( void ) { return( UnportedScreen( file, id ) ); } \
	UINT32 prefix##Shutdown( void ) { return( TRUE ); }

UNPORTED_SCREEN( MainGameScreen, GAME_SCREEN, "gamescreen.c" )
UNPORTED_SCREEN( AniEditScreen, ANIEDIT_SCREEN, "aniviewscreen.c" )
UNPORTED_SCREEN( MapScreen, MAP_SCREEN, "Strategic/mapscreen.c" )
UNPORTED_SCREEN( LaptopScreen, LAPTOP_SCREEN, "Laptop/laptop.c" )
UNPORTED_SCREEN( MapUtilScreen, MAPUTILITY_SCREEN, "Utils/MapUtility.c" )
UNPORTED_SCREEN( AutoResolveScreen, AUTORESOLVE_SCREEN, "Strategic/Auto Resolve.c" )
UNPORTED_SCREEN( SaveLoadScreen, SAVE_LOAD_SCREEN, "SaveLoadScreen.c" )
UNPORTED_SCREEN( ShopKeeperScreen, SHOPKEEPER_SCREEN, "Tactical/ShopKeeper Interface.c" )
UNPORTED_SCREEN( QuestDebugScreen, QUEST_DEBUG_SCREEN, "Strategic/QuestDebugSystem.c" )


// ---------------------------------------------------------------------------
// gamescreen.c

void EnterTacticalScreen( ) { NOT_PORTED( "EnterTacticalScreen", "gamescreen.c" ); }
void FadeInGameScreen( ) { NOT_PORTED( "FadeInGameScreen", "gamescreen.c" ); }
void InternalLeaveTacticalScreen( UINT32 uiNewScreen ) { NOT_PORTED( "InternalLeaveTacticalScreen", "gamescreen.c" ); }
void SetRenderHook( RENDER_HOOK pRenderOverride ) { NOT_PORTED( "SetRenderHook", "gamescreen.c" ); }


// ---------------------------------------------------------------------------
// HelpScreen.c

HELP_SCREEN_STRUCT gHelpScreen;

void InitHelpScreenSystem( ) { memset( &gHelpScreen, 0, sizeof( gHelpScreen ) ); }
void NewScreenSoResetHelpScreen( ) { }


// ---------------------------------------------------------------------------
// Laptop

void InsertCommasForDollarFigure( STR16 pString ) { NOT_PORTED( "InsertCommasForDollarFigure", "Laptop/finances.c" ); }
void ClearOutTempLaptopFiles( void ) { NOT_PORTED( "ClearOutTempLaptopFiles", "Laptop/laptop.c" ); }
void ExitLaptop( ) { NOT_PORTED( "ExitLaptop", "Laptop/laptop.c" ); }

BOOLEAN DoLapTopSystemMessageBox( UINT8 ubStyle, INT16 *zString, UINT32 uiExitScreen, UINT16 usFlags, MSGBOX_CALLBACK ReturnCallback )
{
	NOT_PORTED( "DoLapTopSystemMessageBox", "Laptop/laptop.c" );
	return( FALSE );
}

BOOLEAN DoLapTopSystemMessageBoxWithRect( UINT8 ubStyle, INT16 *zString, UINT32 uiExitScreen, UINT16 usFlags, MSGBOX_CALLBACK ReturnCallback, SGPRect *pCenteringRect )
{
	NOT_PORTED( "DoLapTopSystemMessageBoxWithRect", "Laptop/laptop.c" );
	return( FALSE );
}


// ---------------------------------------------------------------------------
// SaveLoadGame.c, SaveLoadScreen.c

BOOLEAN DoesUserHaveEnoughHardDriveSpace( ) { return( TRUE ); }

BOOLEAN DoSaveLoadMessageBoxWithRect( UINT8 ubStyle, INT16 *zString, UINT32 uiExitScreen, UINT16 usFlags, MSGBOX_CALLBACK ReturnCallback, SGPRect *pCenteringRect )
{
	NOT_PORTED( "DoSaveLoadMessageBoxWithRect", "SaveLoadScreen.c" );
	return( FALSE );
}

BOOLEAN InitSaveGameArray( ) { return( TRUE ); }

// No save games yet: the main menu greys out "Load game", as Windows does
// without saves.
BOOLEAN IsThereAnySavedGameFiles( ) { return( FALSE ); }


// ---------------------------------------------------------------------------
// Strategic

BOOLEAN gfPauseDueToPlayerGamePause = FALSE;

void CreateMouseRegionForPauseOfClock( INT16 sX, INT16 sY ) { NOT_PORTED( "CreateMouseRegionForPauseOfClock", "Strategic/Game Clock.c" ); }
void RemoveMouseRegionForPauseOfClock( void ) { }
BOOLEAN DidGameJustStart( ) { return( TRUE ); }
void InterruptTime( ) { }
void LockPauseState( UINT32 uiUniqueReasonId ) { }
void UnLockPauseState( ) { }
void PauseGame( ) { }
void UnPauseGame( ) { }
void UpdateClock( ) { }
void WarpGameTime( UINT32 uiAdjustment, UINT8 ubWarpCode ) { NOT_PORTED( "WarpGameTime", "Strategic/Game Clock.c" ); }

UINT8 gubScreenCount = 0;

// The screen flow of the original: reset, then on the first call after the
// intro switch to the laptop. Profiles, laptop, strategic layer, e-mails and
// the starting cash wait for their modules.
BOOLEAN InitNewGame( BOOLEAN fReset )
{
	if( fReset )
	{
		gubScreenCount = 0;
		return( TRUE );
	}
	NOT_PORTED( "InitNewGame", "Strategic/Game Init.c" );
	if ( gubScreenCount == 0 )
	{
		SetPendingNewScreen( LAPTOP_SCREEN );
		gubScreenCount = 1;
		return( TRUE );
	}
	if ( gubScreenCount == 1 )
	{
		gubScreenCount = 2;
	}
	return( TRUE );
}

void ReStartingGame( ) { NOT_PORTED( "ReStartingGame", "Strategic/Game Init.c" ); }
void ShutdownStrategicLayer( ) { }

BOOLEAN fInMapMode = FALSE;
BOOLEAN fMapPanelDirty = TRUE;
INT32 giCommonGlowBaseTime = 0;
INT32 giFlashAssignBaseTime = 0;
INT32 giFlashContractBaseTime = 0;
UINT32 guiFlashCursorBaseTime = 0;
INT32 giPotCharPathBaseTime = 0;

void EndMapScreen( BOOLEAN fDuringFade ) { NOT_PORTED( "EndMapScreen", "Strategic/mapscreen.c" ); }
BOOLEAN HandlePreloadOfMapGraphics( void ) { return( TRUE ); }
void HandleRemovalOfPreLoadedMapGraphics( void ) { }

UINT32 guiCompressionStringBaseTime = 0;
INT32 giExitToTactBaseTime = 0;
UINT32 guiSectorLocatorBaseTime = 0;
INT32 giAnimateRouteBaseTime = 0;
INT32 giPotHeliPathBaseTime = 0;
INT32 giClickHeliIconBaseTime = 0;
INT32 giFlashHighlightedItemBaseTime = 0;
INT32 giCompatibleItemBaseTime = 0;

INT32 DoMapMessageBox( UINT8 ubStyle, INT16 *zString, UINT32 uiExitScreen, UINT16 usFlags, MSGBOX_CALLBACK ReturnCallback )
{
	NOT_PORTED( "DoMapMessageBox", "Strategic/Map Screen Interface.c" );
	return( -1 );
}

INT32 DoMapMessageBoxWithRect( UINT8 ubStyle, INT16 *zString, UINT32 uiExitScreen, UINT16 usFlags, MSGBOX_CALLBACK ReturnCallback, SGPRect *pCenteringRect )
{
	NOT_PORTED( "DoMapMessageBoxWithRect", "Strategic/Map Screen Interface.c" );
	return( -1 );
}

void HandleMAPUILoseCursorFromOtherScreen( void ) { }
UINT8 NumEnemiesInAnySector( INT16 sSectorX, INT16 sSectorY, INT16 sSectorZ ) { return( 0 ); }
BOOLEAN CheckAndHandleUnloadingOfCurrentWorld( ) { return( TRUE ); }
BOOLEAN InitStrategicEngine( ) { return( TRUE ); }
void InitStrategicMovementCosts( ) { }


// ---------------------------------------------------------------------------
// Tactical

BOOLEAN DeInitAnimationSystem( ) { return( TRUE ); }
BOOLEAN InitAnimationSystem( ) { NOT_PORTED( "InitAnimationSystem", "Tactical/Animation Data.c" ); return( TRUE ); }
UINT8 CurrentPlayerProgressPercentage( void ) { return( 0 ); }
UINT8 HighestPlayerProgressPercentage( void ) { return( 0 ); }
BOOLEAN InitalizeDialogueControl( ) { return( TRUE ); }
void ShutdownDialogueControl( ) { }

UINT32 PlayJA2GapSample( CHAR8 *zSoundFile, UINT32 usRate, UINT32 ubVolume, UINT32 ubLoops, UINT32 uiPan, AudioGapList* pData )
{
	NOT_PORTED( "PlayJA2GapSample", "Tactical/GAP.C" );
	return( NO_SAMPLE );
}

void ToggleItemGlow( BOOLEAN fOn ) { }

MOUSE_REGION gDisableRegion;
MOUSE_REGION gUserTurnRegion;

void SetUIKeyboardHook( UIKEYBOARD_HOOK KeyboardHookFnc ) { NOT_PORTED( "SetUIKeyboardHook", "Tactical/Handle UI.c" ); }
void HandleTacticalUILoseCursorFromOtherScreen( ) { }

BOOLEAN gfWorldLoaded = FALSE;

BOOLEAN GetSoldier( SOLDIERTYPE **ppSoldier, UINT16 usSoldierIndex ) { *ppSoldier = NULL; return( FALSE ); }
// The steps of the original whose modules are ported run; the rest
// (renderer, tactical interface, palettes, path finding, AI, overhead)
// waits for Tactical/Overhead.c.
BOOLEAN InitTacticalEngine( )
{
	NOT_PORTED( "InitTacticalEngine", "Tactical/Overhead.c" );
	InitializeBaseDirtyRectQueue( );
	InitializeGameVideoObjects( );
	return( TRUE );
}
BOOLEAN ShutdownOverhead( ) { return( TRUE ); }
void ShutdownTacticalEngine( ) { }

BOOLEAN DoSkiMessageBox( UINT8 ubStyle, INT16 *zString, UINT32 uiExitScreen, UINT8 ubFlags, MSGBOX_CALLBACK ReturnCallback )
{
	NOT_PORTED( "DoSkiMessageBox", "Tactical/ShopKeeper Interface.c" );
	return( FALSE );
}

BOOLEAN CreateSoldierPalettes( SOLDIERTYPE *pSoldier ) { NOT_PORTED( "CreateSoldierPalettes", "Tactical/Soldier Control.c" ); return( FALSE ); }
BOOLEAN GetPaletteRepIndexFromID( PaletteRepID aPalRep, UINT8 *pubPalIndex ) { return( FALSE ); }
BOOLEAN InitTacticalSave( BOOLEAN fCreateTempDir ) { return( TRUE ); }
void ClearOutVehicleList( void ) { }


// ---------------------------------------------------------------------------
// TileEngine

void HandleNewSectorAmbience( UINT8 ubAmbientID ) { }
void StopAmbients( ) { }

void ConvertGridNoToXY( INT16 sGridNo, INT16 *sXPos, INT16 *sYPos ) { *sXPos = 0; *sYPos = 0; }
BOOLEAN GetMouseMapPos( INT16 *psMapPos ) { *psMapPos = NOWHERE; return( FALSE ); }
void GetWorldXYAbsoluteScreenXY( INT32 sWorldCellX, INT32 sWorldCellY, INT16 *psWorldScreenX, INT16 *psWorldScreenY ) { *psWorldScreenX = 0; *psWorldScreenY = 0; }

BOOLEAN InitLightingSystem( void ) { return( TRUE ); }
BOOLEAN ShutdownLightingSystem( void ) { return( TRUE ); }
BOOLEAN InOverheadMap( ) { return( FALSE ); }
BOOLEAN gfOverheadMapDirty = FALSE;

// renderworld.c: the idle tactical viewport.
UINT16 *gpZBuffer = NULL;
INT16 gsVIEWPORT_START_X = 0;
INT16 gsVIEWPORT_START_Y = 0;
INT16 gsVIEWPORT_END_Y = 360;
INT16 gsVIEWPORT_WINDOW_END_Y = 360;
INT16 gsVIEWPORT_WINDOW_START_Y = 0;
INT16 gsVIEWPORT_END_X = 640;
BOOLEAN gfIgnoreScrollDueToCenterAdjust = FALSE;
BOOLEAN gfRenderScroll = FALSE;
BOOLEAN gfScrollStart = FALSE;
INT16 gsScrollXIncrement;
INT16 gsScrollYIncrement;
INT32 guiScrollDirection;

void RenderStaticWorldRect( INT16 sLeft, INT16 sTop, INT16 sRight, INT16 sBottom, BOOLEAN fDynamicsToo ) { NOT_PORTED( "RenderStaticWorldRect", "TileEngine/renderworld.c" ); }
void SetRenderFlags( UINT32 uiFlags ) { }

BOOLEAN DeleteShadeTableDir( ) { return( TRUE ); }
void DetermineRGBDistributionSettings( ) { NOT_PORTED( "DetermineRGBDistributionSettings", "TileEngine/Shade Table Util.c" ); }
void UpdateSmokeEffectGraphics( ) { }

BOOLEAN gfTacticalPlacementGUIActive = FALSE;
BOOLEAN gfTacticalPlacementGUIDirty = FALSE;
BOOLEAN gfValidLocationsChanged = FALSE;

void DeleteTileCache( ) { }
BOOLEAN InitTileCache( ) { return( TRUE ); }
void DeinitializeWorld( ) { }
BOOLEAN InitializeWorld( ) { NOT_PORTED( "InitializeWorld", "TileEngine/worlddef.c" ); return( TRUE ); }
void SetTreeTopStateForMap( ) { }

TILESET gTilesets[ NUM_TILESETS ];


// ---------------------------------------------------------------------------
// Utils

BOOLEAN InitializeEventManager( ) { return( TRUE ); }
BOOLEAN ShutdownEventManager( ) { return( TRUE ); }
UINT32 MapUtilScreenShutdown( void );
void DisableScrollMessages( void ) { }

void ScreenMsg( UINT16 usColor, UINT8 ubPriority, STR16 pStringA, ... )
{
	NOT_PORTED( "ScreenMsg", "Utils/message.c" );
}


// ---------------------------------------------------------------------------
// Cheats, RegInst.c

UINT8 gubCheatLevel = 0;
BOOLEAN gfLoadGameUponEntry = FALSE;

// The game reads nothing from the registry.
BOOLEAN InitializeRegistryKeys( STR strAppName, STR strRegistryKey ) { return( TRUE ); }
