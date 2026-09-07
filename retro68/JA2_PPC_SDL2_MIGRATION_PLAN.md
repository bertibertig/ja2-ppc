# PPC SDL2 Rendering Migration Plan

## Decision Summary

This plan documents the agreed scope before any code changes:

- Replace the QuickDraw platform shell in `retro68/ja2_mac.c` with SDL2,
  using [`laulandn/sdl2macos9`](https://github.com/laulandn/sdl2macos9) as
  the SDL2 fork (it targets classic Mac OS 7/8/9 m68k/PPC via Retro68).
- The QuickDraw rendering path is fully replaced, not kept as a parallel
  fallback. Replacement happens in verified stages (see below); nothing is
  deleted until its SDL2 equivalent has been validated on real hardware.
- Color depth stays fixed at 16-bit RGB555, matching the current STCI/ETRLE
  asset pipeline and GWorld code. SDL2 is used to remove the hand-rolled
  RGB555 byte-order/blit code, not to introduce a variable-depth renderer.
- SDL2 is integrated into the existing Retro68/CMake build as a wrapped
  external target (see "Build Integration" below); this is the closest
  practical equivalent of a "CMake subproject" for this codebase, because
  the SDL2 fork itself is not CMake-buildable for this platform.

## Why SDL2 Does Not Remove QuickDraw Knowledge

`sdl2macos9`'s classic backend
(`SDL-main/src/video/macosclassic/macvideo.c`,`macmouse.c`,`macevents.c`)
is implemented **on top of** the same Toolbox APIs the current port already
uses: `NewCWindow`, `GWorld`/`PixMap`, `CopyBits`, `WaitNextEvent`, and
optionally `DrawSprocket`/`InputSprocket`. SDL2 does not bypass QuickDraw; it
gives the game code a stable, portable `SDL_Surface`/`SDL_Event` API instead
of hand-written GWorld and event-record plumbing. The benefit is fewer
JA2-specific bytes-and-byte-order bugs (like the RGB555 issue below), not a
different rendering technology on the Mac side.

## Toolchain Dependency Audit (verified in `$RETRO68_TOOLCHAIN`)

| Dependency | Present | Notes |
| --- | --- | --- |
| `DrawSprocket.h` / `libDrawSprocketLib.a` | Yes | Only needed if SDL2 fullscreen mode switching is used. |
| `InputSprocket.h` / `libInputSprocketLib.a` | Yes | Only needed if `SDL_MACOSCLASSIC_INPUTSPROCKET` is enabled. |
| `CursorDevicesGlue.o` (`universal/libppc/`) | Yes | Required link input for `libSDL2.a` per `Makefile.r68ppc`. |
| AGL / OpenGL headers | Present via Khronos/AGL decls in the fork | Only needed if `SDL_MACOSCLASSIC_AGL` / the OpenGL renderer is enabled. |

Recommendation: for the initial JA2 integration, build SDL2 with
`SDL_MACOSCLASSIC_DRAWSPROCKET` and `SDL_MACOSCLASSIC_AGL` **disabled**, and
`SDL_MACOSCLASSIC_INPUTSPROCKET` disabled as well:

- JA2 is a 2D sprite game; it needs no OpenGL/AGL renderer.
- Windowed presentation (a plain `NewCWindow`-backed `SDL_Window`) avoids
  DrawSprocket's exclusive-fullscreen context entirely, which removes a
  whole class of mode-switch failures on real hardware before they can
  happen. DrawSprocket can be revisited later purely as an optional
  fullscreen enhancement.
- Basic classic-Mac mouse tracking in `macmouse.c` does not require
  InputSprocket; that dependency is only compiled in when the define is set.

This keeps the dependency surface, code size, and RAM footprint down, which
matters for the 32 MB RAM / 603e-604 @ 100-120 MHz target.

### OS 8.6 caveat

`sdl2macos9/macos8addons/` (`MyAccessors.c`, `MyAtomic.c`,
`MyDrawSprocket.c`, `MyInputSprocket.c`) exists specifically for
pre-System-9 compatibility gaps, but the top-level `Makefile.r68ppc` has the
`macos8addons` build step commented out. Since the project's target range is
**OS 8.6 to 9.2.2**, this must be explicitly re-enabled and linked, and OS
8.6 must be tested directly — testing only on 9.x will not surface the gaps
these shims exist for. `macmouse.c` also has a `BUILDING_FOR_PRE9` code path
that needs to be enabled for 8.6 targets.

## Build Integration

`SDL-main/CMakeLists.txt` has no awareness of a `macosclassic`/RetroPPC
platform; the fork is built exclusively through its own GNU Makefiles
(`Makefile.r68ppc`) using `powerpc-apple-macos-gcc` directly. A true
native-CMake source integration (compiling SDL's `.c` files as part of the
`JA2PPC` CMake target) is therefore not realistic without upstreaming
RetroPPC support into SDL2's own build system.

The practical equivalent that still gives a single `cmake --build` entry
point is:

1. Vendor `sdl2macos9` (pinned commit) under `retro68/third_party/sdl2macos9/`.
2. Add an `ExternalProject_Add` (or `add_custom_command` driving
   `make -C SDL-main -f Makefile.r68ppc CC=... AR=... RANLIB=...`) target
   `JA2PPC_SDL2` that produces `libSDL2.a` and `libSDL2main.a`.
3. Declare `IMPORTED STATIC` targets `SDL2::SDL2` / `SDL2::SDL2main` pointing
   at those archives, with `INTERFACE_INCLUDE_DIRECTORIES` set to
   `SDL-main/include`.
4. Add `CursorDevicesGlue.o` and `libDrawSprocketLib.a` /
   `libInputSprocketLib.a` (only if those features stay enabled) to
   `target_link_libraries(JA2PPC ...)`.
5. Make the `JA2PPC` `add_application` target `DEPENDS` on `JA2PPC_SDL2` so
   incremental builds rebuild SDL2 only when its sources change.

This keeps `retro68/build-german-debug` etc. working as the single build
entry point required by `AGENTS.md`, while being honest that the SDL2 layer
is wrapped, not natively expressed in CMake.

## Color Depth Strategy (Lombard 16-bit LCD vs 32-bit VGA-out)

Keep requesting a 16-bit (`SDL_PIXELFORMAT_RGB555`) surface for all game
rendering, unconditionally, regardless of what `macvideo.c`'s `videoInit`
detects from the active `GDevice` (it currently mirrors the real depth: 16
or 32, see `macvideo.c:490-495`). Concretely:

- Game-side surfaces (menu, credits, cursor compositing, decoded intro
  frames) are always created as `SDL_PIXELFORMAT_RGB555`.
- The final present step blits that surface onto the actual
  `SDL_GetWindowSurface()` result with `SDL_BlitSurface`, which performs
  automatic pixel-format conversion when the window surface is 32-bit (VGA
  output) instead of 16-bit (Lombard LCD).

This is the direct fix for the historical Lombard bug: the current code
hand-encodes RGB555 and assumes the destination PixMap matches
(`ja2_mac.c:426-441`, `9a48012 Fix RGB555 byte order and blit origin`). SDL2's
blitter removes that entire class of manual byte-order bugs, because format
conversion is a library-level operation instead of project code.

## Cursor Handling

`macmouse.c` only exposes the classic 1-bit `SetCursor` cursor through
`SDL_ShowCursor`/`SDL_SetCursor` (`macmouse.c:513-530`); it does not add
colored-cursor support. JA2's colored cursor requirement therefore still
needs the game-owned "decode into buffer, composite with frame, restore from
backing surface" model described in `AGENTS.md`, but the target changes from
a raw window `PixMap` to the SDL back-buffer `SDL_Surface`. This removes the
`HideCursor`/`ShowCursor` balance bug class entirely (`ja2_mac.c:1892,1917`),
since the OS cursor is never shown during gameplay — only the composited
in-frame cursor is ever drawn.

## Per-Subsystem Replacement Mapping

| Current QuickDraw (`ja2_mac.c`) | SDL2 replacement |
| --- | --- |
| `InitGraf`/`InitWindows`/`WaitNextEvent` loop | `SDL_Init`, `SDL_CreateWindow`, `SDL_PollEvent` loop |
| `EnsureMenuWorld`/`EnsureCreditsWorld` (`NewGWorld(16,...)`, `ja2_mac.c:442-472`) | `SDL_CreateRGBSurfaceWithFormat(..., SDL_PIXELFORMAT_RGB555)` |
| `WriteMenuPixel` manual RGB555 packing (`ja2_mac.c:426`) | `SDL_LockSurface` + direct writes using `SDL_PixelFormat`, or drop in favor of `SDL_BlitSurface` from a decoded RGB555 source |
| `JA2BlitWorldToWindow`/`CopyBits` (`ja2_mac.c:1203-1298`) | `SDL_BlitSurface` onto `SDL_GetWindowSurface()` + `SDL_UpdateWindowSurface` |
| `JA2InstallNormalCursor`/`SetCursor` (`ja2_mac.c:1106-1198`) | Frame-composited cursor drawn into the back-buffer surface |
| `DebugLog` file logger | Unchanged; keep the existing file logger per `AGENTS.md` diagnostics rule (optionally also feed `SDL_LogXXX`) |
| `ja2_music.c` (`SndNewChannel`/`SndDoCommand`) | Out of scope for this migration — it does not use QuickDraw. Leave on direct Sound Manager calls; migrating to `SDL_OpenAudioDevice` (the fork's `SDL_macaudio.c` also wraps `SndPlayDoubleBuffer`) is an optional future consistency pass, not required to fix the QuickDraw tedium problem. |
| `ja2/Build/Intro.c` + `ja2/Build/Utils/Cinematics.c` (QuickTime Movie Toolbox into a private GWorld) | Movie decode/lifecycle ownership stays as-is (QuickTime, not QuickDraw-window code); only the final per-frame present step changes to copy the movie GWorld's pixels into the SDL back-buffer surface before `SDL_UpdateWindowSurface`, so there is one presentation path for the whole app. |

## Existing Path-Convention Note (pre-existing, unrelated to this migration)

`retro68/ja2/Build/Intro.c` and `retro68/ja2/Build/Utils/Cinematics.c`
currently live under a `ja2/Build/` prefix beneath `retro68/`, which is the
exact layout `AGENTS.md`'s porting rule and this repo's own
`JA2_PPC_VIDEO_BACKEND_PLAN.md` (line 79-81) say not to use — the correct
path is `retro68/Utils/Cinematics.c`. This is a pre-existing inconsistency,
not something introduced by this plan. It is called out here because new
SDL2-era platform files must follow the documented rule
(`retro68/<original-relative-path>` for ports of original files,
`retro68/ja2_<name>.c` for narrowly-scoped new platform files), not the
current `Intro.c`/`Cinematics.c` location. Correcting the existing two files
is a separate, explicit cleanup task and is out of scope here.

## Risks

- `macvideo.c:493-495` hard-fails `SDL_Init` if the active `GDevice` pixel
  depth is neither 16 nor 32. This should be checked with `Gestalt`/GDevice
  inspection before calling `SDL_Init`, with a clear diagnostic, per the
  `AGENTS.md` capability-check rule — do not let an unsupported depth surface
  as a generic SDL error.
- OS 8.6 support depends on the currently-disabled `macos8addons` shim
  library and the `BUILDING_FOR_PRE9` code path; both need explicit build
  and hardware verification, not just code review.
- Static-linking all of SDL2's core, even with AGL/DrawSprocket/InputSprocket
  disabled, still adds audio/render/joystick/event code the game doesn't use.
  Keep `-ffunction-sections`/`-Wl,-gc-sections` (already used for `JA2PPC`)
  and verify the final bundle stays within the 32 MB RAM / disk budget after
  each stage.
- `ExternalProject_Add`-wrapped Makefile builds do not automatically track
  header-level incremental changes as precisely as a native CMake target;
  confirm `retro68/build-german-debug --clean-first` is used when SDL2 fork
  sources change, to avoid stale-object false negatives during hardware
  testing.
- Rewriting `ja2_mac.c`'s event loop is a full replacement of the only
  currently-working, hardware-verified rendering path (menu, splash,
  credits, new-game options). Follow the staged order below so a hardware
  regression is always attributable to one specific, small change.

## Implementation Stages

1. Vendor `sdl2macos9`, wrap `Makefile.r68ppc` in CMake (`JA2PPC_SDL2`
   target), disable DrawSprocket/AGL/InputSprocket, build `libSDL2.a` /
   `libSDL2main.a` with no JA2 code changes yet.
2. Minimal smoke-test `APPL`: `SDL_Init(SDL_INIT_VIDEO)`, `SDL_CreateWindow`
   at the current menu resolution, force an `SDL_PIXELFORMAT_RGB555`
   surface, solid-color clear, `SDL_PollEvent` loop with close-box quit.
   Deploy via `LaunchAPPL` to at least one 16-bit-only Mac and the Lombard
   (LCD 16-bit / VGA-out 32-bit) to validate the fixed-depth + blit-convert
   approach before porting any real rendering code.
3. Port startup splash + main menu background/buttons
   (`RenderStartupSplash`, `RenderMainMenuBackground`,
   `RenderMenuButtonFrames`) to blit their already-decoded RGB555 pixel data
   into `SDL_Surface`s instead of `GWorld`s. The STCI/ETRLE decode code itself
   does not change, only its output target.
4. Port cursor handling to the frame-composited model; remove
   `HideCursor`/`ShowCursor` calls entirely.
5. Port the credits screen and the new-game options screen
   (`ja2_game_init_options.c`, `ja2_new_game_init.c` presentation calls).
6. Retarget `Intro.c`/`Cinematics.c`'s per-frame present step to the SDL back
   buffer (movie decode/lifecycle ownership unchanged).
7. Remove the now-unused QuickDraw window/event code from `ja2_mac.c` once
   each stage above is hardware-verified — this is where the "vollständig
   ersetzen" decision is realized, without a single unverifiable rewrite.
8. Re-run the full main-menu-to-new-game flow on an explicit OS 8.6 install,
   decide on `macos8addons`/`BUILDING_FOR_PRE9` linkage based on the result.

## Verification (per `AGENTS.md`)

- Build `retro68/build-german-debug` after every stage.
- Deploy with `LaunchAPPL` and download/inspect `JA2PPC-Debug.log` after each
  run; delete the remote log before every run.
- Use the Elgato Cam Link capture procedure for every stage that touches
  presentation on the Lombard, since that is the exact hardware combination
  (16-bit LCD, 32-bit VGA-out) that exposed the original QuickDraw bug this
  migration is partly meant to fix.
- Test stage 8 on a real OS 8.6 install, not only 9.x, before considering the
  migration complete.
