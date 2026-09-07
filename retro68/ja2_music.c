#include <Sound.h>
#include <Events.h>
#include <stdlib.h>
#include <string.h>

#include "ja2_music.h"
#include "ja2_slf.h"

#define JA2_MUSIC_VOLUME 63
#define JA2_MUSIC_CALLBACK 0x4a41324dL
#define JA2_SOUND_INIT_STEREO 0x00c0
#define JA2_MUSIC_BASE_FREQUENCY 60

/* Miles uses 0..127 volumes; ampCmd uses the Sound Manager's 0..255 range. */
#define JA2_SOUND_AMPLITUDE(volume) (((volume) * 255) / 127)

static SndChannelPtr gMusicChannel;
static SndCallBackUPP gMusicCallback;
static ExtSoundHeader gMusicHeader;
static unsigned char *gMusicData;
static Boolean gMusicPlaying;
static volatile Boolean gMusicEnded;
static Boolean gMusicAvailable;
static Boolean gMusicFadingIn;
static Boolean gMusicStatusLogged;
static unsigned long gMusicStartTicks;
static JA2MusicLogProc gMusicLog;

#define MusicLog(...) do { if (gMusicLog != NULL) gMusicLog(__VA_ARGS__); } while (0)

static unsigned long ReadLE32(const unsigned char *bytes)
{
    return ((unsigned long)bytes[0]) |
           ((unsigned long)bytes[1] << 8) |
           ((unsigned long)bytes[2] << 16) |
           ((unsigned long)bytes[3] << 24);
}

static unsigned short ReadLE16(const unsigned char *bytes)
{
    return (unsigned short)(bytes[0] | ((unsigned short)bytes[1] << 8));
}

static void SwapPCM16(unsigned char *data, unsigned long length)
{
    unsigned long index;

    for (index = 0; index + 1 < length; index += 2) {
        unsigned char byte = data[index];
        data[index] = data[index + 1];
        data[index + 1] = byte;
    }
}

static pascal void MusicCallback(SndChannelPtr channel, SndCommand *command)
{
    (void)channel;
    if (command->cmd == callBackCmd && command->param2 == JA2_MUSIC_CALLBACK) {
        gMusicEnded = 1;
    }
}

static Boolean LoadMenuMusic(void)
{
    JA2SLFArchive *archive;
    unsigned char wavHeader[12];
    unsigned char chunkHeader[8];
    unsigned char format[16];
    unsigned long entryOffset;
    unsigned long entryLength;
    unsigned long offset;
    unsigned long chunkLength;
    unsigned long dataOffset = 0;
    unsigned long dataLength = 0;
    unsigned long sampleRate = 0;
    unsigned short channels = 0;
    unsigned short bitsPerSample = 0;
    Boolean foundFormat = 0;

    MusicLog("Music: opening Music.slf for MENUMIX1.WAV");
    archive = JA2SLFOpen("Music.slf");
    if (archive == NULL) {
        MusicLog("Music: cannot open Music.slf");
        return 0;
    }
    if (!JA2SLFFind(archive, "MENUMIX1.WAV", &entryOffset, &entryLength)) {
        MusicLog("Music: MENUMIX1.WAV is missing from Music.slf");
        JA2SLFClose(archive);
        return 0;
    }
    MusicLog("Music: found MENUMIX1.WAV offset=%lu length=%lu", entryOffset, entryLength);
    if (entryLength < sizeof(wavHeader) ||
        !JA2SLFRead(archive, entryOffset, wavHeader, sizeof(wavHeader)) ||
        memcmp(wavHeader, "RIFF", 4) != 0 || memcmp(&wavHeader[8], "WAVE", 4) != 0) {
        MusicLog("Music: MENUMIX1.WAV is not a valid RIFF/WAVE file");
        JA2SLFClose(archive);
        return 0;
    }

    offset = sizeof(wavHeader);
    while (offset + sizeof(chunkHeader) <= entryLength) {
        if (!JA2SLFRead(archive, entryOffset + offset, chunkHeader, sizeof(chunkHeader))) {
            break;
        }
        chunkLength = ReadLE32(&chunkHeader[4]);
        offset += sizeof(chunkHeader);
        if (chunkLength > entryLength - offset) {
            break;
        }
        if (memcmp(chunkHeader, "fmt ", 4) == 0 && chunkLength >= sizeof(format)) {
            if (!JA2SLFRead(archive, entryOffset + offset, format, sizeof(format)) ||
                ReadLE16(format) != 1) {
                break;
            }
            channels = ReadLE16(&format[2]);
            sampleRate = ReadLE32(&format[4]);
            bitsPerSample = ReadLE16(&format[14]);
            foundFormat = 1;
        } else if (memcmp(chunkHeader, "data", 4) == 0) {
            dataOffset = entryOffset + offset;
            dataLength = chunkLength;
            break;
        }
        offset += chunkLength + (chunkLength & 1);
    }

    if (!foundFormat || dataLength == 0 || channels != 2 || bitsPerSample != 16 ||
        sampleRate == 0 || sampleRate > 65535 || (dataLength & 3) != 0) {
        MusicLog("Music: unsupported WAV format pcm=%d channels=%u rate=%lu bits=%u data=%lu",
                 foundFormat, channels, sampleRate, bitsPerSample, dataLength);
        JA2SLFClose(archive);
        return 0;
    }
    MusicLog("Music: PCM stereo rate=%lu bits=%u data=%lu", sampleRate, bitsPerSample,
             dataLength);

    gMusicData = (unsigned char *)malloc(dataLength);
    if (gMusicData == NULL || !JA2SLFRead(archive, dataOffset, gMusicData, dataLength)) {
        MusicLog("Music: cannot allocate or read %lu PCM bytes", dataLength);
        free(gMusicData);
        gMusicData = NULL;
        JA2SLFClose(archive);
        return 0;
    }
    JA2SLFClose(archive);

    /* JA2's WAV data is little-endian; Sound Manager samples are native-endian. */
    SwapPCM16(gMusicData, dataLength);
    memset(&gMusicHeader, 0, sizeof(gMusicHeader));
    gMusicHeader.samplePtr = (Ptr)gMusicData;
    gMusicHeader.numChannels = channels;
    gMusicHeader.sampleRate = (Fixed)(sampleRate << 16);
    gMusicHeader.encode = extSH;
    gMusicHeader.baseFrequency = JA2_MUSIC_BASE_FREQUENCY;
    gMusicHeader.numFrames = (long)(dataLength / (channels * (bitsPerSample / 8)));
    gMusicHeader.sampleSize = bitsPerSample;
    MusicLog("Music: converted PCM to native PowerPC byte order (%ld frames)",
             gMusicHeader.numFrames);
    return 1;
}

static void PlayMenuMusic(void)
{
    SndCommand command;
    OSErr error;

    if (!gMusicAvailable || gMusicChannel == NULL || gMusicPlaying) {
        return;
    }

    /* The original starts music silent, then its next music poll fades it in. */
    command.cmd = ampCmd;
    command.param1 = 0;
    command.param2 = 0;
    error = SndDoImmediate(gMusicChannel, &command);
    if (error != noErr) {
        MusicLog("Music: Sound Manager rejected silent initial volume (error=%d)", error);
        return;
    }
    command.cmd = bufferCmd;
    command.param1 = 0;
    command.param2 = (long)&gMusicHeader;
    error = SndDoCommand(gMusicChannel, &command, 1);
    if (error != noErr) {
        MusicLog("Music: Sound Manager rejected PCM buffer command (error=%d)", error);
        return;
    }
    command.cmd = callBackCmd;
    command.param1 = 0;
    command.param2 = JA2_MUSIC_CALLBACK;
    error = SndDoCommand(gMusicChannel, &command, 1);
    if (error != noErr) {
        MusicLog("Music: Sound Manager rejected end callback command (error=%d)", error);
        return;
    }
    gMusicPlaying = 1;
    gMusicFadingIn = 1;
    gMusicStatusLogged = 0;
    gMusicStartTicks = TickCount();
    MusicLog("Music: stereo playback queued at volume 0");
}

void JA2MenuMusicSetLogger(JA2MusicLogProc logger)
{
    gMusicLog = logger;
}

void JA2MenuMusicStart(void)
{
    OSErr error;

    if (gMusicChannel != NULL) {
        PlayMenuMusic();
        return;
    }
    if (!LoadMenuMusic()) {
        return;
    }
    gMusicCallback = NewSndCallBackUPP(MusicCallback);
    if (gMusicCallback == NULL) {
        MusicLog("Music: cannot create Sound Manager callback descriptor");
    }
    if (gMusicCallback == NULL) {
        free(gMusicData);
        gMusicData = NULL;
        return;
    }
    error = SndNewChannel(&gMusicChannel, sampledSynth, JA2_SOUND_INIT_STEREO,
                          gMusicCallback);
    if (error != noErr) {
        MusicLog("Music: cannot create sampled Sound Manager channel (error=%d)", error);
        DisposeSndCallBackUPP(gMusicCallback);
        gMusicCallback = NULL;
        free(gMusicData);
        gMusicData = NULL;
        return;
    }
    gMusicAvailable = 1;
    MusicLog("Music: stereo sampled Sound Manager channel created");
    PlayMenuMusic();
}

void JA2MenuMusicPoll(void)
{
    SndCommand command;
    OSErr error;

    if (gMusicPlaying && !gMusicStatusLogged &&
        TickCount() - gMusicStartTicks >= 60) {
        SCStatus status;

        memset(&status, 0, sizeof(status));
        error = SndChannelStatus(gMusicChannel, sizeof(status), &status);
        if (error == noErr) {
            MusicLog("Music: one-second status busy=%d paused=%d cpu=%ld",
                     status.scChannelBusy, status.scChannelPaused, status.scCPULoad);
        } else {
            MusicLog("Music: cannot query playback status (error=%d)", error);
        }
        gMusicStatusLogged = 1;
    }

    if (gMusicFadingIn && gMusicPlaying && gMusicChannel != NULL) {
        command.cmd = ampCmd;
        command.param1 = JA2_SOUND_AMPLITUDE(JA2_MUSIC_VOLUME);
        command.param2 = 0;
        error = SndDoImmediate(gMusicChannel, &command);
        if (error == noErr) {
            gMusicFadingIn = 0;
            MusicLog("Music: fade-in complete at original volume %d (Sound Manager amplitude %d)",
                     JA2_MUSIC_VOLUME, JA2_SOUND_AMPLITUDE(JA2_MUSIC_VOLUME));
        } else {
            MusicLog("Music: Sound Manager rejected fade-in volume (error=%d)", error);
            gMusicFadingIn = 0;
        }
    }
    if (gMusicEnded) {
        gMusicEnded = 0;
        gMusicPlaying = 0;
        MusicLog("Music: end callback received; restarting MENUMIX1.WAV");
        PlayMenuMusic();
    }
}

void JA2MenuMusicShutdown(void)
{
    MusicLog("Music: shutting down main-menu playback");
    if (gMusicChannel != NULL) {
        SndDisposeChannel(gMusicChannel, 1);
        gMusicChannel = NULL;
    }
    if (gMusicCallback != NULL) {
        DisposeSndCallBackUPP(gMusicCallback);
        gMusicCallback = NULL;
    }
    free(gMusicData);
    gMusicData = NULL;
    gMusicPlaying = 0;
    gMusicEnded = 0;
    gMusicAvailable = 0;
    gMusicFadingIn = 0;
    gMusicStatusLogged = 0;
}

void JA2MenuMusicStop(void)
{
    JA2MenuMusicShutdown();
}
