#include "ja2_autotest.h"

#include "ja2_mac_internal.h"
#include "ja2_options.h"
#include "ja2_prof.h"
#include "ja2_sgp_selftest.h"

#include <SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Script format, one command per line, '#' starts a comment:
 *
 *   wait <ms>            pause the script (game keeps running)
 *   frames <n>           pause for n main-loop frames
 *   move <x> <y>         move the game's mouse to window coordinates
 *   click <x> <y> [right]
 *   key <name>           SDL key name: Escape, Return, Space, Q, Left, F1...
 *   shot <name>          save the window surface to <name>.bmp
 *   expect <screen>      fail unless the shell shows that screen
 *                        (main, new_game, difficulty, intro, load_game,
 *                        options, credits, laptop)
 *   waitfor <screen> <ms> pause until that screen shows; fail after ms
 *   sgptest              run the Standard Gaming Platform self-test
 *                        (ja2_sgp_selftest.c); each failed check counts
 *   sgpvideo             run the SGP video self-test (after sgptest): draws
 *                        a fixed scene through the original blitters and
 *                        presents it in the window
 *   mark <label>         profiler marker (profile builds) + log line
 *   log <text>           log line
 *   timeout <s>          watchdog, quits with result=timeout (default 600)
 *   quit                 end the test and quit the game
 *
 * Input commands yield one frame so the game handles the event before the
 * next command. The end of the script behaves like "quit".
 */

#define AUTOTEST_MAX_LINES 256
#define AUTOTEST_LINE_LENGTH 96
#define AUTOTEST_LOG_NAME "JA2AUTOTEST.log"

static Boolean gActive;
static Boolean gQuit;
static char (*gLines)[AUTOTEST_LINE_LENGTH];
static short gLineCount;
static short gNextLine;
static Uint32 gStartTicks;
static Uint32 gWaitUntilTicks;
static unsigned long gWaitFrames;
static short gWaitForScreen = -1;
static Uint32 gWaitForDeadline;
static unsigned long gFrame;
static Uint32 gTimeoutMs = 600000;
static short gFailures;
static short gShots;
static Boolean gMouseScripted;
static int gMouseX;
static int gMouseY;

static const char *gScreenNames[] = {
    "main", "new_game", "difficulty", "intro",
    "load_game", "options", "credits", "laptop"
};

static void AutotestLog(const char *format, ...)
{
    va_list arguments;
    char line[200];
    FILE *file;

    va_start(arguments, format);
    vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    DebugLog("Autotest: %s", line);
    file = fopen(AUTOTEST_LOG_NAME, "a");
    if (file != NULL) {
        fprintf(file, "t=%lu f=%lu %s\n",
                (unsigned long)(SDL_GetTicks() - gStartTicks), gFrame, line);
        fclose(file);
    }
}

static FILE *OpenScript(const char *name)
{
    FILE *file;
    char macPath[64];

    file = fopen(name, "r");
    if (file == NULL) {
        snprintf(macPath, sizeof(macPath), ":%s", name);
        file = fopen(macPath, "r");
    }
    return file;
}

void JA2AutotestInit(void)
{
    const char *name = JA2OptionsAutotestScript();
    FILE *file;
    char line[AUTOTEST_LINE_LENGTH];

    if (*name == 0) {
        return;
    }
    remove(AUTOTEST_LOG_NAME);
    gStartTicks = SDL_GetTicks();
    file = OpenScript(name);
    if (file == NULL) {
        AutotestLog("result=error script %s not found", name);
        gQuit = 1;
        return;
    }
    gLines = malloc(sizeof(*gLines) * AUTOTEST_MAX_LINES);
    if (gLines == NULL) {
        fclose(file);
        AutotestLog("result=error out of memory");
        gQuit = 1;
        return;
    }
    while (gLineCount < AUTOTEST_MAX_LINES && fgets(line, sizeof(line), file) != NULL) {
        char *text = line;
        char *end;

        while (*text == ' ' || *text == '\t') {
            ++text;
        }
        end = text + strlen(text);
        while (end > text && (end[-1] == '\r' || end[-1] == '\n' ||
                              end[-1] == ' ' || end[-1] == '\t')) {
            --end;
        }
        *end = 0;
        if (*text == 0 || *text == '#') {
            continue;
        }
        strcpy(gLines[gLineCount++], text);
    }
    fclose(file);
    gActive = 1;
    AutotestLog("start script=%s commands=%d", name, gLineCount);
}

Boolean JA2AutotestActive(void)
{
    return gActive;
}

Boolean JA2AutotestShouldQuit(void)
{
    return gQuit;
}

void JA2AutotestGetMouseState(int *x, int *y)
{
    if (gMouseScripted) {
        *x = gMouseX;
        *y = gMouseY;
        return;
    }
    SDL_GetMouseState(x, y);
}

static void PushMouseMotion(int x, int y)
{
    SDL_Event event;

    gMouseScripted = 1;
    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEMOTION;
    event.motion.windowID = SDL_GetWindowID(gWindow);
    event.motion.x = x;
    event.motion.y = y;
    event.motion.xrel = x - gMouseX;
    event.motion.yrel = y - gMouseY;
    gMouseX = x;
    gMouseY = y;
    SDL_PushEvent(&event);
}

static void PushMouseButton(Uint32 type, Uint8 button, int x, int y)
{
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    event.type = type;
    event.button.windowID = SDL_GetWindowID(gWindow);
    event.button.button = button;
    event.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
    event.button.clicks = 1;
    event.button.x = x;
    event.button.y = y;
    SDL_PushEvent(&event);
}

static void PushKey(Uint32 type, SDL_Keycode key)
{
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    event.type = type;
    event.key.windowID = SDL_GetWindowID(gWindow);
    event.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
    event.key.keysym.sym = key;
    event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    SDL_PushEvent(&event);
}

static void TakeShot(const char *name)
{
    SDL_Surface *surface = SDL_GetWindowSurface(gWindow);
    char path[32];

    /* HFS names are limited to 31 characters. */
    snprintf(path, sizeof(path), "%.27s.bmp", name);
    if (surface == NULL || SDL_SaveBMP(surface, path) != 0) {
        ++gFailures;
        AutotestLog("FAIL shot %s: %s", path, SDL_GetError());
        return;
    }
    ++gShots;
    AutotestLog("shot %s", path);
}

static short ScreenIndex(const char *screen)
{
    short index;

    for (index = 0; index < (short)(sizeof(gScreenNames) / sizeof(gScreenNames[0])); ++index) {
        if (strcmp(gScreenNames[index], screen) == 0) {
            return index;
        }
    }
    return -1;
}

static void Expect(const char *screen)
{
    const char *actual = "unknown";

    if (gMenuScreen >= 0 &&
        gMenuScreen < (short)(sizeof(gScreenNames) / sizeof(gScreenNames[0]))) {
        actual = gScreenNames[gMenuScreen];
    }
    if (strcmp(actual, screen) == 0) {
        AutotestLog("ok expect %s", screen);
    } else {
        ++gFailures;
        AutotestLog("FAIL expect %s actual=%s", screen, actual);
    }
}

static void Finish(const char *reason)
{
    AutotestLog("result=%s failures=%d shots=%d frames=%lu",
                gFailures == 0 && strcmp(reason, "done") == 0 ? "pass" : reason,
                gFailures, gShots, gFrame);
    gActive = 0;
    gQuit = 1;
}

/* Runs one command. Returns 1 when the script must yield this frame. */
static Boolean RunCommand(char *line)
{
    char *command = strtok(line, " \t");
    char *argument = strtok(NULL, "");
    int x = 0;
    int y = 0;
    char extra[16] = "";

    while (argument != NULL && (*argument == ' ' || *argument == '\t')) {
        ++argument;
    }
    if (argument == NULL) {
        argument = "";
    }
    if (strcmp(command, "wait") == 0) {
        gWaitUntilTicks = SDL_GetTicks() + (Uint32)strtoul(argument, NULL, 10);
        return 1;
    }
    if (strcmp(command, "frames") == 0) {
        gWaitFrames = strtoul(argument, NULL, 10);
        return 1;
    }
    if (strcmp(command, "move") == 0 && sscanf(argument, "%d %d", &x, &y) == 2) {
        PushMouseMotion(x, y);
        return 1;
    }
    if (strcmp(command, "click") == 0 && sscanf(argument, "%d %d %15s", &x, &y, extra) >= 2) {
        Uint8 button = strcmp(extra, "right") == 0 ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;

        PushMouseMotion(x, y);
        PushMouseButton(SDL_MOUSEBUTTONDOWN, button, x, y);
        PushMouseButton(SDL_MOUSEBUTTONUP, button, x, y);
        AutotestLog("click %d,%d", x, y);
        return 1;
    }
    if (strcmp(command, "key") == 0) {
        SDL_Keycode key = SDL_GetKeyFromName(argument);

        if (key == SDLK_UNKNOWN) {
            ++gFailures;
            AutotestLog("FAIL unknown key name '%s'", argument);
            return 0;
        }
        PushKey(SDL_KEYDOWN, key);
        PushKey(SDL_KEYUP, key);
        AutotestLog("key %s", argument);
        return 1;
    }
    if (strcmp(command, "waitfor") == 0) {
        char screen[16];
        unsigned long limit = 0;

        if (sscanf(argument, "%15s %lu", screen, &limit) != 2 || ScreenIndex(screen) < 0) {
            ++gFailures;
            AutotestLog("FAIL bad waitfor '%s'", argument);
            return 0;
        }
        gWaitForScreen = ScreenIndex(screen);
        gWaitForDeadline = SDL_GetTicks() + (Uint32)limit;
        return 1;
    }
    if (strcmp(command, "shot") == 0) {
        TakeShot(argument);
        return 0;
    }
    if (strcmp(command, "expect") == 0) {
        Expect(argument);
        return 0;
    }
    if (strcmp(command, "sgptest") == 0) {
        gFailures += (short)JA2SGPSelfTest(AutotestLog);
        return 0;
    }
    if (strcmp(command, "sgpvideo") == 0) {
        /* No yield: a following "shot" captures the SGP frame before the
           shell's own screens draw over it. */
        gFailures += (short)JA2SGPGraphicsSelfTest(AutotestLog);
        return 0;
    }
    if (strcmp(command, "mark") == 0) {
        JA2_PROF_MARK(argument);
        AutotestLog("mark %s", argument);
        return 0;
    }
    if (strcmp(command, "log") == 0) {
        AutotestLog("log %s", argument);
        return 0;
    }
    if (strcmp(command, "timeout") == 0) {
        gTimeoutMs = (Uint32)strtoul(argument, NULL, 10) * 1000;
        return 0;
    }
    if (strcmp(command, "quit") == 0) {
        gNextLine = gLineCount;
        return 1;
    }
    ++gFailures;
    AutotestLog("FAIL bad command '%s %s'", command, argument);
    return 0;
}

void JA2AutotestFrame(void)
{
    if (!gActive) {
        return;
    }
    ++gFrame;
    if (SDL_GetTicks() - gStartTicks > gTimeoutMs) {
        Finish("timeout");
        return;
    }
    if (gWaitFrames > 0) {
        --gWaitFrames;
        return;
    }
    if (gWaitForScreen >= 0) {
        if (gMenuScreen == gWaitForScreen) {
            AutotestLog("ok waitfor %s", gScreenNames[gWaitForScreen]);
            gWaitForScreen = -1;
        } else if ((Sint32)(SDL_GetTicks() - gWaitForDeadline) >= 0) {
            ++gFailures;
            AutotestLog("FAIL waitfor %s timed out", gScreenNames[gWaitForScreen]);
            gWaitForScreen = -1;
        } else {
            return;
        }
    }
    if (gWaitUntilTicks != 0) {
        if ((Sint32)(SDL_GetTicks() - gWaitUntilTicks) < 0) {
            return;
        }
        gWaitUntilTicks = 0;
    }
    while (gNextLine < gLineCount) {
        char line[AUTOTEST_LINE_LENGTH];

        strcpy(line, gLines[gNextLine++]);
        if (RunCommand(line)) {
            return;
        }
    }
    Finish("done");
}

void JA2AutotestShutdown(void)
{
    if (gActive) {
        Finish("interrupted");
    }
    free(gLines);
    gLines = NULL;
}
