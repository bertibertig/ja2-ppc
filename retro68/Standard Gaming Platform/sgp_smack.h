#ifndef SGP_SMACK_H
#define SGP_SMACK_H

/* JA2 PPC: the part of RAD's Smacker SDK (smack.h, SMACKW32.LIB) that
 * Utils/Cinematics.c calls, implemented in sgp_smack.c on the port's own
 * decoder (retro68/Smacker/smacker.c) and the Sound Manager. As with the
 * SDK, SmackToBuffer names the destination and SmackDoFrame decodes the
 * current frame into it; SmackWait keeps the movie on its own clock and
 * the audio track plays on a Sound Manager channel. When the Mac falls
 * behind, SmackNextFrame skips ahead and SmackDoFrame decodes the skipped
 * delta frames without converting them, so picture and sound stay in sync.
 */

#include "types.h"

#define SMACKFILEHANDLE 0x01000L
#define SMACKTRACKS 0x0fe000L
#define SMACKAUTOEXTRA 0xffffffffL
#define SMACKAUTOBLIT 0
#define SMACKBUFFER555 0x80000000L
#define SMACKBUFFER565 0xc0000000L

typedef struct SGPSmackPrivate SGPSmackPrivate;

/* The fields Cinematics.c reads. */
typedef struct Smack {
	UINT32 Width;
	UINT32 Height;
	UINT32 Frames;
	UINT32 FrameNum;
	SGPSmackPrivate *pPrivate;
} Smack;

typedef struct SmackBuf {
	UINT32 Width;
	UINT32 Height;
} SmackBuf;

#ifdef __cplusplus
extern "C" {
#endif

/* name is a Win32 file handle (SMACKFILEHANDLE) positioned at the start
   of the movie, as GetRealFileHandleFromFileManFileHandle returns it. */
Smack *SmackOpen( const char *name, UINT32 uiFlags, UINT32 uiExtraBuf );
void SmackClose( Smack *pSmack );
UINT32 SmackWait( Smack *pSmack );
void SmackToBuffer( Smack *pSmack, UINT32 uiLeft, UINT32 uiTop, UINT32 uiPitch, UINT32 uiDestHeight,
					void *pBuffer, UINT32 uiFlags );
UINT32 SmackDoFrame( Smack *pSmack );
void SmackNextFrame( Smack *pSmack );
UINT32 SmackGoto( Smack *pSmack, UINT32 uiFrame );
SmackBuf *SmackBufferOpen( void *hWindow, UINT32 uiBlitType, UINT32 uiWidth, UINT32 uiHeight,
						   UINT32 uiZoomWidth, UINT32 uiZoomHeight );
void SmackBufferClose( SmackBuf *pBuffer );
void SmackUseMMX( UINT32 uiFlag );
UINT8 SmackSoundUseMSS( void *pDigitalDriver );

#ifdef __cplusplus
}
#endif

#endif
