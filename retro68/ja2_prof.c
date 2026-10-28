#include "ja2_prof.h"

#if defined(JA2_PROFILE)

#include "ja2_mac_internal.h"
#include "ja2_options.h"

#include <Gestalt.h>
#include <Memory.h>
#include <Timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Timing uses the PowerPC time base (mftb), which costs a few cycles and
   does not trap into the Toolbox. Its frequency is bus-dependent, so it is
   calibrated once against Microseconds(). The 601 has no time base; it
   falls back to Microseconds() for every sample. */

#define JA2_PROF_COLUMNS (JA2_PROF_ZONE_COUNT + 1)
#define JA2_PROF_FRAME_COLUMN JA2_PROF_ZONE_COUNT
#define JA2_PROF_HISTORY 1024
#define JA2_PROF_BUCKET_US 250
#define JA2_PROF_BUCKETS 400
#define JA2_PROF_MARKERS 64
#define JA2_PROF_MEMORY_INTERVAL 30
#define JA2_PROF_CALIBRATION_US 200000UL

typedef unsigned long long JA2ProfTicks;

typedef struct {
    unsigned long frames;
    unsigned long calls;
    JA2ProfTicks sumUs;
    unsigned long maxUs;
    unsigned long buckets[JA2_PROF_BUCKETS + 1];
} JA2ProfStats;

typedef struct {
    unsigned long frame;
    char label[32];
} JA2ProfMarker;

static const char *gZoneNames[JA2_PROF_COLUMNS] = {
    "events", "logic", "audio", "video", "render", "blit", "present",
    "decode", "file", "idle", "frame"
};

static Boolean gProfActive;
static Boolean gProfUseTimeBase;
static unsigned long gProfTimeBaseHz;
static JA2ProfTicks gProfUsScale; /* microseconds per tick, 16.16 */
static JA2ProfTicks gProfFrameStart;
static JA2ProfTicks gProfZoneStart[JA2_PROF_ZONE_COUNT];
static short gProfZoneDepth[JA2_PROF_ZONE_COUNT];
static JA2ProfTicks gProfZoneTicks[JA2_PROF_ZONE_COUNT];
static unsigned long gProfZoneCalls[JA2_PROF_ZONE_COUNT];
static unsigned long gProfFrameCount;
static unsigned long gProfFrameLimit;
static Boolean gProfUncapped;
static unsigned long *gProfHistory;
static unsigned long *gProfHistoryFreeKB;
static JA2ProfStats *gProfStats;
static JA2ProfMarker gProfMarkers[JA2_PROF_MARKERS];
static short gProfMarkerCount;
static unsigned long gProfFreeKB;
static unsigned long gProfMinFreeKB = 0xffffffffUL;
static char gProfLabel[32];

static JA2ProfTicks ReadMicroseconds(void)
{
    UnsignedWide now;

    Microseconds(&now);
    return ((JA2ProfTicks)now.hi << 32) | now.lo;
}

static JA2ProfTicks ReadTimeBase(void)
{
    unsigned long upper;
    unsigned long lower;
    unsigned long upperCheck;

    do {
        __asm__ volatile("mftbu %0" : "=r"(upper));
        __asm__ volatile("mftb %0" : "=r"(lower));
        __asm__ volatile("mftbu %0" : "=r"(upperCheck));
    } while (upper != upperCheck);
    return ((JA2ProfTicks)upper << 32) | lower;
}

static JA2ProfTicks ReadTicks(void)
{
    return gProfUseTimeBase ? ReadTimeBase() : ReadMicroseconds();
}

static unsigned long TicksToUs(JA2ProfTicks ticks)
{
    return (unsigned long)((ticks * gProfUsScale) >> 16);
}

static void CalibrateTimeBase(void)
{
    long cpuType = 0;
    JA2ProfTicks startUs;
    JA2ProfTicks endUs;
    JA2ProfTicks startTb;
    JA2ProfTicks endTb;

    if (Gestalt(gestaltNativeCPUtype, &cpuType) == noErr &&
        cpuType == gestaltCPU601) {
        gProfUseTimeBase = 0;
        gProfTimeBaseHz = 1000000UL;
        gProfUsScale = 1ULL << 16;
        DebugLog("Profiler: PPC 601 detected; using Microseconds()");
        return;
    }
    startUs = ReadMicroseconds();
    startTb = ReadTimeBase();
    do {
        endUs = ReadMicroseconds();
    } while (endUs - startUs < JA2_PROF_CALIBRATION_US);
    endTb = ReadTimeBase();
    gProfTimeBaseHz = (unsigned long)(((endTb - startTb) * 1000000ULL) /
                                      (endUs - startUs));
    gProfUseTimeBase = gProfTimeBaseHz != 0;
    if (!gProfUseTimeBase) {
        gProfTimeBaseHz = 1000000UL;
    }
    gProfUsScale = (1000000ULL << 16) / gProfTimeBaseHz;
    DebugLog("Profiler: cpu=0x%lx timebase=%lu Hz", cpuType, gProfTimeBaseHz);
}

static void SampleFreeMemory(void)
{
    gProfFreeKB = (unsigned long)FreeMem() / 1024UL;
    if (gProfFreeKB < gProfMinFreeKB) {
        gProfMinFreeKB = gProfFreeKB;
    }
}

void JA2ProfInit(void)
{
    const char *label;

    gProfHistory = (unsigned long *)calloc(
        (size_t)JA2_PROF_HISTORY * JA2_PROF_COLUMNS, sizeof(unsigned long));
    gProfHistoryFreeKB = (unsigned long *)calloc(JA2_PROF_HISTORY,
                                                 sizeof(unsigned long));
    gProfStats = (JA2ProfStats *)calloc(JA2_PROF_COLUMNS, sizeof(JA2ProfStats));
    if (gProfHistory == NULL || gProfHistoryFreeKB == NULL || gProfStats == NULL) {
        DebugLog("Profiler: allocation failed; profiling disabled");
        free(gProfHistory);
        free(gProfHistoryFreeKB);
        free(gProfStats);
        gProfHistory = NULL;
        gProfHistoryFreeKB = NULL;
        gProfStats = NULL;
        return;
    }
    CalibrateTimeBase();
    gProfFrameLimit = JA2OptionsProfileFrames();
    gProfUncapped = JA2OptionsProfileUncapped();
    label = JA2OptionsProfileLabel();
    strncpy(gProfLabel, label, sizeof(gProfLabel) - 1);
    gProfLabel[sizeof(gProfLabel) - 1] = 0;
    SampleFreeMemory();
    gProfActive = 1;
    DebugLog("Profiler: active label=%s frames=%lu uncapped=%d", gProfLabel,
             gProfFrameLimit, gProfUncapped);
}

void JA2ProfBegin(int zone)
{
    if (!gProfActive || zone < 0 || zone >= JA2_PROF_ZONE_COUNT) {
        return;
    }
    if (gProfZoneDepth[zone]++ == 0) {
        gProfZoneStart[zone] = ReadTicks();
    }
    ++gProfZoneCalls[zone];
}

void JA2ProfEnd(int zone)
{
    if (!gProfActive || zone < 0 || zone >= JA2_PROF_ZONE_COUNT ||
        gProfZoneDepth[zone] == 0) {
        return;
    }
    if (--gProfZoneDepth[zone] == 0) {
        gProfZoneTicks[zone] += ReadTicks() - gProfZoneStart[zone];
    }
}

void JA2ProfFrameBegin(void)
{
    if (!gProfActive) {
        return;
    }
    gProfFrameStart = ReadTicks();
}

static void AddSample(int column, unsigned long us, unsigned long calls)
{
    JA2ProfStats *stats = &gProfStats[column];
    unsigned long bucket = us / JA2_PROF_BUCKET_US;

    if (calls == 0) {
        return;
    }
    ++stats->frames;
    stats->calls += calls;
    stats->sumUs += us;
    if (us > stats->maxUs) {
        stats->maxUs = us;
    }
    if (bucket > JA2_PROF_BUCKETS) {
        bucket = JA2_PROF_BUCKETS;
    }
    ++stats->buckets[bucket];
}

void JA2ProfFrameEnd(void)
{
    unsigned long *row;
    unsigned long frameUs;
    int zone;

    if (!gProfActive) {
        return;
    }
    frameUs = TicksToUs(ReadTicks() - gProfFrameStart);
    if (gProfFrameCount % JA2_PROF_MEMORY_INTERVAL == 0) {
        SampleFreeMemory();
    }
    row = &gProfHistory[(gProfFrameCount % JA2_PROF_HISTORY) * JA2_PROF_COLUMNS];
    for (zone = 0; zone < JA2_PROF_ZONE_COUNT; ++zone) {
        unsigned long us = TicksToUs(gProfZoneTicks[zone]);
        row[zone] = us;
        AddSample(zone, us, gProfZoneCalls[zone]);
        gProfZoneTicks[zone] = 0;
        gProfZoneCalls[zone] = 0;
    }
    row[JA2_PROF_FRAME_COLUMN] = frameUs;
    AddSample(JA2_PROF_FRAME_COLUMN, frameUs, 1);
    gProfHistoryFreeKB[gProfFrameCount % JA2_PROF_HISTORY] = gProfFreeKB;
    ++gProfFrameCount;
}

void JA2ProfMark(const char *label)
{
    JA2ProfMarker *marker;

    if (!gProfActive || gProfMarkerCount >= JA2_PROF_MARKERS) {
        return;
    }
    marker = &gProfMarkers[gProfMarkerCount++];
    marker->frame = gProfFrameCount;
    strncpy(marker->label, label, sizeof(marker->label) - 1);
    marker->label[sizeof(marker->label) - 1] = 0;
}

Boolean JA2ProfShouldQuit(void)
{
    return gProfActive && gProfFrameLimit != 0 &&
           gProfFrameCount >= gProfFrameLimit;
}

Boolean JA2ProfUncapped(void)
{
    return gProfActive && gProfUncapped;
}

static unsigned long Percentile(const JA2ProfStats *stats, unsigned long permille)
{
    unsigned long target;
    unsigned long seen = 0;
    int bucket;

    if (stats->frames == 0) {
        return 0;
    }
    target = (stats->frames * permille + 999) / 1000;
    for (bucket = 0; bucket <= JA2_PROF_BUCKETS; ++bucket) {
        seen += stats->buckets[bucket];
        if (seen >= target) {
            /* Report the upper edge of the bucket; the overflow bucket
               reports the observed maximum. */
            if (bucket == JA2_PROF_BUCKETS) {
                return stats->maxUs;
            }
            return (unsigned long)(bucket + 1) * JA2_PROF_BUCKET_US;
        }
    }
    return stats->maxUs;
}

static FILE *OpenProfileFile(void)
{
    FILE *file = fopen("JA2PROF.csv", "w");

    if (file == NULL) {
        file = fopen(":JA2PROF.csv", "w");
    }
    return file;
}

void JA2ProfShutdown(void)
{
    FILE *file;
    unsigned long first;
    unsigned long index;
    int column;

    if (!gProfActive) {
        return;
    }
    gProfActive = 0;
    file = OpenProfileFile();
    if (file == NULL) {
        DebugLog("Profiler: cannot write JA2PROF.csv");
    } else {
        fprintf(file, "meta,version,1\n");
        fprintf(file, "meta,label,%s\n", gProfLabel);
        fprintf(file, "meta,timebase_hz,%lu\n", gProfTimeBaseHz);
        fprintf(file, "meta,uses_timebase,%d\n", gProfUseTimeBase);
        fprintf(file, "meta,frames,%lu\n", gProfFrameCount);
        fprintf(file, "meta,uncapped,%d\n", gProfUncapped);
        fprintf(file, "meta,min_free_kb,%lu\n", gProfMinFreeKB);
        fprintf(file, "summary,zone,frames,calls,mean_us,p50_us,p95_us,p99_us,max_us\n");
        for (column = 0; column < JA2_PROF_COLUMNS; ++column) {
            const JA2ProfStats *stats = &gProfStats[column];
            fprintf(file, "summary,%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
                    gZoneNames[column], stats->frames, stats->calls,
                    stats->frames ? (unsigned long)(stats->sumUs / stats->frames) : 0,
                    Percentile(stats, 500), Percentile(stats, 950),
                    Percentile(stats, 990), stats->maxUs);
        }
        for (index = 0; index < (unsigned long)gProfMarkerCount; ++index) {
            fprintf(file, "marker,%lu,%s\n", gProfMarkers[index].frame,
                    gProfMarkers[index].label);
        }
        fprintf(file, "frame,index,free_kb");
        for (column = 0; column < JA2_PROF_COLUMNS; ++column) {
            fprintf(file, ",%s_us", gZoneNames[column]);
        }
        fprintf(file, "\n");
        first = gProfFrameCount > JA2_PROF_HISTORY ?
                gProfFrameCount - JA2_PROF_HISTORY : 0;
        for (index = first; index < gProfFrameCount; ++index) {
            const unsigned long *row =
                &gProfHistory[(index % JA2_PROF_HISTORY) * JA2_PROF_COLUMNS];
            fprintf(file, "frame,%lu,%lu", index,
                    gProfHistoryFreeKB[index % JA2_PROF_HISTORY]);
            for (column = 0; column < JA2_PROF_COLUMNS; ++column) {
                fprintf(file, ",%lu", row[column]);
            }
            fprintf(file, "\n");
        }
        fclose(file);
        DebugLog("Profiler: wrote JA2PROF.csv frames=%lu", gProfFrameCount);
    }
    free(gProfHistory);
    free(gProfHistoryFreeKB);
    free(gProfStats);
    gProfHistory = NULL;
    gProfHistoryFreeKB = NULL;
    gProfStats = NULL;
}

#endif
