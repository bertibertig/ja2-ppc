/* JA2 PPC: Smacker SDK subset for Utils/Cinematics.c (see sgp_smack.h). */

#include <stdlib.h>
#include <string.h>

/* Mac headers before the Win32 subset (see sgp_win32_msg.c). */
#ifdef macintosh
#include <Sound.h>
#include <Timer.h>
#endif

#include "sgp_smack.h"
#include "sgp_win32.h"
#include "../Smacker/smacker.h"
#include "../ja2_prof.h"

void DebugLog( const char *format, ... );

/* Smacker frames are stored back to back; reading them in frame-sized
   pieces cost ~46 ms per frame on the PowerBook, so the movie is read
   ahead in large sequential chunks. */
#define SGP_SMACK_READ_AHEAD ( 128UL * 1024UL )
#define SGP_SMACK_MAX_CATCHUP 4
#define SGP_SMACK_AUDIO_SLOTS 48
#define SGP_SMACK_AUDIO_CALLBACK 0x4a32534bL

struct SGPSmackPrivate
{
	Smacker *pDecoder;
	HANDLE hFile;
	UINT32 uiBase;
	unsigned char *pReadAhead;
	UINT32 uiReadAheadOffset;
	UINT32 uiReadAheadLength;
	UINT32 uiFrameMicros;
	unsigned long long ullStart;
	int fStarted;
	UINT16 usLut[ 256 ];
	int fLutValid;
	void *pDest;
	UINT32 uiDestPitch, uiDestHeight, uiLeft, uiTop, uiDestFlags;
	UINT32 uiShown, uiSkipped, uiAudioDropped;
};

#ifdef macintosh

typedef struct
{
	ExtSoundHeader header;
	unsigned char *pData;
	UINT32 uiCapacity;
	volatile int fBusy;
} SGPSmackAudioSlot;

static SndChannelPtr gAudioChannel;
static SndCallBackUPP gAudioCallback;
static SGPSmackAudioSlot gAudioSlots[ SGP_SMACK_AUDIO_SLOTS ];
static int giNextAudioSlot;

static unsigned long long NowMicros( void )
{
	UnsignedWide now;
	Microseconds( &now );
	return ( (unsigned long long)now.hi << 32 ) | now.lo;
}

static pascal void AudioCallback( SndChannelPtr channel, SndCommand *command )
{
	(void)channel;
	if ( command->cmd == callBackCmd && command->param2 == SGP_SMACK_AUDIO_CALLBACK &&
		 command->param1 >= 0 && command->param1 < SGP_SMACK_AUDIO_SLOTS )
		gAudioSlots[ command->param1 ].fBusy = 0;
}

static void OpenAudio( Smack *pSmack )
{
	unsigned long rate;
	int channels, bits;
	OSErr error;

	if ( !SmackerAudioInfo( pSmack->pPrivate->pDecoder, 0, &rate, &channels, &bits ) )
		return;
	if ( !gAudioCallback )
		gAudioCallback = NewSndCallBackUPP( AudioCallback );
	error = SndNewChannel( &gAudioChannel, sampledSynth, channels == 2 ? initStereo : 0, gAudioCallback );
	if ( error != noErr )
	{
		DebugLog( "SGP smack: cannot create a Sound Manager channel (%d)", error );
		gAudioChannel = NULL;
		return;
	}
	DebugLog( "SGP smack: audio %lu Hz %d ch %d bit", rate, channels, bits );
}

static void CloseAudio( void )
{
	int index;
	if ( gAudioChannel )
	{
		SndDisposeChannel( gAudioChannel, true );
		gAudioChannel = NULL;
	}
	for ( index = 0; index < SGP_SMACK_AUDIO_SLOTS; ++index )
		gAudioSlots[ index ].fBusy = 0;
	giNextAudioSlot = 0;
}

static void QueueAudio( Smack *pSmack )
{
	SGPSmackAudioSlot *pSlot;
	const void *pSamples;
	unsigned long size, rate;
	int channels, bits;
	SndCommand command;

	if ( !gAudioChannel )
		return;
	size = SmackerAudio( pSmack->pPrivate->pDecoder, 0, &pSamples );
	if ( !size || !SmackerAudioInfo( pSmack->pPrivate->pDecoder, 0, &rate, &channels, &bits ) )
		return;
	pSlot = &gAudioSlots[ giNextAudioSlot ];
	if ( pSlot->fBusy )
	{
		++pSmack->pPrivate->uiAudioDropped;
		return;
	}
	if ( size > pSlot->uiCapacity )
	{
		free( pSlot->pData );
		pSlot->pData = (unsigned char *)malloc( size );
		pSlot->uiCapacity = pSlot->pData ? size : 0;
		if ( !pSlot->pData )
		{
			++pSmack->pPrivate->uiAudioDropped;
			return;
		}
	}
	/* The decoder produces native-endian 16-bit or unsigned 8-bit samples,
	   as the Sound Manager expects. */
	memcpy( pSlot->pData, pSamples, size );
	memset( &pSlot->header, 0, sizeof( pSlot->header ) );
	pSlot->header.samplePtr = (Ptr)pSlot->pData;
	pSlot->header.numChannels = channels;
	pSlot->header.sampleRate = (UnsignedFixed)( rate << 16 );
	pSlot->header.encode = extSH;
	pSlot->header.baseFrequency = kMiddleC;
	pSlot->header.numFrames = (long)( size / (unsigned long)( channels * ( bits / 8 ) ) );
	pSlot->header.sampleSize = (short)bits;
	pSlot->fBusy = 1;
	command.cmd = bufferCmd;
	command.param1 = 0;
	command.param2 = (long)&pSlot->header;
	if ( SndDoCommand( gAudioChannel, &command, true ) != noErr )
	{
		pSlot->fBusy = 0;
		++pSmack->pPrivate->uiAudioDropped;
		return;
	}
	command.cmd = callBackCmd;
	command.param1 = (short)giNextAudioSlot;
	command.param2 = SGP_SMACK_AUDIO_CALLBACK;
	SndDoCommand( gAudioChannel, &command, true );
	giNextAudioSlot = ( giNextAudioSlot + 1 ) % SGP_SMACK_AUDIO_SLOTS;
}

#else

static unsigned long long NowMicros( void )
{
	return (unsigned long long)GetTickCount( ) * 1000ULL;
}

static void OpenAudio( Smack *pSmack ) { (void)pSmack; }
static void CloseAudio( void ) { }
static void QueueAudio( Smack *pSmack ) { (void)pSmack; }

#endif

/* Reads from the library file through the Win32 layer at absolute offsets,
   since FileMan shares the library handle with other files. Short reads at
   the end of the archive are fine: the decoder never asks past the movie. */
static int ReadMovie( void *pContext, unsigned long uiOffset, void *pBuffer, unsigned long uiLength )
{
	SGPSmackPrivate *p = (SGPSmackPrivate *)pContext;
	DWORD uiRead = 0;

	if ( uiLength > SGP_SMACK_READ_AHEAD )
	{
		SetFilePointer( p->hFile, (LONG)( p->uiBase + uiOffset ), NULL, FILE_BEGIN );
		return ReadFile( p->hFile, pBuffer, uiLength, &uiRead, NULL ) && uiRead == uiLength;
	}
	if ( uiOffset < p->uiReadAheadOffset || uiOffset + uiLength > p->uiReadAheadOffset + p->uiReadAheadLength )
	{
		SetFilePointer( p->hFile, (LONG)( p->uiBase + uiOffset ), NULL, FILE_BEGIN );
		if ( !ReadFile( p->hFile, p->pReadAhead, SGP_SMACK_READ_AHEAD, &uiRead, NULL ) )
			uiRead = 0;
		p->uiReadAheadOffset = uiOffset;
		p->uiReadAheadLength = uiRead;
		if ( uiRead < uiLength )
			return 0;
	}
	memcpy( pBuffer, p->pReadAhead + ( uiOffset - p->uiReadAheadOffset ), uiLength );
	return 1;
}

Smack *SmackOpen( const char *name, UINT32 uiFlags, UINT32 uiExtraBuf )
{
	Smack *pSmack;
	SGPSmackPrivate *p;
	(void)uiExtraBuf;

	if ( !( uiFlags & SMACKFILEHANDLE ) )
	{
		DebugLog( "SGP smack: only SMACKFILEHANDLE is supported" );
		return NULL;
	}
	pSmack = (Smack *)calloc( 1, sizeof( Smack ) );
	p = (SGPSmackPrivate *)calloc( 1, sizeof( SGPSmackPrivate ) );
	if ( !pSmack || !p || !( p->pReadAhead = (unsigned char *)malloc( SGP_SMACK_READ_AHEAD ) ) )
	{
		free( p ? p->pReadAhead : NULL );
		free( p );
		free( pSmack );
		return NULL;
	}
	pSmack->pPrivate = p;
	p->hFile = (HANDLE)name;
	p->uiBase = SetFilePointer( p->hFile, 0, NULL, FILE_CURRENT );
	p->pDecoder = SmackerOpen( ReadMovie, p );
	if ( !p->pDecoder )
	{
		DebugLog( "SGP smack: not a supported Smacker file" );
		free( p->pReadAhead );
		free( p );
		free( pSmack );
		return NULL;
	}
	pSmack->Width = SmackerWidth( p->pDecoder );
	pSmack->Height = SmackerHeight( p->pDecoder );
	pSmack->Frames = SmackerFrameCount( p->pDecoder );
	pSmack->FrameNum = 0;
	p->uiFrameMicros = SmackerFrameMicros( p->pDecoder );
	DebugLog( "SGP smack: %lux%lu, %lu frames, %lu us per frame", (unsigned long)pSmack->Width,
			  (unsigned long)pSmack->Height, (unsigned long)pSmack->Frames, (unsigned long)p->uiFrameMicros );
	OpenAudio( pSmack );
	return pSmack;
}

void SmackClose( Smack *pSmack )
{
	SGPSmackPrivate *p;
	unsigned long ulElapsed;

	if ( !pSmack )
		return;
	p = pSmack->pPrivate;
	ulElapsed = p->fStarted ? (unsigned long)( ( NowMicros( ) - p->ullStart ) / 1000 ) : 0;
	/* Effective playback rate against the movie's own (fps_x10). */
	DebugLog( "SGP smack: stats frames=%lu/%lu shown=%lu skipped=%lu ms=%lu fps_x10=%lu source_fps_x10=%lu "
			  "audio_dropped=%lu", (unsigned long)SmackerCurrentFrame( p->pDecoder ), (unsigned long)pSmack->Frames,
			  (unsigned long)p->uiShown, (unsigned long)p->uiSkipped, ulElapsed,
			  ulElapsed ? (unsigned long)p->uiShown * 10000UL / ulElapsed : 0,
			  p->uiFrameMicros ? 10000000UL / p->uiFrameMicros : 0, (unsigned long)p->uiAudioDropped );
	CloseAudio( );
	SmackerClose( p->pDecoder );
	free( p->pReadAhead );
	free( p );
	free( pSmack );
}

UINT32 SmackWait( Smack *pSmack )
{
	SGPSmackPrivate *p = pSmack->pPrivate;
	if ( !p->fStarted )
		return 0;
	return NowMicros( ) < p->ullStart + (unsigned long long)pSmack->FrameNum * p->uiFrameMicros;
}

void SmackToBuffer( Smack *pSmack, UINT32 uiLeft, UINT32 uiTop, UINT32 uiPitch, UINT32 uiDestHeight,
					void *pBuffer, UINT32 uiFlags )
{
	SGPSmackPrivate *p = pSmack->pPrivate;
	p->pDest = pBuffer;
	p->uiLeft = uiLeft;
	p->uiTop = uiTop;
	p->uiDestPitch = uiPitch;
	p->uiDestHeight = uiDestHeight;
	p->uiDestFlags = uiFlags;
}

static void ConvertFrame( Smack *pSmack )
{
	SGPSmackPrivate *p = pSmack->pPrivate;
	const unsigned char *pPixels = SmackerPixels( p->pDecoder );
	UINT32 uiWidth = pSmack->Width, uiHeight = pSmack->Height, uiRow;
	int f565 = ( p->uiDestFlags & SMACKBUFFER565 ) == SMACKBUFFER565;

	if ( !p->pDest )
		return;
	if ( !p->fLutValid || SmackerPaletteChanged( p->pDecoder ) )
	{
		const unsigned char *pPalette = SmackerPalette( p->pDecoder );
		int index;
		for ( index = 0; index < 256; ++index )
		{
			UINT32 r = pPalette[ index * 3 ], g = pPalette[ index * 3 + 1 ], b = pPalette[ index * 3 + 2 ];
			p->usLut[ index ] = f565 ? (UINT16)( ( ( r >> 3 ) << 11 ) | ( ( g >> 2 ) << 5 ) | ( b >> 3 ) )
									 : (UINT16)( ( ( r >> 3 ) << 10 ) | ( ( g >> 3 ) << 5 ) | ( b >> 3 ) );
		}
		p->fLutValid = 1;
	}
	if ( p->uiLeft + uiWidth > 640 )
		uiWidth = p->uiLeft < 640 ? 640 - p->uiLeft : 0;
	if ( uiHeight > p->uiDestHeight )
		uiHeight = p->uiDestHeight;
	for ( uiRow = 0; uiRow < uiHeight; ++uiRow )
	{
		const unsigned char *pSource = pPixels + uiRow * pSmack->Width;
		UINT16 *pLine = (UINT16 *)( (UINT8 *)p->pDest + ( p->uiTop + uiRow ) * p->uiDestPitch ) + p->uiLeft;
		UINT32 uiColumn;
		for ( uiColumn = 0; uiColumn < uiWidth; ++uiColumn )
			pLine[ uiColumn ] = p->usLut[ pSource[ uiColumn ] ];
	}
}

UINT32 SmackDoFrame( Smack *pSmack )
{
	SGPSmackPrivate *p = pSmack->pPrivate;
	int fDecoded = 0;

	/* Every frame is a delta, so frames skipped by SmackNextFrame are
	   decoded (and their audio queued) without being converted. */
	JA2_PROF_BEGIN( JA2_PROF_MOVIE );
	while ( SmackerCurrentFrame( p->pDecoder ) <= pSmack->FrameNum )
	{
		if ( !SmackerDecodeFrame( p->pDecoder ) )
			break;
		QueueAudio( pSmack );
		fDecoded = 1;
	}
	JA2_PROF_END( JA2_PROF_MOVIE );
	if ( !p->fStarted )
	{
		/* Frame 0 (and its audio lead-in) defines time zero. */
		p->fStarted = 1;
		p->ullStart = NowMicros( );
	}
	if ( fDecoded )
	{
		ConvertFrame( pSmack );
		++p->uiShown;
	}
	return 0;
}

void SmackNextFrame( Smack *pSmack )
{
	SGPSmackPrivate *p = pSmack->pPrivate;
	UINT32 uiTarget = pSmack->FrameNum + 1;

	if ( p->fStarted && p->uiFrameMicros )
	{
		UINT32 uiDue = (UINT32)( ( NowMicros( ) - p->ullStart ) / p->uiFrameMicros );
		if ( uiDue > uiTarget )
		{
			if ( uiDue > pSmack->FrameNum + SGP_SMACK_MAX_CATCHUP )
				uiDue = pSmack->FrameNum + SGP_SMACK_MAX_CATCHUP;
			p->uiSkipped += uiDue - uiTarget;
			uiTarget = uiDue;
		}
	}
	if ( uiTarget > pSmack->Frames - 1 )
		uiTarget = pSmack->Frames - 1;
	pSmack->FrameNum = uiTarget;
}

UINT32 SmackGoto( Smack *pSmack, UINT32 uiFrame )
{
	/* Only used for looping flics, which JA2 never plays. */
	DebugLog( "SGP smack: SmackGoto(%lu) is not supported", (unsigned long)uiFrame );
	return 0;
}

SmackBuf *SmackBufferOpen( void *hWindow, UINT32 uiBlitType, UINT32 uiWidth, UINT32 uiHeight,
						   UINT32 uiZoomWidth, UINT32 uiZoomHeight )
{
	/* The frames go straight into the frame buffer (SmackToBuffer). */
	SmackBuf *pBuffer = (SmackBuf *)calloc( 1, sizeof( SmackBuf ) );
	(void)hWindow;
	(void)uiBlitType;
	(void)uiZoomWidth;
	(void)uiZoomHeight;
	if ( pBuffer )
	{
		pBuffer->Width = uiWidth;
		pBuffer->Height = uiHeight;
	}
	return pBuffer;
}

void SmackBufferClose( SmackBuf *pBuffer )
{
	free( pBuffer );
}

void SmackUseMMX( UINT32 uiFlag )
{
	(void)uiFlag;
}

UINT8 SmackSoundUseMSS( void *pDigitalDriver )
{
	/* The audio track gets its own Sound Manager channel. */
	(void)pDigitalDriver;
	return 1;
}
