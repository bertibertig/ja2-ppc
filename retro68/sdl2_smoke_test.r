#include "Types.r"
#include "Processes.r"

/*
 * Retro68's default add_application() template only reserves a 1 MB
 * application heap (see RIncludes/Retro68APPL.r), which is far too small
 * for SDL2's own allocations (window backing GWorld, video mode tables,
 * mouse/event state) plus a second explicit game-side RGB555 surface.
 * Verified on real hardware: without this resource, SDL_CreateRGBSurface-
 * WithFormat fails with "Out of memory" after SDL_CreateWindow already
 * consumed the default 1 MB partition.
 *
 * Match ja2_mac.r's 32 MB preferred/minimum partition, consistent with
 * the AGENTS.md "Toolchain And Target Platform" 32 MB RAM hardware target.
 */
resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    canBackground,
    doesActivateOnFGSwitch,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    is32BitCompatible,
    notHighLevelEventAware,
    onlyLocalHLEvents,
    notStationeryAware,
    dontUseTextEditServices,
    reserved,
    reserved,
    reserved,
    32 * 1024 * 1024,
    32 * 1024 * 1024
};
