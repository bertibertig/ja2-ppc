#include "laptop.h"

#include "../ja2_mac_internal.h"
#include "../ja2_music.h"
#include "../ja2_ppc_text.h"
#include "../ja2_slf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LAPTOP_SCREEN_UL_X 111
#define LAPTOP_SCREEN_UL_Y 27
#define LAPTOP_SCREEN_LR_X 613
#define LAPTOP_SCREEN_LR_Y 427
#define LAPTOP_BASE_X 0
#define LAPTOP_BASE_Y 0

#define LAPTOP_HELP_X 156
#define LAPTOP_HELP_Y 81
#define LAPTOP_HELP_WIDTH 412
#define LAPTOP_HELP_HEIGHT 292

static SDL_Surface *gLaptopBaseSurface;
static SDL_Surface *gLaptopHelpArtSurface;
static SDL_Surface *gLaptopHelpButtonNormal;
static SDL_Surface *gLaptopHelpButtonSelected;
static SDL_Surface *gLaptopHelpExit;
static unsigned char *gLaptopFontData;
static unsigned long gLaptopFontLength;
static unsigned long gLaptopFontStoredSize;
static unsigned short gLaptopFontObjectCount;
static Boolean gLaptopEntered;
static Boolean gLaptopWelcomeVisible;
static short gLaptopHelpPage;

static unsigned short ReadLE16(const unsigned char *bytes)
{
    return (unsigned short)bytes[0] | (unsigned short)(bytes[1] << 8);
}

static unsigned long ReadLE32(const unsigned char *bytes)
{
    return ((unsigned long)bytes[0]) |
           ((unsigned long)bytes[1] << 8) |
           ((unsigned long)bytes[2] << 16) |
           ((unsigned long)bytes[3] << 24);
}

static void WritePixel(SDL_Surface *surface, short x, short y, Uint16 color)
{
    Uint16 *row;

    if (surface == NULL || x < 0 || y < 0 || x >= surface->w || y >= surface->h) {
        return;
    }
    row = (Uint16 *)((Uint8 *)surface->pixels + (long)y * surface->pitch);
    row[x] = color;
}

static Boolean ReadArchiveEntry(const char *archiveName, const char *entryName,
                                unsigned char **data, unsigned long *length)
{
    JA2SLFArchive *archive;
    unsigned long offset;
    unsigned long entryLength;
    unsigned char *entryData;

    archive = JA2SLFOpen(archiveName);
    if (archive == NULL ||
        !JA2SLFFind(archive, entryName, &offset, &entryLength)) {
        if (archive != NULL) {
            JA2SLFClose(archive);
        }
        DebugLog("Laptop asset: %s not found in %s", entryName, archiveName);
        return 0;
    }
    entryData = (unsigned char *)malloc(entryLength);
    if (entryData == NULL || !JA2SLFRead(archive, offset, entryData, entryLength)) {
        DebugLog("Laptop asset: failed reading %s length=%lu", entryName, entryLength);
        free(entryData);
        JA2SLFClose(archive);
        return 0;
    }
    JA2SLFClose(archive);
    *data = entryData;
    *length = entryLength;
    DebugLog("Laptop asset: loaded %s from %s offset=%lu length=%lu",
             entryName, archiveName, offset, entryLength);
    return 1;
}

static Boolean DrawStiObject(SDL_Surface *destination, const char *archiveName,
                             const char *entryName, unsigned short objectIndex,
                             short destinationX, short destinationY)
{
    unsigned char *fileData;
    unsigned long fileLength;
    unsigned long storedSize;
    unsigned long objectCount;
    const unsigned char *object;
    unsigned char *pixels;
    unsigned char *source;
    unsigned char palette[768];
    unsigned long dataOffset;
    unsigned long dataLength;
    unsigned short width;
    unsigned short height;
    unsigned short row;
    unsigned short column;
    unsigned short run;
    unsigned char control;
    Boolean result = 0;

    if (!ReadArchiveEntry(archiveName, entryName, &fileData, &fileLength)) {
        return 0;
    }
    if (fileLength < 64 + 768 + 16 || memcmp(fileData, "STCI", 4) != 0 ||
        !(ReadLE32(&fileData[16]) & 0x0008) ||
        !(ReadLE32(&fileData[16]) & 0x0020) || ReadLE32(&fileData[24]) != 256) {
        DebugLog("Laptop asset: unsupported STI %s", entryName);
        free(fileData);
        return 0;
    }
    storedSize = ReadLE32(&fileData[8]);
    objectCount = ReadLE16(&fileData[28]);
    if (objectIndex >= objectCount ||
        fileLength < 64 + 768 + objectCount * 16 + storedSize) {
        DebugLog("Laptop asset: invalid STI object=%u count=%lu %s",
                 objectIndex, objectCount, entryName);
        free(fileData);
        return 0;
    }
    object = fileData + 64 + 768 + (unsigned long)objectIndex * 16;
    dataOffset = ReadLE32(&object[0]);
    dataLength = ReadLE32(&object[4]);
    width = ReadLE16(&object[14]);
    height = ReadLE16(&object[12]);
    if (width == 0 || height == 0 || dataOffset > storedSize ||
        dataLength > storedSize - dataOffset) {
        DebugLog("Laptop asset: invalid STI object dimensions=%ux%u %s",
                 width, height, entryName);
        free(fileData);
        return 0;
    }
    memcpy(palette, fileData + 64, sizeof(palette));
    pixels = fileData + 64 + 768 + objectCount * 16;
    source = pixels + dataOffset;

    if (SDL_MUSTLOCK(destination)) {
        SDL_LockSurface(destination);
    }
    for (row = 0; row < height; ++row) {
        column = 0;
        while (column < width &&
               (unsigned long)(source - (pixels + dataOffset)) < dataLength) {
            control = *source++;
            if (control == 0) {
                break;
            }
            run = (unsigned short)(control & 0x7f);
            if (run == 0 || column + run > width) {
                goto draw_sti_done;
            }
            if (control & 0x80) {
                column = (unsigned short)(column + run);
            } else {
                if ((unsigned long)(source - (pixels + dataOffset)) + run > dataLength) {
                    goto draw_sti_done;
                }
                while (run-- != 0) {
                    unsigned char paletteIndex = *source++;
                    Uint16 color = JA2_RGB555(palette[paletteIndex * 3],
                                              palette[paletteIndex * 3 + 1],
                                              palette[paletteIndex * 3 + 2]);
                    WritePixel(destination,
                               (short)(destinationX + (short)ReadLE16(&object[8]) + column),
                               (short)(destinationY + (short)ReadLE16(&object[10]) + row),
                               color);
                    ++column;
                }
            }
        }
        if (column == width &&
            (unsigned long)(source - (pixels + dataOffset)) < dataLength &&
            *source == 0) {
            ++source;
        }
    }
    result = 1;

draw_sti_done:
    if (SDL_MUSTLOCK(destination)) {
        SDL_UnlockSurface(destination);
    }
    free(fileData);
    if (!result) {
        DebugLog("Laptop asset: malformed ETRLE data in %s object=%u",
                 entryName, objectIndex);
    }
    return result;
}

static Boolean DrawPcx(SDL_Surface *destination, const char *archiveName,
                       const char *entryName, short destinationX,
                       short destinationY)
{
    unsigned char *fileData;
    unsigned long fileLength;
    unsigned short width;
    unsigned short height;
    unsigned short bytesPerLine;
    unsigned char colorPlanes;
    unsigned long dataEnd;
    unsigned long position;
    unsigned short row;
    unsigned short column;
    unsigned short decoded;
    unsigned char *scanline;
    const unsigned char *palette;

    if (!ReadArchiveEntry(archiveName, entryName, &fileData, &fileLength)) {
        return 0;
    }
    if (fileLength < 128 + 769 || fileData[0] != 0x0a || fileData[3] != 8) {
        DebugLog("Laptop asset: unsupported PCX %s", entryName);
        free(fileData);
        return 0;
    }
    width = (unsigned short)(ReadLE16(&fileData[8]) - ReadLE16(&fileData[4]) + 1);
    height = (unsigned short)(ReadLE16(&fileData[10]) - ReadLE16(&fileData[6]) + 1);
    bytesPerLine = ReadLE16(&fileData[66]);
    colorPlanes = fileData[65];
    dataEnd = fileLength - 769;
    palette = fileData + dataEnd + 1;
    if (width == 0 || height == 0 || colorPlanes != 1 || bytesPerLine < width ||
        fileData[dataEnd] != 12) {
        DebugLog("Laptop asset: invalid PCX geometry %ux%u stride=%u planes=%u %s",
                 width, height, bytesPerLine, colorPlanes, entryName);
        free(fileData);
        return 0;
    }
    scanline = (unsigned char *)malloc(bytesPerLine);
    if (scanline == NULL) {
        free(fileData);
        return 0;
    }
    position = 128;
    if (SDL_MUSTLOCK(destination)) {
        SDL_LockSurface(destination);
    }
    for (row = 0; row < height; ++row) {
        decoded = 0;
        while (decoded < bytesPerLine && position < dataEnd) {
            unsigned char value = fileData[position++];
            unsigned short runLength = 1;
            if ((value & 0xc0) == 0xc0) {
                runLength = (unsigned short)(value & 0x3f);
                if (position >= dataEnd) {
                    break;
                }
                value = fileData[position++];
            }
            while (runLength-- != 0 && decoded < bytesPerLine) {
                scanline[decoded++] = value;
            }
        }
        if (decoded != bytesPerLine) {
            if (SDL_MUSTLOCK(destination)) {
                SDL_UnlockSurface(destination);
            }
            free(scanline);
            free(fileData);
            DebugLog("Laptop asset: truncated PCX row=%u %s", row, entryName);
            return 0;
        }
        for (column = 0; column < width; ++column) {
            unsigned char paletteIndex = scanline[column];
            WritePixel(destination, (short)(destinationX + column),
                       (short)(destinationY + row),
                       JA2_RGB555(palette[paletteIndex * 3],
                                  palette[paletteIndex * 3 + 1],
                                  palette[paletteIndex * 3 + 2]));
        }
    }
    if (SDL_MUSTLOCK(destination)) {
        SDL_UnlockSurface(destination);
    }
    free(scanline);
    free(fileData);
    DebugLog("Laptop asset: rendered PCX %s size=%ux%u at %d,%d",
             entryName, width, height, destinationX, destinationY);
    return 1;
}

static Boolean LoadLaptopFont(void)
{
    if (gLaptopFontData != NULL) {
        return 1;
    }
    if (!ReadArchiveEntry("Fonts.slf", "FONT10ARIAL.sti", &gLaptopFontData,
                          &gLaptopFontLength)) {
        DebugLog("Laptop font: FONT10ARIAL.sti unavailable; using fallback text");
        return 0;
    }
    if (gLaptopFontLength < 64 + 768 + 16 ||
        memcmp(gLaptopFontData, "STCI", 4) != 0 ||
        !(ReadLE32(&gLaptopFontData[16]) & 0x0008) ||
        !(ReadLE32(&gLaptopFontData[16]) & 0x0020) ||
        ReadLE32(&gLaptopFontData[24]) != 256) {
        DebugLog("Laptop font: invalid FONT10ARIAL.sti header");
        free(gLaptopFontData);
        gLaptopFontData = NULL;
        return 0;
    }
    gLaptopFontStoredSize = ReadLE32(&gLaptopFontData[8]);
    gLaptopFontObjectCount = ReadLE16(&gLaptopFontData[28]);
    if (gLaptopFontObjectCount == 0 ||
        gLaptopFontLength < 64 + 768 +
            (unsigned long)gLaptopFontObjectCount * 16 + gLaptopFontStoredSize) {
        DebugLog("Laptop font: invalid FONT10ARIAL.sti object table");
        free(gLaptopFontData);
        gLaptopFontData = NULL;
        return 0;
    }
    DebugLog("Laptop font: loaded FONT10ARIAL.sti objects=%u", gLaptopFontObjectCount);
    return 1;
}

static unsigned short LaptopFontIndex(unsigned char character)
{
    if (character >= 'A' && character <= 'Z') return (unsigned short)(character - 'A');
    if (character >= 'a' && character <= 'z') return (unsigned short)(26 + character - 'a');
    if (character >= '0' && character <= '9') return (unsigned short)(52 + character - '0');
    switch (character) {
    case '!': return 62;
    case '@': return 63;
    case '#': return 64;
    case '$': return 65;
    case '%': return 66;
    case '^': return 67;
    case '&': return 68;
    case '*': return 69;
    case '(': return 70;
    case ')': return 71;
    case '-': return 72;
    case '_': return 73;
    case '+': return 74;
    case '=': return 75;
    case '|': return 76;
    case '\\': return 77;
    case '{': return 78;
    case '}': return 79;
    case '[': return 80;
    case ']': return 81;
    case ':': return 82;
    case ';': return 83;
    case '"': return 84;
    case '\'': return 85;
    case '<': return 86;
    case '>': return 87;
    case ',': return 88;
    case '.': return 89;
    case '?': return 90;
    case '/': return 91;
    case ' ': return 92;
    default: return 0;
    }
}

static void DrawLaptopTextLine(SDL_Surface *surface, short x, short y,
                               const char *text, Uint16 color)
{
    const unsigned char *pixels;
    short cursorX = x;
    const unsigned char *character;

    if (!LoadLaptopFont()) {
        JA2DrawText(surface, x, y, text, color, 9);
        return;
    }
    pixels = gLaptopFontData + 64 + 768 +
             (unsigned long)gLaptopFontObjectCount * 16;
    if (SDL_MUSTLOCK(surface)) {
        SDL_LockSurface(surface);
    }
    while (*text != 0) {
        unsigned short objectIndex = LaptopFontIndex((unsigned char)*text++);
        unsigned long dataOffset;
        unsigned long dataLength;
        unsigned short width;
        unsigned short height;
        unsigned short row;
        unsigned short column;
        unsigned short run;
        unsigned char control;
        unsigned char *source;
        short offsetX;
        short offsetY;

        if (objectIndex >= gLaptopFontObjectCount) objectIndex = 0;
        character = gLaptopFontData + 64 + 768 +
                    (unsigned long)objectIndex * 16;
        dataOffset = ReadLE32(&character[0]);
        dataLength = ReadLE32(&character[4]);
        offsetX = (short)ReadLE16(&character[8]);
        offsetY = (short)ReadLE16(&character[10]);
        height = ReadLE16(&character[12]);
        width = ReadLE16(&character[14]);
        if (width == 0 || height == 0 || dataOffset > gLaptopFontStoredSize ||
            dataLength > gLaptopFontStoredSize - dataOffset) {
            cursorX = (short)(cursorX + 4);
            continue;
        }
        source = (unsigned char *)(pixels + dataOffset);
        for (row = 0; row < height; ++row) {
            column = 0;
            while (column < width &&
                   (unsigned long)(source - (pixels + dataOffset)) < dataLength) {
                control = *source++;
                if (control == 0) {
                    break;
                }
                run = (unsigned short)(control & 0x7f);
                if (run == 0 || column + run > width) {
                    break;
                }
                if (control & 0x80) {
                    column = (unsigned short)(column + run);
                } else {
                    if ((unsigned long)(source - (pixels + dataOffset)) + run > dataLength) {
                        break;
                    }
                    while (run-- != 0) {
                        WritePixel(surface, (short)(cursorX + offsetX + column),
                                   (short)(y + offsetY + row), color);
                        ++column;
                        ++source;
                    }
                }
            }
            if (column == width &&
                (unsigned long)(source - (pixels + dataOffset)) < dataLength &&
                *source == 0) {
                ++source;
            }
        }
        cursorX = (short)(cursorX + width + 1);
    }
    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }
}

static short LaptopTextWidth(const char *text)
{
    short width = 0;

    if (!LoadLaptopFont()) return (short)(strlen(text) * 6);
    while (*text != 0) {
        unsigned short objectIndex = LaptopFontIndex((unsigned char)*text++);
        const unsigned char *character;
        if (objectIndex >= gLaptopFontObjectCount) objectIndex = 0;
        character = gLaptopFontData + 64 + 768 +
                    (unsigned long)objectIndex * 16;
        width = (short)(width + ReadLE16(&character[14]) + 1);
    }
    return width;
}

static Boolean PrepareLaptopHelpAssets(void)
{
    if (gLaptopHelpArtSurface != NULL) return 1;
    gLaptopHelpArtSurface = SDL_CreateRGBSurfaceWithFormat(
        0, LAPTOP_HELP_WIDTH, LAPTOP_HELP_HEIGHT, 16, SDL_PIXELFORMAT_RGB555);
    gLaptopHelpButtonNormal = SDL_CreateRGBSurfaceWithFormat(
        0, 77, 22, 16, SDL_PIXELFORMAT_RGB555);
    gLaptopHelpButtonSelected = SDL_CreateRGBSurfaceWithFormat(
        0, 77, 22, 16, SDL_PIXELFORMAT_RGB555);
    gLaptopHelpExit = SDL_CreateRGBSurfaceWithFormat(
        0, 22, 22, 16, SDL_PIXELFORMAT_RGB555);
    if (gLaptopHelpArtSurface == NULL || gLaptopHelpButtonNormal == NULL ||
        gLaptopHelpButtonSelected == NULL || gLaptopHelpExit == NULL) {
        DebugLog("Laptop help: SDL2 asset surface allocation failed");
        return 0;
    }
    if (!DrawStiObject(gLaptopHelpArtSurface, "Interface.slf", "HelpScreen.sti", 8, 0, 0) ||
        !DrawStiObject(gLaptopHelpArtSurface, "Interface.slf", "HelpScreen.sti", 9, 92, 0) ||
        !DrawStiObject(gLaptopHelpButtonNormal, "Interface.slf", "HelpScreen.sti", 1, 0, 0) ||
        !DrawStiObject(gLaptopHelpButtonSelected, "Interface.slf", "HelpScreen.sti", 3, 0, 0) ||
        !DrawStiObject(gLaptopHelpExit, "Interface.slf", "HelpScreen.sti", 0, 0, 0)) {
        DebugLog("Laptop help: failed to cache HelpScreen.sti objects");
        return 0;
    }
    DebugLog("Laptop help: cached background, button and exit sprites");
    return 1;
}

static void DrawLaptopText(SDL_Surface *surface)
{
    DrawLaptopTextLine(surface, 59, 77, "E-mail", JA2_COLOR_BLACK);
    DrawLaptopTextLine(surface, 59, 109, "Web", JA2_COLOR_BLACK);
    DrawLaptopTextLine(surface, 59, 141, "Dateien", JA2_COLOR_BLACK);
    DrawLaptopTextLine(surface, 59, 173, "Logbuch", JA2_COLOR_BLACK);
    DrawLaptopTextLine(surface, 59, 205, "Personal", JA2_COLOR_BLACK);
    DrawLaptopTextLine(surface, 59, 252, "Finanzen", JA2_COLOR_BLACK);
    DrawLaptopTextLine(surface, 54, 389, "Schliessen", JA2_COLOR_BLACK);
    DrawLaptopTextLine(surface, 47, 224, "Soeldner: 0", JA2_COLOR_BLACK);
    {
        char balance[32];
        sprintf(balance, "$%ld", gNewGameStartingCash);
        DrawLaptopTextLine(surface, 47, 272, balance, JA2_COLOR_BLACK);
    }
    DrawLaptopTextLine(surface, 35, 433, "Tag 1, 01:00", JA2_COLOR_BLACK);
}

static void DrawLaptopWelcome(SDL_Surface *surface)
{
    static const char *buttons[] = {
        "Ueberblick", "E-mail", "Web", "Dateien", "Logbuch", "Personal",
        "Finanzen", "Soldnerstatistik"
    };
    short index;
    static const char *pageTitles[] = {
        "Ueberblick", "E-mail", "Web", "Dateien", "Logbuch", "Personal",
        "Finanzen", "Soldnerstatistik"
    };
    static const char *pageBody[8][4] = {
        { "Hier finden Sie mehrere Programme, mit denen Sie",
          "Ihren Urlaub in Arulco organisieren koennen.", NULL, NULL },
        { "Hier koennen Sie Ihre E-Mails lesen und Nachrichten",
          "an Ihre Kontakte senden.", NULL, NULL },
        { "Hier finden Sie wichtige Webseiten und koennen",
          "Informationen ueber Arulco abrufen.", NULL, NULL },
        { "Hier werden Dateien und E-Mail-Anhaenge angezeigt.", NULL, NULL, NULL },
        { "Hier finden Sie eine Zusammenfassung der Ereignisse",
          "Ihrer Kampagne.", NULL, NULL },
        { "Hier sehen Sie Informationen ueber Ihre Soeldner",
          "und Ihr Team.", NULL, NULL },
        { "Hier finden Sie Ihre finanziellen Transaktionen",
          "und den aktuellen Kontostand.", NULL, NULL },
        { "Hier sehen Sie die Statistik Ihrer Soeldner.", NULL, NULL, NULL }
    };

    /* HelpScreen.sti supplies the original border and dark panel artwork. */
    PrepareLaptopHelpAssets();
    if (gLaptopHelpArtSurface != NULL) {
        SDL_Rect destination = { LAPTOP_HELP_X, LAPTOP_HELP_Y, 0, 0 };
        SDL_BlitSurface(gLaptopHelpArtSurface, NULL, surface, &destination);
    }
    for (index = 0; index < 8; ++index) {
        short buttonX = (short)(LAPTOP_HELP_X + 11);
        short buttonY = (short)(LAPTOP_HELP_Y + 12 + index * 30);
        short textX = (short)(buttonX + (77 - LaptopTextWidth(buttons[index])) / 2);
        SDL_Surface *buttonSurface = index == gLaptopHelpPage ?
            gLaptopHelpButtonSelected : gLaptopHelpButtonNormal;
        if (buttonSurface != NULL) {
            SDL_Rect destination = { buttonX, buttonY, 0, 0 };
            SDL_BlitSurface(buttonSurface, NULL, surface, &destination);
        }
        DrawLaptopTextLine(surface, textX, (short)(buttonY + 6), buttons[index],
                           JA2_COLOR_WHITE);
    }
    DrawLaptopTextLine(surface, (short)(LAPTOP_HELP_X + 107), (short)(LAPTOP_HELP_Y + 7),
                       gLaptopHelpPage == 0 ? "Ihr Laptop heisst Sie herzlich willkommen!" :
                       pageTitles[gLaptopHelpPage], JA2_COLOR_WHITE);
    DrawLaptopTextLine(surface, (short)(LAPTOP_HELP_X + 107), (short)(LAPTOP_HELP_Y + 22),
                       "(Druecken Sie H, um jederzeit Hilfe aufzurufen)", JA2_COLOR_WHITE);
    for (index = 0; index < 4 && pageBody[gLaptopHelpPage][index] != NULL; ++index) {
        DrawLaptopTextLine(surface, (short)(LAPTOP_HELP_X + 107),
                           (short)(LAPTOP_HELP_Y + 48 + index * 15),
                           pageBody[gLaptopHelpPage][index], JA2_COLOR_WHITE);
    }
    FillRectRGB555(surface, (short)(LAPTOP_HELP_X + 11),
                   (short)(LAPTOP_HELP_Y + 263),
                   (short)(LAPTOP_HELP_X + 28),
                   (short)(LAPTOP_HELP_Y + 280), JA2_COLOR_BLACK);
    FrameRectRGB555(surface, (short)(LAPTOP_HELP_X + 11),
                    (short)(LAPTOP_HELP_Y + 263),
                    (short)(LAPTOP_HELP_X + 28),
                    (short)(LAPTOP_HELP_Y + 280), JA2_COLOR_WHITE);
    DrawLaptopTextLine(surface, (short)(LAPTOP_HELP_X + 38),
                       (short)(LAPTOP_HELP_Y + 276),
                       "Diese Hilfe nicht mehr anzeigen", JA2_COLOR_WHITE);
    if (gLaptopHelpExit != NULL) {
        SDL_Rect destination = { (short)(LAPTOP_HELP_X + 92 + 291),
                                 (short)(LAPTOP_HELP_Y + 9), 0, 0 };
        SDL_BlitSurface(gLaptopHelpExit, NULL, surface, &destination);
    }
}

static Boolean BuildLaptopBase(void)
{
    short index;
    static const short buttonY[] = { 66, 98, 130, 162, 194, 241, 378 };
    static const char *buttonEntries[] = {
        "buttonsforlaptop.sti", "buttonsforlaptop.sti",
        "buttonsforlaptop.sti", "buttonsforlaptop.sti",
        "buttonsforlaptop.sti", "buttonsforlaptop.sti",
        "buttonsforlaptop.sti"
    };

    if (gLaptopBaseSurface != NULL) {
        return 1;
    }
    gLaptopBaseSurface = SDL_CreateRGBSurfaceWithFormat(
        0, JA2_SCREEN_WIDTH, JA2_SCREEN_HEIGHT, 16, SDL_PIXELFORMAT_RGB555);
    if (gLaptopBaseSurface == NULL) {
        DebugLog("Laptop: base surface creation failed: %s", SDL_GetError());
        return 0;
    }
    SDL_FillRect(gLaptopBaseSurface, NULL, JA2_RGB555(32, 32, 32));
    DrawStiObject(gLaptopBaseSurface, "Laptop.slf", "laptop3.sti", 0,
                  LAPTOP_BASE_X, LAPTOP_BASE_Y);
    DrawStiObject(gLaptopBaseSurface, "Laptop.slf", "taskbar.sti", 1,
                  25, 23);
#if defined(JA2_LANGUAGE_GERMAN)
    if (!DrawPcx(gLaptopBaseSurface, "Laptop.slf", "desktop.pcx", 109, 24)) {
        DebugLog("Laptop: desktop background unavailable");
    }
#else
    if (!DrawPcx(gLaptopBaseSurface, "Laptop.slf", "desktop.pcx", 109, 24)) {
        DebugLog("Laptop: desktop background unavailable");
    }
#endif
    for (index = 0; index < 7; ++index) {
        DrawStiObject(gLaptopBaseSurface, "Laptop.slf", buttonEntries[index],
                      0, 29, buttonY[index]);
    }
    DrawStiObject(gLaptopBaseSurface, "Laptop.slf", "lights.sti", 0, 44, 466);
    DrawStiObject(gLaptopBaseSurface, "Laptop.slf", "lights.sti", 1, 88, 466);
    DrawStiObject(gLaptopBaseSurface, "Laptop.slf", "icons.sti", 6, 67, 412);
    DrawStiObject(gLaptopBaseSurface, "Laptop.slf", "icons.sti", 7, 83, 412);
    DrawLaptopText(gLaptopBaseSurface);
    DebugLog("Laptop: cached SDL2 RGB555 base composition");
    return 1;
}

Boolean JA2LaptopRender(void)
{
    if (!BuildLaptopBase() || gMenuSurface == NULL) {
        return 0;
    }
    if (!gLaptopEntered) {
        gLaptopEntered = 1;
        gLaptopWelcomeVisible = 1;
        gLaptopHelpPage = 0;
        JA2LaptopMusicStart();
        DebugLog("Laptop: entered original-coordinate SDL2 screen welcomeVisible=1");
    }
    SDL_BlitSurface(gLaptopBaseSurface, NULL, gMenuSurface, NULL);
    if (gLaptopWelcomeVisible) {
        DrawLaptopWelcome(gMenuSurface);
    }
    JA2BlitSurfaceToWindow(gMenuSurface, "laptop screen");
    return 1;
}

void JA2LaptopHandleClick(short x, short y)
{
    DebugLog("Laptop: click local=%d,%d welcomeVisible=%d", x, y,
             gLaptopWelcomeVisible);
    if (gLaptopWelcomeVisible) {
        if (x >= LAPTOP_HELP_X + 92 + 291 && x < LAPTOP_HELP_X + 92 + 313 &&
            y >= LAPTOP_HELP_Y + 9 && y < LAPTOP_HELP_Y + 31) {
            gLaptopWelcomeVisible = 0;
            JA2LaptopRender();
            DebugLog("Laptop: welcome overlay closed");
        } else if (x >= LAPTOP_HELP_X + 11 && x < LAPTOP_HELP_X + 11 + 77 &&
                   y >= LAPTOP_HELP_Y + 12 && y < LAPTOP_HELP_Y + 12 + 8 * 30) {
            gLaptopHelpPage = (short)((y - LAPTOP_HELP_Y - 12) / 30);
            DebugLog("Laptop: help page selected=%d", gLaptopHelpPage);
            JA2LaptopRender();
        }
        return;
    }
    if (x >= 29 && x < 150 && y >= 371 && y < 430) {
        DebugLog("Laptop: close requested; returning to main menu placeholder");
        JA2LaptopShutdown();
        JA2MenuMusicStart();
        RestoreMainMenu();
    }
}

void JA2LaptopHandleKey(SDL_Keycode key)
{
    if (key == SDLK_ESCAPE) {
        if (gLaptopWelcomeVisible) {
            gLaptopWelcomeVisible = 0;
            JA2LaptopRender();
            DebugLog("Laptop: welcome overlay closed by Escape");
        } else {
            JA2LaptopShutdown();
            JA2MenuMusicStart();
            RestoreMainMenu();
        }
    } else if (key == SDLK_h) {
        gLaptopWelcomeVisible = 1;
        JA2LaptopRender();
        DebugLog("Laptop: welcome overlay opened by H");
    }
}

void JA2LaptopShutdown(void)
{
    if (gLaptopBaseSurface != NULL) {
        SDL_FreeSurface(gLaptopBaseSurface);
        gLaptopBaseSurface = NULL;
    }
    if (gLaptopHelpArtSurface != NULL) {
        SDL_FreeSurface(gLaptopHelpArtSurface);
        gLaptopHelpArtSurface = NULL;
    }
    if (gLaptopHelpButtonNormal != NULL) {
        SDL_FreeSurface(gLaptopHelpButtonNormal);
        gLaptopHelpButtonNormal = NULL;
    }
    if (gLaptopHelpButtonSelected != NULL) {
        SDL_FreeSurface(gLaptopHelpButtonSelected);
        gLaptopHelpButtonSelected = NULL;
    }
    if (gLaptopHelpExit != NULL) {
        SDL_FreeSurface(gLaptopHelpExit);
        gLaptopHelpExit = NULL;
    }
    if (gLaptopFontData != NULL) {
        free(gLaptopFontData);
        gLaptopFontData = NULL;
    }
    gLaptopEntered = 0;
    gLaptopWelcomeVisible = 0;
    DebugLog("Laptop: SDL2 surfaces shut down");
}
