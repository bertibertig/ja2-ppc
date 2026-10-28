#include "ja2_sgp_selftest.h"

#include "Types.h"
#include "FileMan.h"
#include "LibraryDataBase.h"
#include "Container.h"
#include "random.h"
#include "timer.h"
#include "video.h"
#include "vsurface.h"
#include "vobject.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, ...) \
    do { \
        if (condition) { \
            report("ok   " __VA_ARGS__); \
        } else { \
            report("FAIL " __VA_ARGS__); \
            ++failures; \
        } \
    } while (0)

int JA2SGPSelfTest(JA2SGPReportProc report)
{
    int failures = 0;
    STRING512 executableDirectory;
    STRING512 dataDirectory;
    STRING512 currentDirectory;
    HWFILE file;
    UINT8 header[4];
    UINT32 bytes = 0;
    UINT32 size;
    INT16 library;
    UINT16 opened = 0;

    /* Same sequence as the original JA2 Splash.c. */
    CHECK(GetExecutableDirectory(executableDirectory), "GetExecutableDirectory '%s'",
          executableDirectory);
    sprintf(dataDirectory, "%s\\Data", executableDirectory);
    CHECK(SetFileManCurrentDirectory(dataDirectory), "SetFileManCurrentDirectory '%s'",
          dataDirectory);
    GetFileManCurrentDirectory(currentDirectory);
    report("info current directory '%s'", currentDirectory);
    CHECK(InitializeFileDatabase(), "InitializeFileDatabase");
    for (library = 0; library < NUMBER_OF_LIBRARIES; ++library) {
        if (gFileDataBase.pLibraries != NULL && gFileDataBase.pLibraries[library].fLibraryOpen) {
            ++opened;
        } else {
            report("FAIL library %s not open", gGameLibaries[library].sLibraryName);
            ++failures;
        }
    }
    report("info %u of %u libraries open", opened, (unsigned)NUMBER_OF_LIBRARIES);

    /* A library file, with the original's backslash path. */
    file = FileOpen("LOADSCREENS\\MAINMENUBACKGROUND.STI",
                    FILE_ACCESS_READ | FILE_OPEN_EXISTING, FALSE);
    CHECK(file != 0, "FileOpen library file MAINMENUBACKGROUND.STI");
    if (file != 0) {
        size = FileGetSize(file);
        CHECK(size == 311408, "FileGetSize %lu (expected 311408)", (unsigned long)size);
        CHECK(FileRead(file, header, 4, &bytes) && bytes == 4 &&
              memcmp(header, "STCI", 4) == 0, "FileRead STCI signature");
        CHECK(FileSeek(file, 100, FILE_SEEK_FROM_START) && FileGetPos(file) == 100,
              "FileSeek/FileGetPos in library file");
        FileClose(file);
    }
    CHECK(FileExists("BINARYDATA\\PROF.DAT"), "FileExists BINARYDATA\\PROF.DAT");
    CHECK(!FileExists("BINARYDATA\\DOES_NOT_EXIST.DAT"), "FileExists rejects a missing file");

    /* A real file next to the application. */
    file = FileOpen("..\\Ja2.ini", FILE_ACCESS_READ | FILE_OPEN_EXISTING, FALSE);
    CHECK(file != 0, "FileOpen real file ..\\Ja2.ini");
    if (file != 0) {
        char text[16];

        memset(text, 0, sizeof(text));
        CHECK(FileRead(file, text, 5, &bytes) && bytes == 5 && text[0] == '[',
              "FileRead real file starts with '[' ('%s')", text);
        FileClose(file);
    }

    /* Write, read back, delete. */
    {
        static const char payload[] = "JA2 PPC SGP file test";
        char readBack[sizeof(payload)];

        file = FileOpen("..\\SGPTEST.TMP", FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE);
        CHECK(file != 0, "FileOpen create ..\\SGPTEST.TMP");
        if (file != 0) {
            CHECK(FileWrite(file, (PTR)payload, sizeof(payload), &bytes) && bytes == sizeof(payload),
                  "FileWrite %lu bytes", (unsigned long)bytes);
            FileClose(file);
        }
        file = FileOpen("..\\SGPTEST.TMP", FILE_ACCESS_READ | FILE_OPEN_EXISTING, FALSE);
        CHECK(file != 0 && FileRead(file, readBack, sizeof(readBack), &bytes) &&
              memcmp(readBack, payload, sizeof(payload)) == 0, "FileRead back written data");
        if (file != 0) {
            FileClose(file);
        }
        CHECK(FileDelete("..\\SGPTEST.TMP") && !FileExists("..\\SGPTEST.TMP"), "FileDelete");
    }

    /* Directory search (save game lists use this). */
    {
        GETFILESTRUCT find;

        CHECK(GetFileFirst("..\\*.ini", &find), "GetFileFirst ..\\*.ini -> '%s'", find.zFileName);
        GetFileClose(&find);
    }

    /* MSVC rand(): seed 1 gives 41, 18467, 6334 on the x86 runtime. */
    {
        int first;
        int second;
        int third;

        srand(1);
        first = rand();
        second = rand();
        third = rand();
        CHECK(first == 41 && second == 18467 && third == 6334,
              "MSVC-compatible rand() %d %d %d", first, second, third);
        CHECK(RAND_MAX == 0x7fff, "RAND_MAX 0x%x", RAND_MAX);
        InitializeRandom();
        CHECK(Random(10) < 10 && Random(1) == 0, "Random() range");
    }

    /* Clock. */
    {
        TIMER start;
        TIMER now;
        UINT32 spin = 0;

        InitializeClockManager();
        start = GetClock();
        do {
            now = GetClock();
        } while (now - start < 20 && ++spin < 100000000UL);
        CHECK(now - start >= 20, "GetClock advances (%lu ms)", (unsigned long)(now - start));
        ShutdownClockManager();
    }

    /* Containers. */
    {
        HQUEUE queue;
        UINT32 value;
        UINT32 sum = 0;

        InitializeContainers();
        queue = CreateQueue(4, sizeof(UINT32));
        for (value = 1; value <= 10; ++value) {
            queue = AddtoQueue(queue, &value);
        }
        while (QueueSize(queue) > 0 && RemfromQueue(queue, &value)) {
            sum += value;
        }
        CHECK(sum == 55, "Queue grows and keeps FIFO data (sum %lu)", (unsigned long)sum);
        DeleteQueue(queue);
        ShutdownContainers();
    }

    report("result sgp failures=%d", failures);
    return failures;
}

static UINT32 FrameBufferChecksum(void)
{
    UINT32 uiPitch;
    UINT8 *pBuffer = (UINT8 *)LockFrameBuffer(&uiPitch);
    UINT32 uiHash = 2166136261UL;
    UINT32 uiRow;
    UINT32 uiCol;

    for (uiRow = 0; uiRow < 480; ++uiRow) {
        UINT16 *pLine = (UINT16 *)(pBuffer + uiRow * uiPitch);

        for (uiCol = 0; uiCol < 640; ++uiCol) {
            uiHash = (uiHash ^ (pLine[uiCol] >> 8)) * 16777619UL;
            uiHash = (uiHash ^ (pLine[uiCol] & 0xff)) * 16777619UL;
        }
    }
    UnlockFrameBuffer();
    return uiHash;
}

static BOOLEAN LoadObject(const char *pFile, UINT32 *puiObject)
{
    VOBJECT_DESC VObjectDesc;

    memset(&VObjectDesc, 0, sizeof(VObjectDesc));
    VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
    strcpy(VObjectDesc.ImageFile, pFile);
    return AddVideoObject(&VObjectDesc, puiObject);
}

int JA2SGPGraphicsSelfTest(JA2SGPReportProc report)
{
    int failures = 0;
    UINT32 uiBackground = 0;
    UINT32 uiHelp = 0;
    UINT32 uiRedMask = 0;
    UINT32 uiGreenMask = 0;
    UINT32 uiBlueMask = 0;
    HVOBJECT hObject;
    UINT32 uiChecksum;

    CHECK(InitializeVideoManager(NULL, 0, NULL), "InitializeVideoManager");
    GetPrimaryRGBDistributionMasks(&uiRedMask, &uiGreenMask, &uiBlueMask);
    CHECK(uiRedMask == 0x7C00 && uiGreenMask == 0x03E0 && uiBlueMask == 0x001F,
          "RGB555 distribution %04lx/%04lx/%04lx", (unsigned long)uiRedMask,
          (unsigned long)uiGreenMask, (unsigned long)uiBlueMask);
    CHECK(InitializeVideoSurfaceManager(), "InitializeVideoSurfaceManager");
    CHECK(InitializeVideoObjectManager(), "InitializeVideoObjectManager");

    CHECK(LoadObject("LOADSCREENS\\MAINMENUBACKGROUND.STI", &uiBackground),
          "AddVideoObject MAINMENUBACKGROUND.STI");
    CHECK(GetVideoObject(&hObject, uiBackground) && hObject->pETRLEObject[0].usWidth == 640 &&
          hObject->pETRLEObject[0].usHeight == 480, "video object is 640x480");
    CHECK(LoadObject("INTERFACE\\HelpScreen.sti", &uiHelp), "AddVideoObject HelpScreen.sti");

    /* The scene: background, two help screen pieces, one of them hanging
       over the right edge (clipped blitter), and a filled rectangle. */
    CHECK(BltVideoObjectFromIndex(FRAME_BUFFER, uiBackground, 0, 0, 0, VO_BLT_SRCTRANSPARENCY, NULL),
          "blit background");
    CHECK(BltVideoObjectFromIndex(FRAME_BUFFER, uiHelp, 8, 100, 60, VO_BLT_SRCTRANSPARENCY, NULL),
          "blit HelpScreen 8");
    CHECK(BltVideoObjectFromIndex(FRAME_BUFFER, uiHelp, 9, 560, 300, VO_BLT_SRCTRANSPARENCY, NULL),
          "blit HelpScreen 9 clipped at the right edge");
    CHECK(ColorFillVideoSurfaceArea(FRAME_BUFFER, 20, 400, 120, 440, Get16BPPColor(FROMRGB(255, 0, 0))),
          "ColorFillVideoSurfaceArea");
    uiChecksum = FrameBufferChecksum();
    report("info frame buffer checksum %08lx", (unsigned long)uiChecksum);

    InvalidateScreen();
    RefreshScreen(NULL);
    report("ok   RefreshScreen presented the frame");

    DeleteVideoObjectFromIndex(uiHelp);
    DeleteVideoObjectFromIndex(uiBackground);
    report("result sgp-video failures=%d checksum=%08lx", failures, (unsigned long)uiChecksum);
    return failures;
}
