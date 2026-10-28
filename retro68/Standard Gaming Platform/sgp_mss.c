/* JA2 PPC: Miles Sound System 2D subset on the Mac Sound Manager (see
 * sgp_mss.h). Each sample and stream owns a sampled-sound channel, created
 * on first use and reused afterwards. Completion is reported by callBackCmd
 * at interrupt time; the callbacks only change counters and queue the next
 * loop, everything else runs on the main thread. On POSIX hosts (link
 * checks and host tests) the same API is silent: every sound ends at once. */

#include <stdlib.h>
#include <string.h>

/* Mac headers before the Win32 subset (see sgp_win32_msg.c). */
#ifdef macintosh
#include <Sound.h>
#include <Timer.h>
#endif

#include "sgp_mss.h"

void DebugLog(const char *format, ...);

#define SGP_MSS_SAMPLES 24
#define SGP_MSS_STREAMS 4
#define SGP_MSS_STREAM_BUFFERS 4
#define SGP_MSS_STREAM_CHUNK (64 * 1024)
#define SGP_MSS_LOOP_QUEUE 2

struct SGPMSSDriver {
    S32 masterVolume;
};

static struct SGPMSSDriver gDriver = {127};
static char gLastError[128] = "";

static void SetError(const char *text)
{
    strncpy(gLastError, text, sizeof(gLastError) - 1);
    gLastError[sizeof(gLastError) - 1] = 0;
}

/* ------------------------------------------------------------------ */
/* WAV parsing                                                         */

static U32 LE32(const unsigned char *bytes)
{
    return (U32)bytes[0] | ((U32)bytes[1] << 8) | ((U32)bytes[2] << 16) | ((U32)bytes[3] << 24);
}

static U32 LE16(const unsigned char *bytes)
{
    return (U32)bytes[0] | ((U32)bytes[1] << 8);
}

/* Fills info from a RIFF/WAVE image of size bytes (0: unknown, trust the
   chunk sizes). Returns the offset of the data chunk's contents, or 0. */
static U32 ParseWAV(const unsigned char *image, U32 size, AILSOUNDINFO *info)
{
    U32 offset = 12, haveFormat = 0;

    memset(info, 0, sizeof(*info));
    if (memcmp(image, "RIFF", 4) != 0 || memcmp(image + 8, "WAVE", 4) != 0)
        return 0;
    if (!size)
        size = LE32(image + 4) + 8;
    while (offset + 8 <= size) {
        const unsigned char *chunk = image + offset;
        U32 length = LE32(chunk + 4);
        if (memcmp(chunk, "fmt ", 4) == 0 && length >= 16) {
            info->format = (S32)LE16(chunk + 8);
            info->channels = (S32)LE16(chunk + 10);
            info->rate = LE32(chunk + 12);
            info->block_size = LE16(chunk + 20);
            info->bits = (S32)LE16(chunk + 22);
            haveFormat = 1;
        } else if (memcmp(chunk, "data", 4) == 0 && haveFormat) {
            info->data_ptr = chunk + 8;
            info->data_len = length;
            if (size > offset + 8 && info->data_len > size - offset - 8)
                info->data_len = size - offset - 8;
            info->initial_ptr = info->data_ptr;
            if (info->channels > 0 && info->bits > 0)
                info->samples = info->data_len / (U32)(info->channels * info->bits / 8);
            return offset + 8;
        }
        offset += 8 + length + (length & 1);
    }
    return 0;
}

S32 AIL_WAV_info(void const *data, AILSOUNDINFO *info)
{
    if (!ParseWAV((const unsigned char *)data, 0, info)) {
        SetError("not a RIFF/WAVE image");
        return 0;
    }
    return 1;
}

/* Miles volume/pan (0..127, pan 64 = centre) as a Sound Manager volumeCmd
   value: right in the high word, left in the low word, 0x100 = unity. */
static long StereoVolume(S32 volume, S32 pan)
{
    long left, right, scale;
    if (volume < 0)
        volume = 0;
    if (volume > 127)
        volume = 127;
    if (pan < 0)
        pan = 0;
    if (pan > 127)
        pan = 127;
    scale = (long)volume * gDriver.masterVolume * 0x100 / (127 * 127);
    left = pan <= 64 ? scale : scale * (127 - pan) / 63;
    right = pan >= 64 ? scale : scale * pan / 64;
    return (right << 16) | (left & 0xffff);
}

/* ------------------------------------------------------------------ */
/* Driver                                                              */

S32 AIL_startup(void)
{
    return 1;
}

void AIL_shutdown(void)
{
}

S32 AIL_set_preference(U32 number, S32 value)
{
    (void)number;
    (void)value;
    return 0;
}

char *AIL_last_error(void)
{
    return gLastError;
}

void *AIL_mem_alloc_lock(U32 size)
{
    return malloc(size);
}

void AIL_mem_free_lock(void *ptr)
{
    free(ptr);
}

S32 AIL_waveOutOpen(HDIGDRIVER *drvr, LPHWAVEOUT *lphWaveOut, S32 wDeviceID, LPWAVEFORMAT lpFormat)
{
    (void)lphWaveOut;
    (void)wDeviceID;
    DebugLog("SGP sound: output %lu Hz, %u channels (Sound Manager mixes every rate)",
             (unsigned long)lpFormat->nSamplesPerSec, (unsigned)lpFormat->nChannels);
    *drvr = &gDriver;
    return 0; /* MMSYSERR_NOERROR */
}

void AIL_waveOutClose(HDIGDRIVER drvr)
{
    (void)drvr;
}

void AIL_digital_configuration(HDIGDRIVER dig, S32 *rate, S32 *format, char *string)
{
    (void)dig;
    if (rate)
        *rate = 44100;
    if (format)
        *format = 3; /* DIG_F_STEREO_16 */
    if (string)
        strcpy(string, "Mac OS Sound Manager");
}

void AIL_set_digital_master_volume(HDIGDRIVER dig, S32 master_volume)
{
    (void)dig;
    gDriver.masterVolume = master_volume < 0 ? 0 : master_volume > 127 ? 127 : master_volume;
}

S32 AIL_digital_master_volume(HDIGDRIVER dig)
{
    (void)dig;
    return gDriver.masterVolume;
}

/* ------------------------------------------------------------------ */
/* Samples                                                             */

struct SGPMSSSample {
    int allocated;
    const unsigned char *data;
    U32 length;
    S32 rate, playRate, bits, channels;
    S32 volume, pan, loops;
    volatile U32 status;
    volatile S32 pending;
    volatile U32 generation;
    U32 startTime;
#ifdef macintosh
    SndChannelPtr channel;
    ExtSoundHeader header;
#endif
};

static struct SGPMSSSample gSamples[SGP_MSS_SAMPLES];

static U32 BytesPerSecond(S32 rate, S32 bits, S32 channels)
{
    U32 bytes = (U32)rate * (U32)(bits / 8) * (U32)channels;
    return bytes ? bytes : 1;
}

#ifdef macintosh

#define SGP_MSS_CALLBACK_SAMPLE 0x534d5031L /* 'SMP1' */
#define SGP_MSS_CALLBACK_STREAM 0x53545231L /* 'STR1' */

static SndCallBackUPP gCallbackUPP;

static void QueueBuffer(SndChannelPtr channel, ExtSoundHeader *header, short tag, long kind)
{
    SndCommand command;
    command.cmd = bufferCmd;
    command.param1 = 0;
    command.param2 = (long)header;
    SndDoCommand(channel, &command, true);
    command.cmd = callBackCmd;
    command.param1 = tag;
    command.param2 = kind;
    SndDoCommand(channel, &command, true);
}

static void SampleFinished(struct SGPMSSSample *sample, short generation);
static void StreamFinished(void *stream, short buffer);

static pascal void SoundCallback(SndChannelPtr channel, SndCommand *command)
{
    if (command->param2 == SGP_MSS_CALLBACK_SAMPLE)
        SampleFinished((struct SGPMSSSample *)channel->userInfo, command->param1);
    else if (command->param2 == SGP_MSS_CALLBACK_STREAM)
        StreamFinished((void *)channel->userInfo, command->param1);
}

static SndChannelPtr NewChannel(void *owner)
{
    SndChannelPtr channel = NULL;
    OSErr error;
    if (!gCallbackUPP)
        gCallbackUPP = NewSndCallBackUPP(SoundCallback);
    error = SndNewChannel(&channel, sampledSynth, initStereo, gCallbackUPP);
    if (error != noErr) {
        DebugLog("SGP sound: SndNewChannel failed (%d)", error);
        return NULL;
    }
    channel->userInfo = (long)owner;
    return channel;
}

static void Immediate(SndChannelPtr channel, short cmd, short param1, long param2)
{
    SndCommand command;
    command.cmd = cmd;
    command.param1 = param1;
    command.param2 = param2;
    SndDoImmediate(channel, &command);
}

static void Silence(SndChannelPtr channel)
{
    Immediate(channel, flushCmd, 0, 0);
    Immediate(channel, quietCmd, 0, 0);
}

static void SetRate(SndChannelPtr channel, S32 playRate, S32 rate)
{
    /* Fixed-point ratio to the recorded rate. */
    long multiplier = rate > 0 ? (long)(((double)playRate / (double)rate) * 65536.0) : 0x10000;
    Immediate(channel, rateMultiplierCmd, 0, multiplier);
}

static void FillHeader(ExtSoundHeader *header, const unsigned char *data, U32 length, S32 rate, S32 bits,
                       S32 channels)
{
    memset(header, 0, sizeof(*header));
    header->samplePtr = (Ptr)data;
    header->numChannels = channels;
    header->sampleRate = (UnsignedFixed)((unsigned long)rate << 16);
    header->encode = extSH;
    header->baseFrequency = kMiddleC;
    header->numFrames = (long)(length / (U32)(channels * (bits / 8)));
    header->sampleSize = (short)bits;
}

/* Interrupt time. */
static void SampleFinished(struct SGPMSSSample *sample, short generation)
{
    if (sample == NULL || (U32)(unsigned short)generation != (sample->generation & 0xffff) ||
        sample->status != SMP_PLAYING)
        return;
    if (sample->loops == 0) {
        QueueBuffer(sample->channel, &sample->header, generation, SGP_MSS_CALLBACK_SAMPLE);
        return;
    }
    if (--sample->pending <= 0)
        sample->status = SMP_DONE;
}

#endif

HSAMPLE AIL_allocate_sample_handle(HDIGDRIVER dig)
{
    int index;
    (void)dig;
    for (index = 0; index < SGP_MSS_SAMPLES; ++index) {
        struct SGPMSSSample *sample = &gSamples[index];
        if (sample->allocated)
            continue;
#ifdef macintosh
        if (!sample->channel && !(sample->channel = NewChannel(sample)))
            return NULL;
#endif
        sample->allocated = 1;
        AIL_init_sample(sample);
        return sample;
    }
    SetError("no free sample handle");
    return NULL;
}

void AIL_release_sample_handle(HSAMPLE S)
{
    if (!S)
        return;
    AIL_stop_sample(S);
    S->allocated = 0;
    S->status = SMP_FREE;
}

void AIL_init_sample(HSAMPLE S)
{
    S->data = NULL;
    S->length = 0;
    S->rate = S->playRate = 22050;
    S->bits = 16;
    S->channels = 1;
    S->volume = 127;
    S->pan = 64;
    S->loops = 1;
    S->status = SMP_DONE;
}

S32 AIL_set_named_sample_file(HSAMPLE S, C8 const *file_type_suffix, void const *file_image, S32 file_size,
                              S32 block)
{
    AILSOUNDINFO info;
    (void)file_type_suffix;
    (void)block;
    if (!ParseWAV((const unsigned char *)file_image, (U32)file_size, &info)) {
        SetError("not a RIFF/WAVE image");
        return 0;
    }
    if (info.format != WAVE_FORMAT_PCM || (info.bits != 8 && info.bits != 16) ||
        (info.channels != 1 && info.channels != 2)) {
        DebugLog("SGP sound: unsupported WAV (format %ld, %ld bits, %ld channels)", (long)info.format,
                 (long)info.bits, (long)info.channels);
        SetError("unsupported WAV format");
        return 0;
    }
    S->data = (const unsigned char *)info.data_ptr;
    S->length = info.data_len;
    S->rate = S->playRate = (S32)info.rate;
    S->bits = info.bits;
    S->channels = info.channels;
    return 1;
}

void AIL_start_sample(HSAMPLE S)
{
    if (!S || !S->data)
        return;
#ifdef macintosh
    {
        S32 queued, count;
        Silence(S->channel);
        ++S->generation;
        FillHeader(&S->header, S->data, S->length, S->rate, S->bits, S->channels);
        Immediate(S->channel, volumeCmd, 0, StereoVolume(S->volume, S->pan));
        SetRate(S->channel, S->playRate, S->rate);
        count = S->loops == 0 ? SGP_MSS_LOOP_QUEUE : S->loops;
        S->pending = count;
        S->status = SMP_PLAYING;
        S->startTime = GetTickCount();
        for (queued = 0; queued < count && queued < 64; ++queued)
            QueueBuffer(S->channel, &S->header, (short)(S->generation & 0xffff), SGP_MSS_CALLBACK_SAMPLE);
        if (count > 64)
            S->pending = 64;
    }
#else
    /* Silent host build: "plays" for the sample's duration. */
    S->startTime = GetTickCount();
    S->status = SMP_PLAYING;
#endif
}

void AIL_stop_sample(HSAMPLE S)
{
    if (!S)
        return;
#ifdef macintosh
    ++S->generation;
    if (S->channel)
        Silence(S->channel);
#endif
    if (S->status == SMP_PLAYING)
        S->status = SMP_STOPPED;
}

U32 AIL_sample_status(HSAMPLE S)
{
#ifndef macintosh
    if (S && S->status == SMP_PLAYING && S->loops != 0 &&
        GetTickCount() - S->startTime >= (U32)((double)S->length * 1000.0 * S->loops /
                                               BytesPerSecond(S->playRate, S->bits, S->channels)))
        S->status = SMP_DONE;
#endif
    return S ? S->status : SMP_FREE;
}

void AIL_set_sample_playback_rate(HSAMPLE S, S32 playback_rate)
{
    S->playRate = playback_rate;
#ifdef macintosh
    if (S->status == SMP_PLAYING)
        SetRate(S->channel, S->playRate, S->rate);
#endif
}

S32 AIL_sample_playback_rate(HSAMPLE S)
{
    return S->playRate;
}

void AIL_set_sample_volume(HSAMPLE S, S32 volume)
{
    S->volume = volume;
#ifdef macintosh
    if (S->channel)
        Immediate(S->channel, volumeCmd, 0, StereoVolume(S->volume, S->pan));
#endif
}

S32 AIL_sample_volume(HSAMPLE S)
{
    return S->volume;
}

void AIL_set_sample_pan(HSAMPLE S, S32 pan)
{
    S->pan = pan;
#ifdef macintosh
    if (S->channel)
        Immediate(S->channel, volumeCmd, 0, StereoVolume(S->volume, S->pan));
#endif
}

S32 AIL_sample_pan(HSAMPLE S)
{
    return S->pan;
}

void AIL_set_sample_loop_count(HSAMPLE S, S32 loop_count)
{
    S->loops = loop_count < 0 ? 1 : loop_count;
}

S32 AIL_sample_loop_count(HSAMPLE S)
{
    return S->loops;
}

static U32 ElapsedBytes(U32 startTime, S32 playRate, S32 bits, S32 channels, U32 length)
{
    U32 elapsed = GetTickCount() - startTime;
    double bytes = (double)elapsed * BytesPerSecond(playRate, bits, channels) / 1000.0;
    U32 position = (U32)bytes;
    return length ? position % length : position;
}

U32 AIL_sample_position(HSAMPLE S)
{
    if (S->status != SMP_PLAYING)
        return S->status == SMP_DONE ? S->length : 0;
    return ElapsedBytes(S->startTime, S->playRate, S->bits, S->channels, S->length);
}

void AIL_sample_ms_position(HSAMPLE S, S32 *total_milliseconds, S32 *current_milliseconds)
{
    U32 bytesPerSecond = BytesPerSecond(S->playRate, S->bits, S->channels);
    if (total_milliseconds)
        *total_milliseconds = (S32)((double)S->length * 1000.0 / bytesPerSecond);
    if (current_milliseconds)
        *current_milliseconds = (S32)((double)AIL_sample_position(S) * 1000.0 / bytesPerSecond);
}

/* ------------------------------------------------------------------ */
/* Streams                                                             */

typedef struct {
    struct SGPMSSStream pub; /* first: HSTREAM points here */
    SGPMSSStreamSample sample;
    int open;
    HANDLE file;
    int ownsFile;
    U32 dataStart, dataLength, readPos;
    S32 rate, playRate, bits, channels;
    S32 volume, pan, loops, loopsDone;
    volatile U32 status;
    volatile int busy[SGP_MSS_STREAM_BUFFERS];
    int next;
    U32 startTime;
    unsigned char *buffer[SGP_MSS_STREAM_BUFFERS];
#ifdef macintosh
    SndChannelPtr channel;
    ExtSoundHeader header[SGP_MSS_STREAM_BUFFERS];
#endif
} SGPStream;

static SGPStream gStreams[SGP_MSS_STREAMS];

#ifdef macintosh
/* Interrupt time. */
static void StreamFinished(void *owner, short buffer)
{
    SGPStream *stream = (SGPStream *)owner;
    if (stream && buffer >= 0 && buffer < SGP_MSS_STREAM_BUFFERS)
        stream->busy[buffer] = 0;
}
#endif

static int ReadAt(HANDLE file, U32 offset, void *buffer, U32 length)
{
    DWORD read = 0;
    if (SetFilePointer(file, (LONG)offset, NULL, FILE_BEGIN) == 0xffffffffUL)
        return 0;
    return ReadFile(file, buffer, length, &read, NULL) && read == length;
}

static void SwapPCM16(unsigned char *data, U32 length)
{
    U32 index;
    for (index = 0; index + 1 < length; index += 2) {
        unsigned char byte = data[index];
        data[index] = data[index + 1];
        data[index + 1] = byte;
    }
}

/* Main thread: refills free buffers in playback order. */
static void ServiceStream(SGPStream *stream)
{
    int busy = 0, index;
    if (stream->status != SMP_PLAYING)
        return;
    for (;;) {
        int slot = stream->next;
        U32 chunk;
        if (stream->busy[slot])
            break;
        if (stream->readPos >= stream->dataLength) {
            if (stream->loops != 0 && stream->loopsDone + 1 >= stream->loops)
                break;
            ++stream->loopsDone;
            stream->readPos = 0;
        }
        chunk = stream->dataLength - stream->readPos;
        if (chunk > SGP_MSS_STREAM_CHUNK)
            chunk = SGP_MSS_STREAM_CHUNK;
        chunk -= chunk % (U32)(stream->channels * (stream->bits / 8));
        if (!chunk || !ReadAt(stream->file, stream->dataStart + stream->readPos, stream->buffer[slot], chunk)) {
            DebugLog("SGP sound: stream read failed at %lu", (unsigned long)stream->readPos);
            stream->readPos = stream->dataLength;
            stream->loops = 1;
            break;
        }
        stream->readPos += chunk;
        if (stream->bits == 16)
            SwapPCM16(stream->buffer[slot], chunk);
#ifdef macintosh
        FillHeader(&stream->header[slot], stream->buffer[slot], chunk, stream->rate, stream->bits,
                   stream->channels);
        stream->busy[slot] = 1;
        QueueBuffer(stream->channel, &stream->header[slot], (short)slot, SGP_MSS_CALLBACK_STREAM);
#endif
        stream->next = (slot + 1) % SGP_MSS_STREAM_BUFFERS;
    }
    for (index = 0; index < SGP_MSS_STREAM_BUFFERS; ++index)
        busy |= stream->busy[index];
    if (!busy && stream->readPos >= stream->dataLength)
        stream->status = SMP_DONE;
}

HSTREAM AIL_open_stream(HDIGDRIVER dig, char const *filename, S32 stream_mem)
{
    SGPStream *stream = NULL;
    unsigned char header[512];
    AILSOUNDINFO info;
    U32 start, dataOffset;
    int index;
    (void)dig;
    (void)stream_mem;

    for (index = 0; index < SGP_MSS_STREAMS; ++index) {
        if (!gStreams[index].open) {
            stream = &gStreams[index];
            break;
        }
    }
    if (!stream) {
        SetError("no free stream");
        return NULL;
    }
    /* soundman.c names a file inside an SLF library by its Win32 handle,
       "\\\\<handle>", positioned at the start of the file. */
    if (filename[0] == '\\' && filename[1] == '\\' && filename[2] == '\\' && filename[3] == '\\') {
        stream->file = (HANDLE)strtoul(filename + 4, NULL, 10);
        stream->ownsFile = 0;
    } else {
        stream->file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        stream->ownsFile = 1;
        if (stream->file == INVALID_HANDLE_VALUE) {
            SetError("cannot open stream file");
            return NULL;
        }
    }
    start = SetFilePointer(stream->file, 0, NULL, FILE_CURRENT);
    if (!ReadAt(stream->file, start, header, sizeof(header)) ||
        !(dataOffset = ParseWAV(header, sizeof(header), &info)) || info.format != WAVE_FORMAT_PCM ||
        (info.bits != 8 && info.bits != 16) || (info.channels != 1 && info.channels != 2)) {
        DebugLog("SGP sound: stream %s is not a supported PCM WAV", filename);
        if (stream->ownsFile)
            CloseHandle(stream->file);
        SetError("unsupported stream format");
        return NULL;
    }
    /* The data chunk's size comes from its header, not the 512 bytes read. */
    info.data_len = LE32(header + dataOffset - 4);
    for (index = 0; index < SGP_MSS_STREAM_BUFFERS; ++index) {
        if (!stream->buffer[index] && !(stream->buffer[index] = (unsigned char *)malloc(SGP_MSS_STREAM_CHUNK))) {
            SetError("out of memory");
            return NULL;
        }
        stream->busy[index] = 0;
    }
#ifdef macintosh
    if (!stream->channel && !(stream->channel = NewChannel(stream))) {
        if (stream->ownsFile)
            CloseHandle(stream->file);
        SetError("no Sound Manager channel");
        return NULL;
    }
#endif
    stream->open = 1;
    stream->dataStart = start + dataOffset;
    stream->dataLength = info.data_len;
    stream->readPos = 0;
    stream->rate = stream->playRate = (S32)info.rate;
    stream->bits = info.bits;
    stream->channels = info.channels;
    stream->volume = 127;
    stream->pan = 64;
    stream->loops = 1;
    stream->loopsDone = 0;
    stream->next = 0;
    stream->status = SMP_DONE;
    stream->sample.format = info.channels * info.bits / 8;
    stream->pub.datarate = (S32)BytesPerSecond(stream->rate, stream->bits, stream->channels);
    stream->pub.bufsize = SGP_MSS_STREAM_CHUNK;
    stream->pub.bufs[0] = stream->buffer[0];
    stream->pub.bufs[1] = stream->buffer[1];
    stream->pub.bufs[2] = stream->buffer[2];
    stream->pub.buf1 = 0;
    stream->pub.samp = &stream->sample;
    DebugLog("SGP sound: stream %lu Hz %ld bit %ld ch, %lu bytes", (unsigned long)info.rate, (long)info.bits,
             (long)info.channels, (unsigned long)info.data_len);
    return &stream->pub;
}

void AIL_close_stream(HSTREAM handle)
{
    SGPStream *stream = (SGPStream *)handle;
    int index;
    if (!stream || !stream->open)
        return;
#ifdef macintosh
    Silence(stream->channel);
#endif
    for (index = 0; index < SGP_MSS_STREAM_BUFFERS; ++index)
        stream->busy[index] = 0;
    if (stream->ownsFile)
        CloseHandle(stream->file);
    stream->open = 0;
    stream->status = SMP_FREE;
}

S32 AIL_service_stream(HSTREAM handle, S32 fillup)
{
    (void)fillup;
#ifdef macintosh
    ServiceStream((SGPStream *)handle);
#else
    (void)handle;
#endif
    /* Nonzero would ask soundman.c to run its stream callback. */
    return 0;
}

void AIL_start_stream(HSTREAM handle)
{
    SGPStream *stream = (SGPStream *)handle;
#ifdef macintosh
    Silence(stream->channel);
    Immediate(stream->channel, volumeCmd, 0, StereoVolume(stream->volume, stream->pan));
    SetRate(stream->channel, stream->playRate, stream->rate);
#endif
    stream->readPos = 0;
    stream->loopsDone = 0;
    stream->next = 0;
    stream->status = SMP_PLAYING;
    stream->startTime = GetTickCount();
#ifdef macintosh
    ServiceStream(stream);
#endif
}

void AIL_auto_service_stream(HSTREAM stream, S32 onoff)
{
    /* Streams are serviced from AIL_service_stream and AIL_stream_status,
       which soundman.c calls every frame (SoundServiceStreams). */
    (void)stream;
    (void)onoff;
}

S32 AIL_stream_status(HSTREAM handle)
{
    SGPStream *stream = (SGPStream *)handle;
#ifdef macintosh
    ServiceStream(stream);
#else
    /* Silent host build: "plays" for the stream's duration. */
    if (stream->status == SMP_PLAYING && stream->loops != 0 &&
        GetTickCount() - stream->startTime >= (U32)((double)stream->dataLength * 1000.0 * stream->loops /
                                                    BytesPerSecond(stream->playRate, stream->bits, stream->channels)))
        stream->status = SMP_DONE;
#endif
    return (S32)stream->status;
}

void AIL_set_stream_volume(HSTREAM handle, S32 volume)
{
    SGPStream *stream = (SGPStream *)handle;
    stream->volume = volume;
#ifdef macintosh
    Immediate(stream->channel, volumeCmd, 0, StereoVolume(stream->volume, stream->pan));
#endif
}

S32 AIL_stream_volume(HSTREAM handle)
{
    return ((SGPStream *)handle)->volume;
}

void AIL_set_stream_pan(HSTREAM handle, S32 pan)
{
    SGPStream *stream = (SGPStream *)handle;
    stream->pan = pan;
#ifdef macintosh
    Immediate(stream->channel, volumeCmd, 0, StereoVolume(stream->volume, stream->pan));
#endif
}

S32 AIL_stream_pan(HSTREAM handle)
{
    return ((SGPStream *)handle)->pan;
}

void AIL_set_stream_playback_rate(HSTREAM handle, S32 rate)
{
    SGPStream *stream = (SGPStream *)handle;
    stream->playRate = rate;
#ifdef macintosh
    SetRate(stream->channel, stream->playRate, stream->rate);
#endif
}

S32 AIL_stream_playback_rate(HSTREAM handle)
{
    return ((SGPStream *)handle)->playRate;
}

void AIL_set_stream_loop_count(HSTREAM handle, S32 count)
{
    ((SGPStream *)handle)->loops = count < 0 ? 1 : count;
}

S32 AIL_stream_loop_count(HSTREAM handle)
{
    return ((SGPStream *)handle)->loops;
}

S32 AIL_stream_position(HSTREAM handle)
{
    SGPStream *stream = (SGPStream *)handle;
    if (stream->status != SMP_PLAYING)
        return 0;
    return (S32)ElapsedBytes(stream->startTime, stream->playRate, stream->bits, stream->channels,
                             stream->dataLength);
}

void AIL_stream_ms_position(HSTREAM handle, S32 *total_milliseconds, S32 *current_milliseconds)
{
    SGPStream *stream = (SGPStream *)handle;
    U32 bytesPerSecond = BytesPerSecond(stream->playRate, stream->bits, stream->channels);
    if (total_milliseconds)
        *total_milliseconds = (S32)((double)stream->dataLength * 1000.0 / bytesPerSecond);
    if (current_milliseconds)
        *current_milliseconds = (S32)((double)(U32)AIL_stream_position(handle) * 1000.0 / bytesPerSecond);
}

void SGPSleep(DWORD uiMilliseconds)
{
    DWORD end = GetTickCount() + uiMilliseconds;
    while ((LONG)(GetTickCount() - end) < 0)
        ;
}

/* ------------------------------------------------------------------ */
/* 3D: no providers                                                    */

S32 AIL_enumerate_3D_providers(HPROENUM *next, HPROVIDER *dest, C8 **name)
{
    (void)next;
    (void)dest;
    (void)name;
    return 0;
}

M3DRESULT AIL_open_3D_provider(HPROVIDER lib) { (void)lib; return 1; }
void AIL_close_3D_provider(HPROVIDER lib) { (void)lib; }
H3DPOBJECT AIL_open_3D_listener(HPROVIDER lib) { (void)lib; return NULL; }
void AIL_close_3D_listener(H3DPOBJECT listener) { (void)listener; }
void AIL_3D_provider_attribute(HPROVIDER lib, C8 const *name, void *val) { (void)lib; (void)name; (void)val; }
void AIL_set_3D_provider_preference(HPROVIDER lib, C8 const *name, void const *val) { (void)lib; (void)name; (void)val; }
S32 AIL_active_3D_sample_count(HPROVIDER lib) { (void)lib; return 0; }
H3DSAMPLE AIL_allocate_3D_sample_handle(HPROVIDER lib) { (void)lib; return NULL; }
void AIL_release_3D_sample_handle(H3DSAMPLE S) { (void)S; }
S32 AIL_set_3D_sample_file(H3DSAMPLE S, void const *file_image) { (void)S; (void)file_image; return 0; }
void AIL_start_3D_sample(H3DSAMPLE S) { (void)S; }
void AIL_stop_3D_sample(H3DSAMPLE S) { (void)S; }
U32 AIL_3D_sample_status(H3DSAMPLE S) { (void)S; return SMP_DONE; }
void AIL_set_3D_sample_volume(H3DSAMPLE S, S32 volume) { (void)S; (void)volume; }
S32 AIL_3D_sample_volume(H3DSAMPLE S) { (void)S; return 0; }
void AIL_set_3D_sample_playback_rate(H3DSAMPLE S, S32 playback_rate) { (void)S; (void)playback_rate; }
S32 AIL_3D_sample_playback_rate(H3DSAMPLE S) { (void)S; return 0; }
void AIL_set_3D_sample_loop_count(H3DSAMPLE S, U32 loops) { (void)S; (void)loops; }
U32 AIL_3D_sample_loop_count(H3DSAMPLE S) { (void)S; return 0; }
void AIL_set_3D_sample_distances(H3DSAMPLE S, F32 max_dist, F32 min_dist) { (void)S; (void)max_dist; (void)min_dist; }
void AIL_set_3D_position(H3DPOBJECT obj, F32 X, F32 Y, F32 Z) { (void)obj; (void)X; (void)Y; (void)Z; }
void AIL_set_3D_velocity_vector(H3DPOBJECT obj, F32 dX, F32 dY, F32 dZ) { (void)obj; (void)dX; (void)dY; (void)dZ; }
void AIL_set_3D_orientation(H3DPOBJECT obj, F32 X_face, F32 Y_face, F32 Z_face, F32 X_up, F32 Y_up, F32 Z_up)
{
    (void)obj; (void)X_face; (void)Y_face; (void)Z_face; (void)X_up; (void)Y_up; (void)Z_up;
}
