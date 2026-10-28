/*
 * Classic Mac video and event shell for the JA2 PPC port.
 *
 * The original SGP backend is a Win32/DirectDraw application. This file
 * replaces that platform shell with SDL2, using the laulandn/sdl2macos9
 * macosclassic backend (which itself still uses QuickDraw's NewCWindow and
 * a persistent window PixMap internally). See
 * retro68/JA2_PPC_SDL2_MIGRATION_PLAN.md and AGENTS.md's "Rendering Backend
 * (SDL2)" for the architecture this file implements.
 *
 * Every offscreen composition surface (gMenuSurface, gCreditsSurface, the
 * cursor sprite) is a fixed SDL_PIXELFORMAT_RGB555 surface, regardless of
 * the real display depth. Only JA2BlitSurfaceToWindow's SDL_BlitSurface call
 * onto SDL_GetWindowSurface() may cross into the real display format; SDL2
 * performs that conversion automatically. This is the documented fix for
 * the PowerBook G3 Lombard mismatch (16-bit LCD, 32-bit VGA output).
 */

#include <Quickdraw.h>
#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "ja2_music.h"
#include "ja2_slf.h"
#include "ja2_mac_internal.h"
#include "ja2_game_init_options.h"
#include "ja2_options.h"
#include "ja2_ppc_text.h"

#if defined(JA2_LANGUAGE_GERMAN)
#define WINDOW_TITLE "JA2 PPC"
#else
#define WINDOW_TITLE "JA2 PPC"
#endif

SDL_Window *gWindow;
static Boolean gRunning = 1;
static Boolean gMenuBackgroundLoaded = 0;
static Boolean gMenuButtonsLoaded = 0;
SDL_Surface *gMenuSurface;
static SDL_Surface *gCreditsSurface;
static SDL_Surface *gCreditsBackgroundSurface;
static SDL_Surface *gVideoSurface;
static SDL_Surface *gFPSOverlaySurface;
static unsigned char *gMenuButtonData;
static Uint16 gMenuButtonPalette[256];
static unsigned char *gCreditsFacesData;
static unsigned long gCreditsFacesDataLength;
Boolean gActionScreenNeedsRender = 0;
static short gMenuButton = -1;
static short gMenuHoverButton = -1;
static Boolean gStartupActive = 0;
static Uint32 gStartupStartTicks;
static Uint32 gCreditsLastScrollTicks;
static short gCreditsScrollOffset;
static short gCreditsHoverFace = -1;
static short gCreditsLastLoggedScroll = -1;

/* Cursor compositing: a small color-keyed SDL surface blitted onto the
   window surface at the current mouse position on every present, instead of
   the classic Cursor Manager (HideCursor/ShowCursor). This removes that
   whole balance-bug class per AGENTS.md's cursor-handling guidance: the OS
   cursor is disabled once at startup and never re-enabled during play. */
#define JA2_CURSOR_TRANSPARENT_KEY JA2_RGB555(255, 0, 255)
static SDL_Surface *gCursorSurface;
static short gCursorWidth;
static short gCursorHeight;
static int gLastMouseX = -1;
static int gLastMouseY = -1;
static Boolean gMouseMotionNeedsPresent;
static Uint32 gFPSTicks;
static unsigned long gFPSFrames;
static unsigned short gFPSValue;
static unsigned short gFPSOverlayValue = 0xffff;
static Boolean gScreenNeedsPresent;

short gMenuScreen = MENU_SCREEN_MAIN;
void RestoreMainMenu(void);
static void RenderMainMenuDirectly(void);
static void PresentCurrentScreen(void);

typedef struct {
    short x;
    short y;
    short width;
    short height;
    short eyeX;
    short eyeY;
    short blinkFrequency;
    unsigned long lastBlink;
    unsigned long eyesClosedUntil;
    const char *name;
    const char *title;
    const char *aside;
} CreditFace;

static CreditFace gCreditFaces[] = {
    {298, 137, 37, 49, 310, 157, 2500, 0, 0, "Chris Camfield", "Game Internals Programmer", ""},
    {348, 137, 43, 47, 354, 153, 3700, 0, 0, "Shaun Lyng", "Co-designer/Writer", "(still learning punctuation)"},
    {407, 132, 30, 50, 408, 151, 3000, 0, 0, "Kris Marnes", "Strategic Systems & Editor Programmer", "(It's done. I'm just fixing it)"},
    {443, 131, 30, 50, 447, 151, 4000, 0, 0, "Ian Currie", "Producer/Co-designer", "(getting much too old for this)"},
    {487, 136, 43, 50, 493, 155, 3500, 0, 0, "Linda Currie", "Co-designer/Map Designer", "(and working on Wizardry 8)"},
    {529, 145, 43, 50, 536, 164, 4000, 0, 0, "Eric Cheng", "Artist", "(forced at gunpoint to also do QA)"},
    {581, 132, 43, 48, 584, 150, 3500, 0, 0, "Lynn Holowka", "Beta Coordinator, Support", "(Left us for the CFSA - go figure...)"},
    {278, 211, 36, 51, 283, 232, 3700, 0, 0, "Norman Olsen", "Artist Extraordinaire", ""},
    {319, 210, 34, 49, 323, 227, 4000, 0, 0, "George Brooks", "Sound Guru", ""},
    {358, 211, 38, 49, 364, 226, 3600, 0, 0, "Andrew Stacey", "Screen Designer/Artist", "(Dead Head and jazz lover)"},
    {396, 200, 42, 50, 406, 220, 4600, 0, 0, "Scot Loving", "Lead Artist/Animator", "(his real name is Robert)"},
    {444, 202, 43, 51, 452, 220, 2800, 0, 0, "Andrew Emmons", "Lead Programmer", "(the only responsible person)"},
    {493, 188, 36, 51, 501, 207, 4500, 0, 0, "Dave French", "Programmer", "(can now get back to motocrossing)"},
    {531, 199, 47, 56, 541, 221, 4000, 0, 0, "Alex Meduna", "Strategic Systems & Game Balance Programmer", "(stolen from Wizardry 8)"},
    {585, 196, 39, 49, 593, 218, 3500, 0, 0, "Joey Whelan", "Portraits Artist", "(did items and loading screens too!)"}
};

static const char *gCredits[] = {
    "JA2 CREDITS", "Sir-tech Canada", "", "Programming", "Chris Camfield",
    "Shaun Lyng", "Kris Marnes", "Ian Currie", "Dave French", "Alex Meduna",
    "Art", "Eric Cheng", "Linda Currie", "Norman Olsen", "Andrew Stacey",
    "Scot Loving", "Joey Whelan", "Sound", "George Brooks", "Writing and design",
    "Ian Currie", "Shaun Lyng", "The Jagged Alliance 2 team", "",
    "Ported to Macintosh PPC", "by BertisRetroKeller."
};
static const short gCreditsCount = sizeof(gCredits) / sizeof(gCredits[0]);
static SDL_Surface *gCreditsTextSurfaces[sizeof(gCredits) / sizeof(gCredits[0])];

void DebugLog(const char *format, ...)
{
    va_list arguments;
    char line[256];
    short lineLength;
    FILE *file;

    va_start(arguments, format);
    vsprintf(line, format, arguments);
    va_end(arguments);
    lineLength = (short)strlen(line);
    if (lineLength > 250) {
        line[250] = 0;
    }
#if defined(JA2_DEBUG_BUILD)
    file = fopen("JA2PPC-Debug.log", "a");
    if (file != NULL) {
        fputs(line, file);
        fputs("\n", file);
        fclose(file);
    }
#else
    (void)file;
#endif
}

static const char *gRequiredLibraries[] = {
    "Data.slf", "Ambient.slf", "Anims.slf", "BattleSnds.slf",
    "BigItems.slf", "BinaryData.slf", "Cursors.slf", "Faces.slf",
    "Fonts.slf", "Interface.slf", "Laptop.slf", "Maps.slf",
    "MercEdt.slf", "Music.slf", "Npc_Speech.slf", "NpcData.slf",
    "RadarMaps.slf", "Sounds.slf", "Speech.slf", "TileSets.slf",
    "LoadScreens.slf", "Intro.slf"
};

static short CountDataLibraries(void)
{
    JA2SLFArchive *archive;
    short index;
    short present = 0;

    for (index = 0; index < 22; ++index) {
        archive = JA2SLFOpen(gRequiredLibraries[index]);
        if (archive != NULL) {
            ++present;
            JA2SLFClose(archive);
        }
    }
    return present;
}

static unsigned long ReadLittleEndian32(const unsigned char *bytes)
{
    return ((unsigned long)bytes[0]) |
           ((unsigned long)bytes[1] << 8) |
           ((unsigned long)bytes[2] << 16) |
           ((unsigned long)bytes[3] << 24);
}

static unsigned short ReadLittleEndian16(const unsigned char *bytes)
{
    return (unsigned short)(bytes[0] | ((unsigned short)bytes[1] << 8));
}

/* Direct, bounds-checked pixel write into a fixed RGB555 surface. Every
   decoder in this file targets an offscreen surface with this format, so no
   per-call format branch is needed (AGENTS.md color depth policy). */
static void WriteRawPixel(SDL_Surface *surface, short x, short y, Uint16 color)
{
    Uint16 *row;

    if (surface == NULL || x < 0 || y < 0 || x >= surface->w || y >= surface->h) {
        return;
    }
    row = (Uint16 *)((Uint8 *)surface->pixels + (long)y * surface->pitch);
    row[x] = color;
}

static void WriteMenuPixel(SDL_Surface *surface, short x, short y,
                           const RGBColor *color)
{
    WriteRawPixel(surface, x, y,
                  JA2_RGB555(color->red >> 8, color->green >> 8, color->blue >> 8));
}

void FillRectRGB555(SDL_Surface *surface, short left, short top,
                    short right, short bottom, Uint16 color)
{
    SDL_Rect rect;

    rect.x = left;
    rect.y = top;
    rect.w = (int)(right - left);
    rect.h = (int)(bottom - top);
    SDL_FillRect(surface, &rect, color);
}

void FrameRectRGB555(SDL_Surface *surface, short left, short top,
                     short right, short bottom, Uint16 color)
{
    FillRectRGB555(surface, left, top, right, (short)(top + 1), color);
    FillRectRGB555(surface, left, (short)(bottom - 1), right, bottom, color);
    FillRectRGB555(surface, left, top, (short)(left + 1), bottom, color);
    FillRectRGB555(surface, (short)(right - 1), top, right, bottom, color);
}

void DrawLineRGB555(SDL_Surface *surface, short x0, short y0,
                    short x1, short y1, Uint16 color)
{
    int dx = abs((int)(x1 - x0));
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs((int)(y1 - y0));
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int e2;

    for (;;) {
        WriteRawPixel(surface, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 = (short)(x0 + sx);
        }
        if (e2 <= dx) {
            err += dx;
            y0 = (short)(y0 + sy);
        }
    }
}

void DrawCString(SDL_Surface *destination, short x, short y, const char *text,
                 Uint16 color)
{
    JA2DrawText(destination, x, y, text, color, 13);
}

static void DrawCenteredCString(SDL_Surface *destination, short y, const char *text,
                                Uint16 color)
{
    short width = JA2TextWidth(text, 12);
    JA2DrawText(destination, (short)((JA2_SCREEN_WIDTH - width) / 2), y, text, color, 12);
}

static void DrawCreditsColumnString(SDL_Surface *destination, short y, const char *text,
                                    Uint16 color, short size)
{
    short width = JA2TextWidth(text, size);
    JA2DrawText(destination, (short)(10 + (210 - width) / 2), y, text, color, size);
}

static SDL_Surface *GetMenuRenderSurface(void)
{
    if (gMenuScreen == MENU_SCREEN_CREDITS && gCreditsSurface != NULL) {
        return gCreditsSurface;
    }
    return gMenuSurface;
}

static Boolean EnsureMenuSurface(void)
{
    if (gMenuSurface != NULL) {
        return 1;
    }
    gMenuSurface = SDL_CreateRGBSurfaceWithFormat(0, JA2_SCREEN_WIDTH,
        JA2_SCREEN_HEIGHT, 16, SDL_PIXELFORMAT_RGB555);
    if (gMenuSurface == NULL) {
        DebugLog("Renderer: menu surface creation failed: %s", SDL_GetError());
        return 0;
    }
    return 1;
}

static Boolean EnsureCreditsSurface(void)
{
    if (gCreditsSurface != NULL) {
        return 1;
    }
    gCreditsSurface = SDL_CreateRGBSurfaceWithFormat(0, JA2_SCREEN_WIDTH,
        JA2_SCREEN_HEIGHT, 16, SDL_PIXELFORMAT_RGB555);
    if (gCreditsSurface == NULL) {
        DebugLog("Credits: frame surface creation failed: %s", SDL_GetError());
        return 0;
    }
    return 1;
}

static Boolean EnsureCreditsBackground(void)
{
    if (gCreditsBackgroundSurface != NULL) {
        return 1;
    }
    gCreditsBackgroundSurface = SDL_CreateRGBSurfaceWithFormat(0,
        JA2_SCREEN_WIDTH, JA2_SCREEN_HEIGHT, 16, SDL_PIXELFORMAT_RGB555);
    if (gCreditsBackgroundSurface == NULL) {
        DebugLog("Credits: background cache creation failed: %s", SDL_GetError());
        return 0;
    }
    if (!RenderIndexedBackground(gCreditsBackgroundSurface, "Interface.slf",
                                 "CREDITS.STI")) {
        DebugLog("Credits: background cache decode failed");
        SDL_FreeSurface(gCreditsBackgroundSurface);
        gCreditsBackgroundSurface = NULL;
        return 0;
    }
    DebugLog("Credits: cached decoded background surface");
    return 1;
}

static void ReleaseCreditsTextSurfaces(void)
{
    short index;

    for (index = 0; index < gCreditsCount; ++index) {
        if (gCreditsTextSurfaces[index] != NULL) {
            SDL_FreeSurface(gCreditsTextSurfaces[index]);
            gCreditsTextSurfaces[index] = NULL;
        }
    }
}

static Boolean EnsureCreditsTextSurfaces(void)
{
    short index;
    Boolean created = 0;

    for (index = 0; index < gCreditsCount; ++index) {
        short size = (index == 0 || index == 3 || index == 10 ||
                      index == 17 || index == 19) ? 14 : 12;
        Uint16 color = (size == 14) ? JA2_COLOR_RED : JA2_COLOR_WHITE;
        short width;
        short x;

        if (gCreditsTextSurfaces[index] != NULL) {
            continue;
        }
        created = 1;
        gCreditsTextSurfaces[index] = SDL_CreateRGBSurfaceWithFormat(
            0, 210, 24, 16, SDL_PIXELFORMAT_RGB555);
        if (gCreditsTextSurfaces[index] == NULL) {
            DebugLog("Credits: text cache surface creation failed line=%d: %s",
                     index, SDL_GetError());
            ReleaseCreditsTextSurfaces();
            return 0;
        }
        SDL_FillRect(gCreditsTextSurfaces[index], NULL, JA2_RGB555(255, 255, 255));
        if (SDL_SetColorKey(gCreditsTextSurfaces[index], 1,
                            JA2_RGB555(255, 255, 255)) != 0) {
            DebugLog("Credits: text cache color key failed line=%d: %s", index,
                     SDL_GetError());
            ReleaseCreditsTextSurfaces();
            return 0;
        }
        width = JA2TextWidth(gCredits[index], size);
        if (width < 0 || width > 210) {
            DebugLog("Credits: text cache width invalid line=%d width=%d", index,
                     width);
            ReleaseCreditsTextSurfaces();
            return 0;
        }
        x = (short)((210 - width) / 2);
        JA2DrawText(gCreditsTextSurfaces[index], x, 0, gCredits[index], color, size);
    }
    if (created) {
        DebugLog("Credits: cached %d text surfaces", gCreditsCount);
    }
    return 1;
}

static Boolean RenderMenuLogo(void)
{
    JA2SLFArchive *archive;
    unsigned char *fileData;
    unsigned char header[64];
    unsigned char palette[768];
    unsigned char object[16];
    unsigned char *pixels;
    unsigned char *source;
    unsigned long fileOffset;
    unsigned long fileLength;
    unsigned long storedSize;
    unsigned long dataOffset;
    unsigned long dataLength;
    unsigned long sourceUsed;
    unsigned short width;
    unsigned short height;
    unsigned short y;
    unsigned short x;
    unsigned short run;
    unsigned char control;
    unsigned short paletteIndex;
    short offsetX;
    short offsetY;
    RGBColor color;
    SDL_Surface *destination;

    archive = JA2SLFOpen("LoadScreens.slf");
    if (archive == NULL || !JA2SLFFind(archive, "JA2LOGO.STI", &fileOffset,
                                       &fileLength)) {
        if (archive != NULL) {
            JA2SLFClose(archive);
        }
        DebugLog("Menu loader: JA2LOGO.STI not found");
        return 0;
    }
    fileData = (unsigned char *)malloc(fileLength);
    if (fileData == NULL || fileLength < 64 ||
        !JA2SLFRead(archive, fileOffset, fileData, fileLength)) {
        free(fileData);
        JA2SLFClose(archive);
        return 0;
    }
    JA2SLFClose(archive);
    memcpy(header, fileData, sizeof(header));
    if (memcmp(header, "STCI", 4) != 0 ||
        !(ReadLittleEndian32(&header[16]) & 0x0008) ||
        !(ReadLittleEndian32(&header[16]) & 0x0020)) {
        free(fileData);
        return 0;
    }
    storedSize = ReadLittleEndian32(&header[8]);
    height = ReadLittleEndian16(&header[20]);
    width = ReadLittleEndian16(&header[22]);
    if (width == 0 || height == 0 || width > JA2_SCREEN_WIDTH + 2 ||
        height > JA2_SCREEN_HEIGHT + 2 || ReadLittleEndian32(&header[24]) != 256 ||
        fileLength < 64 + 768 + 16) {
        free(fileData);
        return 0;
    }
    memcpy(palette, fileData + 64, sizeof(palette));
    memcpy(object, fileData + 64 + 768, sizeof(object));
    offsetX = (short)ReadLittleEndian16(&object[8]);
    offsetY = (short)ReadLittleEndian16(&object[10]);
    height = ReadLittleEndian16(&object[12]);
    width = ReadLittleEndian16(&object[14]);
    dataOffset = ReadLittleEndian32(&object[0]);
    dataLength = ReadLittleEndian32(&object[4]);
    if (dataLength == 0 || dataOffset > storedSize ||
        dataOffset + dataLength > storedSize ||
        fileLength < 64 + 768 + 16 + storedSize) {
        free(fileData);
        return 0;
    }
    pixels = (unsigned char *)malloc(storedSize);
    if (pixels == NULL) {
        free(fileData);
        return 0;
    }
    memcpy(pixels, fileData + 64 + 768 + 16, storedSize);
    free(fileData);
    source = pixels + dataOffset;
    destination = GetMenuRenderSurface();
    if (destination == NULL) {
        free(pixels);
        return 0;
    }
    DebugLog("Menu loader: rendering JA2 logo object %ux%u at %d,%d",
             width, height, 188 + offsetX, 15 + offsetY);
    for (y = 0; y < height; ++y) {
        x = 0;
        while (x < width &&
               (unsigned long)(source - (pixels + dataOffset)) < dataLength) {
            control = *source++;
            if (control == 0) {
                break;
            }
            run = (unsigned short)(control & 0x7f);
            if (run == 0 || x + run > width) {
                free(pixels);
                return 0;
            }
            if (control & 0x80) {
                x = (unsigned short)(x + run);
            } else {
                sourceUsed = (unsigned long)(source - (pixels + dataOffset));
                if (sourceUsed + run > dataLength) {
                    free(pixels);
                    return 0;
                }
                while (run-- != 0) {
                    paletteIndex = *source++;
                    color.red = (unsigned short)(palette[paletteIndex * 3] << 8);
                    color.green = (unsigned short)(palette[paletteIndex * 3 + 1] << 8);
                    color.blue = (unsigned short)(palette[paletteIndex * 3 + 2] << 8);
                    WriteMenuPixel(destination, (short)(188 + offsetX + x),
                                   (short)(15 + offsetY + y), &color);
                    ++x;
                }
            }
        }
        if (x == width &&
            (unsigned long)(source - (pixels + dataOffset)) < dataLength &&
            *source == 0) {
            ++source;
        }
    }
    free(pixels);
    return 1;
}

/* Render the language splash used by InitJA2SplashScreen() in the original
   SGP.  It is kept separate from the menu logo because the splash belongs to
   the startup state and must disappear before the menu becomes interactive. */
static Boolean RenderStartupSplash(void)
{
    static const char *archiveNames[] = {
#if defined(JA2_LANGUAGE_GERMAN)
        "German.slf", "LoadScreens.slf"
#else
        "Interface.slf", "LoadScreens.slf"
#endif
    };
    static const char *entryNames[] = {
#if defined(JA2_LANGUAGE_GERMAN)
        "SPLASH_GERMAN.STI", "SPLASH.STI"
#else
        "SIRTECHSPLASH.STI", "SIRTECHSPLASH.STI"
#endif
    };
    JA2SLFArchive *archive;
    unsigned char *fileData;
    unsigned char header[64];
    unsigned char palette[768];
    unsigned char object[16];
    unsigned char *pixels;
    unsigned char *source;
    unsigned long fileOffset;
    unsigned long fileLength;
    unsigned long storedSize;
    unsigned long dataOffset;
    unsigned long dataLength;
    unsigned long sourceUsed;
    unsigned short width;
    unsigned short height;
    unsigned short x;
    unsigned short y;
    unsigned short run;
    unsigned char control;
    unsigned short paletteIndex;
    unsigned short pixel;
    short offsetX;
    short offsetY;
    RGBColor color;
    unsigned short candidate;

    archive = NULL;
    for (candidate = 0; candidate < sizeof(archiveNames) / sizeof(archiveNames[0]);
         ++candidate) {
        archive = JA2SLFOpen(archiveNames[candidate]);
        if (archive == NULL) {
            DebugLog("Startup: cannot open %s", archiveNames[candidate]);
            continue;
        }
        if (JA2SLFFind(archive, entryNames[candidate], &fileOffset, &fileLength)) {
            DebugLog("Startup: found %s in %s at %lu length %lu",
                     entryNames[candidate], archiveNames[candidate], fileOffset,
                     fileLength);
            break;
        }
        DebugLog("Startup: %s not found in %s", entryNames[candidate],
                 archiveNames[candidate]);
        JA2SLFClose(archive);
        archive = NULL;
    }
    if (archive == NULL) {
        DebugLog("Startup: no usable logo archive/entry found");
        return 0;
    }
    fileData = (unsigned char *)malloc(fileLength);
    if (fileData == NULL) {
        DebugLog("Startup: unable to allocate %lu logo bytes", fileLength);
        JA2SLFClose(archive);
        return 0;
    }
    if (fileLength < 64 + 768 + 16 ||
        !JA2SLFRead(archive, fileOffset, fileData, fileLength)) {
        DebugLog("Startup: failed reading logo at %lu length %lu", fileOffset,
                 fileLength);
        free(fileData);
        JA2SLFClose(archive);
        return 0;
    }
    JA2SLFClose(archive);
    if (memcmp(fileData, "STCI", 4) != 0) {
        DebugLog("Startup: logo entry has no STCI header");
        free(fileData);
        return 0;
    }

    storedSize = ReadLittleEndian32(&fileData[8]);
    memcpy(header, fileData, sizeof(header));
    width = ReadLittleEndian16(&header[22]);
    height = ReadLittleEndian16(&header[20]);
    if (ReadLittleEndian32(&header[16]) & 0x0004) {
        if (ReadLittleEndian32(&header[24]) != 0xf800 ||
            ReadLittleEndian32(&header[28]) != 0x07e0 ||
            ReadLittleEndian32(&header[32]) != 0x001f || header[44] != 16 ||
            width == 0 || height == 0 || width > JA2_SCREEN_WIDTH ||
            height > JA2_SCREEN_HEIGHT || storedSize != (unsigned long)width * height * 2 ||
            fileLength < 64 + storedSize) {
            DebugLog("Startup: unsupported RGB STCI %ux%u depth=%u", width, height,
                     header[44]);
            free(fileData);
            return 0;
        }
        if (!EnsureMenuSurface()) {
            free(fileData);
            return 0;
        }
        source = fileData + 64;
        DebugLog("Startup: rendering RGB565 logo screen %ux%u (%lu bytes)",
                 width, height, storedSize);
        for (y = 0; y < height; ++y) {
            for (x = 0; x < width; ++x) {
                pixel = ReadLittleEndian16(source);
                source += 2;
                color.red = (unsigned short)(((pixel >> 11) * 65535UL) / 31);
                color.green = (unsigned short)((((pixel >> 5) & 0x3f) * 65535UL) / 63);
                color.blue = (unsigned short)(((pixel & 0x1f) * 65535UL) / 31);
                WriteMenuPixel(gMenuSurface, (short)x, (short)y, &color);
            }
        }
        free(fileData);
        DebugLog("Startup: RGB565 logo render complete");
        return 1;
    }
    if (!(ReadLittleEndian32(&header[16]) & 0x0008) ||
        !(ReadLittleEndian32(&header[16]) & 0x0020) ||
        ReadLittleEndian32(&header[24]) != 256) {
        DebugLog("Startup: unsupported indexed STCI flags=%08lx colors=%lu",
                 ReadLittleEndian32(&header[16]), ReadLittleEndian32(&header[24]));
        free(fileData);
        return 0;
    }

    memcpy(palette, fileData + 64, sizeof(palette));
    memcpy(object, fileData + 64 + 768, sizeof(object));
    offsetX = (short)ReadLittleEndian16(&object[8]);
    offsetY = (short)ReadLittleEndian16(&object[10]);
    height = ReadLittleEndian16(&object[12]);
    width = ReadLittleEndian16(&object[14]);
    dataOffset = ReadLittleEndian32(&object[0]);
    dataLength = ReadLittleEndian32(&object[4]);
    if (width == 0 || height == 0 || dataLength == 0 ||
        offsetX < 0 || offsetY < 0 || offsetX + width > JA2_SCREEN_WIDTH ||
        offsetY + height > JA2_SCREEN_HEIGHT ||
        dataOffset > storedSize || dataOffset + dataLength > storedSize ||
        fileLength < 64 + 768 + 16 + storedSize) {
        DebugLog("Startup: invalid logo object %ux%u offset=%lu length=%lu stored=%lu",
                 width, height, dataOffset, dataLength, storedSize);
        free(fileData);
        return 0;
    }
    pixels = (unsigned char *)malloc(storedSize);
    if (pixels == NULL) {
        DebugLog("Startup: unable to allocate %lu decoded logo bytes", storedSize);
        free(fileData);
        return 0;
    }
    memcpy(pixels, fileData + 64 + 768 + 16, storedSize);
    free(fileData);

    if (!EnsureMenuSurface()) {
        free(pixels);
        return 0;
    }
    source = pixels + dataOffset;
    DebugLog("Startup: rendering logo screen %ux%u", width, height);
    for (y = 0; y < height; ++y) {
        x = 0;
        while (x < width &&
               (unsigned long)(source - (pixels + dataOffset)) < dataLength) {
            control = *source++;
            if (control == 0) break;
            run = (unsigned short)(control & 0x7f);
            if (run == 0 || x + run > width) {
                DebugLog("Startup: bad logo ETRLE run row=%u x=%u run=%u", y, x, run);
                free(pixels);
                return 0;
            }
            if (control & 0x80) {
                x = (unsigned short)(x + run);
            } else {
                sourceUsed = (unsigned long)(source - (pixels + dataOffset));
                if (sourceUsed + run > dataLength) {
                    DebugLog("Startup: truncated logo ETRLE row=%u x=%u run=%u",
                             y, x, run);
                    free(pixels);
                    return 0;
                }
                while (run-- != 0) {
                    paletteIndex = *source++;
                    color.red = (unsigned short)(palette[paletteIndex * 3] << 8);
                    color.green = (unsigned short)(palette[paletteIndex * 3 + 1] << 8);
                    color.blue = (unsigned short)(palette[paletteIndex * 3 + 2] << 8);
                    WriteMenuPixel(gMenuSurface, (short)(offsetX + x),
                                   (short)(offsetY + y), &color);
                    ++x;
                }
            }
        }
        if (x == width &&
            (unsigned long)(source - (pixels + dataOffset)) < dataLength &&
            *source == 0) {
            ++source;
        }
    }
    free(pixels);
    return 1;
}

static Boolean EnsureMenuButtonData(void)
{
    static const unsigned short objectIndex[][2] = {
        { 0, 1 }, { 3, 4 }, { 7, 8 }, { 10, 11 }, { 14, 15 }
    };
    JA2SLFArchive *archive;
    const char *archiveName;
    const char *buttonFileName;
    unsigned char *fileData;
    unsigned long fileOffset;
    unsigned long fileLength;
    unsigned long storedSize;
    unsigned short objectCount;
    unsigned short paletteIndex;

    if (gMenuButtonData != NULL) {
        return 1;
    }
#if defined(JA2_LANGUAGE_GERMAN)
    archiveName = "German.slf";
    buttonFileName = "TITLETEXT_GERMAN.STI";
#else
    archiveName = "LoadScreens.slf";
    buttonFileName = "TITLETEXT.STI";
#endif
    archive = JA2SLFOpen(archiveName);
    if (archive == NULL || !JA2SLFFind(archive, buttonFileName, &fileOffset,
                                       &fileLength)) {
        if (archive != NULL) {
            JA2SLFClose(archive);
        }
        DebugLog("Menu loader: %s not found in %s", buttonFileName, archiveName);
        return 0;
    }
    fileData = (unsigned char *)malloc(fileLength);
    if (fileData == NULL || fileLength < 64 + 768 + 16 ||
        !JA2SLFRead(archive, fileOffset, fileData, fileLength)) {
        free(fileData);
        JA2SLFClose(archive);
        DebugLog("Menu loader: cannot cache %s length=%lu", buttonFileName,
                 fileLength);
        return 0;
    }
    JA2SLFClose(archive);
    storedSize = ReadLittleEndian32(&fileData[8]);
    objectCount = ReadLittleEndian16(&fileData[28]);
    if (memcmp(fileData, "STCI", 4) != 0 || objectCount == 0 ||
        objectCount <= objectIndex[4][1] ||
        ReadLittleEndian32(&fileData[24]) != 256 ||
        fileLength < 64 + 768 + (unsigned long)objectCount * 16 + storedSize) {
        DebugLog("Menu loader: invalid cached %s objects=%u", buttonFileName,
                 objectCount);
        free(fileData);
        return 0;
    }
    for (paletteIndex = 0; paletteIndex < 256; ++paletteIndex) {
        gMenuButtonPalette[paletteIndex] = JA2_RGB555(
            fileData[64 + paletteIndex * 3],
            fileData[64 + paletteIndex * 3 + 1],
            fileData[64 + paletteIndex * 3 + 2]);
    }
    gMenuButtonData = fileData;
    DebugLog("Menu loader: cached %s length=%lu objects=%u", buttonFileName,
             fileLength, objectCount);
    return 1;
}

static Boolean RenderMenuButtonFrames(short firstButton, short lastButton,
                                      short highlightedButton)
{
    static const unsigned short objectIndex[][2] = {
        { 0, 1 }, { 3, 4 }, { 7, 8 }, { 10, 11 }, { 14, 15 }
    };
    unsigned char *fileData;
    unsigned char *pixels;
    unsigned long storedSize;
    unsigned long dataOffset;
    unsigned long dataLength;
    unsigned short imageWidth;
    unsigned short imageHeight;
    unsigned short objectCount;
    unsigned short item;
    unsigned short y;
    unsigned short x;
    unsigned short run;
    unsigned char control;
    unsigned short paletteIndex;
    short offsetX;
    short offsetY;
    SDL_Surface *destination;
    unsigned char *source;

    if (!EnsureMenuButtonData()) {
        return 0;
    }
    fileData = gMenuButtonData;
    storedSize = ReadLittleEndian32(&fileData[8]);
    objectCount = ReadLittleEndian16(&fileData[28]);
    pixels = fileData + 64 + 768 + (unsigned long)objectCount * 16;
    destination = GetMenuRenderSurface();
    if (destination == NULL) {
        return 0;
    }
    for (item = (unsigned short)firstButton; item <= (unsigned short)lastButton; ++item) {
        const unsigned char *object = fileData + 64 + 768 +
                                       (unsigned long)objectIndex[item][highlightedButton == item] * 16;
        dataOffset = ReadLittleEndian32(&object[0]);
        dataLength = ReadLittleEndian32(&object[4]);
        offsetX = (short)ReadLittleEndian16(&object[8]);
        offsetY = (short)ReadLittleEndian16(&object[10]);
        imageHeight = ReadLittleEndian16(&object[12]);
        imageWidth = ReadLittleEndian16(&object[14]);
        if (dataOffset + dataLength > storedSize || imageWidth == 0 ||
        imageHeight == 0) {
            return 0;
        }
        source = pixels + dataOffset;
        for (y = 0; y < imageHeight; ++y) {
            x = 0;
            while (x < imageWidth &&
                   (unsigned long)(source - (pixels + dataOffset)) < dataLength) {
                control = *source++;
                if (control == 0) break;
                run = (unsigned short)(control & 0x7f);
                if (run == 0 || x + run > imageWidth) {
                    return 0;
                }
                if (control & 0x80) {
                    x = (unsigned short)(x + run);
                } else {
                    if ((unsigned long)(source - (pixels + dataOffset)) + run >
                        dataLength) {
                        return 0;
                    }
                    while (run-- != 0) {
                        paletteIndex = *source++;
                        WriteRawPixel(destination,
                                       (short)(320 - imageWidth / 2 + offsetX + x),
                                       (short)(277 + item * 37 + offsetY + y),
                                       gMenuButtonPalette[paletteIndex]);
                        ++x;
                    }
                }
            }
            if (x == imageWidth &&
                (unsigned long)(source - (pixels + dataOffset)) < dataLength &&
                *source == 0) {
                ++source;
            }
        }
    }
    return 1;
}

static Boolean RenderMenuButtons(void)
{
    return RenderMenuButtonFrames(0, 4, gMenuHoverButton);
}

Boolean JA2RenderInterfaceButton(SDL_Surface *destination, short objectIndex,
                                 short x, short y)
{
    JA2SLFArchive *archive;
    unsigned char *fileData;
    unsigned char palette[768];
    unsigned char *pixels;
    unsigned long fileOffset;
    unsigned long fileLength;
    unsigned long storedSize;
    unsigned long dataOffset;
    unsigned long dataLength;
    unsigned short objectCount;
    unsigned short imageWidth;
    unsigned short imageHeight;
    unsigned short row;
    unsigned short column;
    unsigned short run;
    unsigned char control;
    unsigned char *source;

    if (objectIndex < 0 || destination == NULL) return 0;
    archive = JA2SLFOpen("Interface.slf");
    if (archive == NULL || !JA2SLFFind(archive, "PreferencesButtons.sti",
                                       &fileOffset, &fileLength)) {
        if (archive != NULL) JA2SLFClose(archive);
        DebugLog("Button loader: PreferencesButtons.sti unavailable");
        return 0;
    }
    fileData = (unsigned char *)malloc(fileLength);
    if (fileData == NULL || !JA2SLFRead(archive, fileOffset, fileData, fileLength)) {
        free(fileData);
        JA2SLFClose(archive);
        DebugLog("Button loader: cannot read PreferencesButtons.sti length=%lu",
                 fileLength);
        return 0;
    }
    JA2SLFClose(archive);
    storedSize = ReadLittleEndian32(&fileData[8]);
    objectCount = ReadLittleEndian16(&fileData[28]);
    if (memcmp(fileData, "STCI", 4) != 0 || objectCount <= (unsigned short)objectIndex ||
        ReadLittleEndian32(&fileData[24]) != 256 ||
        fileLength < 64 + 768 + (unsigned long)objectCount * 16 + storedSize) {
        free(fileData);
        DebugLog("Button loader: invalid PreferencesButtons.sti object=%d count=%u",
                 objectIndex, objectCount);
        return 0;
    }
    memcpy(palette, fileData + 64, sizeof(palette));
    pixels = fileData + 64 + 768 + (unsigned long)objectCount * 16;
    {
        const unsigned char *object = fileData + 64 + 768 +
                                       (unsigned long)objectIndex * 16;
        dataOffset = ReadLittleEndian32(&object[0]);
        dataLength = ReadLittleEndian32(&object[4]);
        imageHeight = ReadLittleEndian16(&object[12]);
        imageWidth = ReadLittleEndian16(&object[14]);
    }
    if (imageWidth == 0 || imageHeight == 0 || dataOffset + dataLength > storedSize) {
        free(fileData);
        DebugLog("Button loader: invalid object=%d size=%ux%u offset=%lu length=%lu",
                 objectIndex, imageWidth, imageHeight, dataOffset, dataLength);
        return 0;
    }
    source = pixels + dataOffset;
    for (row = 0; row < imageHeight; ++row) {
        column = 0;
        while (column < imageWidth &&
               (unsigned long)(source - (pixels + dataOffset)) < dataLength) {
            control = *source++;
            if (control == 0) break;
            run = (unsigned short)(control & 0x7f);
            if (run == 0 || column + run > imageWidth) {
                free(fileData);
                return 0;
            }
            if (control & 0x80) {
                column = (unsigned short)(column + run);
            } else {
                if ((unsigned long)(source - (pixels + dataOffset)) + run > dataLength) {
                    free(fileData);
                    return 0;
                }
                while (run-- != 0) {
                    RGBColor color;
                    unsigned char paletteIndex = *source++;
                    color.red = (unsigned short)(palette[paletteIndex * 3] << 8);
                    color.green = (unsigned short)(palette[paletteIndex * 3 + 1] << 8);
                    color.blue = (unsigned short)(palette[paletteIndex * 3 + 2] << 8);
                    WriteMenuPixel(destination, (short)(x + column), (short)(y + row), &color);
                    ++column;
                }
            }
        }
        if (column == imageWidth &&
            (unsigned long)(source - (pixels + dataOffset)) < dataLength && *source == 0) {
            ++source;
        }
    }
    free(fileData);
    DebugLog("Button loader: rendered PreferencesButtons object=%d at %d,%d size=%ux%u",
             objectIndex, x, y, imageWidth, imageHeight);
    return 1;
}

static Boolean InstallCursorAsset(const char *archiveName, const char *entryName,
                                  const char *label)
{
    JA2SLFArchive *archive;
    unsigned char *fileData;
    unsigned char *pixels;
    unsigned char *source;
    unsigned long fileOffset;
    unsigned long fileLength;
    unsigned long storedSize;
    unsigned long dataOffset;
    unsigned long dataLength;
    unsigned short objectCount;
    unsigned short width;
    unsigned short height;
    unsigned short row;
    unsigned short column;
    unsigned short run;
    unsigned char control;

    archive = JA2SLFOpen(archiveName);
    if (archive == NULL || !JA2SLFFind(archive, entryName, &fileOffset, &fileLength)) {
        if (archive != NULL) JA2SLFClose(archive);
        DebugLog("Cursor loader: %s unavailable in %s", entryName, archiveName);
        return 0;
    }
    fileData = (unsigned char *)malloc(fileLength);
    if (fileData == NULL || !JA2SLFRead(archive, fileOffset, fileData, fileLength)) {
        free(fileData);
        JA2SLFClose(archive);
        DebugLog("Cursor loader: cannot read %s length=%lu", entryName, fileLength);
        return 0;
    }
    JA2SLFClose(archive);
    storedSize = ReadLittleEndian32(&fileData[8]);
    objectCount = ReadLittleEndian16(&fileData[28]);
    if (memcmp(fileData, "STCI", 4) != 0 || objectCount == 0 ||
        ReadLittleEndian32(&fileData[24]) != 256 ||
        fileLength < 64 + 768 + (unsigned long)objectCount * 16 + storedSize) {
        free(fileData);
        DebugLog("Cursor loader: invalid %s object count=%u", entryName, objectCount);
        return 0;
    }
    {
        const unsigned char *object = fileData + 64 + 768;
        dataOffset = ReadLittleEndian32(&object[0]);
        dataLength = ReadLittleEndian32(&object[4]);
        height = ReadLittleEndian16(&object[12]);
        width = ReadLittleEndian16(&object[14]);
    }
    if (width == 0 || height == 0 || dataOffset + dataLength > storedSize) {
        free(fileData);
        DebugLog("Cursor loader: unsupported cursor size=%ux%u", width, height);
        return 0;
    }

    if (gCursorSurface != NULL) {
        SDL_FreeSurface(gCursorSurface);
        gCursorSurface = NULL;
    }
    gCursorSurface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 16,
        SDL_PIXELFORMAT_RGB555);
    if (gCursorSurface == NULL) {
        DebugLog("Cursor loader: surface creation failed: %s", SDL_GetError());
        free(fileData);
        return 0;
    }
    SDL_FillRect(gCursorSurface, NULL, JA2_CURSOR_TRANSPARENT_KEY);
    if (SDL_SetColorKey(gCursorSurface, 1, JA2_CURSOR_TRANSPARENT_KEY) != 0) {
        DebugLog("Cursor loader: color key setup failed: %s", SDL_GetError());
        SDL_FreeSurface(gCursorSurface);
        gCursorSurface = NULL;
        free(fileData);
        return 0;
    }
    gCursorWidth = (short)width;
    gCursorHeight = (short)height;

    pixels = fileData + 64 + 768 + (unsigned long)objectCount * 16;
    source = pixels + dataOffset;
    for (row = 0; row < height; ++row) {
        column = 0;
        while (column < width &&
               (unsigned long)(source - (pixels + dataOffset)) < dataLength) {
            control = *source++;
            if (control == 0) break;
            run = (unsigned short)(control & 0x7f);
            if (run == 0 || column + run > width) break;
            if (control & 0x80) {
                column = (unsigned short)(column + run);
            } else {
                while (run-- != 0 &&
                       (unsigned long)(source - (pixels + dataOffset)) < dataLength) {
                    unsigned char paletteIndex = *source++;
                    Uint16 color = JA2_RGB555(fileData[64 + paletteIndex * 3],
                                              fileData[64 + paletteIndex * 3 + 1],
                                              fileData[64 + paletteIndex * 3 + 2]);
                    if (color == JA2_CURSOR_TRANSPARENT_KEY) {
                        /* Avoid an opaque cursor pixel colliding with the
                           transparency key by nudging it off-key. */
                        color = (Uint16)(color ^ 0x0001);
                    }
                    WriteRawPixel(gCursorSurface, (short)column, (short)row, color);
                    ++column;
                }
            }
        }
    }
    free(fileData);
    DebugLog("Cursor loader: installed %s size=%ux%u", label, width, height);
    return 1;
}

Boolean JA2InstallNormalCursor(void)
{
    return InstallCursorAsset("Cursors.slf", "CURSOR.STI", "normal cursor");
}

Boolean JA2InstallLaptopCursor(void)
{
    return InstallCursorAsset("Laptop.slf", "LAPTOPCURSOR.STI", "laptop cursor");
}

static void DrawCursorOverlay(SDL_Surface *destination)
{
    int mouseX;
    int mouseY;
    int sourceX;
    int sourceY;
    int width;
    int height;
    SDL_Rect sourceRect;
    SDL_Rect destinationRect;

    if (gCursorSurface == NULL || destination == NULL ||
        gMenuScreen == MENU_SCREEN_INTRO) {
        return;
    }
    SDL_GetMouseState(&mouseX, &mouseY);
    sourceX = mouseX < 0 ? -mouseX : 0;
    sourceY = mouseY < 0 ? -mouseY : 0;
    width = gCursorWidth - sourceX;
    height = gCursorHeight - sourceY;
    if (mouseX + width > destination->w) {
        width = destination->w - mouseX;
    }
    if (mouseY + height > destination->h) {
        height = destination->h - mouseY;
    }
    if (width <= 0 || height <= 0) {
        return;
    }
    sourceRect.x = sourceX;
    sourceRect.y = sourceY;
    sourceRect.w = width;
    sourceRect.h = height;
    destinationRect.x = mouseX < 0 ? 0 : mouseX;
    destinationRect.y = mouseY < 0 ? 0 : mouseY;
    destinationRect.w = width;
    destinationRect.h = height;
    if (SDL_BlitSurface(gCursorSurface, &sourceRect, destination,
                        &destinationRect) != 0) {
        DebugLog("Renderer: cursor blit failed: %s", SDL_GetError());
    }
}

static Boolean EnsureFPSOverlay(void)
{
    if (gFPSOverlaySurface != NULL) {
        return 1;
    }
    gFPSOverlaySurface = SDL_CreateRGBSurfaceWithFormat(0, 104, 24, 16,
                                                         SDL_PIXELFORMAT_RGB555);
    if (gFPSOverlaySurface == NULL) {
        DebugLog("Performance: FPS overlay creation failed: %s", SDL_GetError());
        return 0;
    }
    SDL_FillRect(gFPSOverlaySurface, NULL, JA2_RGB555(255, 255, 255));
    if (SDL_SetColorKey(gFPSOverlaySurface, 1, JA2_RGB555(255, 255, 255)) != 0) {
        DebugLog("Performance: FPS overlay color key failed: %s", SDL_GetError());
        SDL_FreeSurface(gFPSOverlaySurface);
        gFPSOverlaySurface = NULL;
        return 0;
    }
    return 1;
}

static void RecordPresentedFrame(void)
{
    Uint32 now;
    Uint32 elapsed;
    unsigned long frames;

    if (!JA2OptionsShowFPS()) {
        return;
    }
    now = SDL_GetTicks();
    if (gFPSTicks == 0) {
        gFPSTicks = now;
    }
    ++gFPSFrames;
    elapsed = now - gFPSTicks;
    if (elapsed >= 1000) {
        frames = gFPSFrames;
        gFPSValue = (unsigned short)((gFPSFrames * 1000) / elapsed);
        gFPSFrames = 0;
        gFPSTicks = now;
        gFPSOverlayValue = 0xffff;
        DebugLog("Performance: fps=%u successful_presents=%lu", gFPSValue,
                 frames);
    }
}

static void DrawFPSCounter(SDL_Surface *destination)
{
    char text[24];
    SDL_Rect overlayRect;

    if (!JA2OptionsShowFPS() || destination == NULL || !EnsureFPSOverlay()) {
        return;
    }
    if (gFPSOverlayValue != gFPSValue) {
        SDL_FillRect(gFPSOverlaySurface, NULL, JA2_RGB555(255, 255, 255));
        sprintf(text, "FPS: %u", gFPSValue);
        JA2DrawText(gFPSOverlaySurface, 4, 3, text, JA2_COLOR_RED, 12);
        gFPSOverlayValue = gFPSValue;
    }
    overlayRect.x = 0;
    overlayRect.y = 0;
    overlayRect.w = gFPSOverlaySurface->w;
    overlayRect.h = gFPSOverlaySurface->h;
    SDL_BlitSurface(gFPSOverlaySurface, NULL, destination, &overlayRect);
}

void JA2BlitSurfaceToWindow(SDL_Surface *surface, const char *name)
{
    SDL_Surface *windowSurface;

    if (surface == NULL || gWindow == NULL) {
        return;
    }
    windowSurface = SDL_GetWindowSurface(gWindow);
    if (windowSurface == NULL) {
        DebugLog("Renderer: %s SDL_GetWindowSurface failed: %s", name, SDL_GetError());
        return;
    }
    /* The source is already a fixed RGB555 composition surface. Let SDL do
       the single final format conversion instead of copying the whole frame
       through a second RGB555 surface first. */
    if (SDL_BlitSurface(surface, NULL, windowSurface, NULL) != 0) {
        DebugLog("Renderer: %s SDL_BlitSurface failed: %s", name, SDL_GetError());
        return;
    }
    DrawFPSCounter(windowSurface);
    DrawCursorOverlay(windowSurface);
    if (SDL_UpdateWindowSurface(gWindow) != 0) {
        DebugLog("Renderer: %s SDL_UpdateWindowSurface failed: %s", name, SDL_GetError());
        return;
    }
    SDL_GetMouseState(&gLastMouseX, &gLastMouseY);
    gMouseMotionNeedsPresent = 0;
    gScreenNeedsPresent = 0;
    RecordPresentedFrame();
}

Boolean JA2PresentVideoWorld(GWorldPtr world)
{
    PixMapHandle pixMap;
    unsigned char *source;
    unsigned char *destination;
    long sourceRowBytes;
    int row;

    if (world == NULL || gWindow == NULL) {
        return 0;
    }
    if (gVideoSurface == NULL) {
        gVideoSurface = SDL_CreateRGBSurfaceWithFormat(0, JA2_SCREEN_WIDTH,
            JA2_SCREEN_HEIGHT, 16, SDL_PIXELFORMAT_RGB555);
        if (gVideoSurface == NULL) {
            DebugLog("Video: SDL frame surface creation failed: %s", SDL_GetError());
            return 0;
        }
    }
    pixMap = GetGWorldPixMap(world);
    if (pixMap == NULL || !LockPixels(pixMap)) {
        DebugLog("Video: cannot lock QuickTime frame GWorld");
        return 0;
    }
    if (SDL_MUSTLOCK(gVideoSurface)) {
        SDL_LockSurface(gVideoSurface);
    }
    source = (unsigned char *)GetPixBaseAddr(pixMap);
    destination = (unsigned char *)gVideoSurface->pixels;
    sourceRowBytes = (**pixMap).rowBytes & 0x3fff;
    if (sourceRowBytes == JA2_SCREEN_WIDTH * 2 &&
        gVideoSurface->pitch == JA2_SCREEN_WIDTH * 2) {
        memcpy(destination, source, JA2_SCREEN_WIDTH * JA2_SCREEN_HEIGHT * 2);
    } else {
        for (row = 0; row < JA2_SCREEN_HEIGHT; ++row) {
            memcpy(destination + (long)row * gVideoSurface->pitch,
                   source + (long)row * sourceRowBytes,
                   JA2_SCREEN_WIDTH * 2);
        }
    }
    if (SDL_MUSTLOCK(gVideoSurface)) {
        SDL_UnlockSurface(gVideoSurface);
    }
    UnlockPixels(pixMap);
    JA2BlitSurfaceToWindow(gVideoSurface, "intro video frame");
    return 1;
}

void JA2BlitMenuBackground(void)
{
    JA2BlitSurfaceToWindow(gMenuSurface, "menu background");
}

Boolean RenderIndexedBackground(SDL_Surface *destination, const char *archiveName,
                                const char *entryName)
{
    JA2SLFArchive *archive;
    unsigned char *fileData;
    unsigned char header[64];
    unsigned char palette[768];
    unsigned char object[16];
    unsigned char *pixels;
    unsigned long fileOffset;
    unsigned long fileLength;
    unsigned long storedSize;
    unsigned long dataOffset;
    unsigned long dataLength;
    unsigned short width;
    unsigned short height;
    short offsetX;
    short offsetY;
    unsigned short y;
    unsigned short x;
    unsigned short run;
    unsigned char control;
    unsigned char *source;
    RGBColor color;

    if (destination == NULL) {
        return 0;
    }
    archive = JA2SLFOpen(archiveName);
    if (archive == NULL) {
        DebugLog("STCI loader: cannot open %s", archiveName);
        return 0;
    }
    if (!JA2SLFFind(archive, entryName, &fileOffset, &fileLength)) {
        DebugLog("STCI loader: %s not found in %s", entryName, archiveName);
        JA2SLFClose(archive);
        return 0;
    }
    fileData = (unsigned char *)malloc(fileLength);
    if (fileData == NULL || fileLength < 64 ||
        !JA2SLFRead(archive, fileOffset, fileData, fileLength)) {
        DebugLog("Failed reading menu entry at offset=%lu length=%lu", fileOffset, fileLength);
        free(fileData);
        JA2SLFClose(archive);
        return 0;
    }
    JA2SLFClose(archive);
    memcpy(header, fileData, sizeof(header));
    if (memcmp(header, "STCI", 4) != 0 || !(ReadLittleEndian32(&header[16]) & 0x0008) ||
        !(ReadLittleEndian32(&header[16]) & 0x0020)) {
        DebugLog("STCI loader: unsupported %s flags=%08lx", entryName,
                 ReadLittleEndian32(&header[16]));
        free(fileData);
        return 0;
    }

    storedSize = ReadLittleEndian32(&header[8]);
    width = ReadLittleEndian16(&header[22]);
    height = ReadLittleEndian16(&header[20]);
    if (width == 0 || height == 0 || width > JA2_SCREEN_WIDTH + 2 ||
        height > JA2_SCREEN_HEIGHT + 2 || ReadLittleEndian32(&header[24]) != 256 ||
        fileLength < 64 + 768 + 16) {
        DebugLog("Invalid menu header width=%u height=%u colors=%lu", width, height,
                 ReadLittleEndian32(&header[24]));
        free(fileData);
        return 0;
    }
    memcpy(palette, fileData + 64, sizeof(palette));
    memcpy(object, fileData + 64 + 768, sizeof(object));

    dataOffset = ReadLittleEndian32(&object[0]);
    dataLength = ReadLittleEndian32(&object[4]);
    offsetX = (short)ReadLittleEndian16(&object[8]);
    offsetY = (short)ReadLittleEndian16(&object[10]);
    width = ReadLittleEndian16(&object[14]);
    height = ReadLittleEndian16(&object[12]);
    if (width == 0 || height == 0 || dataLength == 0 || dataLength > storedSize || dataOffset > storedSize ||
        dataOffset + dataLength > storedSize ||
        fileLength < 64 + 768 + 16 + storedSize) {
        DebugLog("Invalid menu object offset=%lu length=%lu stored=%lu", dataOffset,
                 dataLength, storedSize);
        free(fileData);
        return 0;
    }
    pixels = (unsigned char *)malloc(storedSize);
    if (pixels == NULL) {
        DebugLog("Unable to allocate/read menu pixels: %lu bytes", storedSize);
        free(fileData);
        return 0;
    }
    memcpy(pixels, fileData + 64 + 768 + 16, storedSize);
    free(fileData);

    source = pixels + dataOffset;
    SDL_FillRect(destination, NULL, 0);
    DebugLog("STCI loader: cleared indexed image destination; rendering %ux%u ETRLE bytes=%lu",
             width, height, dataLength);
    for (y = 0; y < height; ++y) {
        x = 0;
        while (x < width && (unsigned long)(source - (pixels + dataOffset)) < dataLength) {
            control = *source++;
            if (control == 0) {
                break;
            }
            run = (unsigned short)(control & 0x7f);
            if (run == 0 || x + run > width) {
                DebugLog("Bad ETRLE run row=%u x=%u run=%u", y, x, run);
                free(pixels);
                return 0;
            }
            if (control & 0x80) {
                x = (unsigned short)(x + run);
            } else {
                if ((unsigned long)(source - (pixels + dataOffset)) + run > dataLength) {
                    DebugLog("Truncated ETRLE literal row=%u x=%u run=%u", y, x, run);
                    free(pixels);
                    return 0;
                }
                while (run-- != 0) {
                    unsigned short paletteIndex = *source++;
                    color.red = (unsigned short)(palette[paletteIndex * 3] << 8);
                    color.green = (unsigned short)(palette[paletteIndex * 3 + 1] << 8);
                    color.blue = (unsigned short)(palette[paletteIndex * 3 + 2] << 8);
                    WriteMenuPixel(destination, (short)(offsetX + x),
                                   (short)(offsetY + y), &color);
                    ++x;
                }
            }
        }
        if (x == width &&
            (unsigned long)(source - (pixels + dataOffset)) < dataLength &&
            *source == 0) {
            ++source;
        }
    }
    free(pixels);
    return 1;
}

static Boolean RenderMainMenuBackground(void)
{
    return RenderIndexedBackground(gMenuSurface, "LoadScreens.slf", "MAINMENUBACKGROUND.STI");
}

static Boolean EnsureCreditsFaceData(void)
{
    JA2SLFArchive *archive;
    unsigned char *fileData;
    unsigned long offset;
    unsigned long length;
    unsigned long storedSize;
    unsigned short count;

    if (gCreditsFacesData != NULL) {
        return 1;
    }
    archive = JA2SLFOpen("Interface.slf");
    if (archive == NULL || !JA2SLFFind(archive, "CREDIT FACES.STI", &offset,
                                        &length)) {
        DebugLog("Credits: Credit Faces.sti unavailable");
        if (archive != NULL) {
            JA2SLFClose(archive);
        }
        return 0;
    }
    fileData = (unsigned char *)malloc(length);
    if (fileData == NULL || !JA2SLFRead(archive, offset, fileData, length)) {
        DebugLog("Credits: cannot cache Credit Faces.sti length=%lu", length);
        free(fileData);
        JA2SLFClose(archive);
        return 0;
    }
    JA2SLFClose(archive);
    if (length < 64 + 768 || memcmp(fileData, "STCI", 4) != 0) {
        DebugLog("Credits: invalid Credit Faces.sti header");
        free(fileData);
        return 0;
    }
    storedSize = ReadLittleEndian32(&fileData[8]);
    count = ReadLittleEndian16(&fileData[28]);
    if (count < 45 || length < 64 + 768 + (unsigned long)count * 16 + storedSize) {
        DebugLog("Credits: invalid Credit Faces.sti objects=%u length=%lu stored=%lu",
                 count, length, storedSize);
        free(fileData);
        return 0;
    }
    gCreditsFacesData = fileData;
    gCreditsFacesDataLength = length;
    DebugLog("Credits: cached Credit Faces.sti length=%lu objects=%u", length, count);
    return 1;
}

/* The original credits use the first image in each three-frame face group. */
static Boolean RenderCreditFaces(void)
{
    unsigned char *fileData;
    unsigned char palette[768];
    unsigned char *pixels;
    unsigned long length, storedSize, dataOffset, dataLength;
    unsigned short count, face, x, y, run, imageWidth, imageHeight, paletteIndex;
    short offsetX, offsetY;
    unsigned char control;
    const unsigned char *object;
    const unsigned char *source;
    RGBColor color;

    if (!EnsureCreditsFaceData()) return 0;
    fileData = gCreditsFacesData;
    length = gCreditsFacesDataLength;
    storedSize = ReadLittleEndian32(&fileData[8]);
    count = ReadLittleEndian16(&fileData[28]);
    if (count < 45 || length < 64 + 768 + (unsigned long)count * 16 + storedSize) {
        return 0;
    }
    memcpy(palette, fileData + 64, sizeof(palette));
    pixels = fileData + 64 + 768 + (unsigned long)count * 16;
    for (face = 0; face < 15; ++face) {
        if (gCreditFaces[face].eyesClosedUntil == 0) continue;
        object = fileData + 64 + 768 + (unsigned long)(face * 3) * 16;
        dataOffset = ReadLittleEndian32(&object[0]);
        dataLength = ReadLittleEndian32(&object[4]);
        offsetX = (short)ReadLittleEndian16(&object[8]);
        offsetY = (short)ReadLittleEndian16(&object[10]);
        imageWidth = ReadLittleEndian16(&object[14]);
        imageHeight = ReadLittleEndian16(&object[12]);
        if (dataOffset + dataLength > storedSize) continue;
        source = pixels + dataOffset;
        for (y = 0; y < imageHeight; ++y) {
            x = 0;
            while (x < imageWidth && (unsigned long)(source - (pixels + dataOffset)) < dataLength) {
                control = *source++;
                if (control == 0) break;
                run = (unsigned short)(control & 0x7f);
                if (run == 0 || x + run > imageWidth) break;
                if (control & 0x80) {
                    x = (unsigned short)(x + run);
                } else {
                    while (run-- != 0 && (unsigned long)(source - (pixels + dataOffset)) < dataLength) {
                        paletteIndex = *source++;
                        color.red = (unsigned short)(palette[paletteIndex * 3] << 8);
                        color.green = (unsigned short)(palette[paletteIndex * 3 + 1] << 8);
                        color.blue = (unsigned short)(palette[paletteIndex * 3 + 2] << 8);
                        WriteMenuPixel(gCreditsSurface,
                                       (short)(gCreditFaces[face].eyeX + offsetX + x),
                                       (short)(gCreditFaces[face].eyeY + offsetY + y),
                                       &color);
                        ++x;
                    }
                }
            }
            if (x == imageWidth &&
                (unsigned long)(source - (pixels + dataOffset)) < dataLength &&
                *source == 0) {
                ++source;
            }
        }
    }
    return 1;
}

static void DrawCreditsScreen(void)
{
    short index;
    short y;
    Boolean anyEyesClosed = 0;
    Boolean textCached;

    if (!EnsureCreditsSurface() || !EnsureCreditsBackground()) return;
    textCached = EnsureCreditsTextSurfaces();
    if (SDL_BlitSurface(gCreditsBackgroundSurface, NULL, gCreditsSurface, NULL) != 0) {
        DebugLog("Credits: background cache blit failed: %s", SDL_GetError());
        return;
    }
    for (index = 0; index < 15; ++index) {
        if (gCreditFaces[index].eyesClosedUntil != 0) {
            anyEyesClosed = 1;
            break;
        }
    }
    if (anyEyesClosed) RenderCreditFaces();
    for (index = 0; index < gCreditsCount; ++index) {
        Boolean heading = index == 0 || index == 3 || index == 10 || index == 17 || index == 19;
        y = (short)(479 + index * 16 - gCreditsScrollOffset);
        if (y < -18 || y > 480) continue;
        if (textCached) {
            SDL_Rect destination = { 10, y, 0, 0 };
            SDL_BlitSurface(gCreditsTextSurfaces[index], NULL, gCreditsSurface,
                            &destination);
        } else {
            DrawCreditsColumnString(gCreditsSurface, y, gCredits[index],
                                    heading ? JA2_COLOR_RED : JA2_COLOR_WHITE,
                                    heading ? 14 : 12);
        }
    }
    if (gCreditsHoverFace >= 0) {
        DrawCenteredCString(gCreditsSurface, 420, gCreditFaces[gCreditsHoverFace].name, JA2_COLOR_WHITE);
        DrawCenteredCString(gCreditsSurface, 435, gCreditFaces[gCreditsHoverFace].title, JA2_COLOR_WHITE);
        DrawCenteredCString(gCreditsSurface, 450, gCreditFaces[gCreditsHoverFace].aside, JA2_COLOR_WHITE);
    }
    if (gCreditsScrollOffset == 0 ||
        gCreditsScrollOffset >= gCreditsLastLoggedScroll + 60) {
        gCreditsLastLoggedScroll = gCreditsScrollOffset;
        DebugLog("Credits: rendered scroll=%d hover=%d", gCreditsScrollOffset,
                 gCreditsHoverFace);
    }
    JA2BlitSurfaceToWindow(gCreditsSurface, "credits frame");
}

static void EnterCreditsScreen(void)
{
    unsigned short face;
    gMenuScreen = MENU_SCREEN_CREDITS;
    gCreditsLastScrollTicks = SDL_GetTicks();
    gCreditsScrollOffset = 0;
    gCreditsHoverFace = -1;
    gCreditsLastLoggedScroll = -1;
    DebugLog("Credits: entering screen, %d lines and %d face regions",
             gCreditsCount, 15);
    for (face = 0; face < 15; ++face) {
        gCreditFaces[face].lastBlink = SDL_GetTicks() +
            (unsigned long)(rand() % gCreditFaces[face].blinkFrequency);
        gCreditFaces[face].eyesClosedUntil = 0;
    }
    DrawCreditsScreen();
}

static void PollCredits(void)
{
    int mouseX;
    int mouseY;
    short face;
    unsigned short index;
    Uint32 now;
    Boolean needsRedraw = 0;

    if (gMenuScreen != MENU_SCREEN_CREDITS) return;
    now = SDL_GetTicks();
    for (index = 0; index < 15; ++index) {
        if (gCreditFaces[index].eyesClosedUntil != 0 &&
            now >= gCreditFaces[index].eyesClosedUntil) {
            gCreditFaces[index].eyesClosedUntil = 0;
            needsRedraw = 1;
        }
        if (gCreditFaces[index].eyesClosedUntil == 0 &&
            now >= gCreditFaces[index].lastBlink) {
            gCreditFaces[index].lastBlink = now + gCreditFaces[index].blinkFrequency;
            gCreditFaces[index].eyesClosedUntil = now + 150 + (unsigned long)(rand() % 160);
            needsRedraw = 1;
        }
    }
    /* The original moves 40 pixels per second. */
    if (now - gCreditsLastScrollTicks >= 25) {
        gCreditsLastScrollTicks = now;
        gCreditsScrollOffset += 1;
        needsRedraw = 1;
        if (gCreditsScrollOffset > (short)(gCreditsCount * 16 + 480)) {
            DebugLog("Credits: reached end, returning to main menu");
            RestoreMainMenu();
            return;
        }
    }
    SDL_GetMouseState(&mouseX, &mouseY);
    face = -1;
    for (face = 0; face < 15; ++face) {
        if (mouseX >= gCreditFaces[face].x && mouseX < gCreditFaces[face].x + gCreditFaces[face].width &&
            mouseY >= gCreditFaces[face].y && mouseY < gCreditFaces[face].y + gCreditFaces[face].height) break;
    }
    if (face == 15) face = -1;
    if (face != gCreditsHoverFace) {
        gCreditsHoverFace = face;
        needsRedraw = 1;
    }
    if (needsRedraw) {
        DrawCreditsScreen();
    }
}

static void DrawMainMenu(void)
{
    static const char *labels[] = {
        "Neues Spiel", "Spiel laden", "Einstellungen", "Credits", "Beenden"
    };
    short index;

    for (index = 0; index < 5; ++index) {
        short top = (short)(267 + index * 36);
        short bottom = (short)(302 + index * 36);

        if (index == gMenuButton) {
            FillRectRGB555(gMenuSurface, 205, top, 435, bottom, JA2_COLOR_ACCENT);
            DrawCString(gMenuSurface, (short)(205 + 18), (short)(top + 18), labels[index],
                        JA2_COLOR_WHITE);
        } else {
            FrameRectRGB555(gMenuSurface, 205, top, 435, bottom, JA2_COLOR_ACCENT);
            DrawCString(gMenuSurface, (short)(205 + 18), (short)(top + 18), labels[index],
                        JA2_COLOR_WHITE);
        }
    }
}

static void DrawMenuActionScreen(void)
{
    static const char *titles[] = {
        "Spielstartoptionen", "Spiel laden", "Einstellungen", "Credits"
    };
    static const char *details[] = {
        "Die Spielstartoptionen werden hier angezeigt.",
        "Die gespeicherten Spiele werden hier angezeigt.",
        "Die Spieleinstellungen werden hier angezeigt.",
        "Die Credits werden hier angezeigt."
    };
    short action;
    char status[96];

    if (gMenuScreen == MENU_SCREEN_DIFFICULTY) {
        JA2DrawDifficultyScreen();
        return;
    }
    if (!EnsureMenuSurface()) return;

    if (gMenuScreen == MENU_SCREEN_LAPTOP) {
        (void)status;
        JA2LaptopRender();
        return;
    }

    action = (short)(gMenuScreen - MENU_SCREEN_NEW_GAME);
    if (gMenuScreen > MENU_SCREEN_DIFFICULTY) {
        --action;
    }

    SDL_FillRect(gMenuSurface, NULL, JA2_RGB555(18, 22, 34));
    JA2DrawText(gMenuSurface, 60, 110, titles[action], JA2_COLOR_ACCENT, 24);
    if (gMenuScreen == MENU_SCREEN_NEW_GAME && gNewGameState == NEW_GAME_READY) {
        DrawCString(gMenuSurface, 60, 160, "Kampagne initialisiert. Laptop-Bildschirm folgt.",
                    JA2_COLOR_WHITE);
        sprintf(status, "Startkapital: %ld   Eroeffnungs-E-Mails: %d",
                gNewGameStartingCash, gNewGameOpeningEmails);
        DrawCString(gMenuSurface, 60, 190, status, JA2_COLOR_WHITE);
        sprintf(status, "Startsektor: A9 Omerta (%d,%d,0)",
                gNewGameSectorX, gNewGameSectorY);
        DrawCString(gMenuSurface, 60, 212, status, JA2_COLOR_WHITE);
        DrawCString(gMenuSurface, 60, 250, "ESC: Zurueck zum Hauptmenue", JA2_COLOR_WHITE);
    } else {
        DrawCString(gMenuSurface, 60, 160, details[action], JA2_COLOR_WHITE);
        DrawCString(gMenuSurface, 60, 210, "ESC: Zurueck zum Hauptmenue", JA2_COLOR_WHITE);
    }
    JA2BlitMenuBackground();
}

static void DrawJA2Shell(void)
{
    char dataStatus[48];
    char entryStatus[48];
    char assetStatus[64];

    if (!EnsureMenuSurface()) return;
    SDL_FillRect(gMenuSurface, NULL, JA2_RGB555(18, 22, 34));
    FillRectRGB555(gMenuSurface, 28, 28, JA2_SCREEN_WIDTH - 28, JA2_SCREEN_HEIGHT - 28,
                   JA2_RGB555(38, 48, 72));

    JA2DrawText(gMenuSurface, 58, 92, "JAGGED ALLIANCE 2", JA2_COLOR_ACCENT, 24);
    DrawCString(gMenuSurface, 60, 124, "PowerPC / System 9 platform shell (SDL2)", JA2_COLOR_WHITE);
    DrawCString(gMenuSurface, 60, 154, "SDL2 rendering backend and Macintosh event loop online.",
                JA2_COLOR_WHITE);
    DrawCString(gMenuSurface, 60, 176, "Press Command-Q or close the window to quit.",
                JA2_COLOR_WHITE);

    sprintf(dataStatus, "JA2 data archives: %d/22", CountDataLibraries());
    DrawCString(gMenuSurface, 60, 214, dataStatus, JA2_COLOR_WHITE);
    sprintf(entryStatus, "Menu background loaded: %d", gMenuBackgroundLoaded);
    DrawCString(gMenuSurface, 60, 234, entryStatus, JA2_COLOR_WHITE);
    strcpy(assetStatus, gMenuBackgroundLoaded ? "Main menu graphic found" : "Main menu graphic missing");
    DrawCString(gMenuSurface, 60, 254, assetStatus, JA2_COLOR_WHITE);
    JA2BlitMenuBackground();
}

static void RenderMainMenuDirectly(void)
{
    Uint32 startTicks = SDL_GetTicks();

    if (!EnsureMenuSurface()) return;
    DebugLog("Main menu: composing RGB555 background and buttons");
    gMenuBackgroundLoaded = RenderMainMenuBackground();
    if (gMenuBackgroundLoaded) {
        RenderMenuLogo();
        gMenuButtonsLoaded = RenderMenuButtons();
        if (!gMenuButtonsLoaded) {
            DrawMainMenu();
        }
        JA2BlitMenuBackground();
    } else {
        gMenuButtonsLoaded = 0;
        DrawJA2Shell();
    }
    DebugLog("Main menu: composition complete loaded=%d buttons=%d ms=%lu",
             gMenuBackgroundLoaded, gMenuButtonsLoaded,
             (unsigned long)(SDL_GetTicks() - startTicks));
}

static void EnterMainMenu(void)
{
    gStartupActive = 0;
    DebugLog("Main menu: rebuilding archive-backed composition");
    RenderMainMenuDirectly();
    JA2MenuMusicStart();
    DebugLog("Startup: entered main menu");
}

void RestoreMainMenu(void)
{
    DebugLog("Main menu: restoring from screen %d", gMenuScreen);
    gMenuScreen = MENU_SCREEN_MAIN;
    gActionScreenNeedsRender = 0;
    gMenuButton = -1;
    gMenuHoverButton = -1;
    gMenuBackgroundLoaded = 0;
    gMenuButtonsLoaded = 0;
    RenderMainMenuDirectly();
}

static void PollStartup(void)
{
    if (gStartupActive && SDL_GetTicks() - gStartupStartTicks >= 4000) {
        EnterMainMenu();
    }
}

static short GetMainMenuButtonAt(int localX, int localY)
{
    short index;

    for (index = 0; index < 5; ++index) {
        if (localX >= 180 && localX < 460 &&
            localY >= 277 + index * 37 && localY < 305 + index * 37) {
            return index;
        }
    }
    return -1;
}

static void UpdateMainMenuHover(void)
{
    int mouseX;
    int mouseY;
    Boolean rendered;
    short previousButton;
    short hoverButton;

    if (gStartupActive || gMenuScreen != MENU_SCREEN_MAIN || !gMenuBackgroundLoaded) {
        return;
    }
    SDL_GetMouseState(&mouseX, &mouseY);
    hoverButton = GetMainMenuButtonAt(mouseX, mouseY);
    if (hoverButton != gMenuHoverButton) {
        previousButton = gMenuHoverButton;
        gMenuHoverButton = hoverButton;
        if (previousButton < 0) {
            rendered = RenderMenuButtonFrames(hoverButton, hoverButton, hoverButton);
        } else if (hoverButton < 0) {
            rendered = RenderMenuButtonFrames(previousButton, previousButton, -1);
        } else {
            rendered = RenderMenuButtonFrames(previousButton < hoverButton ? previousButton : hoverButton,
                                               previousButton > hoverButton ? previousButton : hoverButton,
                                               hoverButton);
        }
        if (!rendered) {
            RenderMainMenuDirectly();
        } else {
            /* Coalesce the hover redraw with the cursor present at the end
               of this loop. A mouse-motion event otherwise caused two full
               640x480 copies in the same iteration. */
            gScreenNeedsPresent = 1;
        }
    }
}

static void HandleMainMenuClick(int localX, int localY)
{
    short index = GetMainMenuButtonAt(localX, localY);

    if (index >= 0) {
        gMenuButton = index;
        DebugLog("German main menu: click at %d,%d selected button %d",
                 localX, localY, index);
        switch (index) {
            case 0:
                DebugLog("German main menu: new game selected");
                gMenuScreen = MENU_SCREEN_DIFFICULTY;
                DebugLog("Difficulty: entering GameInitOptionsScreen PPC implementation");
                break;
            case 1:
                gMenuScreen = MENU_SCREEN_LOAD_GAME;
                DebugLog("German main menu: load game selected");
                break;
            case 2:
                gMenuScreen = MENU_SCREEN_OPTIONS;
                DebugLog("German main menu: preferences selected");
                break;
            case 3:
                DebugLog("German main menu: credits selected");
                EnterCreditsScreen();
                break;
            case 4:
                DebugLog("German main menu: quit selected");
                gRunning = 0;
                break;
        }
        if (gMenuScreen != MENU_SCREEN_MAIN && gMenuScreen != MENU_SCREEN_CREDITS) {
            gActionScreenNeedsRender = 1;
        }
    }
}

static void HandleJA2Event(const SDL_Event *event)
{
    switch (event->type) {
    case SDL_QUIT:
        DebugLog("Input: window close requested");
        gRunning = 0;
        break;
    case SDL_KEYDOWN: {
        SDL_Keycode key = event->key.keysym.sym;
        Uint16 mods = event->key.keysym.mod;

        DebugLog("Input: keyDown key=%d screen=%d startup=%d",
                 (int)key, gMenuScreen, gStartupActive);
        if (gMenuScreen == MENU_SCREEN_LAPTOP) {
            JA2LaptopHandleKey(key);
        } else if (gMenuScreen == MENU_SCREEN_INTRO && (key == SDLK_ESCAPE || key == SDLK_SPACE)) {
            JA2PPCIntroSkip();
        } else if (gMenuScreen == MENU_SCREEN_DIFFICULTY && key == SDLK_RETURN) {
            JA2HandleDifficultyClick(200, 430);
        } else if (gMenuScreen != MENU_SCREEN_MAIN &&
                   (key == SDLK_ESCAPE || key == SDLK_b)) {
            if (gMenuScreen == MENU_SCREEN_DIFFICULTY) {
                JA2ResetDifficultyScreen();
            }
            RestoreMainMenu();
        } else if (key == SDLK_q || (mods & KMOD_GUI) != 0) {
            gRunning = 0;
        }
        break;
    }
    case SDL_MOUSEMOTION:
        gLastMouseX = event->motion.x;
        gLastMouseY = event->motion.y;
        gMouseMotionNeedsPresent = 1;
        break;
    case SDL_MOUSEWHEEL:
        if (gMenuScreen == MENU_SCREEN_LAPTOP) {
            JA2LaptopHandleWheel((short)event->wheel.y);
        }
        break;
    case SDL_MOUSEBUTTONDOWN: {
        int localX = event->button.x;
        int localY = event->button.y;

        DebugLog("Input: content click local=%d,%d screen=%d startup=%d",
                 localX, localY, gMenuScreen, gStartupActive);
        if (!gStartupActive && gMenuScreen == MENU_SCREEN_LAPTOP) {
            JA2LaptopHandleClick((short)localX, (short)localY);
        } else if (!gStartupActive && gMenuScreen == MENU_SCREEN_INTRO) {
            JA2PPCIntroSkip();
        } else if (!gStartupActive && gMenuScreen == MENU_SCREEN_MAIN) {
            HandleMainMenuClick(localX, localY);
        } else if (!gStartupActive && gMenuScreen == MENU_SCREEN_DIFFICULTY) {
            JA2HandleDifficultyClick((short)localX, (short)localY);
        }
        break;
    }
    case SDL_WINDOWEVENT:
        if (event->window.event == SDL_WINDOWEVENT_EXPOSED) {
            gScreenNeedsPresent = 1;
        }
        break;
    default:
        break;
    }
}

static void PresentCurrentScreen(void)
{
    switch (gMenuScreen) {
    case MENU_SCREEN_CREDITS:
        if (gCreditsSurface != NULL) {
            JA2BlitSurfaceToWindow(gCreditsSurface, "credits frame");
        }
        break;
    case MENU_SCREEN_INTRO:
        /* The cinematics backend owns its own per-frame presentation. */
        break;
    default:
        if (gMenuSurface != NULL) {
            JA2BlitSurfaceToWindow(gMenuSurface, "menu frame");
        }
        break;
    }
}

static void UpdateCursorPresentIfMoved(void)
{
    int mouseX;
    int mouseY;

    if (gMenuScreen == MENU_SCREEN_INTRO) {
        gScreenNeedsPresent = 0;
        gMouseMotionNeedsPresent = 0;
        return;
    }
    SDL_GetMouseState(&mouseX, &mouseY);
    if (gScreenNeedsPresent || gMouseMotionNeedsPresent ||
        mouseX != gLastMouseX || mouseY != gLastMouseY) {
        gLastMouseX = mouseX;
        gLastMouseY = mouseY;
        gMouseMotionNeedsPresent = 0;
        PresentCurrentScreen();
    }
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        return 1;
    }
    DebugLog("JA2 PPC startup (SDL2 rendering backend)");
    JA2OptionsLoad();
    JA2MenuMusicSetLogger(DebugLog);
    DebugLog("Language: %s",
#if defined(JA2_LANGUAGE_GERMAN)
             "german"
#else
             "english"
#endif
    );

    gWindow = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED, JA2_SCREEN_WIDTH,
                               JA2_SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (gWindow == NULL) {
        DebugLog("Startup: SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    {
        SDL_Surface *windowSurface = SDL_GetWindowSurface(gWindow);
        if (windowSurface != NULL) {
            DebugLog("Startup: window surface format=%s",
                     SDL_GetPixelFormatName(windowSurface->format->format));
        }
    }
    SDL_ShowCursor(SDL_DISABLE);
    JA2TextInit();
    JA2InstallNormalCursor();

    DebugLog("Startup: attempting logo screen before menu load");
    if (EnsureMenuSurface() && RenderStartupSplash()) {
        gStartupActive = 1;
        gStartupStartTicks = SDL_GetTicks();
        JA2BlitMenuBackground();
        DebugLog("Startup: logo screen displayed for 4 seconds");
    } else {
        DebugLog("Startup: logo screen unavailable; skipping startup hold");
        EnterMainMenu();
    }

    while (gRunning) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            HandleJA2Event(&event);
        }
        PollStartup();
        JA2MenuMusicPoll();
        UpdateMainMenuHover();
        PollCredits();
        JA2PPCIntroPoll();
        if (gActionScreenNeedsRender) {
            gActionScreenNeedsRender = 0;
            DrawMenuActionScreen();
        }
        UpdateCursorPresentIfMoved();
        SDL_Delay(16);
    }

    if (gMenuSurface != NULL) {
        SDL_FreeSurface(gMenuSurface);
        gMenuSurface = NULL;
    }
    if (gCreditsSurface != NULL) {
        SDL_FreeSurface(gCreditsSurface);
        gCreditsSurface = NULL;
    }
    if (gCreditsBackgroundSurface != NULL) {
        SDL_FreeSurface(gCreditsBackgroundSurface);
        gCreditsBackgroundSurface = NULL;
    }
    ReleaseCreditsTextSurfaces();
    if (gCursorSurface != NULL) {
        SDL_FreeSurface(gCursorSurface);
        gCursorSurface = NULL;
    }
    if (gVideoSurface != NULL) {
        SDL_FreeSurface(gVideoSurface);
        gVideoSurface = NULL;
    }
    if (gFPSOverlaySurface != NULL) {
        SDL_FreeSurface(gFPSOverlaySurface);
        gFPSOverlaySurface = NULL;
    }
    free(gMenuButtonData);
    gMenuButtonData = NULL;
    free(gCreditsFacesData);
    gCreditsFacesData = NULL;
    JA2TextShutdown();
    JA2LaptopShutdown();
    JA2PPCIntroShutdown();
    JA2PPCVideoShutdown();
    JA2MenuMusicShutdown();
    SDL_DestroyWindow(gWindow);
    SDL_Quit();
    return 0;
}
