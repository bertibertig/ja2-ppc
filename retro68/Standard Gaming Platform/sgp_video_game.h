#ifndef SGP_VIDEO_GAME_H
#define SGP_VIDEO_GAME_H

/* JA2 PPC: the game-side globals and functions that the original video.c
 * uses for tactical background scrolling and fades (renderworld.h, Render
 * Dirty.h, Isometric utils.h, fade screen.h, timer control.h). Until those
 * game modules are ported, sgp_video_game.c provides their idle values. */

#include "types.h"

/* From TileEngine/renderworld.h. */
#define	SCROLL_UP							0x00000001
#define	SCROLL_DOWN						0x00000002
#define	SCROLL_RIGHT					0x00000004
#define	SCROLL_LEFT						0x00000008
#define	SCROLL_UPLEFT					0x00000020
#define	SCROLL_UPRIGHT				0x00000040
#define	SCROLL_DOWNLEFT				0x00000080
#define	SCROLL_DOWNRIGHT			0x00000200
extern UINT16 *gpZBuffer;

/* From TileEngine/renderworld.h, TileEngine/Render Dirty.h and
   timer control.h; only reached while the tactical view scrolls. */
extern void RenderStaticWorldRect( INT16 sLeft, INT16 sTop, INT16 sRight, INT16 sBottom, BOOLEAN fDynamicsToo );
extern BOOLEAN RestoreShiftedVideoOverlays( INT16 sShiftX, INT16 sShiftY );
extern void SaveVideoOverlaysArea( UINT32 uiSrcBuffer );
extern void ExecuteVideoOverlaysToAlternateBuffer( UINT32 uiNewDestBuffer );
extern void PauseTime( BOOLEAN fPaused );

/* From input.h. */
extern void FreeMouseCursor( void );

extern BOOLEAN gfRenderScroll;
extern BOOLEAN gfScrollStart;
extern INT16 gsScrollXIncrement;
extern INT16 gsScrollYIncrement;
extern INT32 guiScrollDirection;
extern INT16 gsVIEWPORT_START_X;
extern INT16 gsVIEWPORT_START_Y;
extern INT16 gsVIEWPORT_WINDOW_START_Y;
extern INT16 gsVIEWPORT_END_Y;
extern INT16 gsVIEWPORT_WINDOW_END_Y;
extern INT16 gsVIEWPORT_END_X;
extern BOOLEAN gfFadeInitialized;
extern BOOLEAN gfFadeInVideo;
extern void ( *gFadeFunction )( void );
extern BOOLEAN gfIgnoreScrollDueToCenterAdjust;

#endif
