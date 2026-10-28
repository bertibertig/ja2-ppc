#ifdef JA2_PPC
/* JA2 PPC: zlib is only reachable through STCI_ZLIB_COMPRESSED images, and
 * none of the 7524 STI files in the game data uses that flag (checked
 * 2026-10-28), so the Mac build has no zlib. The entry points fail loudly
 * instead of linking a compressor nobody calls. */
	#include "types.h"
	#include "Compression.h"

void DebugLog( const char *format, ... );

PTR DecompressInit( BYTE * pCompressedData, UINT32 uiDataSize )
{
	DebugLog( "SGP: zlib decompression requested (%lu bytes) but not built in", (unsigned long)uiDataSize );
	return( NULL );
}

UINT32 Decompress( PTR pDecompPtr, BYTE * pBuffer, UINT32 uiBufferLen )
{
	return( 0 );
}

void DecompressFini( PTR pDecompPtr )
{
}

PTR CompressInit( BYTE * pUncompressedData, UINT32 uiDataSize )
{
	DebugLog( "SGP: zlib compression requested but not built in" );
	return( NULL );
}

UINT32 Compress( PTR pCompPtr, BYTE * pBuffer, UINT32 uiBufferLen )
{
	return( 0 );
}

void CompressFini( PTR pCompPtr )
{
}

UINT32 CompressedBufferSize( UINT32 uiDataSize )
{
	return( uiDataSize + uiDataSize / 1000 + 13 );
}

#else

#ifdef JA2_PRECOMPILED_HEADERS
	#include "JA2 SGP ALL.H"
#elif defined( WIZ8_PRECOMPILED_HEADERS )
	#include "WIZ8 SGP ALL.H"
#else
	#include "MemMan.h"
	#include "debug.h"
	#include "zlib.h"
#endif


// mem allocation functions for ZLIB's purposes

voidpf ZAlloc( voidpf opaque, uInt items, uInt size )
{
  return( MemAlloc( items * size ) );
}

void ZFree( voidpf opaque, voidpf address )
{
	MemFree( address );
}

PTR DecompressInit( BYTE * pCompressedData, UINT32 uiDataSize )
{
	z_stream *	pZStream;
	int					iZRetCode;
	
	// allocate memory for the z_stream struct
	pZStream = MemAlloc( sizeof( z_stream ) );
	if( pZStream == NULL )
	{ // out of memory!
		return( NULL );
	}

	// initial defines
	pZStream->zalloc = ZAlloc;
	pZStream->zfree = ZFree;
	pZStream->opaque = NULL;
	
	// call the ZLIB init routine
	iZRetCode = inflateInit( pZStream );
	if( iZRetCode != Z_OK )
	{ // ZLIB init error!
		MemFree( pZStream );
		return( NULL );
	}
	
	// set up our parameters
	pZStream->next_in = pCompressedData;
	pZStream->avail_in = uiDataSize;
	return( (PTR) pZStream );
}

UINT32 Decompress( PTR pDecompPtr, BYTE * pBuffer, UINT32 uiBufferLen )
{
	int					iZRetCode;
	z_stream *	pZStream = (z_stream *) pDecompPtr;

	// these assertions is in here to ensure that we get passed a proper z_stream pointer
	Assert( pZStream != NULL );
	Assert( pZStream->zalloc == ZAlloc );

	if (pZStream->avail_in == 0)
	{ // There is nothing left to decompress!
		return( 0 );
	}

	// set up the z_stream with our parameters
	pZStream->next_out = pBuffer;
	pZStream->avail_out = uiBufferLen;

	// decompress!
	iZRetCode = inflate( pZStream, Z_PARTIAL_FLUSH );
	Assert( iZRetCode == Z_OK || iZRetCode == Z_STREAM_END );
	
	return( uiBufferLen - pZStream->avail_out );
}

void DecompressFini( PTR pDecompPtr )
{
	z_stream *	pZStream = (z_stream *) pDecompPtr;

	// these assertions is in here to ensure that we get passed a proper z_stream pointer
	Assert( pZStream != NULL );
	Assert( pZStream->zalloc == ZAlloc );

	inflateEnd( pZStream );
	MemFree( pZStream );
}

UINT32 CompressedBufferSize( UINT32 uiDataSize )
{ // Function that calculates the worst-case buffer size needed to
	// hold uiDataSize bytes compressed

	return( uiDataSize + uiDataSize / 10 + 13 );
}

PTR CompressInit( BYTE * pUncompressedData, UINT32 uiDataSize )
{
	z_stream *	pZStream;
	int					iZRetCode;
	
	// allocate memory for the z_stream struct
	pZStream = MemAlloc( sizeof( z_stream ) );
	if( pZStream == NULL )
	{ // out of memory!
		return( NULL );
	}

	// initial defines
	pZStream->zalloc = ZAlloc;
	pZStream->zfree = ZFree;
	pZStream->opaque = NULL;
	
	// call the ZLIB init routine
	iZRetCode = deflateInit( pZStream, Z_BEST_COMPRESSION );
	if( iZRetCode != Z_OK )
	{ // ZLIB init error!
		MemFree( pZStream );
		return( NULL );
	}
	
	// set up our parameters
	pZStream->next_in = pUncompressedData;
	pZStream->avail_in = uiDataSize;
	return( (PTR) pZStream );
}

UINT32 Compress( PTR pCompPtr, BYTE * pBuffer, UINT32 uiBufferLen )
{
	int					iZRetCode;
	z_stream *	pZStream = (z_stream *) pCompPtr;

	// these assertions is in here to ensure that we get passed a proper z_stream pointer
	Assert( pZStream != NULL );
	Assert( pZStream->zalloc == ZAlloc );

	if (pZStream->avail_in == 0)
	{ // There is nothing left to compress!
		return( 0 );
	}

	// set up the z_stream with our parameters
	pZStream->next_out = pBuffer;
	pZStream->avail_out = uiBufferLen;

	// decompress!
	iZRetCode = deflate( pZStream, Z_FINISH );
	Assert( iZRetCode == Z_STREAM_END );
	
	return( uiBufferLen - pZStream->avail_out );
}

void CompressFini( PTR pCompPtr )
{
	z_stream *	pZStream = (z_stream *) pCompPtr;

	// these assertions is in here to ensure that we get passed a proper z_stream pointer
	Assert( pZStream != NULL );
	Assert( pZStream->zalloc == ZAlloc );

	deflateEnd( pZStream );
	MemFree( pZStream );
}

#endif // JA2_PPC
