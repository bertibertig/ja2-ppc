/*
 * Classic Mac video and event shell for the JA2 PPC port.
 *
 * The original SGP backend is a Win32/DirectDraw application. This file
 * replaces that platform shell with QuickDraw calls available in System 9's
 * InterfaceLib. Game systems can be connected here as they are migrated.
 */

#include <Quickdraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <Events.h>
#include <Menus.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "ja2_music.h"
#include "ja2_slf.h"
#include "ja2_mac_internal.h"
#include "ja2_game_init_options.h"

#define JA2_SCREEN_WIDTH 640
#define JA2_SCREEN_HEIGHT 480

#if defined(JA2_LANGUAGE_GERMAN)
#define WINDOW_TITLE "\pJA2 PPC"
#define PLATFORM_TEXT "\pPowerPC / System 9 Plattform"
#define EVENT_TEXT "\pQuickDraw-Video und Macintosh-Ereignisschleife aktiv."
#define QUIT_TEXT "\pZum Beenden Command-Q druecken oder Fenster schliessen."
#define ARCHIVE_FORMAT "JA2-Datenarchive: %d/22"
#define ENTRY_FORMAT "Data.slf-Dateien indiziert: %d"
#define MENU_FOUND_TEXT "Hauptmenue-Grafik gefunden"
#define MENU_MISSING_TEXT "Hauptmenue-Grafik fehlt"
#else
#define WINDOW_TITLE "\pJA2 PPC"
#define PLATFORM_TEXT "\pPowerPC / System 9 platform shell"
#define EVENT_TEXT "\pQuickDraw video and Macintosh event loop online."
#define QUIT_TEXT "\pPress Command-Q or close the window to quit."
#define ARCHIVE_FORMAT "JA2 data archives: %d/22"
#define ENTRY_FORMAT "Data.slf files indexed: %d"
#define MENU_FOUND_TEXT "Main menu graphic found"
#define MENU_MISSING_TEXT "Main menu graphic missing"
#endif

WindowPtr gWindow;
static WindowPtr gDiagnosticsWindow;
static Boolean gRunning = 1;
static Boolean gMenuBackgroundLoaded = 0;
static Boolean gMenuButtonsLoaded = 0;
GWorldPtr gMenuWorld;
static GWorldPtr gCreditsWorld;
static Boolean gMenuBlitLogged = 0;
Boolean gMenuRenderDirectToWindow = 0;
static Boolean gMainMenuNeedsRender = 0;
Boolean gActionScreenNeedsRender = 0;
static short gMenuButton = -1;
static short gMenuHoverButton = -1;
static Boolean gStartupActive = 0;
static unsigned long gStartupStartTicks;
static unsigned long gCreditsStartTicks;
static short gCreditsScrollOffset;
static unsigned short gCreditsScrollPhase;
static short gCreditsHoverFace = -1;
static Boolean gCreditsBackgroundLoaded = 0;
static short gCreditsLastLoggedScroll = -1;
static Boolean gCreditsTextDiagnosticsLogged = 0;
static Boolean gCreditsFrameCopyDiagnosticsLogged = 0;
static char gDiagnosticsText[4096];
static unsigned short gDiagnosticsLength;
static Boolean gDrawingDiagnostics;

short gMenuScreen = MENU_SCREEN_MAIN;
void RestoreMainMenu(void);

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

void DebugLog(const char *format, ...);

static PixMapHandle GetMenuRenderPixMap(void)
{
    if (gMenuRenderDirectToWindow) {
        SetPort(gWindow);
        return ((CWindowPtr)gWindow)->portPixMap;
    }
    SetGWorld(gMenuWorld, NULL);
    return GetGWorldPixMap(gMenuWorld);
}

static void LogPixMap(const char *name, PixMapHandle pixMap)
{
    if (pixMap == NULL) {
        DebugLog("Renderer: %s PixMap is null", name);
        return;
    }
    DebugLog("Renderer: %s PixMap bounds=%d,%d,%d,%d rowBytes=%ld depth=%d",
             name, (**pixMap).bounds.left, (**pixMap).bounds.top,
             (**pixMap).bounds.right, (**pixMap).bounds.bottom,
             (long)((**pixMap).rowBytes & 0x3fff), (**pixMap).pixelSize);
}

static void LogPixMapSamples(const char *name, PixMapHandle pixMap)
{
    static const short sampleX[] = { 0, 32, 320, 600 };
    static const short sampleY[] = { 0, 240, 240, 440 };
    unsigned char *base;
    long rowBytes;
    unsigned short index;

    if (pixMap == NULL || !LockPixels(pixMap)) {
        DebugLog("Renderer: cannot lock %s PixMap for pixel diagnostics", name);
        return;
    }
    base = (unsigned char *)GetPixBaseAddr(pixMap);
    rowBytes = (**pixMap).rowBytes & 0x3fff;
    for (index = 0; index < 4; ++index) {
        unsigned char *pixel = base +
            (long)(sampleY[index] - (**pixMap).bounds.top) * rowBytes +
            (long)(sampleX[index] - (**pixMap).bounds.left) * 4;
        DebugLog("Renderer: %s pixel %d,%d ARGB=%02x%02x%02x%02x", name,
                 sampleX[index], sampleY[index], pixel[0], pixel[1], pixel[2], pixel[3]);
    }
    UnlockPixels(pixMap);
}

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
        lineLength = 250;
        line[lineLength] = 0;
    }
    if (gDiagnosticsLength + lineLength + 1 < sizeof(gDiagnosticsText)) {
        memcpy(&gDiagnosticsText[gDiagnosticsLength], line, lineLength);
        gDiagnosticsLength += lineLength;
        gDiagnosticsText[gDiagnosticsLength++] = '\r';
        gDiagnosticsText[gDiagnosticsLength] = 0;
    }
#if defined(JA2_DEBUG_BUILD)
    file = fopen("JA2PPC-Debug.log", "a");
    if (file != NULL) {
        fputs(line, file);
        fputs("\n", file);
        fclose(file);
    }
#else
    file = NULL;
#endif
    if (gDiagnosticsWindow != NULL && !gDrawingDiagnostics) {
        InvalRect(&gDiagnosticsWindow->portRect);
    }
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

static short CountDataArchiveEntries(void)
{
    FILE *file;
    unsigned char header[532];
    unsigned char entry[280];
    unsigned long entryCount;
    long archiveSize;
    long directoryOffset;
    short validEntries = 0;

    file = fopen(":Data:Data.slf", "rb");
    if (file == NULL) {
        file = fopen("Data/Data.slf", "rb");
    }
    if (file == NULL || fread(header, 1, sizeof(header), file) != sizeof(header)) {
        if (file != NULL) {
            fclose(file);
        }
        return -1;
    }

    /* SLF was written on little-endian PCs; do not cast these fields on PPC. */
    entryCount = ReadLittleEndian32(&header[512]);
    if (entryCount == 0 || entryCount > 65535) {
        fclose(file);
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    archiveSize = ftell(file);
    directoryOffset = archiveSize - ((long)entryCount * (long)sizeof(entry));
    if (directoryOffset < (long)sizeof(header) || fseek(file, directoryOffset, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    while (entryCount-- != 0) {
        if (fread(entry, 1, sizeof(entry), file) != sizeof(entry)) {
            fclose(file);
            return -1;
        }
        if (entry[264] == 0) {
            ++validEntries;
        }
    }
    fclose(file);
    return validEntries;
}

static short FindDataArchiveFile(const char *archiveName, const char *wantedName,
                                unsigned long *fileOffset, unsigned long *fileLength)
{
    FILE *file;
    unsigned char header[532];
    unsigned char entry[280];
    char entryName[257];
    unsigned long entryCount;
    long archiveSize;
    long directoryOffset;
    short found = 0;

    if (fileOffset != NULL) {
        *fileOffset = 0;
    }
    DebugLog("Searching %s for %s", archiveName, wantedName);
    if (fileLength != NULL) {
        *fileLength = 0;
    }

    sprintf(entryName, ":Data:%s", archiveName);
    file = fopen(entryName, "rb");
    if (file == NULL) {
        sprintf(entryName, "Data/%s", archiveName);
        file = fopen(entryName, "rb");
    }
    if (file == NULL || fread(header, 1, sizeof(header), file) != sizeof(header)) {
        DebugLog("Cannot open/read archive %s", archiveName);
        if (file != NULL) {
            fclose(file);
        }
        return 0;
    }

    entryCount = ReadLittleEndian32(&header[512]);
    if (entryCount == 0 || entryCount > 65535 || fseek(file, 0, SEEK_END) != 0) {
        DebugLog("Invalid directory count in %s: %lu", archiveName, entryCount);
        fclose(file);
        return 0;
    }
    archiveSize = ftell(file);
    directoryOffset = archiveSize - ((long)entryCount * (long)sizeof(entry));
    if (directoryOffset < (long)sizeof(header) || fseek(file, directoryOffset, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    while (entryCount-- != 0) {
        unsigned short index;
        if (fread(entry, 1, sizeof(entry), file) != sizeof(entry)) {
            fclose(file);
            return 0;
        }
        for (index = 0; index < 256 && entry[index] != 0; ++index) {
            entryName[index] = entry[index];
        }
        entryName[index] = 0;
        for (index = 0; entryName[index] != 0; ++index) {
            if (entryName[index] >= 'a' && entryName[index] <= 'z') {
                entryName[index] = (char)(entryName[index] - 'a' + 'A');
            }
        }
        if (strcmp(entryName, wantedName) == 0 && entry[264] == 0) {
            found = 1;
            if (fileOffset != NULL) {
                *fileOffset = ReadLittleEndian32(&entry[256]);
            }
            if (fileLength != NULL) {
                *fileLength = ReadLittleEndian32(&entry[260]);
            }
            DebugLog("Found %s offset=%lu length=%lu", entryName,
                     ReadLittleEndian32(&entry[256]), ReadLittleEndian32(&entry[260]));
            break;
        }
    }
    fclose(file);
    return found;
}

static unsigned short ReadLittleEndian16(const unsigned char *bytes)
{
    return (unsigned short)(bytes[0] | ((unsigned short)bytes[1] << 8));
}

static void WriteMenuPixel(PixMapHandle pixMap, short x, short y,
                           const RGBColor *color)
{
    unsigned char *base = (unsigned char *)GetPixBaseAddr(pixMap);
    long rowBytes = (**pixMap).rowBytes & 0x3fff;
    unsigned char *pixel = base +
                            (long)(y - (**pixMap).bounds.top) * rowBytes +
                            (long)(x - (**pixMap).bounds.left) * 4;

    /* Classic 32-bit Mac PixMaps use native ARGB byte order. */
    pixel[0] = 0;
    pixel[1] = (unsigned char)(color->red >> 8);
    pixel[2] = (unsigned char)(color->green >> 8);
    pixel[3] = (unsigned char)(color->blue >> 8);
}

static Boolean EnsureMenuWorld(void)
{
    QDErr worldError;
    Rect worldBounds = { -40, -40, 560, 760 };

    if (gMenuWorld != NULL) {
        return 1;
    }
    DebugLog("Renderer: creating 32-bit 800x600 transfer world");
    worldError = NewGWorld(&gMenuWorld, 32, &worldBounds, NULL, NULL, 0);
    if (worldError != noErr || gMenuWorld == NULL) {
        DebugLog("Renderer: 32-bit NewGWorld failed error=%d", worldError);
        return 0;
    }
    return 1;
}

static Boolean EnsureCreditsWorld(void)
{
    QDErr worldError;
    Rect worldBounds = { -40, -40, 560, 760 };

    if (gCreditsWorld != NULL) return 1;
    worldError = NewGWorld(&gCreditsWorld, 32, &worldBounds, NULL, NULL, 0);
    if (worldError != noErr || gCreditsWorld == NULL) {
        DebugLog("Credits: 32-bit frame GWorld creation failed error=%d", worldError);
        return 0;
    }
    DebugLog("Credits: created 32-bit compositing GWorld");
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
    PixMapHandle menuPixMap;

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
    /* The STCI header describes the full canvas. The subimage record gives
       the actual drawable bounds and offset used by the original blitter. */
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
    menuPixMap = GetMenuRenderPixMap();
    if (menuPixMap == NULL || !LockPixels(menuPixMap)) {
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
                UnlockPixels(menuPixMap);
                free(pixels);
                return 0;
            }
            if (control & 0x80) {
                x = (unsigned short)(x + run);
            } else {
                sourceUsed = (unsigned long)(source - (pixels + dataOffset));
                if (sourceUsed + run > dataLength) {
                    UnlockPixels(menuPixMap);
                    free(pixels);
                    return 0;
                }
                while (run-- != 0) {
                    paletteIndex = *source++;
                    color.red = (unsigned short)(palette[paletteIndex * 3] << 8);
                    color.green = (unsigned short)(palette[paletteIndex * 3 + 1] << 8);
                    color.blue = (unsigned short)(palette[paletteIndex * 3 + 2] << 8);
                    WriteMenuPixel(menuPixMap, (short)(188 + offsetX + x),
                                   (short)(15 + offsetY + y), &color);
                    ++x;
                }
            }
        }
        /* ETRLE stores a zero after every scanline. A literal/transparent run
           can end exactly at width, which otherwise leaves that zero to be
           decoded as a blank row on the next iteration. */
        if (x == width &&
            (unsigned long)(source - (pixels + dataOffset)) < dataLength &&
            *source == 0) {
            ++source;
        }
    }
    UnlockPixels(menuPixMap);
    SetPort(gWindow);
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
    PixMapHandle menuPixMap;
    unsigned short candidate;

    archive = NULL;
    for (candidate = 0; candidate < sizeof(archiveNames) / sizeof(archiveNames[0]);
         ++candidate) {
        archive = JA2SLFOpen(archiveNames[candidate]);
        if (archive == NULL) {
            DebugLog("Startup: cannot open %s", archiveNames[candidate]);
            continue;
        }
        DebugLog("Startup: opened %s with %lu entries", archiveNames[candidate],
                 JA2SLFEntryCount(archive));
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
    DebugLog("Startup: read logo entry (%lu bytes)", fileLength);
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
            DebugLog("Startup: unsupported RGB STCI %ux%u depth=%u masks=%08lx/%08lx/%08lx stored=%lu",
                     width, height, header[44], ReadLittleEndian32(&header[24]),
                     ReadLittleEndian32(&header[28]), ReadLittleEndian32(&header[32]),
                     storedSize);
            free(fileData);
            return 0;
        }
        SetGWorld(gMenuWorld, NULL);
        menuPixMap = GetGWorldPixMap(gMenuWorld);
        if (menuPixMap == NULL || !LockPixels(menuPixMap)) {
            DebugLog("Startup: unable to lock RGB logo destination PixMap");
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
                WriteMenuPixel(menuPixMap, (short)x, (short)y, &color);
            }
        }
        UnlockPixels(menuPixMap);
        SetPort(gWindow);
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

    SetGWorld(gMenuWorld, NULL);
    menuPixMap = GetGWorldPixMap(gMenuWorld);
    if (menuPixMap == NULL || !LockPixels(menuPixMap)) {
        DebugLog("Startup: unable to lock logo destination PixMap");
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
                UnlockPixels(menuPixMap);
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
                    UnlockPixels(menuPixMap);
                    free(pixels);
                    return 0;
                }
                while (run-- != 0) {
                    paletteIndex = *source++;
                    color.red = (unsigned short)(palette[paletteIndex * 3] << 8);
                    color.green = (unsigned short)(palette[paletteIndex * 3 + 1] << 8);
                    color.blue = (unsigned short)(palette[paletteIndex * 3 + 2] << 8);
                    WriteMenuPixel(menuPixMap, (short)(offsetX + x),
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
    UnlockPixels(menuPixMap);
    SetPort(gWindow);
    free(pixels);
    return 1;
}

static Boolean RenderMenuButtonFrames(short firstButton, short lastButton,
                                      short highlightedButton)
{
    static const unsigned short objectIndex[][2] = {
        { 0, 1 }, { 3, 4 }, { 7, 8 }, { 10, 11 }, { 14, 15 }
    };
    JA2SLFArchive *archive;
    const char *archiveName;
    const char *buttonFileName;
    unsigned char *fileData;
    unsigned char header[64];
    unsigned char palette[768];
    unsigned char *pixels;
    unsigned long fileOffset;
    unsigned long fileLength;
    unsigned long storedSize;
    unsigned long dataOffset;
    unsigned long dataLength;
    unsigned short imageWidth;
    unsigned short imageHeight;
    unsigned short objectCount;
    unsigned short item;
    unsigned short index;
    unsigned short y;
    unsigned short x;
    unsigned short run;
    unsigned char control;
    unsigned short paletteIndex;
    short offsetX;
    short offsetY;
    RGBColor color;
    PixMapHandle menuPixMap;
    unsigned char *source;

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
        if (archive != NULL) JA2SLFClose(archive);
        DebugLog("Menu loader: %s not found in %s", buttonFileName, archiveName);
        return 0;
    }
    fileData = (unsigned char *)malloc(fileLength);
    if (fileData == NULL || fileLength < 64 + 768 + 16 ||
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
    imageHeight = ReadLittleEndian16(&header[20]);
    imageWidth = ReadLittleEndian16(&header[22]);
    objectCount = ReadLittleEndian16(&header[28]);
    if (objectCount == 0 || objectCount <= objectIndex[4][1] ||
        ReadLittleEndian32(&header[24]) != 256 ||
        fileLength < 64 + 768 + (unsigned long)objectCount * 16 + storedSize) {
        free(fileData);
        return 0;
    }
    memcpy(palette, fileData + 64, sizeof(palette));
    pixels = fileData + 64 + 768 + (unsigned long)objectCount * 16;
    menuPixMap = GetMenuRenderPixMap();
    if (menuPixMap == NULL || !LockPixels(menuPixMap)) {
        free(fileData);
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
            UnlockPixels(menuPixMap);
            free(fileData);
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
                    UnlockPixels(menuPixMap);
                    free(fileData);
                    return 0;
                }
                if (control & 0x80) {
                    x = (unsigned short)(x + run);
                } else {
                    if ((unsigned long)(source - (pixels + dataOffset)) + run >
                        dataLength) {
                        UnlockPixels(menuPixMap);
                        free(fileData);
                        return 0;
                    }
                    while (run-- != 0) {
                        paletteIndex = *source++;
                        color.red = (unsigned short)(palette[paletteIndex * 3] << 8);
                        color.green = (unsigned short)(palette[paletteIndex * 3 + 1] << 8);
                        color.blue = (unsigned short)(palette[paletteIndex * 3 + 2] << 8);
                        WriteMenuPixel(menuPixMap,
                                       (short)(320 - imageWidth / 2 + offsetX + x),
                                       (short)(277 + item * 37 + offsetY + y),
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
    UnlockPixels(menuPixMap);
    SetPort(gWindow);
    free(fileData);
    return 1;
}

static Boolean RenderMenuButtons(void)
{
    return RenderMenuButtonFrames(0, 4, gMenuHoverButton);
}

Boolean JA2RenderInterfaceButton(short objectIndex, short x, short y)
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
    PixMapHandle pixMap;
    unsigned char *source;

    if (objectIndex < 0) return 0;
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
    pixMap = GetMenuRenderPixMap();
    if (pixMap == NULL || !LockPixels(pixMap)) {
        free(fileData);
        DebugLog("Button loader: cannot lock destination for object=%d", objectIndex);
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
                UnlockPixels(pixMap);
                free(fileData);
                return 0;
            }
            if (control & 0x80) {
                column = (unsigned short)(column + run);
            } else {
                if ((unsigned long)(source - (pixels + dataOffset)) + run > dataLength) {
                    UnlockPixels(pixMap);
                    free(fileData);
                    return 0;
                }
                while (run-- != 0) {
                    RGBColor color;
                    unsigned char paletteIndex = *source++;
                    color.red = (unsigned short)(palette[paletteIndex * 3] << 8);
                    color.green = (unsigned short)(palette[paletteIndex * 3 + 1] << 8);
                    color.blue = (unsigned short)(palette[paletteIndex * 3 + 2] << 8);
                    WriteMenuPixel(pixMap, (short)(x + column), (short)(y + row), &color);
                    ++column;
                }
            }
        }
        if (column == imageWidth &&
            (unsigned long)(source - (pixels + dataOffset)) < dataLength && *source == 0) {
            ++source;
        }
    }
    UnlockPixels(pixMap);
    SetPort(gWindow);
    free(fileData);
    DebugLog("Button loader: rendered PreferencesButtons object=%d at %d,%d size=%ux%u",
             objectIndex, x, y, imageWidth, imageHeight);
    return 1;
}

Boolean JA2InstallNormalCursor(void)
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
    Cursor cursor;

    archive = JA2SLFOpen("Cursors.slf");
    if (archive == NULL || !JA2SLFFind(archive, "CURSOR.STI", &fileOffset, &fileLength)) {
        if (archive != NULL) JA2SLFClose(archive);
        DebugLog("Cursor loader: CURSOR.STI unavailable; using Toolbox cursor");
        InitCursor();
        return 0;
    }
    fileData = (unsigned char *)malloc(fileLength);
    if (fileData == NULL || !JA2SLFRead(archive, fileOffset, fileData, fileLength)) {
        free(fileData);
        JA2SLFClose(archive);
        DebugLog("Cursor loader: cannot read CURSOR.STI length=%lu", fileLength);
        InitCursor();
        return 0;
    }
    JA2SLFClose(archive);
    storedSize = ReadLittleEndian32(&fileData[8]);
    objectCount = ReadLittleEndian16(&fileData[28]);
    if (memcmp(fileData, "STCI", 4) != 0 || objectCount == 0 ||
        ReadLittleEndian32(&fileData[24]) != 256 ||
        fileLength < 64 + 768 + (unsigned long)objectCount * 16 + storedSize) {
        free(fileData);
        DebugLog("Cursor loader: invalid CURSOR.STI object count=%u", objectCount);
        InitCursor();
        return 0;
    }
    {
        const unsigned char *object = fileData + 64 + 768;
        dataOffset = ReadLittleEndian32(&object[0]);
        dataLength = ReadLittleEndian32(&object[4]);
        height = ReadLittleEndian16(&object[12]);
        width = ReadLittleEndian16(&object[14]);
    }
    if (width == 0 || height == 0 || width > 16 || height > 16 ||
        dataOffset + dataLength > storedSize) {
        free(fileData);
        DebugLog("Cursor loader: unsupported cursor size=%ux%u", width, height);
        InitCursor();
        return 0;
    }
    memset(&cursor, 0, sizeof(cursor));
    cursor.hotSpot.h = 0;
    cursor.hotSpot.v = 0;
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
                    unsigned short bit = (unsigned short)(0x8000 >> column);
                    unsigned short brightness =
                        (unsigned short)fileData[64 + paletteIndex * 3] +
                        (unsigned short)fileData[64 + paletteIndex * 3 + 1] +
                        (unsigned short)fileData[64 + paletteIndex * 3 + 2];
                    cursor.mask[row] |= bit;
                    if (brightness > 384) cursor.data[row] |= bit;
                    ++column;
                }
            }
        }
    }
    free(fileData);
    SetCursor(&cursor);
    DebugLog("Cursor loader: installed CURSOR.STI normal cursor size=%ux%u", width, height);
    return 1;
}

void JA2BlitWorldToWindow(GWorldPtr world, const char *name)
{
    Rect sourceBounds;
    Rect destinationBounds;
    PixMapHandle sourcePixMap;
    PixMapHandle destinationPixMap;
    CGrafPtr savedPort;
    GDHandle savedDevice;
    RgnHandle savedClip;
    RgnHandle presentationClip;
    Boolean logBlit;
    Boolean sourceLocked;
    Boolean destinationLocked;

    if (world == NULL) {
        return;
    }
    /* CopyBits takes rectangles in each port's local coordinate system, not
       offsets into its PixMap. It also lets Color QuickDraw manage the
       window's backing pixels, which must not be treated as a framebuffer. */
    SetRect(&sourceBounds, 0, 0, JA2_SCREEN_WIDTH, JA2_SCREEN_HEIGHT);
    SetRect(&destinationBounds, 0, 0, JA2_SCREEN_WIDTH, JA2_SCREEN_HEIGHT);
    GetGWorld(&savedPort, &savedDevice);
    /* SetPort alone does not select a color graphics device. Always target
       the device that owns this application window before CopyBits. */
    SetGWorld((CGrafPtr)gWindow, GetMainDevice());
    logBlit = !gMenuBlitLogged;
    savedClip = NewRgn();
    presentationClip = NewRgn();
    if (savedClip == NULL || presentationClip == NULL) {
        if (savedClip != NULL) DisposeRgn(savedClip);
        if (presentationClip != NULL) DisposeRgn(presentationClip);
        DebugLog("Renderer: cannot allocate window clipping regions for %s", name);
        SetGWorld(savedPort, savedDevice);
        return;
    }
    GetClip(savedClip);
    if (logBlit) {
        CGrafPtr currentPort;
        GDHandle currentDevice;
        GetGWorld(&currentPort, &currentDevice);
        DebugLog("Renderer: %s port=%p current=%p device=%p clip=%d,%d,%d,%d visible=%d,%d,%d,%d portRect=%d,%d,%d,%d",
                 name, gWindow, currentPort, currentDevice, (**savedClip).rgnBBox.left,
                 (**savedClip).rgnBBox.top, (**savedClip).rgnBBox.right,
                 (**savedClip).rgnBBox.bottom, (**gWindow->visRgn).rgnBBox.left,
                 (**gWindow->visRgn).rgnBBox.top, (**gWindow->visRgn).rgnBBox.right,
                 (**gWindow->visRgn).rgnBBox.bottom, gWindow->portRect.left,
                 gWindow->portRect.top, gWindow->portRect.right, gWindow->portRect.bottom);
    }
    RectRgn(presentationClip, &gWindow->portRect);
    SetClip(presentationClip);
    sourcePixMap = GetGWorldPixMap(world);
    destinationPixMap = ((CWindowPtr)gWindow)->portPixMap;
    if (sourcePixMap == NULL || destinationPixMap == NULL) {
        DebugLog("Renderer: %s blit missing source or destination PixMap", name);
        SetClip(savedClip);
        DisposeRgn(presentationClip);
        DisposeRgn(savedClip);
        SetGWorld(savedPort, savedDevice);
        return;
    }
    sourceLocked = LockPixels(sourcePixMap);
    destinationLocked = sourceLocked && LockPixels(destinationPixMap);
    if (!destinationLocked) {
        DebugLog("Renderer: %s cannot lock source=%p destination=%p pixels", name,
                 sourcePixMap, destinationPixMap);
        if (sourceLocked) UnlockPixels(sourcePixMap);
        SetClip(savedClip);
        DisposeRgn(presentationClip);
        DisposeRgn(savedClip);
        SetGWorld(savedPort, savedDevice);
        return;
    }
    if (logBlit) {
        DebugLog("Renderer: %s locked sourceBase=%p destinationBase=%p", name,
                 GetPixBaseAddr(sourcePixMap), GetPixBaseAddr(destinationPixMap));
    }
    CopyBits((BitMap *)&((CGrafPtr)world)->portPixMap,
             (BitMap *)&((CGrafPtr)gWindow)->portPixMap,
             &sourceBounds, &destinationBounds, srcCopy, NULL);
    UnlockPixels(destinationPixMap);
    UnlockPixels(sourcePixMap);
    if (logBlit) {
        gMenuBlitLogged = 1;
        DebugLog("Renderer: copied %s 640x480 32-bit GWorld to color window", name);
        LogPixMap("menu source", sourcePixMap);
        LogPixMap("window destination", destinationPixMap);
        LogPixMapSamples("menu source", sourcePixMap);
        LogPixMapSamples("window destination", destinationPixMap);
    }
    SetClip(savedClip);
    DisposeRgn(presentationClip);
    DisposeRgn(savedClip);
    SetGWorld(savedPort, savedDevice);
}

void JA2BlitMenuBackground(void)
{
    JA2BlitWorldToWindow(gMenuWorld, "menu background");
}

void JA2LogDifficultyRenderState(const char *stage)
{
    CGrafPtr currentPort;
    GDHandle currentDevice;
    PixMapHandle cachePixMap = NULL;
    PixMapHandle windowPixMap = NULL;

    GetGWorld(&currentPort, &currentDevice);
    if (gMenuWorld != NULL) {
        cachePixMap = GetGWorldPixMap(gMenuWorld);
    }
    if (gWindow != NULL) {
        windowPixMap = ((CWindowPtr)gWindow)->portPixMap;
    }
    DebugLog("Difficulty render: %s currentPort=%p device=%p window=%p cache=%p textMode=%d",
             stage, currentPort, currentDevice, gWindow, gMenuWorld,
             gWindow == NULL ? -1 : ((CGrafPtr)gWindow)->txMode);
    LogPixMap("difficulty cache", cachePixMap);
    LogPixMap("difficulty window", windowPixMap);
    LogPixMapSamples("difficulty cache", cachePixMap);
    LogPixMapSamples("difficulty window", windowPixMap);
}

static Boolean CopyWorld(GWorldPtr sourceWorld, GWorldPtr destinationWorld)
{
    Rect bounds;
    PixMapHandle sourcePixMap;
    PixMapHandle destinationPixMap;
    CGrafPtr savedPort;
    GDHandle savedDevice;

    if (sourceWorld == NULL || destinationWorld == NULL) return 0;
    SetRect(&bounds, 0, 0, JA2_SCREEN_WIDTH, JA2_SCREEN_HEIGHT);
    sourcePixMap = GetGWorldPixMap(sourceWorld);
    destinationPixMap = GetGWorldPixMap(destinationWorld);
    if (sourcePixMap == NULL || destinationPixMap == NULL) return 0;
    GetGWorld(&savedPort, &savedDevice);
    SetGWorld(destinationWorld, NULL);
    CopyBits((BitMap *)&((CGrafPtr)sourceWorld)->portPixMap,
             (BitMap *)&((CGrafPtr)destinationWorld)->portPixMap,
             &bounds, &bounds, srcCopy, NULL);
    SetGWorld(savedPort, savedDevice);
    if (!gCreditsFrameCopyDiagnosticsLogged) {
        gCreditsFrameCopyDiagnosticsLogged = 1;
        DebugLog("Credits: cache-to-frame copy complete");
        LogPixMapSamples("credits cache", sourcePixMap);
        LogPixMapSamples("credits frame", destinationPixMap);
    }
    return 1;
}

Boolean RenderIndexedBackground(const char *archiveName, const char *entryName)
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
    PixMapHandle menuPixMap;
    CGrafPtr savedPort;
    GDHandle savedDevice;

    GetGWorld(&savedPort, &savedDevice);
    DebugLog("STCI loader: opening %s (saved port=%p device=%p)", archiveName,
             savedPort, savedDevice);
    archive = JA2SLFOpen(archiveName);
    if (archive == NULL) {
        DebugLog("STCI loader: cannot open %s", archiveName);
        return 0;
    }
    DebugLog("STCI loader: opened %s with %lu entries", archiveName,
             JA2SLFEntryCount(archive));
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
    DebugLog("STCI loader: %s offset=%lu length=%lu", entryName,
             fileOffset, fileLength);
    DebugLog("STCI loader: read %lu byte %s", fileLength, entryName);
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
    DebugLog("STCI loader: %s palette[0]=%02x,%02x,%02x palette[1]=%02x,%02x,%02x palette[128]=%02x,%02x,%02x palette[255]=%02x,%02x,%02x",
             entryName, palette[0], palette[1], palette[2], palette[3], palette[4],
             palette[5], palette[384], palette[385], palette[386], palette[765],
             palette[766], palette[767]);
    memcpy(object, fileData + 64 + 768, sizeof(object));

    dataOffset = ReadLittleEndian32(&object[0]);
    dataLength = ReadLittleEndian32(&object[4]);
    offsetX = (short)ReadLittleEndian16(&object[8]);
    offsetY = (short)ReadLittleEndian16(&object[10]);
    width = ReadLittleEndian16(&object[14]);
    height = ReadLittleEndian16(&object[12]);
    DebugLog("STCI loader: %s object %ux%u at %d,%d stored=%lu",
             entryName, width, height, offsetX, offsetY, storedSize);
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
    if (!gMenuRenderDirectToWindow && !EnsureMenuWorld()) {
        free(pixels);
        return 0;
    }
    menuPixMap = GetMenuRenderPixMap();
    if (menuPixMap == NULL || !LockPixels(menuPixMap)) {
        DebugLog("Menu loader: unable to lock offscreen PixMap");
        free(pixels);
        return 0;
    }
    if ((**menuPixMap).pixelSize != 32) {
        DebugLog("Menu loader: invalid 32-bit PixMap depth=%d",
                 (**menuPixMap).pixelSize);
        UnlockPixels(menuPixMap);
        free(pixels);
        return 0;
    }
    if (gMenuRenderDirectToWindow) {
        EraseRect(&gWindow->portRect);
    } else {
        EraseRect(&((GrafPtr)gMenuWorld)->portRect);
    }
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
                    UnlockPixels(menuPixMap);
                    free(pixels);
                    return 0;
                }
                while (run-- != 0) {
                    unsigned short paletteIndex = *source++;
                    color.red = (unsigned short)(palette[paletteIndex * 3] << 8);
                    color.green = (unsigned short)(palette[paletteIndex * 3 + 1] << 8);
                    color.blue = (unsigned short)(palette[paletteIndex * 3 + 2] << 8);
                    WriteMenuPixel(menuPixMap, (short)(offsetX + x),
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
    UnlockPixels(menuPixMap);
    SetGWorld(savedPort, savedDevice);
    free(pixels);
    return 1;
}

static Boolean RenderMainMenuBackground(void)
{
    return RenderIndexedBackground("LoadScreens.slf", "MAINMENUBACKGROUND.STI");
}

void DrawCString(short x, short y, const char *text)
{
    Str255 pascalText;
    short length = (short)strlen(text);

    if (length > 255) {
        length = 255;
    }
    pascalText[0] = (unsigned char)length;
    memcpy(&pascalText[1], text, (size_t)length);
    MoveTo(x, y);
    DrawString(pascalText);
}

static void DrawCenteredCString(short y, const char *text, RGBColor *color)
{
    Str255 pascalText;
    short length = (short)strlen(text);
    short width;

    if (length > 255) length = 255;
    pascalText[0] = (unsigned char)length;
    memcpy(&pascalText[1], text, (size_t)length);
    width = StringWidth(pascalText);
    RGBForeColor(color);
    DrawCString((short)((JA2_SCREEN_WIDTH - width) / 2), y, text);
}

static void DrawCreditsColumnString(short y, const char *text, RGBColor *color)
{
    Str255 pascalText;
    short length = (short)strlen(text);
    short width;

    if (length > 255) length = 255;
    pascalText[0] = (unsigned char)length;
    memcpy(&pascalText[1], text, (size_t)length);
    width = StringWidth(pascalText);
    RGBForeColor(color);
    DrawCString((short)(10 + (210 - width) / 2), y, text);
}

/* The original credits use the first image in each three-frame face group. */
static Boolean RenderCreditFaces(void)
{
    JA2SLFArchive *archive;
    unsigned char *fileData;
    unsigned char palette[768];
    unsigned char *pixels;
    unsigned long offset, length, storedSize, dataOffset, dataLength;
    unsigned short count, face, x, y, run, imageWidth, imageHeight, paletteIndex;
    short offsetX, offsetY;
    unsigned char control;
    const unsigned char *object;
    const unsigned char *source;
    RGBColor color;
    Rect pixel;

    archive = JA2SLFOpen("Interface.slf");
    if (archive == NULL || !JA2SLFFind(archive, "CREDIT FACES.STI", &offset, &length)) {
        DebugLog("Credits: Credit Faces.sti unavailable");
        if (archive != NULL) JA2SLFClose(archive);
        return 0;
    }
    DebugLog("Credits: Credit Faces.sti offset=%lu length=%lu", offset, length);
    fileData = (unsigned char *)malloc(length);
    if (fileData == NULL || !JA2SLFRead(archive, offset, fileData, length)) {
        free(fileData);
        JA2SLFClose(archive);
        return 0;
    }
    JA2SLFClose(archive);
    if (length < 64 + 768 || memcmp(fileData, "STCI", 4) != 0) {
        free(fileData);
        return 0;
    }
    storedSize = ReadLittleEndian32(&fileData[8]);
    count = ReadLittleEndian16(&fileData[28]);
    DebugLog("Credits: face sprite object count=%u stored=%lu", count, storedSize);
    if (count < 45 || length < 64 + 768 + (unsigned long)count * 16 + storedSize) {
        free(fileData);
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
                        RGBForeColor(&color);
                        SetRect(&pixel, (short)(gCreditFaces[face].eyeX + offsetX + x),
                                (short)(gCreditFaces[face].eyeY + offsetY + y),
                                (short)(gCreditFaces[face].eyeX + offsetX + x + 1),
                                (short)(gCreditFaces[face].eyeY + offsetY + y + 1));
                        PaintRect(&pixel);
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
    free(fileData);
    return 1;
}

static void DrawCreditsScreen(void)
{
    RGBColor white = {0xffff, 0xffff, 0xffff};
    RGBColor red = {0xffff, 0x0000, 0x0000};
    RGBColor black = {0, 0, 0};
    short index;
    short y;
    Boolean anyEyesClosed = 0;

    SetGWorld((CGrafPtr)gWindow, GetMainDevice());
    gMenuRenderDirectToWindow = 1;
    if (!RenderIndexedBackground("Interface.slf", "CREDITS.STI")) {
        DebugLog("Credits: direct background render failed");
        gMenuRenderDirectToWindow = 0;
        return;
    }
    DebugLog("Credits: rendered background directly to color window");
    for (index = 0; index < 15; ++index) {
        if (gCreditFaces[index].eyesClosedUntil != 0) {
            anyEyesClosed = 1;
            break;
        }
    }
    if (anyEyesClosed) RenderCreditFaces();
    gMenuRenderDirectToWindow = 0;
    TextFont(systemFont);
    /* Match the original color-keyed text surface: glyphs overlay the image
       without copying a white text-background rectangle into the window. */
    TextMode(srcOr);
    for (index = 0; index < gCreditsCount; ++index) {
        y = (short)(479 + index * 16 - gCreditsScrollOffset);
        if (y < -18 || y > 480) continue;
        TextSize(index == 0 || index == 3 || index == 10 || index == 17 || index == 19 ? 14 : 12);
        DrawCreditsColumnString(y, gCredits[index],
                                (index == 0 || index == 3 || index == 10 ||
                                 index == 17 || index == 19) ? &red : &white);
    }
    if (gCreditsHoverFace >= 0) {
        TextSize(12);
        DrawCenteredCString(420, gCreditFaces[gCreditsHoverFace].name, &white);
        DrawCenteredCString(435, gCreditFaces[gCreditsHoverFace].title, &white);
        DrawCenteredCString(450, gCreditFaces[gCreditsHoverFace].aside, &white);
    }
    /* Keep the color device state neutral after the red/white text overlay. */
    RGBForeColor(&black);
    RGBBackColor(&white);
    if (!gCreditsTextDiagnosticsLogged) {
        gCreditsTextDiagnosticsLogged = 1;
        DebugLog("Credits: window samples after direct text overlay");
        LogPixMapSamples("credits window after text", ((CWindowPtr)gWindow)->portPixMap);
    }
    if (gCreditsScrollOffset == 0 ||
        gCreditsScrollOffset >= gCreditsLastLoggedScroll + 60) {
        gCreditsLastLoggedScroll = gCreditsScrollOffset;
        DebugLog("Credits: rendered scroll=%d hover=%d", gCreditsScrollOffset,
                 gCreditsHoverFace);
    }
    SetGWorld((CGrafPtr)gWindow, GetMainDevice());
}

static void EnterCreditsScreen(void)
{
    unsigned short face;
    gMenuScreen = MENU_SCREEN_CREDITS;
    gCreditsStartTicks = TickCount();
    gCreditsScrollOffset = 0;
    gCreditsScrollPhase = 0;
    gCreditsHoverFace = -1;
    gCreditsBackgroundLoaded = 0;
    gCreditsLastLoggedScroll = -1;
    gCreditsTextDiagnosticsLogged = 0;
    gCreditsFrameCopyDiagnosticsLogged = 0;
    gMenuBlitLogged = 0;
    DebugLog("Credits: entering screen, %d lines and %d face regions",
             gCreditsCount, 15);
    for (face = 0; face < 15; ++face) {
        gCreditFaces[face].lastBlink = TickCount() +
            (unsigned long)(rand() % ((gCreditFaces[face].blinkFrequency * 120L) / 1000L));
        gCreditFaces[face].eyesClosedUntil = 0;
    }
    DrawCreditsScreen();
}

static void PollCredits(void)
{
    Point point;
    short face;
    unsigned short index;
    unsigned long now;

    if (gMenuScreen != MENU_SCREEN_CREDITS) return;
    now = TickCount();
    for (index = 0; index < 15; ++index) {
        if (gCreditFaces[index].eyesClosedUntil != 0 &&
            now >= gCreditFaces[index].eyesClosedUntil) {
            gCreditFaces[index].eyesClosedUntil = 0;
            DebugLog("Credits: face %d eyes open", index);
        }
        if (gCreditFaces[index].eyesClosedUntil == 0 &&
            now >= gCreditFaces[index].lastBlink) {
            gCreditFaces[index].lastBlink = now +
                (unsigned long)((gCreditFaces[index].blinkFrequency * 60L) / 1000L);
            gCreditFaces[index].eyesClosedUntil = now + 9 + (unsigned long)(rand() % 10);
            DebugLog("Credits: face %d blink until tick %lu", index,
                     gCreditFaces[index].eyesClosedUntil);
        }
    }
    /* The original moves one pixel every 25 ms. At 60 Hz, two pixels every
       three ticks preserves that 40-pixel-per-second rate without fractions. */
    if (TickCount() != gCreditsStartTicks) {
        gCreditsStartTicks = now;
        if (++gCreditsScrollPhase != 3) return;
        gCreditsScrollPhase = 0;
        gCreditsScrollOffset += 2;
        if (gCreditsScrollOffset > (short)(gCreditsCount * 16 + 480)) {
            DebugLog("Credits: reached end, returning to main menu");
            RestoreMainMenu();
            return;
        }
        DrawCreditsScreen();
    }
    GetMouse(&point);
    face = -1;
    for (face = 0; face < 15; ++face) {
        if (point.h >= gCreditFaces[face].x && point.h < gCreditFaces[face].x + gCreditFaces[face].width &&
            point.v >= gCreditFaces[face].y && point.v < gCreditFaces[face].y + gCreditFaces[face].height) break;
    }
    if (face == 15) face = -1;
    if (face != gCreditsHoverFace) {
        gCreditsHoverFace = face;
        DebugLog("Credits: hover face=%d", face);
        DrawCreditsScreen();
    }
}

static void DrawDiagnostics(void)
{
    Rect bounds;
    char *line;
    char *next;
    short y = 16;

    gDrawingDiagnostics = 1;
    SetPort(gDiagnosticsWindow);
    bounds = gDiagnosticsWindow->portRect;
    EraseRect(&bounds);
    TextFont(systemFont);
    TextSize(9);
    line = gDiagnosticsText;
    while (*line != 0 && y < bounds.bottom - 10) {
        next = strchr(line, '\r');
        if (next != NULL) {
            *next = 0;
        }
        DrawCString(8, y, line);
        if (next == NULL) {
            break;
        }
        *next = '\r';
        line = next + 1;
        y += 12;
    }
    gDrawingDiagnostics = 0;
}

static void DrawMainMenu(void)
{
    static const char *labels[] = {
        "Neues Spiel", "Spiel laden", "Einstellungen", "Credits", "Beenden"
    };
    Rect button;
    RGBColor border = { 0xd900, 0x9b00, 0x4500 };
    RGBColor text = { 0xffff, 0xffff, 0xffff };
    short index;

    SetPort(gWindow);
    TextFont(systemFont);
    TextSize(13);
    for (index = 0; index < 5; ++index) {
        button.left = 205;
        button.top = (short)(267 + index * 36);
        button.right = 435;
        button.bottom = (short)(302 + index * 36);
        RGBForeColor(&border);
        if (index == gMenuButton) {
            PaintRoundRect(&button, 8, 8);
            RGBForeColor(&text);
        } else {
            FrameRoundRect(&button, 8, 8);
        }
        DrawCString((short)(button.left + 18), (short)(button.top + 18), labels[index]);
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
    Rect bounds;
    RGBColor background = { 0x1200, 0x1600, 0x2200 };
    RGBColor accent = { 0xd900, 0x9b00, 0x4500 };
    RGBColor white = { 0xffff, 0xffff, 0xffff };
    short action;
    char status[96];

    if (gMenuScreen == MENU_SCREEN_DIFFICULTY) {
        JA2DrawDifficultyScreen();
        return;
    }
    action = (short)(gMenuScreen - MENU_SCREEN_NEW_GAME);
    if (gMenuScreen > MENU_SCREEN_DIFFICULTY) {
        --action;
    }

    HideCursor();
    SetGWorld((CGrafPtr)gWindow, GetMainDevice());
    bounds = gWindow->portRect;
    RGBForeColor(&background);
    PaintRect(&bounds);
    TextFont(systemFont);
    TextSize(24);
    RGBForeColor(&accent);
    MoveTo(60, 110);
    DrawCString(60, 110, titles[action]);
    TextSize(13);
    RGBForeColor(&white);
    if (gMenuScreen == MENU_SCREEN_NEW_GAME && gNewGameState == NEW_GAME_READY) {
        DrawCString(60, 160, "Kampagne initialisiert. Laptop-Bildschirm folgt.");
        sprintf(status, "Startkapital: %ld   Eroeffnungs-E-Mails: %d",
                gNewGameStartingCash, gNewGameOpeningEmails);
        DrawCString(60, 190, status);
        sprintf(status, "Startsektor: A9 Omerta (%d,%d,0)",
                gNewGameSectorX, gNewGameSectorY);
        DrawCString(60, 212, status);
        DrawCString(60, 250, "ESC: Zurueck zum Hauptmenue");
    } else {
        DrawCString(60, 160, details[action]);
        DrawCString(60, 210, "ESC: Zurueck zum Hauptmenue");
    }
    ShowCursor();
}

static void DrawJA2Shell(void);
static void EnterMainMenu(void);

static void RenderMainMenuDirectly(void)
{
    HideCursor();
    gMenuRenderDirectToWindow = 1;
    gMenuBackgroundLoaded = RenderMainMenuBackground();
    if (gMenuBackgroundLoaded) {
        RenderMenuLogo();
        gMenuButtonsLoaded = RenderMenuButtons();
        if (!gMenuButtonsLoaded) {
            DrawMainMenu();
        }
    } else {
        gMenuButtonsLoaded = 0;
        DrawJA2Shell();
    }
    gMenuRenderDirectToWindow = 0;
    ShowCursor();
}

static void EnterMainMenu(void)
{
    gStartupActive = 0;
    gMenuBlitLogged = 0;
    DebugLog("Main menu: rebuilding archive-backed composition");
    /* The splash was drawn over the cached world; reload the original
       background before restoring the menu logo and title graphics. */
    RenderMainMenuDirectly();
    gMainMenuNeedsRender = 0;
    JA2MenuMusicStart();
    DebugLog("Startup: entered main menu");
}

void RestoreMainMenu(void)
{
    DebugLog("Main menu: restoring from screen %d", gMenuScreen);
    if (gMenuScreen == MENU_SCREEN_CREDITS && gCreditsWorld != NULL) {
        DisposeGWorld(gCreditsWorld);
        gCreditsWorld = NULL;
    }
    gMenuScreen = MENU_SCREEN_MAIN;
    gActionScreenNeedsRender = 0;
    gMenuButton = -1;
    gMenuHoverButton = -1;
    gMainMenuNeedsRender = 1;
    SetGWorld((CGrafPtr)gWindow, GetMainDevice());
    InvalRect(&gWindow->portRect);
}

static void PollStartup(void)
{
    if (gStartupActive && TickCount() - gStartupStartTicks >= 60 * 4) {
        EnterMainMenu();
    }
}

static short GetMainMenuButtonAt(Point point)
{
    short index;

    for (index = 0; index < 5; ++index) {
        if (point.h >= 180 && point.h < 460 &&
            point.v >= 277 + index * 37 && point.v < 305 + index * 37) {
            return index;
        }
    }
    return -1;
}

static void UpdateMainMenuHover(void)
{
    Point point;
    Boolean rendered;
    short previousButton;
    short hoverButton;

    if (gStartupActive || gMenuScreen != MENU_SCREEN_MAIN || !gMenuBackgroundLoaded) {
        return;
    }
    SetPort(gWindow);
    GetMouse(&point);
    hoverButton = GetMainMenuButtonAt(point);
    if (hoverButton != gMenuHoverButton) {
        previousButton = gMenuHoverButton;
        gMenuHoverButton = hoverButton;
        HideCursor();
        gMenuRenderDirectToWindow = 1;
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
        }
        gMenuRenderDirectToWindow = 0;
        ShowCursor();
    }
}

static void HandleMainMenuClick(Point point)
{
    short index = GetMainMenuButtonAt(point);

    if (index >= 0) {
            gMenuButton = index;
            DebugLog("German main menu: click at %d,%d selected button %d",
                     point.h, point.v, index);
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
                if (gMenuScreen == MENU_SCREEN_DIFFICULTY) {
                    JA2DrawDifficultyScreen();
                    gActionScreenNeedsRender = 0;
                } else {
                    DrawMenuActionScreen();
                }
            }
            return;
    }
}

static void DrawJA2Shell(void)
{
    Rect bounds;
    RGBColor background = { 0x1200, 0x1600, 0x2200 };
    RGBColor panel = { 0x2600, 0x3000, 0x4800 };
    RGBColor accent = { 0xd900, 0x9b00, 0x4500 };
    RGBColor white = { 0xffff, 0xffff, 0xffff };
    RGBColor black = { 0x0000, 0x0000, 0x0000 };
    char dataStatus[48];
    char entryStatus[48];
    char assetStatus[64];

    SetPort(gWindow);
    bounds = gWindow->portRect;

    RGBForeColor(&background);
    PaintRect(&bounds);

    bounds.left += 28;
    bounds.top += 28;
    bounds.right -= 28;
    bounds.bottom -= 28;
    RGBForeColor(&panel);
    PaintRoundRect(&bounds, 14, 14);

    TextFont(systemFont);
    TextSize(24);
    RGBForeColor(&accent);
    MoveTo(58, 92);
    DrawString((ConstStr255Param) "\pJAGGED ALLIANCE 2");

    TextSize(12);
    RGBForeColor(&white);
    MoveTo(60, 124);
    DrawString((ConstStr255Param) PLATFORM_TEXT);
    MoveTo(60, 154);
    DrawString((ConstStr255Param) EVENT_TEXT);
    MoveTo(60, 176);
    DrawString((ConstStr255Param) QUIT_TEXT);

    sprintf(dataStatus, ARCHIVE_FORMAT, CountDataLibraries());
    DrawCString(60, 214, dataStatus);
    sprintf(entryStatus, ENTRY_FORMAT, CountDataArchiveEntries());
    DrawCString(60, 234, entryStatus);
    if (gMenuBackgroundLoaded) {
        strcpy(assetStatus, MENU_FOUND_TEXT);
    } else {
        strcpy(assetStatus, MENU_MISSING_TEXT);
    }
    DrawCString(60, 254, assetStatus);

    RGBForeColor(&black);
}

static void HandleJA2Event(const EventRecord *event)
{
    char key;
    WindowPtr clicked;
    short part;
    Boolean redrawMenuAfterUpdate;

    if (event->what == keyDown || event->what == autoKey) {
        key = (char)(event->message & charCodeMask);
        DebugLog("Input: key=%d screen=%d", (unsigned char)key, gMenuScreen);
        if (gMenuScreen == MENU_SCREEN_INTRO &&
            (key == 27 || key == ' ')) {
            JA2PPCIntroSkip();
        } else if (gMenuScreen == MENU_SCREEN_DIFFICULTY && key == 13) {
            Point point = { 200, 430 };
            JA2HandleDifficultyClick(point);
        } else if (gMenuScreen != MENU_SCREEN_MAIN && (key == 27 || key == 'b' || key == 'B')) {
            if (gMenuScreen == MENU_SCREEN_DIFFICULTY) {
                JA2ResetDifficultyScreen();
            }
            RestoreMainMenu();
        } else if (key == 'q' || key == 'Q' || (event->modifiers & cmdKey)) {
            gRunning = 0;
        }
    } else if (event->what == mouseDown) {
        part = FindWindow(event->where, &clicked);
        if (part == inDrag) {
            DragWindow(clicked, event->where, &qd.screenBits.bounds);
        } else if (part == inGoAway && TrackGoAway(clicked, event->where)) {
            if (clicked == gDiagnosticsWindow) {
                HideWindow(gDiagnosticsWindow);
            } else {
                gRunning = 0;
            }
        } else if (clicked == gWindow && part == inContent) {
            Point point = event->where;
            SetPort(gWindow);
            GlobalToLocal(&point);
            if (!gStartupActive && gMenuScreen == MENU_SCREEN_INTRO) {
                JA2PPCIntroSkip();
            } else if (!gStartupActive && gMenuScreen == MENU_SCREEN_MAIN) {
                HandleMainMenuClick(point);
            } else if (!gStartupActive && gMenuScreen == MENU_SCREEN_DIFFICULTY) {
                JA2HandleDifficultyClick(point);
            }
        }
    } else if (event->what == updateEvt) {
        WindowPtr updatedWindow = (WindowPtr)event->message;
        redrawMenuAfterUpdate = 0;
        BeginUpdate(updatedWindow);
        if (updatedWindow == gDiagnosticsWindow) {
            DrawDiagnostics();
            EndUpdate(updatedWindow);
            return;
        }
        if (gStartupActive ||
            (gMenuBackgroundLoaded && gMenuScreen == MENU_SCREEN_MAIN)) {
            redrawMenuAfterUpdate = gStartupActive;
        } else if (gMenuScreen == MENU_SCREEN_CREDITS) {
            DrawCreditsScreen();
        } else if (gMenuScreen == MENU_SCREEN_INTRO) {
            JA2PPCIntroUpdate();
        } else if (gMenuScreen != MENU_SCREEN_MAIN) {
            if (gActionScreenNeedsRender) {
                gActionScreenNeedsRender = 0;
                DrawMenuActionScreen();
            }
        } else {
            DrawJA2Shell();
        }
        EndUpdate(updatedWindow);
        if (updatedWindow == gWindow && redrawMenuAfterUpdate) {
            /* Copy after EndUpdate so QuickDraw cannot clear the new frame as
               part of finalizing the invalidated content region. */
            JA2BlitMenuBackground();
        }
        if (updatedWindow == gWindow && gMainMenuNeedsRender) {
            gMainMenuNeedsRender = 0;
            RenderMainMenuDirectly();
        }
        if (updatedWindow == gWindow && gMenuBackgroundLoaded &&
            !gMenuButtonsLoaded && !gStartupActive) {
            /* The update region clips drawing. Restore controls after the
               region is ended so a moved window redraws every button. */
            DrawMainMenu();
        }
    }
}

int main(void)
{
    EventRecord event;
    WindowPtr window;
    OSErr depthError;
    Rect bounds = { 40, 40, 40 + JA2_SCREEN_HEIGHT, 40 + JA2_SCREEN_WIDTH };
    Rect diagnosticsBounds = { 80, 80, 320, 560 };

    InitGraf(&qd.thePort);
    MaxApplZone();
    DebugLog("Memory: expanded application zone free=%ld bytes", (long)FreeMem());
    InitFonts();
    InitWindows();
    InitMenus();
    InitCursor();

    depthError = SetDepth(GetMainDevice(), 32, 0, 0);
    DebugLog("Display depth request: 32-bit result=%d", depthError);

    gWindow = NewCWindow(NULL, &bounds, (ConstStr255Param) WINDOW_TITLE, 1,
                         documentProc, (WindowPtr)-1, 1, 0);
    if (gWindow == NULL) {
        return 1;
    }

#if defined(JA2_DEBUG_BUILD)
    gDiagnosticsWindow = NewCWindow(NULL, &diagnosticsBounds,
                                    (ConstStr255Param) "\pJA2 Debug", 1,
                                    documentProc, (WindowPtr)-1, 1, 0);
#endif
    DebugLog("JA2 PPC startup");
    JA2MenuMusicSetLogger(DebugLog);
    DebugLog("Diagnostics build: %s",
#if defined(JA2_DEBUG_BUILD)
             "enabled"
#else
             "disabled"
#endif
    );
    DebugLog("Language: %s",
#if defined(JA2_LANGUAGE_GERMAN)
             "german"
#else
             "english"
#endif
    );

    ShowWindow(gWindow);
    SelectWindow(gWindow);
    if (gDiagnosticsWindow != NULL) {
        ShowWindow(gDiagnosticsWindow);
        DrawDiagnostics();
    }
    DebugLog("Startup: creating render world; attempting logo screen before menu load");
    if (EnsureMenuWorld() && RenderStartupSplash()) {
        gStartupActive = 1;
        gStartupStartTicks = TickCount();
        JA2BlitMenuBackground();
        DebugLog("Startup: logo screen displayed for 4 seconds");
    } else {
        DebugLog("Startup: logo screen unavailable; skipping startup hold");
        EnterMainMenu();
    }

    while (gRunning) {
        PollStartup();
        JA2MenuMusicPoll();
        UpdateMainMenuHover();
        PollCredits();
        JA2PPCIntroPoll();
        if (WaitNextEvent(everyEvent, &event,
                          (gMenuScreen == MENU_SCREEN_CREDITS ||
                           gMenuScreen == MENU_SCREEN_INTRO) ? 1 : 10, NULL)) {
            HandleJA2Event(&event);
        }
    }

    window = gWindow;
    if (gMenuWorld != NULL) {
        DisposeGWorld(gMenuWorld);
    }
    if (gDiagnosticsWindow != NULL) {
        DisposeWindow(gDiagnosticsWindow);
    }
    JA2PPCIntroShutdown();
    JA2PPCVideoShutdown();
    JA2MenuMusicShutdown();
    DisposeWindow(window);
    return 0;
}
