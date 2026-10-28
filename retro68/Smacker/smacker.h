#ifndef JA2_SMACKER_H
#define JA2_SMACKER_H

/* Smacker (SMK2/SMK4) video and audio decoder.
 *
 * The x86 game played its intro and endgame movies with RAD Game Tools'
 * Smacker SDK (smack.h, SMACKW32.LIB), which is not part of the source
 * release. This is a self-contained replacement written from the public
 * description of the file format: it decodes frames into an 8-bit indexed
 * buffer plus a 256-entry palette and unpacks the audio tracks to PCM.
 * It has no Mac dependencies, so tools/smktest.c can verify it on the host
 * against FFmpeg's decoder. All file data is little-endian and read
 * byte by byte, so the decoder is endian-neutral.
 */

#define SMACKER_MAX_TRACKS 7

typedef struct Smacker Smacker;

/* Reads length bytes at offset into buffer; returns nonzero on success. */
typedef int (*SmackerReadProc)(void *context, unsigned long offset, void *buffer,
                               unsigned long length);

Smacker *SmackerOpen(SmackerReadProc read, void *context);
void SmackerClose(Smacker *smacker);

unsigned long SmackerWidth(const Smacker *smacker);
unsigned long SmackerHeight(const Smacker *smacker);
unsigned long SmackerFrameCount(const Smacker *smacker);
/* Display time of one frame in microseconds. */
unsigned long SmackerFrameMicros(const Smacker *smacker);

/* Audio track properties; returns 0 when the track is absent. */
int SmackerAudioInfo(const Smacker *smacker, int track, unsigned long *rate,
                     int *channels, int *bits);

/* Decodes the next frame (palette, audio, video). Returns 0 at the end of
   the movie or on a decode error. */
int SmackerDecodeFrame(Smacker *smacker);
unsigned long SmackerCurrentFrame(const Smacker *smacker);

/* Indexed pixels of the last decoded frame, width bytes per row. */
const unsigned char *SmackerPixels(const Smacker *smacker);
/* 256 RGB triplets (8 bits per component). */
const unsigned char *SmackerPalette(const Smacker *smacker);
/* Nonzero when the last decoded frame changed the palette. */
int SmackerPaletteChanged(const Smacker *smacker);

/* PCM decoded from the last frame for a track: interleaved native-endian
   signed 16-bit samples (16-bit tracks) or unsigned 8-bit samples. Returns
   the size in bytes (0 when the frame has no audio for this track). */
unsigned long SmackerAudio(const Smacker *smacker, int track, const void **samples);

#endif
