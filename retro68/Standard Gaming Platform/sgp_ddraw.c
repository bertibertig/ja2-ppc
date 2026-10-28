#include "sgp_ddraw.h"

#include <stdlib.h>
#include <string.h>

void DebugLog( const char *format, ... );

struct SGPDDObject
{
	int iUnused;
};

struct SGPDDPalette
{
	PALETTEENTRY Entries[ 256 ];
};

struct SGPDDClipper
{
	RECT Bounds;
	BOOLEAN fHasBounds;
};

struct SGPDDSurface
{
	UINT32 uiWidth;
	UINT32 uiHeight;
	UINT32 uiBitDepth;
	UINT32 uiPitch;
	UINT8 *pPixels;
	DWORD dwCaps;
	DDCOLORKEY SrcColorKey;
	BOOLEAN fHasSrcColorKey;
	LPDIRECTDRAWPALETTE pPalette;
	LPDIRECTDRAWCLIPPER pClipper;
	INT32 iRefCount;
	struct SGPDDSurface *pAttached;
};

static SGPDDObject gDirectDraw;
const int IID_IDirectDrawSurface2 = 2;
const int IID_IDirectDraw2 = 1;
BOOLEAN gfDontUseDDBlits = FALSE;
SGPDDPresentProc gpSGPDDPresent;

/* The SGP's pixel format: RGB555 for 16-bit surfaces (see video.c). */
static void FillPixelFormat( DDPIXELFORMAT *pFormat, UINT32 uiBitDepth )
{
	memset( pFormat, 0, sizeof( *pFormat ) );
	pFormat->dwSize = sizeof( *pFormat );
	pFormat->dwRGBBitCount = uiBitDepth;
	if( uiBitDepth == 8 )
	{
		pFormat->dwFlags = DDPF_RGB | DDPF_PALETTEINDEXED8;
	}
	else
	{
		pFormat->dwFlags = DDPF_RGB;
		pFormat->dwRBitMask = 0x7C00;
		pFormat->dwGBitMask = 0x03E0;
		pFormat->dwBBitMask = 0x001F;
	}
}

LPDIRECTDRAW2 SGPDDGetDirectDraw( void )
{
	return( &gDirectDraw );
}

LPDIRECTDRAWSURFACE2 SGPDDCreateMemorySurface( UINT32 uiWidth, UINT32 uiHeight, UINT32 uiBitDepth )
{
	SGPDDSurface *pSurface;

	if( uiWidth == 0 || uiHeight == 0 || ( uiBitDepth != 8 && uiBitDepth != 16 ) )
	{
		return( NULL );
	}
	pSurface = (SGPDDSurface *)calloc( 1, sizeof( *pSurface ) );
	if( pSurface == NULL )
	{
		return( NULL );
	}
	pSurface->iRefCount = 1;
	pSurface->uiWidth = uiWidth;
	pSurface->uiHeight = uiHeight;
	pSurface->uiBitDepth = uiBitDepth;
	pSurface->uiPitch = ( uiWidth * ( uiBitDepth / 8 ) + 3 ) & ~3UL;
	pSurface->pPixels = (UINT8 *)calloc( pSurface->uiPitch, uiHeight );
	if( pSurface->pPixels == NULL )
	{
		DebugLog( "SGP DD: out of memory for %lux%lux%lu surface", (unsigned long)uiWidth,
		          (unsigned long)uiHeight, (unsigned long)uiBitDepth );
		free( pSurface );
		return( NULL );
	}
	return( pSurface );
}

void *SGPDDSurfacePixels( LPDIRECTDRAWSURFACE2 pSurface, UINT32 *puiPitch )
{
	if( pSurface == NULL )
	{
		return( NULL );
	}
	if( puiPitch )
	{
		*puiPitch = pSurface->uiPitch;
	}
	return( pSurface->pPixels );
}

HRESULT SGPDD_CreateSurface( LPDIRECTDRAW2 pDD, LPDDSURFACEDESC pDesc, LPDIRECTDRAWSURFACE *ppSurface, IUnknown *pOuter )
{
	UINT32 uiBitDepth = 16;

	(void)pDD;
	(void)pOuter;
	if( pDesc == NULL || ppSurface == NULL )
	{
		return( DDERR_INVALIDPARAMS );
	}
	if( ( pDesc->dwFlags & DDSD_PIXELFORMAT ) && pDesc->ddpfPixelFormat.dwRGBBitCount != 0 )
	{
		uiBitDepth = pDesc->ddpfPixelFormat.dwRGBBitCount;
	}
	if( pDesc->ddsCaps.dwCaps & DDSCAPS_ZBUFFER )
	{
		uiBitDepth = 16;
	}
	if( pDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE )
	{
		/* The primary surface is the 640x480 screen; a flip chain gets an
		   attached back buffer of the same size. */
		*ppSurface = SGPDDCreateMemorySurface( SGPDD_SCREEN_WIDTH, SGPDD_SCREEN_HEIGHT, 16 );
		if( *ppSurface && ( pDesc->ddsCaps.dwCaps & DDSCAPS_FLIP ) )
		{
			( *ppSurface )->pAttached = SGPDDCreateMemorySurface( SGPDD_SCREEN_WIDTH, SGPDD_SCREEN_HEIGHT, 16 );
			if( ( *ppSurface )->pAttached )
			{
				( *ppSurface )->pAttached->dwCaps = DDSCAPS_BACKBUFFER;
			}
		}
	}
	else
	{
		*ppSurface = SGPDDCreateMemorySurface( pDesc->dwWidth, pDesc->dwHeight, uiBitDepth );
	}
	if( *ppSurface == NULL )
	{
		return( DDERR_OUTOFMEMORY );
	}
	( *ppSurface )->dwCaps = pDesc->ddsCaps.dwCaps;
	return( DD_OK );
}

/* Blitters whose x86 code has no C port yet (vobject_blitters.c). Logged
   once each, so a screen that needs one shows up in JA2PPC-Debug.log. */
void SGPBlitterNotPorted( const char *pName )
{
	static const char *pLogged[ 32 ];
	static int iLogged;
	int iIndex;

	for( iIndex = 0; iIndex < iLogged; iIndex++ )
	{
		if( pLogged[ iIndex ] == pName )
			return;
	}
	if( iLogged < 32 )
		pLogged[ iLogged++ ] = pName;
	DebugLog( "SGP: blitter %s is not ported yet", pName );
}

HRESULT SGPDD_DirectDrawCreate( void *pGuid, LPDIRECTDRAW *ppDD, IUnknown *pOuter )
{
	(void)pGuid;
	(void)pOuter;
	*ppDD = &gDirectDraw;
	return( DD_OK );
}

HRESULT SGPDDS_GetAttachedSurface( LPDIRECTDRAWSURFACE2 pSurface, LPDDSCAPS pCaps, LPDIRECTDRAWSURFACE2 *ppAttached )
{
	(void)pCaps;
	if( pSurface == NULL || pSurface->pAttached == NULL )
	{
		return( DDERR_GENERIC );
	}
	/* COM semantics: the caller gets its own reference. */
	*ppAttached = pSurface->pAttached;
	pSurface->pAttached->iRefCount++;
	return( DD_OK );
}

/* Like a hardware flip: the back buffer's pixels become the visible
   primary surface and the old front pixels end up in the back buffer
   (RefreshScreen restores it afterwards, as on Windows). */
HRESULT SGPDDS_Flip( LPDIRECTDRAWSURFACE pSurface, LPDIRECTDRAWSURFACE pOverride, DWORD dwFlags )
{
	UINT8 *pPixels;

	(void)pOverride;
	(void)dwFlags;
	if( pSurface == NULL || pSurface->pAttached == NULL )
	{
		return( DDERR_GENERIC );
	}
	pPixels = pSurface->pPixels;
	pSurface->pPixels = pSurface->pAttached->pPixels;
	pSurface->pAttached->pPixels = pPixels;
	if( gpSGPDDPresent )
	{
		gpSGPDDPresent( (const UINT16 *)pSurface->pPixels, pSurface->uiPitch, pSurface->uiWidth, pSurface->uiHeight );
	}
	return( DD_OK );
}

HRESULT SGPDD_CreatePalette( LPDIRECTDRAW2 pDD, DWORD dwFlags, LPPALETTEENTRY pEntries, LPDIRECTDRAWPALETTE *ppPalette, IUnknown *pOuter )
{
	(void)pDD;
	(void)dwFlags;
	(void)pOuter;
	*ppPalette = (SGPDDPalette *)calloc( 1, sizeof( SGPDDPalette ) );
	if( *ppPalette == NULL )
	{
		return( DDERR_OUTOFMEMORY );
	}
	if( pEntries )
	{
		memcpy( ( *ppPalette )->Entries, pEntries, sizeof( ( *ppPalette )->Entries ) );
	}
	return( DD_OK );
}

HRESULT SGPDD_CreateClipper( LPDIRECTDRAW2 pDD, DWORD dwFlags, LPDIRECTDRAWCLIPPER *ppClipper, IUnknown *pOuter )
{
	(void)pDD;
	(void)dwFlags;
	(void)pOuter;
	*ppClipper = (SGPDDClipper *)calloc( 1, sizeof( SGPDDClipper ) );
	return( *ppClipper ? DD_OK : DDERR_OUTOFMEMORY );
}

HRESULT SGPDDS_QueryInterface( LPDIRECTDRAWSURFACE pSurface, const void *riid, void **ppOut )
{
	/* The IDirectDrawSurface2 interface is the same object; like COM,
	   QueryInterface adds a reference that the second Release drops. */
	(void)riid;
	*ppOut = pSurface;
	if( pSurface )
	{
		pSurface->iRefCount++;
	}
	return( DD_OK );
}

HRESULT SGPDDS_Release( LPDIRECTDRAWSURFACE2 pSurface )
{
	if( pSurface && --pSurface->iRefCount <= 0 )
	{
		if( pSurface->pAttached )
		{
			SGPDDS_Release( pSurface->pAttached );
		}
		free( pSurface->pPixels );
		pSurface->pPixels = NULL;
		free( pSurface );
	}
	return( DD_OK );
}

HRESULT SGPDDS_Lock( LPDIRECTDRAWSURFACE2 pSurface, LPRECT pRect, LPDDSURFACEDESC pDesc, DWORD dwFlags, HANDLE hEvent )
{
	(void)dwFlags;
	(void)hEvent;
	if( pSurface == NULL || pDesc == NULL )
	{
		return( DDERR_INVALIDPARAMS );
	}
	SGPDDS_GetSurfaceDesc( pSurface, pDesc );
	pDesc->lpSurface = pSurface->pPixels;
	if( pRect )
	{
		pDesc->lpSurface = pSurface->pPixels + pRect->top * pSurface->uiPitch +
		                   pRect->left * ( pSurface->uiBitDepth / 8 );
	}
	return( DD_OK );
}

HRESULT SGPDDS_Unlock( LPDIRECTDRAWSURFACE2 pSurface, LPVOID pData )
{
	(void)pSurface;
	(void)pData;
	return( DD_OK );
}

HRESULT SGPDDS_GetSurfaceDesc( LPDIRECTDRAWSURFACE2 pSurface, LPDDSURFACEDESC pDesc )
{
	if( pSurface == NULL || pDesc == NULL )
	{
		return( DDERR_INVALIDPARAMS );
	}
	memset( pDesc, 0, sizeof( *pDesc ) );
	pDesc->dwSize = sizeof( *pDesc );
	pDesc->dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT;
	pDesc->dwWidth = pSurface->uiWidth;
	pDesc->dwHeight = pSurface->uiHeight;
	pDesc->lPitch = (LONG)pSurface->uiPitch;
	pDesc->ddsCaps.dwCaps = pSurface->dwCaps;
	pDesc->ddckCKSrcBlt = pSurface->SrcColorKey;
	FillPixelFormat( &pDesc->ddpfPixelFormat, pSurface->uiBitDepth );
	return( DD_OK );
}

HRESULT SGPDDS_GetCaps( LPDIRECTDRAWSURFACE2 pSurface, LPDDSCAPS pCaps )
{
	pCaps->dwCaps = pSurface ? pSurface->dwCaps : 0;
	return( DD_OK );
}

HRESULT SGPDDS_SetColorKey( LPDIRECTDRAWSURFACE2 pSurface, DWORD dwFlags, LPDDCOLORKEY pKey )
{
	if( pSurface == NULL )
	{
		return( DDERR_INVALIDPARAMS );
	}
	if( dwFlags & DDCKEY_SRCBLT )
	{
		pSurface->fHasSrcColorKey = pKey != NULL;
		if( pKey )
		{
			pSurface->SrcColorKey = *pKey;
		}
	}
	return( DD_OK );
}

HRESULT SGPDDS_GetColorKey( LPDIRECTDRAWSURFACE2 pSurface, DWORD dwFlags, LPDDCOLORKEY pKey )
{
	if( pSurface == NULL || !( dwFlags & DDCKEY_SRCBLT ) || !pSurface->fHasSrcColorKey )
	{
		return( DDERR_NOCOLORKEY );
	}
	*pKey = pSurface->SrcColorKey;
	return( DD_OK );
}

HRESULT SGPDDS_SetPalette( LPDIRECTDRAWSURFACE2 pSurface, LPDIRECTDRAWPALETTE pPalette )
{
	if( pSurface )
	{
		pSurface->pPalette = pPalette;
	}
	return( DD_OK );
}

HRESULT SGPDDS_GetPalette( LPDIRECTDRAWSURFACE2 pSurface, LPDIRECTDRAWPALETTE *ppPalette )
{
	*ppPalette = pSurface ? pSurface->pPalette : NULL;
	return( *ppPalette ? DD_OK : DDERR_GENERIC );
}

HRESULT SGPDDS_SetClipper( LPDIRECTDRAWSURFACE2 pSurface, LPDIRECTDRAWCLIPPER pClipper )
{
	if( pSurface )
	{
		pSurface->pClipper = pClipper;
	}
	return( DD_OK );
}

HRESULT SGPDDS_Restore( LPDIRECTDRAWSURFACE2 pSurface )
{
	/* Memory surfaces are never lost. */
	(void)pSurface;
	return( DD_OK );
}

static BOOLEAN ClipCopy( LPDIRECTDRAWSURFACE2 pDest, INT32 *piX, INT32 *piY, LPDIRECTDRAWSURFACE2 pSrc, RECT *pSrc_ )
{
	if( pSrc_->left < 0 ) pSrc_->left = 0;
	if( pSrc_->top < 0 ) pSrc_->top = 0;
	if( pSrc_->right > (LONG)pSrc->uiWidth ) pSrc_->right = (LONG)pSrc->uiWidth;
	if( pSrc_->bottom > (LONG)pSrc->uiHeight ) pSrc_->bottom = (LONG)pSrc->uiHeight;
	if( *piX < 0 ) { pSrc_->left -= *piX; *piX = 0; }
	if( *piY < 0 ) { pSrc_->top -= *piY; *piY = 0; }
	if( *piX + ( pSrc_->right - pSrc_->left ) > (LONG)pDest->uiWidth )
		pSrc_->right = pSrc_->left + (LONG)pDest->uiWidth - *piX;
	if( *piY + ( pSrc_->bottom - pSrc_->top ) > (LONG)pDest->uiHeight )
		pSrc_->bottom = pSrc_->top + (LONG)pDest->uiHeight - *piY;
	return( pSrc_->right > pSrc_->left && pSrc_->bottom > pSrc_->top );
}

HRESULT SGPDDS_BltFast( LPDIRECTDRAWSURFACE2 pDest, DWORD x, DWORD y, LPDIRECTDRAWSURFACE2 pSrc, LPRECT pSrcRect, DWORD dwTrans )
{
	RECT Src;
	INT32 iX = (INT32)x;
	INT32 iY = (INT32)y;
	INT32 iRow;
	UINT32 uiBytes;
	BOOLEAN fKey;
	UINT16 usKey;

	if( pDest == NULL || pSrc == NULL || pDest->uiBitDepth != pSrc->uiBitDepth )
	{
		return( DDERR_INVALIDPARAMS );
	}
	if( pSrcRect )
	{
		Src = *pSrcRect;
	}
	else
	{
		Src.left = 0;
		Src.top = 0;
		Src.right = (LONG)pSrc->uiWidth;
		Src.bottom = (LONG)pSrc->uiHeight;
	}
	if( !ClipCopy( pDest, &iX, &iY, pSrc, &Src ) )
	{
		return( DD_OK );
	}
	uiBytes = pSrc->uiBitDepth / 8;
	fKey = ( dwTrans & DDBLTFAST_SRCCOLORKEY ) && pSrc->fHasSrcColorKey;
	usKey = (UINT16)pSrc->SrcColorKey.dwColorSpaceLowValue;
	for( iRow = 0; iRow < Src.bottom - Src.top; iRow++ )
	{
		UINT8 *pS = pSrc->pPixels + ( Src.top + iRow ) * pSrc->uiPitch + Src.left * uiBytes;
		UINT8 *pD = pDest->pPixels + ( iY + iRow ) * pDest->uiPitch + iX * uiBytes;
		INT32 iWidth = Src.right - Src.left;

		if( !fKey )
		{
			memmove( pD, pS, (size_t)iWidth * uiBytes );
		}
		else if( uiBytes == 2 )
		{
			INT32 iCol;
			for( iCol = 0; iCol < iWidth; iCol++ )
			{
				if( ( (UINT16 *)pS )[ iCol ] != usKey )
					( (UINT16 *)pD )[ iCol ] = ( (UINT16 *)pS )[ iCol ];
			}
		}
		else
		{
			INT32 iCol;
			for( iCol = 0; iCol < iWidth; iCol++ )
			{
				if( pS[ iCol ] != (UINT8)usKey )
					pD[ iCol ] = pS[ iCol ];
			}
		}
	}
	return( DD_OK );
}

HRESULT SGPDDS_Blt( LPDIRECTDRAWSURFACE2 pDest, LPRECT pDestRect, LPDIRECTDRAWSURFACE2 pSrc, LPRECT pSrcRect, DWORD dwFlags, LPDDBLTFX pFx )
{
	RECT Dest;
	RECT Src;
	INT32 iRow;
	UINT32 uiBytes;

	if( pDest == NULL )
	{
		return( DDERR_INVALIDPARAMS );
	}
	uiBytes = pDest->uiBitDepth / 8;
	if( pDestRect )
	{
		Dest = *pDestRect;
	}
	else
	{
		Dest.left = 0;
		Dest.top = 0;
		Dest.right = (LONG)pDest->uiWidth;
		Dest.bottom = (LONG)pDest->uiHeight;
	}
	if( dwFlags & DDBLT_COLORFILL )
	{
		if( Dest.left < 0 ) Dest.left = 0;
		if( Dest.top < 0 ) Dest.top = 0;
		if( Dest.right > (LONG)pDest->uiWidth ) Dest.right = (LONG)pDest->uiWidth;
		if( Dest.bottom > (LONG)pDest->uiHeight ) Dest.bottom = (LONG)pDest->uiHeight;
		for( iRow = Dest.top; iRow < Dest.bottom; iRow++ )
		{
			UINT8 *pD = pDest->pPixels + iRow * pDest->uiPitch + Dest.left * uiBytes;
			INT32 iCol;

			for( iCol = 0; iCol < Dest.right - Dest.left; iCol++ )
			{
				if( uiBytes == 2 )
					( (UINT16 *)pD )[ iCol ] = (UINT16)pFx->dwFillColor;
				else
					pD[ iCol ] = (UINT8)pFx->dwFillColor;
			}
		}
		return( DD_OK );
	}
	if( pSrc == NULL || pSrc->uiBitDepth != pDest->uiBitDepth )
	{
		return( DDERR_INVALIDPARAMS );
	}
	if( pSrcRect )
	{
		Src = *pSrcRect;
	}
	else
	{
		Src.left = 0;
		Src.top = 0;
		Src.right = (LONG)pSrc->uiWidth;
		Src.bottom = (LONG)pSrc->uiHeight;
	}
	{
		/* Nearest-neighbour stretch, the only scaling DirectDraw's
		   software emulation offered as well. */
		INT32 iDestWidth = Dest.right - Dest.left;
		INT32 iDestHeight = Dest.bottom - Dest.top;
		INT32 iSrcWidth = Src.right - Src.left;
		INT32 iSrcHeight = Src.bottom - Src.top;
		BOOLEAN fKey = ( dwFlags & DDBLT_KEYSRC ) && pSrc->fHasSrcColorKey;
		BOOLEAN fMirror = ( dwFlags & DDBLT_DDFX ) && pFx && ( pFx->dwDDFX & DDBLTFX_MIRRORUPDOWN );
		UINT16 usKey = (UINT16)pSrc->SrcColorKey.dwColorSpaceLowValue;

		if( iDestWidth <= 0 || iDestHeight <= 0 || iSrcWidth <= 0 || iSrcHeight <= 0 )
		{
			return( DD_OK );
		}
		for( iRow = 0; iRow < iDestHeight; iRow++ )
		{
			INT32 iDY = Dest.top + iRow;
			INT32 iSY = Src.top + ( fMirror ? ( iDestHeight - 1 - iRow ) : iRow ) * iSrcHeight / iDestHeight;
			INT32 iCol;

			if( iDY < 0 || iDY >= (INT32)pDest->uiHeight || iSY < 0 || iSY >= (INT32)pSrc->uiHeight )
				continue;
			for( iCol = 0; iCol < iDestWidth; iCol++ )
			{
				INT32 iDX = Dest.left + iCol;
				INT32 iSX = Src.left + iCol * iSrcWidth / iDestWidth;

				if( iDX < 0 || iDX >= (INT32)pDest->uiWidth || iSX < 0 || iSX >= (INT32)pSrc->uiWidth )
					continue;
				if( uiBytes == 2 )
				{
					UINT16 usPixel = ( (UINT16 *)( pSrc->pPixels + iSY * pSrc->uiPitch ) )[ iSX ];
					if( !fKey || usPixel != usKey )
						( (UINT16 *)( pDest->pPixels + iDY * pDest->uiPitch ) )[ iDX ] = usPixel;
				}
				else
				{
					UINT8 ubPixel = pSrc->pPixels[ iSY * pSrc->uiPitch + iSX ];
					if( !fKey || ubPixel != (UINT8)usKey )
						pDest->pPixels[ iDY * pDest->uiPitch + iDX ] = ubPixel;
				}
			}
		}
	}
	return( DD_OK );
}

HRESULT SGPDDS_GetDC( LPDIRECTDRAWSURFACE2 pSurface, HDC *phDC )
{
	/* GDI text on surfaces is a Windows feature (WinFont.c); not available. */
	(void)pSurface;
	*phDC = NULL;
	return( DDERR_GENERIC );
}

HRESULT SGPDDS_ReleaseDC( LPDIRECTDRAWSURFACE2 pSurface, HDC hDC )
{
	(void)pSurface;
	(void)hDC;
	return( DD_OK );
}

HRESULT SGPDDS_GetDDInterface( LPDIRECTDRAWSURFACE2 pSurface, LPVOID *ppDD )
{
	(void)pSurface;
	*ppDD = &gDirectDraw;
	return( DD_OK );
}

HRESULT SGPDDS_AttachedSurface( LPDIRECTDRAWSURFACE2 pParent, LPDIRECTDRAWSURFACE2 pChild )
{
	(void)pParent;
	(void)pChild;
	return( DD_OK );
}

HRESULT SGPDDP_SetEntries( LPDIRECTDRAWPALETTE pPalette, DWORD dwFlags, DWORD dwStart, DWORD dwCount, LPPALETTEENTRY pEntries )
{
	(void)dwFlags;
	if( pPalette == NULL || dwStart >= 256 )
	{
		return( DDERR_INVALIDPARAMS );
	}
	if( dwStart + dwCount > 256 )
	{
		dwCount = 256 - dwStart;
	}
	memcpy( &pPalette->Entries[ dwStart ], pEntries, dwCount * sizeof( PALETTEENTRY ) );
	return( DD_OK );
}

HRESULT SGPDDP_GetEntries( LPDIRECTDRAWPALETTE pPalette, DWORD dwFlags, DWORD dwStart, DWORD dwCount, LPPALETTEENTRY pEntries )
{
	(void)dwFlags;
	if( pPalette == NULL || dwStart >= 256 )
	{
		return( DDERR_INVALIDPARAMS );
	}
	if( dwStart + dwCount > 256 )
	{
		dwCount = 256 - dwStart;
	}
	memcpy( pEntries, &pPalette->Entries[ dwStart ], dwCount * sizeof( PALETTEENTRY ) );
	return( DD_OK );
}

HRESULT SGPDDP_Release( LPDIRECTDRAWPALETTE pPalette )
{
	free( pPalette );
	return( DD_OK );
}

HRESULT SGPDDC_SetClipList( LPDIRECTDRAWCLIPPER pClipper, LPRGNDATA pList, DWORD dwFlags )
{
	(void)dwFlags;
	if( pClipper && pList )
	{
		pClipper->Bounds = pList->rdh.rcBound;
		pClipper->fHasBounds = TRUE;
	}
	return( DD_OK );
}

HRESULT SGPDDC_Release( LPDIRECTDRAWCLIPPER pClipper )
{
	free( pClipper );
	return( DD_OK );
}
