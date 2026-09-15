/*
 * Off-screen QuickDraw text rasterizer for the SDL2 rendering backend.
 * See ja2_ppc_text.h for why this exists.
 */

#include "ja2_ppc_text.h"
#include <string.h>

#define JA2_TEXT_WORLD_WIDTH 640
#define JA2_TEXT_WORLD_HEIGHT 40
/* xRGB1555 white: used as the offscreen background/transparency key. Real
   glyphs are always drawn in black, so this key never collides with them. */
#define JA2_TEXT_BACKGROUND_PIXEL 0x7fff

static GWorldPtr gFontWorld;

static Boolean EnsureFontWorld(void)
{
    Rect bounds;
    QDErr error;

    if (gFontWorld != NULL) {
        return 1;
    }
    SetRect(&bounds, 0, 0, JA2_TEXT_WORLD_WIDTH, JA2_TEXT_WORLD_HEIGHT);
    error = NewGWorld(&gFontWorld, 16, &bounds, NULL, NULL, 0);
    return error == noErr && gFontWorld != NULL;
}

void JA2TextInit(void)
{
    EnsureFontWorld();
}

void JA2TextShutdown(void)
{
    if (gFontWorld != NULL) {
        DisposeGWorld(gFontWorld);
        gFontWorld = NULL;
    }
}

static short MakePascalString(Str255 out, const char *text)
{
    short length = (short)strlen(text);

    if (length > 255) {
        length = 255;
    }
    out[0] = (unsigned char)length;
    memcpy(&out[1], text, (size_t)length);
    return length;
}

short JA2TextWidth(const char *text, short size)
{
    Str255 pascalText;
    CGrafPtr savedPort;
    GDHandle savedDevice;
    short width;

    if (!EnsureFontWorld()) {
        return 0;
    }
    MakePascalString(pascalText, text);
    GetGWorld(&savedPort, &savedDevice);
    SetGWorld(gFontWorld, NULL);
    TextFont(systemFont);
    TextSize(size);
    width = StringWidth(pascalText);
    SetGWorld(savedPort, savedDevice);
    return width;
}

short JA2TextHeight(short size)
{
    CGrafPtr savedPort;
    GDHandle savedDevice;
    FontInfo info;

    if (!EnsureFontWorld()) {
        return size;
    }
    GetGWorld(&savedPort, &savedDevice);
    SetGWorld(gFontWorld, NULL);
    TextFont(systemFont);
    TextSize(size);
    GetFontInfo(&info);
    SetGWorld(savedPort, savedDevice);
    return (short)(info.ascent + info.descent + info.leading);
}

void JA2DrawText(SDL_Surface *destination, short x, short y, const char *text,
                 Uint16 color, short size)
{
    Str255 pascalText;
    short length;
    CGrafPtr savedPort;
    GDHandle savedDevice;
    FontInfo info;
    short width;
    short height;
    PixMapHandle pixMap;
    Uint8 *srcBase;
    long srcRowBytes;
    Uint8 *dstBase;
    int dstPitch;
    short row;
    short col;

    if (destination == NULL || text == NULL || text[0] == 0) {
        return;
    }
    if (!EnsureFontWorld()) {
        return;
    }
    length = MakePascalString(pascalText, text);
    if (length == 0) {
        return;
    }

    GetGWorld(&savedPort, &savedDevice);
    SetGWorld(gFontWorld, NULL);
    TextFont(systemFont);
    TextSize(size);
    TextMode(srcCopy);
    GetFontInfo(&info);
    width = StringWidth(pascalText);
    height = (short)(info.ascent + info.descent);
    if (width <= 0 || width > JA2_TEXT_WORLD_WIDTH ||
        height <= 0 || height > JA2_TEXT_WORLD_HEIGHT) {
        SetGWorld(savedPort, savedDevice);
        return;
    }

    pixMap = GetGWorldPixMap(gFontWorld);
    if (pixMap == NULL || !LockPixels(pixMap)) {
        SetGWorld(savedPort, savedDevice);
        return;
    }
    BackColor(whiteColor);
    EraseRect(&((GrafPtr)gFontWorld)->portRect);
    ForeColor(blackColor);
    MoveTo(0, info.ascent);
    DrawString(pascalText);

    srcBase = (Uint8 *)GetPixBaseAddr(pixMap);
    srcRowBytes = (**pixMap).rowBytes & 0x3fff;

    if (SDL_MUSTLOCK(destination)) {
        SDL_LockSurface(destination);
    }
    dstBase = (Uint8 *)destination->pixels;
    dstPitch = destination->pitch;
    for (row = 0; row < height; ++row) {
        short destRow = (short)(y + row);
        Uint16 *srcRow;

        if (destRow < 0 || destRow >= destination->h) {
            continue;
        }
        srcRow = (Uint16 *)(srcBase + (long)row * srcRowBytes);
        for (col = 0; col < width; ++col) {
            short destCol = (short)(x + col);
            Uint16 srcPixel;
            Uint16 *dstPixel;

            if (destCol < 0 || destCol >= destination->w) {
                continue;
            }
            srcPixel = srcRow[col];
            if (srcPixel == JA2_TEXT_BACKGROUND_PIXEL) {
                continue;
            }
            dstPixel = (Uint16 *)(dstBase + (long)destRow * dstPitch + destCol * 2);
            *dstPixel = color;
        }
    }
    if (SDL_MUSTLOCK(destination)) {
        SDL_UnlockSurface(destination);
    }
    UnlockPixels(pixMap);
    SetGWorld(savedPort, savedDevice);
}
