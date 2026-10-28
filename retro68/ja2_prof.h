#ifndef JA2_PROF_H
#define JA2_PROF_H

/* PPC-only frame profiler. There is no original counterpart: the x86 build
   relied on external tools (VTune) for profiling. All entry points compile
   to nothing unless JA2_PROFILE is defined, so call sites can stay in
   release code. Zones are inclusive: a nested zone's time is also counted
   in its caller. */

#include <Quickdraw.h>

enum {
    JA2_PROF_EVENTS,
    JA2_PROF_LOGIC,
    JA2_PROF_AUDIO,
    JA2_PROF_VIDEO,
    JA2_PROF_RENDER,
    JA2_PROF_BLIT,
    JA2_PROF_PRESENT,
    JA2_PROF_DECODE,
    JA2_PROF_FILE,
    JA2_PROF_IDLE,
    JA2_PROF_MOVIE,
    JA2_PROF_ZONE_COUNT
};

#if defined(JA2_PROFILE)

void JA2ProfInit(void);
void JA2ProfShutdown(void);
void JA2ProfFrameBegin(void);
void JA2ProfFrameEnd(void);
void JA2ProfBegin(int zone);
void JA2ProfEnd(int zone);
void JA2ProfMark(const char *label);
Boolean JA2ProfShouldQuit(void);
Boolean JA2ProfUncapped(void);

#define JA2_PROF_BEGIN(zone) JA2ProfBegin(zone)
#define JA2_PROF_END(zone) JA2ProfEnd(zone)
#define JA2_PROF_MARK(label) JA2ProfMark(label)

#else

#define JA2ProfInit() ((void)0)
#define JA2ProfShutdown() ((void)0)
#define JA2ProfFrameBegin() ((void)0)
#define JA2ProfFrameEnd() ((void)0)
#define JA2ProfShouldQuit() 0
#define JA2ProfUncapped() 0
#define JA2_PROF_BEGIN(zone) ((void)0)
#define JA2_PROF_END(zone) ((void)0)
#define JA2_PROF_MARK(label) ((void)0)

#endif

#endif
