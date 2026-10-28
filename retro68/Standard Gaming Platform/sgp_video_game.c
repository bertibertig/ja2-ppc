/* JA2 PPC: idle stand-ins for the game modules the original video.c
 * reaches into (TileEngine/renderworld.c, TileEngine/Render Dirty.c,
 * Fade Screen.c, Utils/Timer Control.c, input.c). Values match the
 * originals' initial state, so RefreshScreen takes its non-scrolling
 * path. Each definition here is removed when its original file is ported;
 * the linker then reports any duplicate. */

#include "sgp_video_game.h"

void DebugLog( const char *format, ... );

/* From sgp.c (the SGP's WinMain), until it is ported. */
UINT8 gbPixelDepth = 16;
BOOLEAN gfProgramIsRunning = TRUE;

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
BOOLEAN gfFadeInitialized = FALSE;
BOOLEAN gfFadeInVideo;
void ( *gFadeFunction )( void ) = NULL;

static void NotPorted( const char *pName )
{
	DebugLog( "SGP: %s called before its game module is ported", pName );
}

void RenderStaticWorldRect( INT16 sLeft, INT16 sTop, INT16 sRight, INT16 sBottom, BOOLEAN fDynamicsToo )
{
	NotPorted( "RenderStaticWorldRect" );
}

BOOLEAN RestoreShiftedVideoOverlays( INT16 sShiftX, INT16 sShiftY )
{
	NotPorted( "RestoreShiftedVideoOverlays" );
	return( FALSE );
}

void SaveVideoOverlaysArea( UINT32 uiSrcBuffer )
{
	NotPorted( "SaveVideoOverlaysArea" );
}

void ExecuteVideoOverlaysToAlternateBuffer( UINT32 uiNewDestBuffer )
{
	NotPorted( "ExecuteVideoOverlaysToAlternateBuffer" );
}

void PauseTime( BOOLEAN fPaused )
{
}

void FreeMouseCursor( void )
{
}
