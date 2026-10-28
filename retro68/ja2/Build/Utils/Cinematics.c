#include "Cinematics.h"

#include <QuickTime.h>
#include <Timer.h>
#include <stdlib.h>
#include <string.h>

enum {
    JA2_NEW_MOVIE_ACTIVE = 1 << 0
};

#define JA2_GESTALT_QUICKTIME_VERSION \
    ((long)('q' << 24) | (long)('t' << 16) | (long)('i' << 8) | (long)'m')
#define JA2_MOVIE_TASK_BUDGET_MS 20
#define JA2_SCREEN_WIDTH 640
#define JA2_SCREEN_HEIGHT 480

void DebugLog(const char *format, ...);
Boolean JA2PresentVideoWorld(GWorldPtr world);

struct JA2PPCVideo {
    Movie movie;
    short fileRefNum;
    Boolean done;
    unsigned long pollCount;
    unsigned long lastPollTicks;
    unsigned long delayedPolls;
    TimeValue lastPresentedMovieTime;
    Boolean hasPresentedFrame;
};

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
    if (leadingColon) {
        path[0] = (unsigned char)(length + 12);
        memcpy(&path[1], ":Data:Intro:", 12);
        memcpy(&path[13], name, length);
    } else {
        path[0] = (unsigned char)(length + 11);
        memcpy(&path[1], "Data:Intro:", 11);
        memcpy(&path[12], name, length);
    }
}

Boolean JA2PPCVideoInitialize(void *window, GWorldPtr frameWorld)
{
    OSErr error;
    long quickTimeVersion = 0;
    Rect bounds;

    (void)window;
    gVideoFrameWorld = frameWorld;
    DebugLog("Video: initialized SDL presentation bridge frameWorld=%p", frameWorld);
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
    }
    if (gVideoFrameWorld == NULL) {
        SetRect(&bounds, 0, 0, JA2_SCREEN_WIDTH, JA2_SCREEN_HEIGHT);
        error = NewGWorld(&gVideoFrameWorld, 16, &bounds, NULL, NULL, 0);
        if (error != noErr || gVideoFrameWorld == NULL) {
            DebugLog("Video: frame GWorld creation failed error=%d", error);
            return 0;
        }
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
            DebugLog("Video: MPEG FSSpec unavailable entry=%s error=%d", entryName, error);
            free(video);
            return NULL;
        }
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
    SetMovieGWorld(video->movie, gVideoFrameWorld, GetMainDevice());
    SetRect(&movieBox, 0, 0, JA2_SCREEN_WIDTH, JA2_SCREEN_HEIGHT);
    SetMovieBox(video->movie, &movieBox);
    GoToBeginningOfMovie(video->movie);
    StartMovie(video->movie);
    DebugLog("Video: QuickTime MPEG started entry=%s dataRefChanged=%d", entryName,
             dataRefChanged);
    LogVideoMemory("after movie start");
    return video;
}

Boolean JA2PPCVideoPoll(JA2PPCVideo *video)
{
    unsigned long now;
    TimeRecord movieTimeRecord;
    TimeValue movieTime;

    if (video == NULL || video->movie == NULL || video->done) return 0;
    now = (unsigned long)TickCount();
    if (video->lastPollTicks != 0 &&
        (unsigned long)(now - video->lastPollTicks) > 2) {
        ++video->delayedPolls;
    }
    video->lastPollTicks = now;
    ++video->pollCount;
    MoviesTask(video->movie, JA2_MOVIE_TASK_BUDGET_MS);
    movieTime = GetMovieTime(video->movie, &movieTimeRecord);
    if (!video->hasPresentedFrame || movieTime != video->lastPresentedMovieTime) {
        if (JA2PresentVideoWorld(gVideoFrameWorld)) {
            video->lastPresentedMovieTime = movieTime;
            video->hasPresentedFrame = 1;
        }
    }
    video->done = IsMovieDone(video->movie);
    if ((video->pollCount % 120) == 0) {
        DebugLog("Video: polls=%lu delayed=%lu", video->pollCount,
                 video->delayedPolls);
    }
    return video->done ? 0 : 1;
}

void JA2PPCVideoUpdate(JA2PPCVideo *video)
{
    if (video == NULL || video->movie == NULL) return;
    UpdateMovie(video->movie);
    JA2PresentVideoWorld(gVideoFrameWorld);
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
        StopMovie(video->movie);
        SetMovieActive(video->movie, 0);
        DisposeMovie(video->movie);
        video->movie = NULL;
    }
    if (video->fileRefNum != 0) {
        CloseMovieFile(video->fileRefNum);
        video->fileRefNum = 0;
    }
    free(video);
    LogVideoMemory("after movie close");
}

void JA2PPCVideoShutdown(void)
{
    DebugLog("Video: shutdown");
    if (gMoviesEntered) {
        ExitMovies();
        gMoviesEntered = 0;
    }
    if (gVideoFrameWorld != NULL) {
        DisposeGWorld(gVideoFrameWorld);
        gVideoFrameWorld = NULL;
    }
    gVideoFrameWorld = NULL;
    LogVideoMemory("after video shutdown");
}
