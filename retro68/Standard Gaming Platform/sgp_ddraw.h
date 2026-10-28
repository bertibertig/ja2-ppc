#ifndef SGP_DDRAW_H
#define SGP_DDRAW_H

/* JA2 PPC: a software DirectDraw for the Standard Gaming Platform.
 *
 * The x86 SGP kept every video surface in a DirectDraw surface and blitted
 * through "DirectDraw Calls.c". On the Mac a surface is a block of memory
 * (8 or 16 bits per pixel, RGB555 like the window surface) and the
 * IDirectDraw* methods those files call are plain C functions
 * (sgp_ddraw.c) with DirectDraw's semantics (e.g. exclusive right/bottom
 * edges). gfDontUseDDBlits stays FALSE as on Windows: the original's
 * software fallback differs from DirectDraw in details (its colour fill
 * includes the right and bottom edge), and players saw the DirectDraw
 * behaviour. The window itself is owned by the shell (SDL). */

#include "sgp_win32.h"
#include "types.h"

typedef INT32 HRESULT;
typedef void *HDC;
typedef void IUnknown;
#define FAR

typedef struct SGPDDObject SGPDDObject, *LPDIRECTDRAW, *LPDIRECTDRAW2;
typedef struct SGPDDSurface SGPDDSurface, *LPDIRECTDRAWSURFACE, *LPDIRECTDRAWSURFACE2;
typedef struct SGPDDPalette SGPDDPalette, *LPDIRECTDRAWPALETTE;
typedef struct SGPDDClipper SGPDDClipper, *LPDIRECTDRAWCLIPPER;

typedef struct
{
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
} RECT, *LPRECT;


typedef struct
{
	BYTE peRed;
	BYTE peGreen;
	BYTE peBlue;
	BYTE peFlags;
} PALETTEENTRY, *LPPALETTEENTRY;

typedef struct
{
	DWORD dwSize;
	DWORD iType;
	DWORD nCount;
	DWORD nRgnSize;
	RECT rcBound;
} RGNDATAHEADER;

typedef struct
{
	RGNDATAHEADER rdh;
	char Buffer[ 1 ];
} RGNDATA, *LPRGNDATA;

typedef struct
{
	DWORD dwColorSpaceLowValue;
	DWORD dwColorSpaceHighValue;
} DDCOLORKEY, *LPDDCOLORKEY;

typedef struct
{
	DWORD dwCaps;
} DDSCAPS, *LPDDSCAPS;

typedef struct
{
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwFourCC;
	DWORD dwRGBBitCount;
	DWORD dwRBitMask;
	DWORD dwGBitMask;
	DWORD dwBBitMask;
	DWORD dwRGBAlphaBitMask;
} DDPIXELFORMAT;

typedef struct
{
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwHeight;
	DWORD dwWidth;
	LONG lPitch;
	DWORD dwBackBufferCount;
	DWORD dwZBufferBitDepth;
	DWORD dwAlphaBitDepth;
	DWORD dwRefreshRate;
	LPVOID lpSurface;
	DDCOLORKEY ddckCKDestOverlay;
	DDCOLORKEY ddckCKDestBlt;
	DDCOLORKEY ddckCKSrcOverlay;
	DDCOLORKEY ddckCKSrcBlt;
	DDPIXELFORMAT ddpfPixelFormat;
	DDSCAPS ddsCaps;
} DDSURFACEDESC, *LPDDSURFACEDESC;

typedef struct
{
	DWORD dwSize;
	DWORD dwDDFX;
	DWORD dwROP;
	DWORD dwFillColor;
	DDCOLORKEY ddckDestColorkey;
	DDCOLORKEY ddckSrcColorkey;
} DDBLTFX, *LPDDBLTFX;

#define DD_OK 0
#define CLR_INVALID 0xFFFFFFFFUL
#define RDH_RECTANGLES 1
#define DDERR_GENERIC ( (HRESULT)0x80004005L )
#define DDERR_INVALIDPARAMS ( (HRESULT)0x80070057L )
#define DDERR_OUTOFMEMORY ( (HRESULT)0x8007000EL )
#define DDERR_WASSTILLDRAWING ( (HRESULT)0x8876021CL )
#define DDERR_SURFACELOST ( (HRESULT)0x887601C2L )
#define DDERR_NOCOLORKEY ( (HRESULT)0x887600D7L )

#define DDSD_CAPS 0x00000001UL
#define DDSD_HEIGHT 0x00000002UL
#define DDSD_WIDTH 0x00000004UL
#define DDSD_PITCH 0x00000008UL
#define DDSD_BACKBUFFERCOUNT 0x00000020UL
#define DDSD_ZBUFFERBITDEPTH 0x00000040UL
#define DDSD_CKSRCBLT 0x00010000UL
#define DDSD_PIXELFORMAT 0x00001000UL
#define DDSD_LPSURFACE 0x00000800UL

#define DDSCAPS_3DDEVICE 0x00002000UL
#define DDSCAPS_BACKBUFFER 0x00000004UL
#define DDSCAPS_COMPLEX 0x00000008UL
#define DDSCAPS_FLIP 0x00000010UL
#define DDSCAPS_OFFSCREENPLAIN 0x00000040UL
#define DDSCAPS_PRIMARYSURFACE 0x00000200UL
#define DDSCAPS_SYSTEMMEMORY 0x00000800UL
#define DDSCAPS_VIDEOMEMORY 0x00004000UL
#define DDSCAPS_ZBUFFER 0x00020000UL

#define DDPF_RGB 0x00000040UL
#define DDPF_PALETTEINDEXED8 0x00000020UL

#define DDPCAPS_8BIT 0x00000004UL
#define DDPCAPS_ALLOW256 0x00000040UL

#define DDLOCK_WAIT 0x00000001UL
#define DDLOCK_SURFACEMEMORYPTR 0x00000000UL
#define DDLOCK_NOSYSLOCK 0x00000800UL

#define DDBLT_COLORFILL 0x00000400UL
#define DDBLT_DDFX 0x00000800UL
#define DDBLT_KEYDEST 0x00002000UL
#define DDBLT_KEYSRC 0x00008000UL
#define DDBLT_WAIT 0x01000000UL
#define DDBLTFX_MIRRORUPDOWN 0x00000004UL
#define DDBLTFX_MIRRORLEFTRIGHT 0x00000002UL

#define DDBLTFAST_NOCOLORKEY 0x00000000UL
#define DDBLTFAST_SRCCOLORKEY 0x00000001UL
#define DDBLTFAST_DESTCOLORKEY 0x00000002UL
#define DDBLTFAST_WAIT 0x00000010UL

#define DDFLIP_WAIT 0x00000001UL
#define DDSCL_NORMAL 0x00000008UL
#define DDSCL_EXCLUSIVE 0x00000010UL
#define DDSCL_FULLSCREEN 0x00000001UL

#define DDCKEY_DESTBLT 0x00000002UL
#define DDCKEY_SRCBLT 0x00000008UL

#ifdef __cplusplus
extern "C" {
#endif

/* Port helpers used by video.c. The primary surface of a flip chain is
   handed to this hook after every Flip; the Mac shell (ja2_mac.c) sets it
   to present the pixels in its SDL window. */
typedef void ( *SGPDDPresentProc )( const UINT16 *pPixels, UINT32 uiPitch, UINT32 uiWidth, UINT32 uiHeight );
extern SGPDDPresentProc gpSGPDDPresent;
#define SGPDD_SCREEN_WIDTH 640
#define SGPDD_SCREEN_HEIGHT 480
HRESULT SGPDD_DirectDrawCreate( void *pGuid, LPDIRECTDRAW *ppDD, IUnknown *pOuter );
HRESULT SGPDDS_GetAttachedSurface( LPDIRECTDRAWSURFACE2 pSurface, LPDDSCAPS pCaps, LPDIRECTDRAWSURFACE2 *ppAttached );
HRESULT SGPDDS_Flip( LPDIRECTDRAWSURFACE pSurface, LPDIRECTDRAWSURFACE pOverride, DWORD dwFlags );

/* Port helpers used by video.c. */
LPDIRECTDRAWSURFACE2 SGPDDCreateMemorySurface( UINT32 uiWidth, UINT32 uiHeight, UINT32 uiBitDepth );
void *SGPDDSurfacePixels( LPDIRECTDRAWSURFACE2 pSurface, UINT32 *puiPitch );
LPDIRECTDRAW2 SGPDDGetDirectDraw( void );

HRESULT SGPDD_CreateSurface( LPDIRECTDRAW2 pDD, LPDDSURFACEDESC pDesc, LPDIRECTDRAWSURFACE *ppSurface, IUnknown *pOuter );
HRESULT SGPDD_CreatePalette( LPDIRECTDRAW2 pDD, DWORD dwFlags, LPPALETTEENTRY pEntries, LPDIRECTDRAWPALETTE *ppPalette, IUnknown *pOuter );
HRESULT SGPDD_CreateClipper( LPDIRECTDRAW2 pDD, DWORD dwFlags, LPDIRECTDRAWCLIPPER *ppClipper, IUnknown *pOuter );

HRESULT SGPDDS_QueryInterface( LPDIRECTDRAWSURFACE pSurface, const void *riid, void **ppOut );
HRESULT SGPDDS_Release( LPDIRECTDRAWSURFACE2 pSurface );
HRESULT SGPDDS_Lock( LPDIRECTDRAWSURFACE2 pSurface, LPRECT pRect, LPDDSURFACEDESC pDesc, DWORD dwFlags, HANDLE hEvent );
HRESULT SGPDDS_Unlock( LPDIRECTDRAWSURFACE2 pSurface, LPVOID pData );
HRESULT SGPDDS_GetSurfaceDesc( LPDIRECTDRAWSURFACE2 pSurface, LPDDSURFACEDESC pDesc );
HRESULT SGPDDS_GetCaps( LPDIRECTDRAWSURFACE2 pSurface, LPDDSCAPS pCaps );
HRESULT SGPDDS_SetColorKey( LPDIRECTDRAWSURFACE2 pSurface, DWORD dwFlags, LPDDCOLORKEY pKey );
HRESULT SGPDDS_GetColorKey( LPDIRECTDRAWSURFACE2 pSurface, DWORD dwFlags, LPDDCOLORKEY pKey );
HRESULT SGPDDS_SetPalette( LPDIRECTDRAWSURFACE2 pSurface, LPDIRECTDRAWPALETTE pPalette );
HRESULT SGPDDS_GetPalette( LPDIRECTDRAWSURFACE2 pSurface, LPDIRECTDRAWPALETTE *ppPalette );
HRESULT SGPDDS_SetClipper( LPDIRECTDRAWSURFACE2 pSurface, LPDIRECTDRAWCLIPPER pClipper );
HRESULT SGPDDS_Restore( LPDIRECTDRAWSURFACE2 pSurface );
HRESULT SGPDDS_BltFast( LPDIRECTDRAWSURFACE2 pDest, DWORD x, DWORD y, LPDIRECTDRAWSURFACE2 pSrc, LPRECT pSrcRect, DWORD dwTrans );
HRESULT SGPDDS_Blt( LPDIRECTDRAWSURFACE2 pDest, LPRECT pDestRect, LPDIRECTDRAWSURFACE2 pSrc, LPRECT pSrcRect, DWORD dwFlags, LPDDBLTFX pFx );
HRESULT SGPDDS_GetDC( LPDIRECTDRAWSURFACE2 pSurface, HDC *phDC );
HRESULT SGPDDS_ReleaseDC( LPDIRECTDRAWSURFACE2 pSurface, HDC hDC );
HRESULT SGPDDS_GetDDInterface( LPDIRECTDRAWSURFACE2 pSurface, LPVOID *ppDD );
HRESULT SGPDDS_AttachedSurface( LPDIRECTDRAWSURFACE2 pParent, LPDIRECTDRAWSURFACE2 pChild );

HRESULT SGPDDP_SetEntries( LPDIRECTDRAWPALETTE pPalette, DWORD dwFlags, DWORD dwStart, DWORD dwCount, LPPALETTEENTRY pEntries );
HRESULT SGPDDP_GetEntries( LPDIRECTDRAWPALETTE pPalette, DWORD dwFlags, DWORD dwStart, DWORD dwCount, LPPALETTEENTRY pEntries );
HRESULT SGPDDP_Release( LPDIRECTDRAWPALETTE pPalette );
HRESULT SGPDDC_SetClipList( LPDIRECTDRAWCLIPPER pClipper, LPRGNDATA pList, DWORD dwFlags );
HRESULT SGPDDC_Release( LPDIRECTDRAWCLIPPER pClipper );

#ifdef __cplusplus
}
#endif

#define DirectDrawCreate( a, b, c )                             SGPDD_DirectDrawCreate( a, b, c )
#define IDirectDraw_QueryInterface( p, a, b )                   ( *(b) = (LPVOID)(p), DD_OK )
#define IDirectDraw2_SetCooperativeLevel( p, a, b )             ( (void)(p), (void)(a), (void)(b), DD_OK )
#define IDirectDraw2_SetDisplayMode( p, a, b, c, d, e )         ( (void)(p), DD_OK )
#define IDirectDraw2_RestoreDisplayMode( p )                    ( (void)(p), DD_OK )
#define IDirectDraw2_Release( p )                               ( (void)(p), DD_OK )
#define IDirectDraw_Release( p )                                ( (void)(p), DD_OK )
#define IDirectDraw_CreatePalette( p, a, b, c, d )              SGPDD_CreatePalette( p, a, b, c, d )
#define IDirectDrawSurface_SetPalette( p, a )                   SGPDDS_SetPalette( p, a )
#define IDirectDrawSurface_Flip( p, a, b )                      SGPDDS_Flip( p, a, b )
#define IDirectDrawSurface_Blt( p, a, b, c, d, e )              SGPDDS_Blt( p, a, b, c, d, e )
#define IDirectDrawSurface2_GetAttachedSurface( p, a, b )       SGPDDS_GetAttachedSurface( p, a, b )
#define IDirectDraw2_CreateSurface( p, a, b, c )                SGPDD_CreateSurface( p, a, b, c )
#define IDirectDraw2_CreatePalette( p, a, b, c, d )             SGPDD_CreatePalette( p, a, b, c, d )
#define IDirectDraw2_CreateClipper( p, a, b, c )                SGPDD_CreateClipper( p, a, b, c )
#define IDirectDrawSurface_QueryInterface( p, a, b )            SGPDDS_QueryInterface( p, a, (void **)( b ) )
#define IDirectDrawSurface_Release( p )                         SGPDDS_Release( p )
#define IDirectDrawSurface2_Release( p )                        SGPDDS_Release( p )
#define IDirectDrawSurface2_Lock( p, a, b, c, d )               SGPDDS_Lock( p, a, b, c, d )
#define IDirectDrawSurface2_Unlock( p, a )                      SGPDDS_Unlock( p, a )
#define IDirectDrawSurface2_GetSurfaceDesc( p, a )              SGPDDS_GetSurfaceDesc( p, a )
#define IDirectDrawSurface2_GetCaps( p, a )                     SGPDDS_GetCaps( p, a )
#define IDirectDrawSurface2_SetColorKey( p, a, b )              SGPDDS_SetColorKey( p, a, b )
#define IDirectDrawSurface2_GetColorKey( p, a, b )              SGPDDS_GetColorKey( p, a, b )
#define IDirectDrawSurface2_SetPalette( p, a )                  SGPDDS_SetPalette( p, a )
#define IDirectDrawSurface2_GetPalette( p, a )                  SGPDDS_GetPalette( p, a )
#define IDirectDrawSurface2_SetClipper( p, a )                  SGPDDS_SetClipper( p, a )
#define IDirectDrawSurface2_Restore( p )                        SGPDDS_Restore( p )
#define IDirectDrawSurface2_BltFast( p, a, b, c, d, e )         SGPDDS_BltFast( p, a, b, c, d, e )
#define IDirectDrawSurface2_Blt( p, a, b, c, d, e )             SGPDDS_Blt( p, a, b, c, d, e )
#define IDirectDrawSurface2_GetDC( p, a )                       SGPDDS_GetDC( p, a )
#define IDirectDrawSurface2_ReleaseDC( p, a )                   SGPDDS_ReleaseDC( p, a )
#define IDirectDrawSurface2_GetDDInterface( p, a )              SGPDDS_GetDDInterface( p, (LPVOID *)( a ) )
#define IDirectDrawSurface2_AddAttachedSurface( p, a )          SGPDDS_AttachedSurface( p, a )
#define IDirectDrawSurface2_DeleteAttachedSurface( p, a, b )    SGPDDS_AttachedSurface( p, b )
#define IDirectDrawPalette_SetEntries( p, a, b, c, d )          SGPDDP_SetEntries( p, a, b, c, d )
#define IDirectDrawPalette_GetEntries( p, a, b, c, d )          SGPDDP_GetEntries( p, a, b, c, d )
#define IDirectDrawPalette_Release( p )                         SGPDDP_Release( p )
#define IDirectDrawClipper_SetClipList( p, a, b )               SGPDDC_SetClipList( p, a, b )
#define IDirectDrawClipper_Release( p )                         SGPDDC_Release( p )

/* COM interface ids are irrelevant here; QueryInterface returns the same
   object. They exist because the original passes their addresses. */
extern const int IID_IDirectDrawSurface2;
extern const int IID_IDirectDraw2;

/* Defined by sgp.c on Windows (FALSE unless the INI asks otherwise). */
extern BOOLEAN gfDontUseDDBlits;

#endif
