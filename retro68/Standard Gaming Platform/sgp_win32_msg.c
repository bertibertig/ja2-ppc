/* JA2 PPC: window messages, input hooks, multimedia timers and INI files
 * of the Win32 subset (see sgp_win32.h). The original sgp.c message loop,
 * its WindowProcedure and input.c's hooks run unchanged on top of this;
 * sgp_sdl.c feeds it from SDL events. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Mac headers first: sgp_win32.h's Win32 macros (ShowWindow, ...) would
   clash with Toolbox prototypes of the same names. */
#ifdef macintosh
#include <Timer.h>
#else
#endif

#include "sgp_win32.h"

void DebugLog(const char *format, ...);
FILE *SGPFOpen(const char *windowsPath, const char *mode);

/* ------------------------------------------------------------------ */
/* Messages                                                            */

#define SGP_MESSAGE_QUEUE 64

static MSG gQueue[SGP_MESSAGE_QUEUE];
static int gQueueHead;
static int gQueueCount;
static WNDPROC gWindowProc;

SGPPumpProc gpSGPPumpEvents;
SGPWarpMouseProc gpSGPWarpMouse;

void SGPSetWindowProc(WNDPROC proc)
{
    gWindowProc = proc;
}

#ifndef macintosh
static void ServiceHostTimers(void);
#endif

static void Pump(BOOL wait)
{
#ifndef macintosh
    ServiceHostTimers();
#endif
    if (gpSGPPumpEvents)
        gpSGPPumpEvents(wait);
}

static int Matches(const MSG *message, UINT first, UINT last)
{
    if (first == 0 && last == 0)
        return 1;
    return message->message >= first && message->message <= last;
}

static int FindMessage(UINT first, UINT last)
{
    int index;
    for (index = 0; index < gQueueCount; ++index) {
        int slot = (gQueueHead + index) % SGP_MESSAGE_QUEUE;
        if (Matches(&gQueue[slot], first, last))
            return index;
    }
    return -1;
}

static void RemoveMessage(int index, MSG *message)
{
    int slot = (gQueueHead + index) % SGP_MESSAGE_QUEUE;
    if (message)
        *message = gQueue[slot];
    /* Close the gap; the queue is short and messages are rare. */
    for (; index + 1 < gQueueCount; ++index) {
        int to = (gQueueHead + index) % SGP_MESSAGE_QUEUE;
        int from = (gQueueHead + index + 1) % SGP_MESSAGE_QUEUE;
        gQueue[to] = gQueue[from];
    }
    --gQueueCount;
}

BOOL PostMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    MSG *entry;
    if (gQueueCount == SGP_MESSAGE_QUEUE) {
        DebugLog("SGP: message queue full, dropped message 0x%x", message);
        return FALSE;
    }
    entry = &gQueue[(gQueueHead + gQueueCount) % SGP_MESSAGE_QUEUE];
    entry->hwnd = window;
    entry->message = message;
    entry->wParam = wParam;
    entry->lParam = lParam;
    entry->time = GetTickCount();
    entry->pt.x = 0;
    entry->pt.y = 0;
    ++gQueueCount;
    return TRUE;
}

void PostQuitMessage(int exitCode)
{
    PostMessage(NULL, WM_QUIT, (WPARAM)exitCode, 0);
}

BOOL PeekMessage(MSG *message, HWND window, UINT first, UINT last, UINT remove)
{
    int index;
    (void)window;
    Pump(FALSE);
    index = FindMessage(first, last);
    if (index < 0)
        return FALSE;
    if (remove & PM_REMOVE) {
        RemoveMessage(index, message);
        if (index == 0 && gQueueCount == 0)
            gQueueHead = 0;
    } else if (message) {
        *message = gQueue[(gQueueHead + index) % SGP_MESSAGE_QUEUE];
    }
    return TRUE;
}

BOOL GetMessage(MSG *message, HWND window, UINT first, UINT last)
{
    int index;
    (void)window;
    while ((index = FindMessage(first, last)) < 0)
        Pump(TRUE);
    RemoveMessage(index, message);
    return message->message != WM_QUIT;
}

BOOL TranslateMessage(const MSG *message)
{
    /* The game reads keys through its keyboard hook, not WM_CHAR. */
    (void)message;
    return FALSE;
}

LRESULT DispatchMessage(const MSG *message)
{
    if (gWindowProc && message->message != WM_QUIT)
        return gWindowProc(message->hwnd, message->message, message->wParam, message->lParam);
    return 0;
}

BOOL WaitMessage(void)
{
    Pump(TRUE);
    return TRUE;
}

LRESULT DefWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    (void)window;
    (void)message;
    (void)wParam;
    (void)lParam;
    return 0;
}

UINT RegisterWindowMessage(LPCSTR name)
{
    /* Windows hands out ids from 0xC000 up; the only registered message
       (the old IntelliMouse wheel message) never arrives. */
    (void)name;
    return 0xC000;
}

HWND FindWindowEx(HWND parent, HWND after, LPCSTR className, LPCSTR windowName)
{
    /* The Process Manager never starts a second copy of an application. */
    (void)parent;
    (void)after;
    (void)className;
    (void)windowName;
    return NULL;
}

BOOL SetForegroundWindow(HWND window)
{
    (void)window;
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Hooks                                                               */

static HOOKPROC gKeyboardHook;
static HOOKPROC gMouseHook;

HHOOK SetWindowsHookEx(int hookType, HOOKPROC proc, HINSTANCE module, DWORD threadId)
{
    (void)module;
    (void)threadId;
    if (hookType == WH_KEYBOARD) {
        gKeyboardHook = proc;
        return (HHOOK)&gKeyboardHook;
    }
    if (hookType == WH_MOUSE) {
        gMouseHook = proc;
        return (HHOOK)&gMouseHook;
    }
    return NULL;
}

BOOL UnhookWindowsHookEx(HHOOK hook)
{
    if (hook == (HHOOK)&gKeyboardHook)
        gKeyboardHook = NULL;
    else if (hook == (HHOOK)&gMouseHook)
        gMouseHook = NULL;
    else
        return FALSE;
    return TRUE;
}

LRESULT CallNextHookEx(HHOOK hook, int code, WPARAM wParam, LPARAM lParam)
{
    (void)hook;
    (void)code;
    (void)wParam;
    (void)lParam;
    return 0;
}

DWORD GetCurrentThreadId(void)
{
    return 1;
}

/* keyData has the WM_KEYDOWN lParam layout: repeat count in bits 0-15,
   scan code in 16-23, extended-key flag in 24, transition flag in 31. */
void SGPCallKeyboardHook(DWORD virtualKey, DWORD keyData)
{
    if (gKeyboardHook)
        gKeyboardHook(0, (WPARAM)virtualKey, (LPARAM)keyData);
}

void SGPCallMouseHook(UINT message, LONG x, LONG y)
{
    MOUSEHOOKSTRUCT info;
    if (!gMouseHook)
        return;
    info.pt.x = x;
    info.pt.y = y;
    info.hwnd = (HWND)1;
    info.wHitTestCode = 0;
    info.dwExtraInfo = 0;
    gMouseHook(0, (WPARAM)message, (LPARAM)&info);
}

static RECT gClipRect;
static BOOL gClipActive;

BOOL ClipCursor(const RECT *rect)
{
    if (rect) {
        gClipRect = *rect;
        gClipActive = TRUE;
    } else {
        gClipActive = FALSE;
    }
    return TRUE;
}

BOOL GetClipCursor(RECT *rect)
{
    if (gClipActive) {
        *rect = gClipRect;
    } else {
        rect->left = 0;
        rect->top = 0;
        rect->right = 640;
        rect->bottom = 480;
    }
    return TRUE;
}

BOOL SGPGetCursorClip(RECT *rect)
{
    if (gClipActive && rect)
        *rect = gClipRect;
    return gClipActive;
}

void mouse_event(DWORD flags, DWORD x, DWORD y, DWORD data, DWORD extraInfo)
{
    (void)data;
    (void)extraInfo;
    /* Only the absolute move SimulateMouseMovement sends: 0..65535 maps
       onto the 640x480 screen. */
    if ((flags & (MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE)) == (MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE)
        && gpSGPWarpMouse)
        gpSGPWarpMouse((LONG)(x * 640 / 65536), (LONG)(y * 480 / 65536));
}

/* ------------------------------------------------------------------ */
/* Multimedia timers                                                   */

#define SGP_TIMERS 4

typedef struct {
#ifdef macintosh
    TMTask task; /* first: the Time Manager hands back a TMTaskPtr */
#endif
    LPTIMECALLBACK proc;
    DWORD user;
    UINT delay;
    UINT flags;
    volatile int active;
#ifndef macintosh
    long remaining;
#endif
} SGPTimer;

static SGPTimer gTimers[SGP_TIMERS];

MMRESULT timeGetDevCaps(TIMECAPS *caps, UINT size)
{
    (void)size;
    caps->wPeriodMin = 1;
    caps->wPeriodMax = 1000000;
    return TIMERR_NOERROR;
}

#ifdef macintosh

static TimerUPP gTimerUPP;

static void TimerTask(TMTaskPtr task)
{
    SGPTimer *timer = (SGPTimer *)task;
    UINT id = (UINT)(timer - gTimers) + 1;
    if (!timer->active)
        return;
    timer->proc(id, 0, timer->user, 0, 0);
    /* Re-priming from inside the task keeps the extended Time Manager's
       schedule drift-free. */
    if ((timer->flags & TIME_PERIODIC) && timer->active)
        PrimeTime((QElemPtr)&timer->task, (long)timer->delay);
    else
        timer->active = 0;
}

MMRESULT timeSetEvent(UINT delay, UINT resolution, LPTIMECALLBACK proc, DWORD user, UINT flags)
{
    int index;
    (void)resolution;
    if (!gTimerUPP)
        gTimerUPP = NewTimerUPP(TimerTask);
    for (index = 0; index < SGP_TIMERS; ++index) {
        SGPTimer *timer = &gTimers[index];
        if (timer->active || timer->task.tmAddr)
            continue;
        memset(timer, 0, sizeof(*timer));
        timer->proc = proc;
        timer->user = user;
        timer->delay = delay ? delay : 1;
        timer->flags = flags;
        timer->task.tmAddr = gTimerUPP;
        timer->active = 1;
        InsXTime((QElemPtr)&timer->task);
        PrimeTime((QElemPtr)&timer->task, (long)timer->delay);
        DebugLog("SGP: timer %d every %u ms (Time Manager)", index + 1, timer->delay);
        return (MMRESULT)(index + 1);
    }
    DebugLog("SGP: no free multimedia timer");
    return 0;
}

MMRESULT timeKillEvent(UINT id)
{
    SGPTimer *timer;
    if (id < 1 || id > SGP_TIMERS)
        return 1;
    timer = &gTimers[id - 1];
    if (!timer->task.tmAddr)
        return 1;
    timer->active = 0;
    RmvTime((QElemPtr)&timer->task);
    timer->task.tmAddr = NULL;
    return TIMERR_NOERROR;
}

#else

/* POSIX hosts (debugging builds): the timers fire from the message pump,
   once per elapsed millisecond, so the game clock follows real time even
   when the host program sleeps between frames. */
static DWORD gHostTimerLast;

static void ServiceHostTimers(void)
{
    DWORD now = GetTickCount();
    if (!gHostTimerLast)
        gHostTimerLast = now;
    while (gHostTimerLast != now) {
        int index;
        ++gHostTimerLast;
        for (index = 0; index < SGP_TIMERS; ++index) {
            SGPTimer *timer = &gTimers[index];
            if (!timer->active || --timer->remaining > 0)
                continue;
            timer->proc((UINT)index + 1, 0, timer->user, 0, 0);
            if (timer->flags & TIME_PERIODIC)
                timer->remaining = (long)timer->delay;
            else
                timer->active = 0;
        }
    }
}

MMRESULT timeSetEvent(UINT delay, UINT resolution, LPTIMECALLBACK proc, DWORD user, UINT flags)
{
    int index;
    (void)resolution;
    for (index = 0; index < SGP_TIMERS; ++index) {
        SGPTimer *timer = &gTimers[index];
        if (timer->active)
            continue;
        timer->proc = proc;
        timer->user = user;
        timer->delay = delay ? delay : 1;
        timer->flags = flags;
        timer->remaining = (long)timer->delay;
        timer->active = 1;
        return (MMRESULT)(index + 1);
    }
    return 0;
}

MMRESULT timeKillEvent(UINT id)
{
    if (id < 1 || id > SGP_TIMERS)
        return 1;
    gTimers[id - 1].active = 0;
    return TIMERR_NOERROR;
}

#endif

DWORD timeGetTime(void)
{
    return GetTickCount();
}

/* ------------------------------------------------------------------ */
/* INI files                                                           */

/* Reads the value of key in [section]; returns 1 when found. */
static int IniLookup(LPCSTR fileName, LPCSTR section, LPCSTR key, char *value, size_t size)
{
    char line[512];
    int inSection = 0;
    FILE *file = SGPFOpen(fileName, "r");
    if (!file)
        return 0;
    while (fgets(line, sizeof(line), file)) {
        char *start = line, *end;
        while (isspace((unsigned char)*start))
            ++start;
        end = start + strlen(start);
        while (end > start && isspace((unsigned char)end[-1]))
            *--end = 0;
        if (*start == '[') {
            char *close = strchr(start, ']');
            if (close)
                *close = 0;
            inSection = _stricmp(start + 1, section) == 0;
        } else if (inSection && *start && *start != ';') {
            char *equals = strchr(start, '=');
            char *nameEnd;
            if (!equals)
                continue;
            nameEnd = equals;
            while (nameEnd > start && isspace((unsigned char)nameEnd[-1]))
                --nameEnd;
            *nameEnd = 0;
            if (_stricmp(start, key) == 0) {
                char *text = equals + 1;
                while (isspace((unsigned char)*text))
                    ++text;
                strncpy(value, text, size - 1);
                value[size - 1] = 0;
                fclose(file);
                return 1;
            }
        }
    }
    fclose(file);
    return 0;
}

DWORD GetPrivateProfileString(LPCSTR section, LPCSTR key, LPCSTR defaultValue, LPSTR buffer,
                              DWORD size, LPCSTR fileName)
{
    if (!size)
        return 0;
    if (!IniLookup(fileName, section, key, buffer, size)) {
        strncpy(buffer, defaultValue ? defaultValue : "", size - 1);
        buffer[size - 1] = 0;
    }
    return (DWORD)strlen(buffer);
}

UINT GetPrivateProfileInt(LPCSTR section, LPCSTR key, int defaultValue, LPCSTR fileName)
{
    char value[64];
    if (!IniLookup(fileName, section, key, value, sizeof(value)))
        return (UINT)defaultValue;
    return (UINT)strtol(value, NULL, 10);
}

BOOL WritePrivateProfileString(LPCSTR section, LPCSTR key, LPCSTR value, LPCSTR fileName)
{
    /* The game only rewrites its CD location, which the port never looks
       for (GameSettings.c). */
    DebugLog("SGP: WritePrivateProfileString [%s] %s=%s in %s ignored", section, key,
             value ? value : "", fileName);
    return FALSE;
}
