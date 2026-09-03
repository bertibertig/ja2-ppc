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

#include "ja2_slf.h"

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

static WindowPtr gWindow;
static WindowPtr gDiagnosticsWindow;
static Boolean gRunning = 1;
static Boolean gMenuBackgroundLoaded = 0;
static Boolean gMenuButtonsLoaded = 0;
static GWorldPtr gMenuWorld;
static Boolean gMenuBlitLogged = 0;
static short gMenuButton = -1;
static char gDiagnosticsText[4096];
static unsigned short gDiagnosticsLength;
static Boolean gDrawingDiagnostics;

static void DebugLog(const char *format, ...)
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
    if (width == 0 || height == 0 || width > JA2_SCREEN_WIDTH ||
        height > JA2_SCREEN_HEIGHT || ReadLittleEndian32(&header[24]) != 256 ||
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
    SetGWorld(gMenuWorld, NULL);
    menuPixMap = GetGWorldPixMap(gMenuWorld);
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

static Boolean RenderMenuButtons(void)
{
    static const unsigned short objectIndex[] = { 0, 3, 7, 10, 15 };
    JA2SLFArchive *archive;
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

    archive = JA2SLFOpen("LoadScreens.slf");
    if (archive == NULL || !JA2SLFFind(archive, "TITLETEXT.STI", &fileOffset,
                                       &fileLength)) {
        if (archive != NULL) JA2SLFClose(archive);
        DebugLog("Menu loader: TITLETEXT.STI not found");
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
    if (objectCount == 0 || objectCount <= objectIndex[4] ||
        ReadLittleEndian32(&header[24]) != 256 ||
        fileLength < 64 + 768 + (unsigned long)objectCount * 16 + storedSize) {
        free(fileData);
        return 0;
    }
    memcpy(palette, fileData + 64, sizeof(palette));
    pixels = fileData + 64 + 768 + (unsigned long)objectCount * 16;
    SetGWorld(gMenuWorld, NULL);
    menuPixMap = GetGWorldPixMap(gMenuWorld);
    if (menuPixMap == NULL || !LockPixels(menuPixMap)) {
        free(fileData);
        return 0;
    }
    for (item = 0; item < 5; ++item) {
        const unsigned char *object = fileData + 64 + 768 +
                                      (unsigned long)objectIndex[item] * 16;
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
    DebugLog("Menu loader: rendered original TITLETEXT button graphics");
    return 1;
}

static void BlitMenuBackground(void)
{
    Rect sourceBounds;
    Rect destinationBounds;
    PixMapHandle sourcePixMap;
    PixMapHandle destinationPixMap;

    if (gMenuWorld == NULL) {
        return;
    }
    /* CopyBits takes rectangles in each port's local coordinate system, not
       offsets into its PixMap. It also lets Color QuickDraw manage the
       window's backing pixels, which must not be treated as a framebuffer. */
    SetRect(&sourceBounds, 0, 0, JA2_SCREEN_WIDTH, JA2_SCREEN_HEIGHT);
    SetRect(&destinationBounds, 0, 0, JA2_SCREEN_WIDTH, JA2_SCREEN_HEIGHT);
    SetPort(gWindow);
    sourcePixMap = GetGWorldPixMap(gMenuWorld);
    destinationPixMap = ((CWindowPtr)gWindow)->portPixMap;
    if (sourcePixMap == NULL || destinationPixMap == NULL) {
        return;
    }
    CopyBits((BitMap *)&((CGrafPtr)gMenuWorld)->portPixMap,
             (BitMap *)&((CWindowPtr)gWindow)->portPixMap,
             &sourceBounds, &destinationBounds, srcCopy, NULL);
    if (!gMenuBlitLogged) {
        gMenuBlitLogged = 1;
        DebugLog("Menu blit: CopyBits 32-bit GWorld to color window");
    }
}

static Boolean RenderMainMenuBackground(void)
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
    unsigned short y;
    unsigned short x;
    unsigned short run;
    unsigned char control;
    unsigned char *source;
    RGBColor color;
    QDErr worldError;
    PixMapHandle menuPixMap;

    DebugLog("Menu loader: opening LoadScreens.slf");
    archive = JA2SLFOpen("LoadScreens.slf");
    if (archive == NULL) {
        DebugLog("Menu loader: cannot open LoadScreens.slf");
        return 0;
    }
    if (!JA2SLFFind(archive, "MAINMENUBACKGROUND.STI", &fileOffset, &fileLength)) {
        DebugLog("Menu loader: MAINMENUBACKGROUND.STI not found in LoadScreens.slf");
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
    DebugLog("Menu loader: read %lu byte STI", fileLength);
    memcpy(header, fileData, sizeof(header));
    if (memcmp(header, "STCI", 4) != 0 || !(ReadLittleEndian32(&header[16]) & 0x0008) ||
        !(ReadLittleEndian32(&header[16]) & 0x0020)) {
        DebugLog("Unsupported menu STCI flags=%08lx", ReadLittleEndian32(&header[16]));
        free(fileData);
        return 0;
    }

    storedSize = ReadLittleEndian32(&header[8]);
    width = ReadLittleEndian16(&header[22]);
    height = ReadLittleEndian16(&header[20]);
    if (width == 0 || height == 0 || width > JA2_SCREEN_WIDTH ||
        height > JA2_SCREEN_HEIGHT || ReadLittleEndian32(&header[24]) != 256 ||
        fileLength < 64 + 768 + 16) {
        DebugLog("Invalid menu header width=%u height=%u colors=%lu", width, height,
                 ReadLittleEndian32(&header[24]));
        free(fileData);
        return 0;
    }
    memcpy(palette, fileData + 64, sizeof(palette));
    memcpy(object, fileData + 64 + 768, sizeof(object));
    DebugLog("Menu loader: header %ux%u stored=%lu", width, height, storedSize);

    dataOffset = ReadLittleEndian32(&object[0]);
    dataLength = ReadLittleEndian32(&object[4]);
    if (dataLength == 0 || dataLength > storedSize || dataOffset > storedSize ||
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
    if (gMenuWorld == NULL) {
        Rect worldBounds = { -40, -40, 560, 760 };
        DebugLog("Menu loader: creating 32-bit 800x600 transfer world");
        worldError = NewGWorld(&gMenuWorld, 32, &worldBounds, NULL, NULL, 0);
        if (worldError != noErr || gMenuWorld == NULL) {
            DebugLog("Menu loader: 32-bit NewGWorld failed error=%d", worldError);
            free(pixels);
            return 0;
        }
    }
    SetGWorld(gMenuWorld, NULL);
    menuPixMap = GetGWorldPixMap(gMenuWorld);
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
    EraseRect(&((GrafPtr)gMenuWorld)->portRect);
    DebugLog("Rendering menu image offscreen %ux%u, ETRLE bytes=%lu",
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
                    WriteMenuPixel(menuPixMap, (short)x, (short)y, &color);
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
    BlitMenuBackground();
    free(pixels);
    return 1;
}

static void DrawCString(short x, short y, const char *text)
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

static void HandleMainMenuClick(Point point)
{
    short index;

    for (index = 0; index < 5; ++index) {
        if (point.h >= 180 && point.h < 460 &&
            point.v >= 277 + index * 37 && point.v < 305 + index * 37) {
            gMenuButton = index;
            if (index == 4) {
                DebugLog("German main menu: quit selected");
                gRunning = 0;
            } else {
                DebugLog("German main menu: button %d selected", index);
            }
            return;
        }
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

    if (event->what == keyDown || event->what == autoKey) {
        key = (char)(event->message & charCodeMask);
        if (key == 'q' || key == 'Q' || (event->modifiers & cmdKey)) {
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
            GlobalToLocal(&point);
            HandleMainMenuClick(point);
        }
    } else if (event->what == updateEvt) {
        WindowPtr updatedWindow = (WindowPtr)event->message;
        BeginUpdate(updatedWindow);
        if (updatedWindow == gDiagnosticsWindow) {
            DrawDiagnostics();
            EndUpdate(updatedWindow);
            return;
        }
        if (gMenuBackgroundLoaded) {
            BlitMenuBackground();
        } else {
            DrawJA2Shell();
        }
        EndUpdate(updatedWindow);
        if (updatedWindow == gWindow && gMenuBackgroundLoaded && !gMenuButtonsLoaded) {
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
    gMenuBackgroundLoaded = RenderMainMenuBackground();
    if (gMenuBackgroundLoaded) {
        RenderMenuLogo();
        gMenuButtonsLoaded = RenderMenuButtons();
        BlitMenuBackground();
        if (!gMenuButtonsLoaded) {
            DrawMainMenu();
        }
    } else {
        DrawJA2Shell();
    }

    while (gRunning) {
        if (WaitNextEvent(everyEvent, &event, 10, NULL)) {
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
    DisposeWindow(window);
    return 0;
}
