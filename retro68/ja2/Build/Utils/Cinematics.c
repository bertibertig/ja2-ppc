#include "Cinematics.h"

#include "../../../ja2_mac_internal.h"

#include <QuickTime.h>
#include <Timer.h>
#include <stdlib.h>
#include <string.h>

enum {
    JA2_NEW_MOVIE_ACTIVE = 1 << 0
};

#define JA2_GESTALT_QUICKTIME_VERSION \
    ((long)('q' << 24) | (long)('t' << 16) | (long)('i' << 8) | (long)'m')

/* A zero budget only performs a small amount of idle work.  That is not
   enough for MPEG decoding on slower PPC machines, and makes frame delivery
   depend on unrelated event-loop work.  Twenty milliseconds is below the
   25-fps frame interval while still returning regularly for input/events. */
#define JA2_MOVIE_TASK_BUDGET_MS 20

struct JA2PPCVideo {
    Movie movie;
    short fileRefNum;
    Boolean done;
    unsigned long pollCount;
    unsigned long lastPollTicks;
    unsigned long delayedPolls;
};

static WindowPtr gVideoWindow;
static GWorldPtr gVideoFrameWorld;
static Boolean gMoviesEntered;

static void LogVideoMemory(const char *stage)
{
    DebugLog("Video memory: %s free=%ld bytes", stage, (long)FreeMem());
}

static void MakeVideoPath(const char *entryName, Str255 path, Boolean leadingColon)
{
    const char *name = entryName;
    short length;

    if (strcmp(entryName, "REBEL_CR.SMK") == 0) name = "REBEL_CR.mpg";
    else if (strcmp(entryName, "OMERTA.SMK") == 0) name = "OMERTA.mpg";
    else if (strcmp(entryName, "PRAGUE_CR.SMK") == 0) name = "PRAGUE_CR.mpg";
    else if (strcmp(entryName, "PRAGUE.SMK") == 0) name = "PRAGUE.mpg";
    length = (short)strlen(name);
    path[0] = (unsigned char)(length + (leadingColon ? 12 : 11));
    if (leadingColon) {
        memcpy(&path[1], ":Data:Intro:", 12);
        memcpy(&path[13], name, length);
    } else {
        memcpy(&path[1], "Data:Intro:", 11);
        memcpy(&path[12], name, length);
    }
}

Boolean JA2PPCVideoInitialize(WindowPtr window, GWorldPtr frameWorld)
{
    OSErr error;
    long quickTimeVersion = 0;

    gVideoWindow = window;
    gVideoFrameWorld = frameWorld;
    DebugLog("Video: initialized window=%p frameWorld=%p", window, frameWorld);
    LogVideoMemory("before QuickTime initialization");
    if (window == NULL) {
        DebugLog("Video: initialization failed because window is null");
        return 0;
    }
    if (!gMoviesEntered) {
        error = EnterMovies();
        if (error != noErr) {
            DebugLog("Video: EnterMovies failed error=%d", error);
            return 0;
        }
        gMoviesEntered = 1;
        DebugLog("Video: QuickTime entered");
        LogVideoMemory("after QuickTime initialization");
    }
    error = Gestalt(JA2_GESTALT_QUICKTIME_VERSION, &quickTimeVersion);
    DebugLog("Video: QuickTime Gestalt error=%d version=%08lx", error,
             quickTimeVersion);
    return 1;
}

JA2PPCVideo *JA2PPCVideoOpen(const char *archiveName, const char *entryName)
{
    JA2PPCVideo *video;
    FSSpec spec;
    Str255 path;
    Str255 alternatePath;
    OSErr error;
    Boolean dataRefChanged = 0;
    Rect movieBox;

    (void)archiveName;
    DebugLog("Video: opening converted MPEG-1 file for entry=%s", entryName);
    video = (JA2PPCVideo *)calloc(1, sizeof(*video));
    if (video == NULL) {
        DebugLog("Video: cannot allocate playback object entry=%s", entryName);
        return NULL;
    }
    MakeVideoPath(entryName, path, 1);
    error = FSMakeFSSpec(0, 0, path, &spec);
    if (error != noErr) {
        MakeVideoPath(entryName, alternatePath, 0);
        error = FSMakeFSSpec(0, 0, alternatePath, &spec);
        if (error != noErr) {
        DebugLog("Video: MPEG FSSpec unavailable entry=%s error=%d paths=colon+relative",
                     entryName, error);
            free(video);
            return NULL;
        }
        DebugLog("Video: resolved MPEG using relative path form entry=%s", entryName);
    } else {
        DebugLog("Video: resolved MPEG using colon-relative path form entry=%s", entryName);
    }
    error = OpenMovieFile(&spec, &video->fileRefNum, fsRdPerm);
    if (error != noErr) {
        DebugLog("Video: OpenMovieFile failed entry=%s error=%d", entryName, error);
        free(video);
        return NULL;
    }
    error = NewMovieFromFile(&video->movie, video->fileRefNum, NULL, NULL,
                             JA2_NEW_MOVIE_ACTIVE, &dataRefChanged);
    if (error != noErr || video->movie == NULL) {
        DebugLog("Video: NewMovieFromFile failed entry=%s error=%d movie=%p",
                 entryName, error, video->movie);
        CloseMovieFile(video->fileRefNum);
        free(video);
        return NULL;
    }
    /* Infinite Mac can corrupt repeated frame GWorld-to-window CopyBits.
       Let QuickTime own presentation directly in the color window instead. */
    SetMovieGWorld(video->movie, (CGrafPtr)gVideoWindow, GetMainDevice());
    SetRect(&movieBox, 0, 0, 640, 480);
    SetMovieBox(video->movie, &movieBox);
    GoToBeginningOfMovie(video->movie);
    StartMovie(video->movie);
    DebugLog("Video: QuickTime MPEG started entry=%s dataRefChanged=%d", entryName,
             dataRefChanged);
    DebugLog("Video: Movie Toolbox task budget=%d ms", JA2_MOVIE_TASK_BUDGET_MS);
    LogVideoMemory("after movie start");
    return video;
}

Boolean JA2PPCVideoPoll(JA2PPCVideo *video)
{
    unsigned long now;

    if (video == NULL || video->movie == NULL || video->done) return 0;
    now = (unsigned long)TickCount();
    if (video->lastPollTicks != 0 &&
        (unsigned long)(now - video->lastPollTicks) > 2) {
        ++video->delayedPolls;
    }
    video->lastPollTicks = now;
    ++video->pollCount;
    /* Give QuickTime enough time to decode a frame.  MoviesTask remains
       bounded, so a slow decode cannot permanently starve system events. */
    MoviesTask(video->movie, JA2_MOVIE_TASK_BUDGET_MS);
    video->done = IsMovieDone(video->movie);
    if ((video->pollCount % 120) == 0) {
        DebugLog("Video: polls=%lu delayed=%lu", video->pollCount,
                 video->delayedPolls);
    }
    return video->done ? 0 : 1;
}

void JA2PPCVideoUpdate(JA2PPCVideo *video)
{
    OSErr error;

    if (video == NULL || video->movie == NULL) return;
    /* Movies.h requires UpdateMovie to execute inside BeginUpdate/EndUpdate. */
    error = UpdateMovie(video->movie);
    if (error != noErr) {
        DebugLog("Video: UpdateMovie failed error=%d", error);
    }
}

Boolean JA2PPCVideoDone(JA2PPCVideo *video)
{
    return video == NULL || video->done;
}

void JA2PPCVideoSkip(JA2PPCVideo *video)
{
    if (video != NULL) {
        DebugLog("Video: skip requested");
        video->done = 1;
        StopMovie(video->movie);
    }
}

void JA2PPCVideoClose(JA2PPCVideo *video)
{
    if (video == NULL) return;
    DebugLog("Video: closing playback object");
    if (video->movie != NULL) {
        DebugLog("Video: stopping movie=%p", video->movie);
        StopMovie(video->movie);
        SetMovieActive(video->movie, 0);
        DebugLog("Video: disposing movie=%p", video->movie);
        DisposeMovie(video->movie);
        video->movie = NULL;
        DebugLog("Video: movie disposed");
    }
    if (video->fileRefNum != 0) {
        DebugLog("Video: closing movie file ref=%d", video->fileRefNum);
        CloseMovieFile(video->fileRefNum);
        video->fileRefNum = 0;
        DebugLog("Video: movie file closed");
    }
    free(video);
    LogVideoMemory("after movie close");
}

void JA2PPCVideoShutdown(void)
{
    DebugLog("Video: shutdown window=%p frameWorld=%p", gVideoWindow, gVideoFrameWorld);
    if (gMoviesEntered) {
        ExitMovies();
        gMoviesEntered = 0;
    }
    gVideoWindow = NULL;
    gVideoFrameWorld = NULL;
    LogVideoMemory("after video shutdown");
}
