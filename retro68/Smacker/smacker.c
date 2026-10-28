#include "smacker.h"

#include <stdlib.h>
#include <string.h>

#define SMK_HEADER_SIZE 104
#define SMK_NODE 0x80000000UL
#define SMK_FLAG_RING_FRAME 0x01
#define SMK_AUDIO_PACKED 0x80
#define SMK_AUDIO_PRESENT 0x40
#define SMK_AUDIO_16BIT 0x20
#define SMK_AUDIO_STEREO 0x10
#define SMK_AUDIO_BINK 0x08

enum {
    SMK_TREE_MMAP,
    SMK_TREE_MCLR,
    SMK_TREE_FULL,
    SMK_TREE_TYPE,
    SMK_TREE_COUNT
};

enum {
    SMK_BLOCK_MONO,
    SMK_BLOCK_FULL,
    SMK_BLOCK_SKIP,
    SMK_BLOCK_FILL
};

/* Huffman trees are stored as flat arrays: an internal node holds
   SMK_NODE | size of its left subtree, its left child follows directly and
   its right child follows the left subtree. A leaf holds its value. */
typedef struct {
    unsigned long *values;
    long length;
    long count;
    long last[3];
} SmkTree;

typedef struct {
    const unsigned char *data;
    const unsigned char *end;
    unsigned long cache;
    int cacheBits;
} SmkBits;

struct Smacker {
    SmackerReadProc read;
    void *context;
    unsigned long width;
    unsigned long height;
    unsigned long frames;
    unsigned long frameMicros;
    int isSmk4;
    unsigned long audioRate[SMACKER_MAX_TRACKS];
    unsigned long *frameSizes;
    unsigned char *frameTypes;
    unsigned long dataOffset;
    unsigned long nextOffset;
    unsigned long frame;
    unsigned char *frameBuffer;
    unsigned long frameBufferSize;
    SmkTree trees[SMK_TREE_COUNT];
    unsigned char *pixels;
    unsigned char palette[768];
    int paletteChanged;
    unsigned char *audio[SMACKER_MAX_TRACKS];
    unsigned long audioCapacity[SMACKER_MAX_TRACKS];
    unsigned long audioSize[SMACKER_MAX_TRACKS];
};

static const unsigned short gBlockRuns[64] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 128, 256, 512, 1024, 2048
};

static unsigned long ReadLE32(const unsigned char *bytes)
{
    return (unsigned long)bytes[0] | ((unsigned long)bytes[1] << 8) |
           ((unsigned long)bytes[2] << 16) | ((unsigned long)bytes[3] << 24);
}

static void BitsInit(SmkBits *bits, const unsigned char *data, unsigned long length)
{
    bits->data = data;
    bits->end = data + length;
    bits->cache = 0;
    bits->cacheBits = 0;
}

/* Smacker bitstreams are read least significant bit first. */
static unsigned BitsRead1(SmkBits *bits)
{
    unsigned value;

    if (bits->cacheBits == 0) {
        bits->cache = bits->data < bits->end ? *bits->data++ : 0;
        bits->cacheBits = 8;
    }
    value = (unsigned)(bits->cache & 1);
    bits->cache >>= 1;
    --bits->cacheBits;
    return value;
}

static unsigned long BitsRead(SmkBits *bits, int count)
{
    unsigned long value = 0;
    int index;

    for (index = 0; index < count; ++index) {
        value |= (unsigned long)BitsRead1(bits) << index;
    }
    return value;
}

static int BitsOverrun(const SmkBits *bits)
{
    return bits->data >= bits->end && bits->cacheBits == 0;
}

static unsigned long TreeWalk(const unsigned long *values, SmkBits *bits)
{
    const unsigned long *node = values;

    while (*node & SMK_NODE) {
        if (BitsRead1(bits)) {
            node += *node & ~SMK_NODE;
        }
        ++node;
    }
    return *node;
}

/* 8-bit tree: leaves carry an 8-bit value. Returns the subtree size. */
static long DecodeByteTree(SmkBits *bits, unsigned long *values, long *count, long capacity,
                           int depth)
{
    long index;
    long left;
    long right;

    if (*count >= capacity || depth > 32) {
        return -1;
    }
    index = (*count)++;
    if (!BitsRead1(bits)) {
        values[index] = BitsRead(bits, 8);
        return 1;
    }
    left = DecodeByteTree(bits, values, count, capacity, depth + 1);
    if (left < 0) {
        return -1;
    }
    values[index] = SMK_NODE | (unsigned long)left;
    right = DecodeByteTree(bits, values, count, capacity, depth + 1);
    if (right < 0) {
        return -1;
    }
    return 1 + left + right;
}

/* Reads an optional 8-bit tree as used by the header and audio trees:
   a presence bit, the tree, and an end bit. An absent tree yields 0. */
static int ReadOptionalByteTree(SmkBits *bits, unsigned long values[512])
{
    long count = 0;

    values[0] = 0;
    if (!BitsRead1(bits)) {
        return 1;
    }
    if (DecodeByteTree(bits, values, &count, 512, 0) < 0) {
        return 0;
    }
    BitsRead1(bits);
    return 1;
}

typedef struct {
    const unsigned long *low;
    const unsigned long *high;
    unsigned long escapes[3];
    SmkTree *tree;
} SmkBigTreeContext;

static long DecodeBigTree(SmkBits *bits, SmkBigTreeContext *context, int depth)
{
    SmkTree *tree = context->tree;
    long index;
    long left;
    long right;

    if (tree->count + 1 >= tree->length || depth > 64) {
        return -1;
    }
    index = tree->count++;
    if (!BitsRead1(bits)) {
        unsigned long value = TreeWalk(context->low, bits) |
                              (TreeWalk(context->high, bits) << 8);
        int slot;

        for (slot = 0; slot < 3; ++slot) {
            if (value == context->escapes[slot]) {
                tree->last[slot] = index;
                value = 0;
                break;
            }
        }
        tree->values[index] = value;
        return 1;
    }
    left = DecodeBigTree(bits, context, depth + 1);
    if (left < 0) {
        return -1;
    }
    tree->values[index] = SMK_NODE | (unsigned long)left;
    right = DecodeBigTree(bits, context, depth + 1);
    if (right < 0) {
        return -1;
    }
    return 1 + left + right;
}

static int ReadBigTree(SmkBits *bits, SmkTree *tree, unsigned long size)
{
    unsigned long low[512];
    unsigned long high[512];
    SmkBigTreeContext context;
    int slot;

    if (!BitsRead1(bits)) {
        /* Absent tree: every code decodes to 0; slot 1 is a dummy cache. */
        tree->length = 2;
        tree->values = (unsigned long *)calloc(2, sizeof(unsigned long));
        tree->last[0] = tree->last[1] = tree->last[2] = 1;
        return tree->values != NULL;
    }
    if (!ReadOptionalByteTree(bits, low) || !ReadOptionalByteTree(bits, high)) {
        return 0;
    }
    for (slot = 0; slot < 3; ++slot) {
        context.escapes[slot] = BitsRead(bits, 16);
        tree->last[slot] = -1;
    }
    tree->length = (long)((size + 3) >> 2) + 4;
    tree->values = (unsigned long *)calloc((size_t)tree->length, sizeof(unsigned long));
    if (tree->values == NULL) {
        return 0;
    }
    tree->count = 0;
    context.low = low;
    context.high = high;
    context.tree = tree;
    if (DecodeBigTree(bits, &context, 0) < 0) {
        return 0;
    }
    BitsRead1(bits);
    for (slot = 0; slot < 3; ++slot) {
        if (tree->last[slot] == -1) {
            if (tree->count >= tree->length) {
                return 0;
            }
            tree->last[slot] = tree->count++;
        }
    }
    return 1;
}

static void TreeResetCache(SmkTree *tree)
{
    tree->values[tree->last[0]] = 0;
    tree->values[tree->last[1]] = 0;
    tree->values[tree->last[2]] = 0;
}

/* Big-tree codes go through a three-entry most-recently-used cache whose
   slots are the escape leaves of the tree. */
static unsigned long TreeGetCode(SmkTree *tree, SmkBits *bits)
{
    unsigned long *values = tree->values;
    unsigned long value = TreeWalk(values, bits);

    if (value != values[tree->last[0]]) {
        values[tree->last[2]] = values[tree->last[1]];
        values[tree->last[1]] = values[tree->last[0]];
        values[tree->last[0]] = value;
    }
    return value;
}

Smacker *SmackerOpen(SmackerReadProc read, void *context)
{
    unsigned char header[SMK_HEADER_SIZE];
    Smacker *smacker;
    unsigned long frameEntries;
    unsigned long treesSize;
    unsigned long treeSizes[SMK_TREE_COUNT];
    unsigned long sizesBytes;
    unsigned char *table;
    unsigned char *treeData;
    unsigned long index;
    long rate;
    unsigned long value;
    SmkBits bits;

    if (!read(context, 0, header, sizeof(header)) ||
        memcmp(header, "SMK", 3) != 0 || (header[3] != '2' && header[3] != '4')) {
        return NULL;
    }
    smacker = (Smacker *)calloc(1, sizeof(*smacker));
    if (smacker == NULL) {
        return NULL;
    }
    smacker->read = read;
    smacker->context = context;
    smacker->isSmk4 = header[3] == '4';
    smacker->width = ReadLE32(header + 4);
    smacker->height = ReadLE32(header + 8);
    smacker->frames = ReadLE32(header + 12);
    value = ReadLE32(header + 16);
    rate = (value & 0x80000000UL) ? -(long)((~value + 1) & 0xffffffffUL) : (long)value;
    /* Positive: milliseconds per frame; negative: units of 10 us. */
    smacker->frameMicros = rate > 0 ? (unsigned long)rate * 1000UL
                         : rate < 0 ? (unsigned long)(-rate) * 10UL : 100000UL;
    treesSize = ReadLE32(header + 52);
    for (index = 0; index < SMK_TREE_COUNT; ++index) {
        treeSizes[index] = ReadLE32(header + 56 + index * 4);
    }
    for (index = 0; index < SMACKER_MAX_TRACKS; ++index) {
        smacker->audioRate[index] = ReadLE32(header + 72 + index * 4);
    }
    if (smacker->width == 0 || smacker->height == 0 || smacker->width > 4096 ||
        smacker->height > 4096 || smacker->frames == 0 || smacker->frames > 100000 ||
        (smacker->width & 3) != 0 || (smacker->height & 3) != 0) {
        free(smacker);
        return NULL;
    }
    frameEntries = smacker->frames + ((ReadLE32(header + 20) & SMK_FLAG_RING_FRAME) ? 1 : 0);

    sizesBytes = frameEntries * 4;
    table = (unsigned char *)malloc(sizesBytes + frameEntries);
    smacker->frameSizes = (unsigned long *)malloc(frameEntries * sizeof(unsigned long));
    smacker->frameTypes = (unsigned char *)malloc(frameEntries);
    treeData = (unsigned char *)malloc(treesSize + 1);
    smacker->pixels = (unsigned char *)calloc(smacker->width, smacker->height);
    if (table == NULL || smacker->frameSizes == NULL || smacker->frameTypes == NULL ||
        treeData == NULL || smacker->pixels == NULL ||
        !read(context, SMK_HEADER_SIZE, table, sizesBytes + frameEntries) ||
        !read(context, SMK_HEADER_SIZE + sizesBytes + frameEntries, treeData, treesSize)) {
        free(table);
        free(treeData);
        SmackerClose(smacker);
        return NULL;
    }
    for (index = 0; index < frameEntries; ++index) {
        smacker->frameSizes[index] = ReadLE32(table + index * 4) & ~3UL;
        smacker->frameTypes[index] = table[sizesBytes + index];
        if (smacker->frameSizes[index] > smacker->frameBufferSize) {
            smacker->frameBufferSize = smacker->frameSizes[index];
        }
    }
    free(table);

    BitsInit(&bits, treeData, treesSize);
    for (index = 0; index < SMK_TREE_COUNT; ++index) {
        if (!ReadBigTree(&bits, &smacker->trees[index], treeSizes[index])) {
            free(treeData);
            SmackerClose(smacker);
            return NULL;
        }
    }
    free(treeData);

    smacker->frameBuffer = (unsigned char *)malloc(smacker->frameBufferSize + 4);
    if (smacker->frameBuffer == NULL) {
        SmackerClose(smacker);
        return NULL;
    }
    smacker->dataOffset = SMK_HEADER_SIZE + sizesBytes + frameEntries + treesSize;
    smacker->nextOffset = smacker->dataOffset;
    return smacker;
}

void SmackerClose(Smacker *smacker)
{
    int index;

    if (smacker == NULL) {
        return;
    }
    for (index = 0; index < SMK_TREE_COUNT; ++index) {
        free(smacker->trees[index].values);
    }
    for (index = 0; index < SMACKER_MAX_TRACKS; ++index) {
        free(smacker->audio[index]);
    }
    free(smacker->frameSizes);
    free(smacker->frameTypes);
    free(smacker->frameBuffer);
    free(smacker->pixels);
    free(smacker);
}

unsigned long SmackerWidth(const Smacker *smacker) { return smacker->width; }
unsigned long SmackerHeight(const Smacker *smacker) { return smacker->height; }
unsigned long SmackerFrameCount(const Smacker *smacker) { return smacker->frames; }
unsigned long SmackerFrameMicros(const Smacker *smacker) { return smacker->frameMicros; }
unsigned long SmackerCurrentFrame(const Smacker *smacker) { return smacker->frame; }
const unsigned char *SmackerPixels(const Smacker *smacker) { return smacker->pixels; }
const unsigned char *SmackerPalette(const Smacker *smacker) { return smacker->palette; }
int SmackerPaletteChanged(const Smacker *smacker) { return smacker->paletteChanged; }

int SmackerAudioInfo(const Smacker *smacker, int track, unsigned long *rate,
                     int *channels, int *bits)
{
    unsigned long value;

    if (track < 0 || track >= SMACKER_MAX_TRACKS) {
        return 0;
    }
    value = smacker->audioRate[track];
    if (((value >> 24) & SMK_AUDIO_PRESENT) == 0) {
        return 0;
    }
    *rate = value & 0xffffff;
    *channels = ((value >> 24) & SMK_AUDIO_STEREO) ? 2 : 1;
    *bits = ((value >> 24) & SMK_AUDIO_16BIT) ? 16 : 8;
    return 1;
}

unsigned long SmackerAudio(const Smacker *smacker, int track, const void **samples)
{
    if (track < 0 || track >= SMACKER_MAX_TRACKS) {
        return 0;
    }
    *samples = smacker->audio[track];
    return smacker->audioSize[track];
}

/* 6-bit palette components scale to 8 bits by replicating the top bits
   (0x00, 0x04 ... 0x3c, 0x41 ... 0xff), as RAD's player does. */
static unsigned char Scale6(unsigned value)
{
    value &= 0x3f;
    return (unsigned char)(value * 4 + (value >> 4));
}

static void DecodePalette(Smacker *smacker, const unsigned char *data, unsigned long length)
{
    unsigned char old[768];
    unsigned char *palette = smacker->palette;
    const unsigned char *end = data + length;
    int entry = 0;

    memcpy(old, palette, sizeof(old));
    while (entry < 256 && data < end) {
        unsigned value = *data++;

        if (value & 0x80) {
            entry += (int)(value & 0x7f) + 1;
        } else if (value & 0x40) {
            int count = (int)(value & 0x3f) + 1;
            int source;

            if (data >= end) {
                break;
            }
            source = *data++;
            while (count-- > 0 && entry < 256 && source < 256) {
                memcpy(palette + entry * 3, old + source * 3, 3);
                ++entry;
                ++source;
            }
        } else {
            if (end - data < 2) {
                break;
            }
            palette[entry * 3] = Scale6(value);
            palette[entry * 3 + 1] = Scale6(data[0]);
            palette[entry * 3 + 2] = Scale6(data[1]);
            data += 2;
            ++entry;
        }
    }
    smacker->paletteChanged = 1;
}

static int EnsureAudio(Smacker *smacker, int track, unsigned long size)
{
    if (size > smacker->audioCapacity[track]) {
        unsigned char *buffer = (unsigned char *)realloc(smacker->audio[track], size);

        if (buffer == NULL) {
            return 0;
        }
        smacker->audio[track] = buffer;
        smacker->audioCapacity[track] = size;
    }
    return 1;
}

static long SignExtend(unsigned long value, int bits)
{
    unsigned long sign = 1UL << (bits - 1);

    value &= (sign << 1) - 1;
    return (long)(value ^ sign) - (long)sign;
}

static int DecodeAudio(Smacker *smacker, int track, const unsigned char *data,
                       unsigned long length)
{
    unsigned long flags = smacker->audioRate[track] >> 24;
    unsigned long unpacked;
    SmkBits bits;
    static unsigned long trees[4][512];
    int stereo;
    int sixteen;
    int treeCount;
    int index;
    long predictor[2];
    unsigned long sample;
    unsigned long sampleCount;

    smacker->audioSize[track] = 0;
    if ((flags & SMK_AUDIO_PACKED) == 0) {
        /* Raw little-endian PCM. */
        if (!EnsureAudio(smacker, track, length)) {
            return 0;
        }
        memcpy(smacker->audio[track], data, length);
        if (flags & SMK_AUDIO_16BIT) {
            for (sample = 0; sample + 1 < length; sample += 2) {
                short value = (short)(data[sample] | (data[sample + 1] << 8));
                memcpy(smacker->audio[track] + sample, &value, 2);
            }
        }
        smacker->audioSize[track] = length;
        return 1;
    }
    if ((flags & SMK_AUDIO_BINK) || length < 4) {
        return 0;
    }
    unpacked = ReadLE32(data);
    BitsInit(&bits, data + 4, length - 4);
    if (!BitsRead1(&bits)) {
        return 1;
    }
    stereo = (int)BitsRead1(&bits);
    sixteen = (int)BitsRead1(&bits);
    if (stereo != ((flags & SMK_AUDIO_STEREO) != 0) ||
        sixteen != ((flags & SMK_AUDIO_16BIT) != 0) || unpacked > 0x400000) {
        return 0;
    }
    treeCount = 1 << (sixteen + stereo);
    for (index = 0; index < treeCount; ++index) {
        if (!ReadOptionalByteTree(&bits, trees[index])) {
            return 0;
        }
    }
    if (!EnsureAudio(smacker, track, unpacked)) {
        return 0;
    }
    if (sixteen) {
        short *out = (short *)smacker->audio[track];

        sampleCount = unpacked / 2;
        for (index = stereo; index >= 0; --index) {
            unsigned long high = BitsRead(&bits, 8);
            unsigned long low = BitsRead(&bits, 8);

            predictor[index] = SignExtend((high << 8) | low, 16);
        }
        for (sample = 0; sample <= (unsigned long)stereo && sample < sampleCount; ++sample) {
            out[sample] = (short)predictor[sample];
        }
        for (; sample < sampleCount; ++sample) {
            int channel = (int)(sample & (unsigned long)stereo);
            unsigned long value = TreeWalk(trees[channel * 2], &bits) |
                                  (TreeWalk(trees[channel * 2 + 1], &bits) << 8);

            /* The format relies on 16-bit wraparound, not clipping. */
            predictor[channel] = SignExtend((unsigned long)predictor[channel] + value, 16);
            out[sample] = (short)predictor[channel];
        }
    } else {
        unsigned char *out = smacker->audio[track];

        sampleCount = unpacked;
        for (index = stereo; index >= 0; --index) {
            predictor[index] = (long)BitsRead(&bits, 8);
        }
        for (sample = 0; sample <= (unsigned long)stereo && sample < sampleCount; ++sample) {
            out[sample] = (unsigned char)predictor[sample];
        }
        for (; sample < sampleCount; ++sample) {
            int channel = (int)(sample & (unsigned long)stereo);

            predictor[channel] = (predictor[channel] + TreeWalk(trees[channel], &bits)) & 0xff;
            out[sample] = (unsigned char)predictor[channel];
        }
    }
    smacker->audioSize[track] = unpacked;
    return 1;
}

static void DecodeVideo(Smacker *smacker, const unsigned char *data, unsigned long length)
{
    SmkBits bits;
    unsigned long width = smacker->width;
    unsigned long blocksWide = width / 4;
    unsigned long blocks = blocksWide * (smacker->height / 4);
    unsigned long block = 0;
    int index;

    for (index = 0; index < SMK_TREE_COUNT; ++index) {
        TreeResetCache(&smacker->trees[index]);
    }
    BitsInit(&bits, data, length);
    while (block < blocks && !BitsOverrun(&bits)) {
        unsigned long type = TreeGetCode(&smacker->trees[SMK_TREE_TYPE], &bits);
        unsigned long run = gBlockRuns[(type >> 2) & 0x3f];

        switch (type & 3) {
        case SMK_BLOCK_MONO:
            while (run-- > 0 && block < blocks) {
                unsigned long colors = TreeGetCode(&smacker->trees[SMK_TREE_MCLR], &bits);
                unsigned long map = TreeGetCode(&smacker->trees[SMK_TREE_MMAP], &bits);
                unsigned char high = (unsigned char)(colors >> 8);
                unsigned char low = (unsigned char)colors;
                unsigned char *out = smacker->pixels + (block / blocksWide) * width * 4 +
                                     (block % blocksWide) * 4;
                int row;

                for (row = 0; row < 4; ++row) {
                    out[0] = (map & 1) ? high : low;
                    out[1] = (map & 2) ? high : low;
                    out[2] = (map & 4) ? high : low;
                    out[3] = (map & 8) ? high : low;
                    map >>= 4;
                    out += width;
                }
                ++block;
            }
            break;
        case SMK_BLOCK_FULL: {
            int mode = 0;

            if (smacker->isSmk4) {
                if (BitsRead1(&bits)) {
                    mode = 1;
                } else if (BitsRead1(&bits)) {
                    mode = 2;
                }
            }
            while (run-- > 0 && block < blocks) {
                unsigned char *out = smacker->pixels + (block / blocksWide) * width * 4 +
                                     (block % blocksWide) * 4;
                int row;

                if (mode == 0) {
                    for (row = 0; row < 4; ++row) {
                        unsigned long pair = TreeGetCode(&smacker->trees[SMK_TREE_FULL], &bits);

                        out[2] = (unsigned char)pair;
                        out[3] = (unsigned char)(pair >> 8);
                        pair = TreeGetCode(&smacker->trees[SMK_TREE_FULL], &bits);
                        out[0] = (unsigned char)pair;
                        out[1] = (unsigned char)(pair >> 8);
                        out += width;
                    }
                } else if (mode == 1) {
                    /* SMK4 double: 2x2 pixel doubling. */
                    for (row = 0; row < 2; ++row) {
                        unsigned long pair = TreeGetCode(&smacker->trees[SMK_TREE_FULL], &bits);
                        unsigned char p0 = (unsigned char)pair;
                        unsigned char p1 = (unsigned char)(pair >> 8);

                        out[0] = out[1] = out[width] = out[width + 1] = p0;
                        out[2] = out[3] = out[width + 2] = out[width + 3] = p1;
                        out += width * 2;
                    }
                } else {
                    /* SMK4 half: each code fills two rows. */
                    for (row = 0; row < 2; ++row) {
                        unsigned long pair = TreeGetCode(&smacker->trees[SMK_TREE_FULL], &bits);
                        unsigned long pair2;

                        out[0] = out[width] = (unsigned char)pair;
                        out[1] = out[width + 1] = (unsigned char)(pair >> 8);
                        pair2 = TreeGetCode(&smacker->trees[SMK_TREE_FULL], &bits);
                        out[2] = out[width + 2] = (unsigned char)pair2;
                        out[3] = out[width + 3] = (unsigned char)(pair2 >> 8);
                        out += width * 2;
                    }
                }
                ++block;
            }
            break;
        }
        case SMK_BLOCK_SKIP:
            while (run-- > 0 && block < blocks) {
                ++block;
            }
            break;
        case SMK_BLOCK_FILL: {
            unsigned char color = (unsigned char)(type >> 8);

            while (run-- > 0 && block < blocks) {
                unsigned char *out = smacker->pixels + (block / blocksWide) * width * 4 +
                                     (block % blocksWide) * 4;
                int row;

                for (row = 0; row < 4; ++row) {
                    out[0] = out[1] = out[2] = out[3] = color;
                    out += width;
                }
                ++block;
            }
            break;
        }
        }
    }
}

int SmackerDecodeFrame(Smacker *smacker)
{
    unsigned long size;
    unsigned char type;
    unsigned long position = 0;
    int track;

    if (smacker->frame >= smacker->frames) {
        return 0;
    }
    size = smacker->frameSizes[smacker->frame];
    type = smacker->frameTypes[smacker->frame];
    if (!smacker->read(smacker->context, smacker->nextOffset, smacker->frameBuffer, size)) {
        return 0;
    }
    smacker->nextOffset += size;
    smacker->paletteChanged = 0;
    if (type & 1) {
        unsigned long paletteSize = (unsigned long)smacker->frameBuffer[0] * 4;

        if (paletteSize == 0 || paletteSize > size) {
            return 0;
        }
        DecodePalette(smacker, smacker->frameBuffer + 1, paletteSize - 1);
        position = paletteSize;
    }
    for (track = 0; track < SMACKER_MAX_TRACKS; ++track) {
        smacker->audioSize[track] = 0;
        if (type & (2 << track)) {
            unsigned long length;

            if (position + 4 > size) {
                return 0;
            }
            length = ReadLE32(smacker->frameBuffer + position);
            if (length < 4 || position + length > size) {
                return 0;
            }
            DecodeAudio(smacker, track, smacker->frameBuffer + position + 4, length - 4);
            position += length;
        }
    }
    DecodeVideo(smacker, smacker->frameBuffer + position, size - position);
    ++smacker->frame;
    return 1;
}
