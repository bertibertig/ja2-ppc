#include "Cinematics.h"
#include "../../../ja2_mac_internal.h"
#include "../../../ja2_prof.h"
#include "../../../ja2_slf.h"
#include "../../../Smacker/smacker.h"

#include <Sound.h>
#include <Timer.h>
#include <stdlib.h>
#include <string.h>

/*
 * Smacker movie playback, the PPC counterpart of the original
 * Utils/Cinematics.c. The original decoded the .SMK files from Intro.slf
 * with RAD's Smacker SDK straight into the 16-bit frame buffer and let MSS
 * play the audio. Here retro68/Smacker/smacker.c decodes the same files at
 * their native rate (15 fps for the intros) straight into the window
 * surface, and the audio track is streamed through a Sound Manager channel, one
 * buffer command per frame.
 */

#define JA2_VIDEO_AUDIO_SLOTS 48
#define JA2_VIDEO_AUDIO_CALLBACK 0x4a32534bL
#define JA2_VIDEO_MAX_CATCHUP 4
#define JA2_SOUND_INIT_STEREO 0x00c0
/* Smacker frames are stored back to back; reading them through stdio in
   frame-sized pieces cost ~46 ms per frame on the PowerBook, so the movie
   is read ahead in large sequential chunks instead. */
#define JA2_VIDEO_READ_AHEAD (128UL * 1024UL)


typedef struct {
    ExtSoundHeader header;
    unsigned char *data;
    unsigned long capacity;
    volatile Boolean busy;
} JA2VideoAudioSlot;

struct JA2PPCVideo {
    JA2SLFArchive *archive;
    unsigned long entryOffset;
    unsigned long entryLength;
    unsigned char *readAhead;
    unsigned long readAheadOffset;
    unsigned long readAheadLength;
    Smacker *smacker;
    Uint32 lut[256];
    Uint32 lutFormat;
    Boolean lutValid;
    unsigned long long startMicros;
    unsigned long frameMicros;
    unsigned long framesPresented;
    unsigned long framesLate;
    unsigned long audioDropped;
    Boolean started;
    Boolean done;
    char name[16];
};

static SndChannelPtr gVideoChannel;
static SndCallBackUPP gVideoCallback;
static JA2VideoAudioSlot gAudioSlots[JA2_VIDEO_AUDIO_SLOTS];
static short gNextAudioSlot;

static void LogVideoMemory(const char *stage)
{
    DebugLog("Video memory: %s free=%ld bytes", stage, (long)FreeMem());
}

static unsigned long long NowMicros(void)
{
    UnsignedWide now;

    Microseconds(&now);
    return ((unsigned long long)now.hi << 32) | now.lo;
}

static int ReadMovieBytes(void *context, unsigned long offset, void *buffer,
                          unsigned long length)
{
    JA2PPCVideo *video = (JA2PPCVideo *)context;

    if (offset > video->entryLength || length > video->entryLength - offset) {
        return 0;
    }
    if (video->readAhead == NULL || length > JA2_VIDEO_READ_AHEAD) {
        return JA2SLFRead(video->archive, video->entryOffset + offset, buffer, length);
    }
    if (offset < video->readAheadOffset ||
        offset + length > video->readAheadOffset + video->readAheadLength) {
        unsigned long chunk = video->entryLength - offset;

        if (chunk > JA2_VIDEO_READ_AHEAD) {
            chunk = JA2_VIDEO_READ_AHEAD;
        }
        if (!JA2SLFRead(video->archive, video->entryOffset + offset, video->readAhead, chunk)) {
            video->readAheadLength = 0;
            return 0;
        }
        video->readAheadOffset = offset;
        video->readAheadLength = chunk;
    }
    memcpy(buffer, video->readAhead + (offset - video->readAheadOffset), length);
    return 1;
}

static pascal void VideoAudioCallback(SndChannelPtr channel, SndCommand *command)
{
    (void)channel;
    if (command->cmd == callBackCmd && command->param2 == JA2_VIDEO_AUDIO_CALLBACK &&
        command->param1 >= 0 && command->param1 < JA2_VIDEO_AUDIO_SLOTS) {
        gAudioSlots[command->param1].busy = 0;
    }
}

static void OpenAudio(JA2PPCVideo *video)
{
    unsigned long rate;
    int channels;
    int bits;
    OSErr error;

    if (!SmackerAudioInfo(video->smacker, 0, &rate, &channels, &bits)) {
        DebugLog("Video: %s has no audio track", video->name);
        return;
    }
    if (gVideoCallback == NULL) {
        gVideoCallback = NewSndCallBackUPP(VideoAudioCallback);
    }
    error = SndNewChannel(&gVideoChannel, sampledSynth,
                          channels == 2 ? JA2_SOUND_INIT_STEREO : 0, gVideoCallback);
    if (error != noErr) {
        DebugLog("Video: cannot create Sound Manager channel error=%d", error);
        gVideoChannel = NULL;
        return;
    }
    DebugLog("Video: audio %lu Hz %d ch %d bit", rate, channels, bits);
}

static void QueueAudio(JA2PPCVideo *video)
{
    JA2VideoAudioSlot *slot;
    const void *samples;
    unsigned long size;
    unsigned long rate;
    int channels;
    int bits;
    SndCommand command;

    if (gVideoChannel == NULL) {
        return;
    }
    size = SmackerAudio(video->smacker, 0, &samples);
    if (size == 0 || !SmackerAudioInfo(video->smacker, 0, &rate, &channels, &bits)) {
        return;
    }
    slot = &gAudioSlots[gNextAudioSlot];
    if (slot->busy) {
        ++video->audioDropped;
        return;
    }
    if (size > slot->capacity) {
        free(slot->data);
        slot->data = (unsigned char *)malloc(size);
        slot->capacity = slot->data != NULL ? size : 0;
        if (slot->data == NULL) {
            ++video->audioDropped;
            return;
        }
    }
    /* The decoder produces native-endian samples; 8-bit Smacker audio is
       unsigned, as the Sound Manager expects ('raw '). */
    memcpy(slot->data, samples, size);
    memset(&slot->header, 0, sizeof(slot->header));
    slot->header.samplePtr = (Ptr)slot->data;
    slot->header.numChannels = channels;
    slot->header.sampleRate = (Fixed)(rate << 16);
    slot->header.encode = extSH;
    slot->header.baseFrequency = 60;
    slot->header.numFrames = (long)(size / (unsigned long)(channels * (bits / 8)));
    slot->header.sampleSize = (short)bits;
    slot->busy = 1;
    command.cmd = bufferCmd;
    command.param1 = 0;
    command.param2 = (long)&slot->header;
    if (SndDoCommand(gVideoChannel, &command, 1) != noErr) {
        slot->busy = 0;
        ++video->audioDropped;
        return;
    }
    command.cmd = callBackCmd;
    command.param1 = gNextAudioSlot;
    command.param2 = JA2_VIDEO_AUDIO_CALLBACK;
    SndDoCommand(gVideoChannel, &command, 1);
    gNextAudioSlot = (short)((gNextAudioSlot + 1) % JA2_VIDEO_AUDIO_SLOTS);
}

static void CloseAudio(void)
{
    short index;

    if (gVideoChannel != NULL) {
        SndDisposeChannel(gVideoChannel, 1);
        gVideoChannel = NULL;
    }
    for (index = 0; index < JA2_VIDEO_AUDIO_SLOTS; ++index) {
        gAudioSlots[index].busy = 0;
    }
    gNextAudioSlot = 0;
}

/* Writes the indexed frame straight into the window surface through a
   palette lookup in the display's own pixel format, the way the original
   let Smacker write into the 16-bit frame buffer. This replaces an RGB555
   intermediate surface plus a second full-frame conversion blit. */
static Boolean ConvertFrame(JA2PPCVideo *video)
{
    SDL_Surface *window = SDL_GetWindowSurface(gWindow);
    const unsigned char *pixels = SmackerPixels(video->smacker);
    unsigned long sourceWidth = SmackerWidth(video->smacker);
    unsigned long width = sourceWidth;
    unsigned long height = SmackerHeight(video->smacker);
    int bytesPerPixel;
    unsigned long row;

    if (window == NULL) {
        return 0;
    }
    bytesPerPixel = window->format->BytesPerPixel;
    if (bytesPerPixel != 2 && bytesPerPixel != 4) {
        DebugLog("Video: unsupported window depth %d bytes", bytesPerPixel);
        return 0;
    }
    if (!video->lutValid || video->lutFormat != window->format->format ||
        SmackerPaletteChanged(video->smacker)) {
        const unsigned char *palette = SmackerPalette(video->smacker);
        short index;

        for (index = 0; index < 256; ++index) {
            video->lut[index] = SDL_MapRGB(window->format, palette[index * 3],
                                           palette[index * 3 + 1], palette[index * 3 + 2]);
        }
        video->lutFormat = window->format->format;
        video->lutValid = 1;
    }
    if (width > (unsigned long)window->w) width = (unsigned long)window->w;
    if (height > (unsigned long)window->h) height = (unsigned long)window->h;
    if (SDL_MUSTLOCK(window)) {
        SDL_LockSurface(window);
    }
    for (row = 0; row < height; ++row) {
        const unsigned char *source = pixels + row * sourceWidth;
        unsigned char *line = (unsigned char *)window->pixels + row * window->pitch;
        unsigned long column;

        if (bytesPerPixel == 2) {
            Uint16 *destination = (Uint16 *)line;

            for (column = 0; column < width; ++column) {
                destination[column] = (Uint16)video->lut[source[column]];
            }
        } else {
            Uint32 *destination = (Uint32 *)line;

            for (column = 0; column < width; ++column) {
                destination[column] = video->lut[source[column]];
            }
        }
    }
    if (SDL_MUSTLOCK(window)) {
        SDL_UnlockSurface(window);
    }
    return 1;
}

static Boolean DecodeNextFrame(JA2PPCVideo *video)
{
    Boolean decoded;

    JA2_PROF_BEGIN(JA2_PROF_MOVIE);
    decoded = SmackerDecodeFrame(video->smacker) != 0;
    JA2_PROF_END(JA2_PROF_MOVIE);
    if (decoded) {
        QueueAudio(video);
    }
    return decoded;
}

Boolean JA2PPCVideoInitialize(void)
{
    if (gWindow == NULL || SDL_GetWindowSurface(gWindow) == NULL) {
        DebugLog("Video: no window surface for movie playback");
        return 0;
    }
    LogVideoMemory("after video initialization");
    return 1;
}

JA2PPCVideo *JA2PPCVideoOpen(const char *archiveName, const char *entryName)
{
    JA2PPCVideo *video;

    video = (JA2PPCVideo *)calloc(1, sizeof(*video));
    if (video == NULL) {
        DebugLog("Video: cannot allocate playback object entry=%s", entryName);
        return NULL;
    }
    strncpy(video->name, entryName, sizeof(video->name) - 1);
    video->archive = JA2SLFOpen(archiveName);
    if (video->archive == NULL ||
        !JA2SLFFind(video->archive, entryName, &video->entryOffset, &video->entryLength)) {
        DebugLog("Video: %s not found in %s", entryName, archiveName);
        JA2PPCVideoClose(video);
        return NULL;
    }
    video->readAhead = (unsigned char *)malloc(JA2_VIDEO_READ_AHEAD);
    video->smacker = SmackerOpen(ReadMovieBytes, video);
    if (video->smacker == NULL) {
        DebugLog("Video: %s is not a supported Smacker file", entryName);
        JA2PPCVideoClose(video);
        return NULL;
    }
    video->frameMicros = SmackerFrameMicros(video->smacker);
    DebugLog("Video: opened %s %lux%lu frames=%lu frame_us=%lu", entryName,
             SmackerWidth(video->smacker), SmackerHeight(video->smacker),
             SmackerFrameCount(video->smacker), video->frameMicros);
    OpenAudio(video);
    LogVideoMemory("after movie open");
    return video;
}

Boolean JA2PPCVideoPoll(JA2PPCVideo *video)
{
    unsigned long target;
    unsigned long decoded = 0;
    Boolean converted;

    if (video == NULL || video->smacker == NULL || video->done) return 0;
    if (!video->started) {
        /* Frame 0 (and its audio lead-in) defines time zero. */
        if (!DecodeNextFrame(video)) {
            video->done = 1;
            return 0;
        }
        video->started = 1;
        video->startMicros = NowMicros();
        decoded = 1;
    } else {
        unsigned long long elapsed = NowMicros() - video->startMicros;

        target = (unsigned long)(elapsed / video->frameMicros) + 1;
        if (target > SmackerFrameCount(video->smacker)) {
            target = SmackerFrameCount(video->smacker);
        }
        /* Every frame is a delta, so all of them must be decoded; when the
           Mac falls behind, only the newest one is shown. */
        while (SmackerCurrentFrame(video->smacker) < target &&
               decoded < JA2_VIDEO_MAX_CATCHUP) {
            if (!DecodeNextFrame(video)) {
                video->done = 1;
                break;
            }
            ++decoded;
        }
        if (decoded > 1) {
            video->framesLate += decoded - 1;
        }
        if (SmackerCurrentFrame(video->smacker) >= SmackerFrameCount(video->smacker) &&
            elapsed >= (unsigned long long)SmackerFrameCount(video->smacker) *
                       video->frameMicros) {
            video->done = 1;
        }
    }
    if (decoded > 0) {
        JA2_PROF_BEGIN(JA2_PROF_BLIT);
        converted = ConvertFrame(video);
        JA2_PROF_END(JA2_PROF_BLIT);
        if (converted) {
            JA2PresentWindowSurface("intro video frame");
            ++video->framesPresented;
        }
    }
    return video->done ? 0 : 1;
}

void JA2PPCVideoUpdate(JA2PPCVideo *video)
{
    if (video == NULL || !video->started) return;
    JA2PresentWindowSurface("intro video refresh");
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
        CloseAudio();
    }
}

void JA2PPCVideoClose(JA2PPCVideo *video)
{
    if (video == NULL) return;
    if (video->smacker != NULL) {
        unsigned long elapsed = video->started ?
            (unsigned long)((NowMicros() - video->startMicros) / 1000) : 0;

        /* Effective playback: frames shown per second of wall time against
           the movie's own rate (fps_x10, 150 = 15.0 fps). */
        DebugLog("Video: stats entry=%s frames=%lu/%lu presented=%lu late=%lu "
                 "ms=%lu fps_x10=%lu source_fps_x10=%lu audio_dropped=%lu",
                 video->name, SmackerCurrentFrame(video->smacker),
                 SmackerFrameCount(video->smacker), video->framesPresented,
                 video->framesLate, elapsed,
                 elapsed ? video->framesPresented * 10000UL / elapsed : 0,
                 10000000UL / video->frameMicros, video->audioDropped);
    }
    CloseAudio();
    SmackerClose(video->smacker);
    free(video->readAhead);
    if (video->archive != NULL) {
        JA2SLFClose(video->archive);
    }
    free(video);
    LogVideoMemory("after movie close");
}

void JA2PPCVideoShutdown(void)
{
    short index;

    DebugLog("Video: shutdown");
    CloseAudio();
    for (index = 0; index < JA2_VIDEO_AUDIO_SLOTS; ++index) {
        free(gAudioSlots[index].data);
        gAudioSlots[index].data = NULL;
        gAudioSlots[index].capacity = 0;
    }
    if (gVideoCallback != NULL) {
        DisposeSndCallBackUPP(gVideoCallback);
        gVideoCallback = NULL;
    }
    LogVideoMemory("after video shutdown");
}
