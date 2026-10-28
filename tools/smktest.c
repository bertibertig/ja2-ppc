/* Host-side check of retro68/Smacker/smacker.c against FFmpeg.
 *
 *   cc -O2 -I retro68/Smacker -o smktest tools/smktest.c retro68/Smacker/smacker.c
 *   smktest MOVIE.SMK video.rgb audio.pcm
 *
 * Writes every frame as RGB24 and track 0 audio as signed 16-bit
 * little-endian PCM, the same layout as
 *   ffmpeg -i MOVIE.SMK -f rawvideo -pix_fmt rgb24 video.rgb
 *   ffmpeg -i MOVIE.SMK -f s16le audio.pcm
 * so the outputs can be compared with cmp. Also prints decode time.
 */
#include "smacker.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int ReadFile(void *context, unsigned long offset, void *buffer, unsigned long length)
{
    FILE *file = (FILE *)context;

    return fseek(file, (long)offset, SEEK_SET) == 0 &&
           fread(buffer, 1, length, file) == length;
}

int main(int argc, char **argv)
{
    FILE *input;
    FILE *video;
    FILE *audio;
    Smacker *smacker;
    unsigned char *rgb;
    unsigned long pixels;
    unsigned long index;
    unsigned long rate;
    int channels = 0;
    int bits = 0;
    clock_t start;

    if (argc != 4) {
        fprintf(stderr, "usage: %s MOVIE.SMK video.rgb audio.pcm\n", argv[0]);
        return 2;
    }
    input = fopen(argv[1], "rb");
    video = fopen(argv[2], "wb");
    audio = fopen(argv[3], "wb");
    if (input == NULL || video == NULL || audio == NULL) {
        perror("open");
        return 1;
    }
    smacker = SmackerOpen(ReadFile, input);
    if (smacker == NULL) {
        fprintf(stderr, "SmackerOpen failed\n");
        return 1;
    }
    SmackerAudioInfo(smacker, 0, &rate, &channels, &bits);
    printf("%lux%lu frames=%lu frame_us=%lu audio=%lu Hz %d ch %d bit\n",
           SmackerWidth(smacker), SmackerHeight(smacker), SmackerFrameCount(smacker),
           SmackerFrameMicros(smacker), rate, channels, bits);
    pixels = SmackerWidth(smacker) * SmackerHeight(smacker);
    rgb = (unsigned char *)malloc(pixels * 3);
    start = clock();
    while (SmackerDecodeFrame(smacker)) {
        const unsigned char *indexed = SmackerPixels(smacker);
        const unsigned char *palette = SmackerPalette(smacker);
        const void *samples;
        unsigned long size = SmackerAudio(smacker, 0, &samples);

        for (index = 0; index < pixels; ++index) {
            rgb[index * 3] = palette[indexed[index] * 3];
            rgb[index * 3 + 1] = palette[indexed[index] * 3 + 1];
            rgb[index * 3 + 2] = palette[indexed[index] * 3 + 2];
        }
        fwrite(rgb, 1, pixels * 3, video);
        if (bits == 16) {
            const short *pcm = (const short *)samples;

            for (index = 0; index < size / 2; ++index) {
                unsigned char le[2] = { (unsigned char)pcm[index],
                                        (unsigned char)((unsigned short)pcm[index] >> 8) };
                fwrite(le, 1, 2, audio);
            }
        } else {
            fwrite(samples, 1, size, audio);
        }
    }
    printf("decoded %lu frames in %.3f s\n", SmackerCurrentFrame(smacker),
           (double)(clock() - start) / CLOCKS_PER_SEC);
    SmackerClose(smacker);
    fclose(video);
    fclose(audio);
    fclose(input);
    free(rgb);
    return SmackerCurrentFrame(smacker) == 0;
}
