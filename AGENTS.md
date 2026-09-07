# JA2 PPC Development Instructions

These instructions apply to every interaction that changes or evaluates the
JA2 PPC codebase.

## Repository

Use the private JA2 PPC repository and no other remote for this port:

```text
https://github.com/bertibertig/ja2-ppc
```

Create issues, push branches, and open pull requests only against this
repository. Do not use the upstream `dariusk/ja2` repository for port work.

## Toolchain And Target Platform

Use Retro68 (`powerpc-apple-macos` toolchain) as the only compiler for the
PPC target, and use Retro68's `LaunchAPPL` (via the `classic-mac-hardware`
MCP tools) as the only way to run and debug builds on real hardware. Do not
introduce a second compiler or a second deployment path for the PPC target.

The supported OS range is **Mac OS 8.6 through 9.2.2**. Do not assume a
System-9-only Toolbox surface: any API, compatibility shim, or build option
that is version-gated (for example `BUILDING_FOR_PRE9`, or optional
`DrawSprocket`/`InputSprocket` availability checks) must be exercised on an
8.6 install, not only validated on 9.x.

Hardware target: PowerPC 603e or 604 at 100-120 MHz, 32 MB RAM, SVGA
(800x600). This is comparable to the original x86 requirements (Windows
95/98, Pentium 133 MHz, 32 MB RAM, SVGA, 373 MB free disk space). See
"Rendering And Performance" below for the display-depth policy that follows
from real hardware differing in supported color depth.

## Porting Rule

The PPC port must preserve the original x86 source organization 1:1, but the
PPC implementation tree is rooted at `retro68/`. Do not mirror the original
repository's `ja2/Build/` path beneath `retro68/`.

- Keep the original file names and subsystem boundaries. Map the original
  source path relative to `ja2/Build/` into the corresponding path under
  `retro68/`. For example, `ja2/Build/Utils/Cinematics.c` belongs at
  `retro68/Utils/Cinematics.c`, and `ja2/Build/Intro.c` belongs at
  `retro68/Intro.c`.
- Do not place a reimplementation at a path such as
  `retro68/ja2/Build/Utils/Cinematics.c` merely to match the original tree.
  (`retro68/ja2/Build/Intro.c` and `retro68/ja2/Build/Utils/Cinematics.c`
  currently violate this rule as a pre-existing exception; do not copy that
  layout for new files, and treat correcting those two paths as a separate,
  explicit cleanup task.)
- Port code into the PPC counterpart of the original source file instead of
  accumulating game logic in a central platform or shell file.
- Preserve the original ownership of state, call flow, and lifecycle functions.
- Add a PPC source file only when there is no suitable original counterpart;
  document the platform-specific reason and keep the file narrowly scoped.
- Keep platform-specific Mac OS 9, SDL2/QuickDraw, Sound Manager, file, and
  event code behind the smallest practical platform interface.

## Original Source Requirement

Before implementing, translating, or modifying any feature, always reference
the original x86 source code first. The original source tree is located at:

```text
../ja2-master/ja2/Build/
```

From the repository root this resolves to:

```text
../ja2-master/ja2/Build/
```

Trace the original entry points, state transitions, assets, constants, and
error handling before writing the PPC version. Use the matching original file
as the primary porting location and compare behavior against it after changes.

## Mac OS 9 Documentation

The authoritative local Apple and Inside Macintosh documentation is located
at:

```text
resources/os9docs-master/
```

The project-specific condensed reference is:

```text
MAC_OS9_GAME_DEVELOPMENT.md
```

Consult these references before using Toolbox APIs or relying on classic Mac
behavior, especially for QuickDraw, Event Manager, Memory Manager, Resource
Manager, File Manager, Sound Manager, timing, callbacks, resource forks,
Pascal strings, and 32-bit PowerPC ABI details.

## Rendering Backend (SDL2)

The platform shell renders through SDL2, using the
[`laulandn/sdl2macos9`](https://github.com/laulandn/sdl2macos9) fork (the
only SDL2 fork targeting classic Mac OS 7/8/9 m68k/PPC via Retro68), not
directly through QuickDraw. See `retro68/JA2_PPC_SDL2_MIGRATION_PLAN.md` for
the full migration plan, dependency audit, build integration approach, and
staged replacement order; consult it before touching `retro68/ja2_mac.c`'s
rendering or event-loop code.

- SDL2 on classic Mac OS does not replace QuickDraw; the backend
  (`SDL-main/src/video/macosclassic/`) is implemented on top of `NewCWindow`,
  `GWorld`/`PixMap`, `CopyBits`, and `WaitNextEvent`. Treat SDL2 as the
  required game-code rendering interface (`SDL_Surface`, `SDL_BlitSurface`,
  `SDL_UpdateWindowSurface`, `SDL_PollEvent`); do not add new direct
  QuickDraw GWorld/CopyBits call sites in game code once a screen has been
  migrated. The QuickDraw-specific guidance in "Rendering And Performance"
  below still applies to the SDL2 backend fork itself and to the QuickTime
  Movie Toolbox GWorld interop in `Cinematics.c`.
- Color depth is fixed at 16-bit `SDL_PIXELFORMAT_RGB555` for every game
  surface (menu, credits, cursor compositing, decoded intro frames),
  regardless of the actual display depth SDL2 detects from the active
  `GDevice` (16 or 32-bit). Only the final present step
  (`SDL_BlitSurface` onto `SDL_GetWindowSurface()`) may convert to the real
  display format; do not hand-roll RGB555 byte-order/packing code in game
  code. This is the documented fix for the PowerBook G3 Lombard mismatch
  (16-bit LCD panel, 32-bit VGA output).
- Vendor the SDL2 fork under `retro68/` and build it as a wrapped external
  target (`ExternalProject_Add`/`add_custom_command` driving its own
  `Makefile.r68ppc`), since its CMake files have no RetroPPC/`macosclassic`
  support. Import the resulting `libSDL2.a`/`libSDL2main.a` as `IMPORTED`
  CMake targets rather than compiling SDL2 sources directly.
- Disable `SDL_MACOSCLASSIC_DRAWSPROCKET`, `SDL_MACOSCLASSIC_AGL`, and
  `SDL_MACOSCLASSIC_INPUTSPROCKET` unless a specific feature explicitly
  needs fullscreen mode switching, OpenGL, or joystick input. JA2 is a 2D
  sprite game and should run windowed to avoid DrawSprocket's exclusive
  fullscreen context entirely.
- The fork's `macos8addons/` compatibility shims and its
  `BUILDING_FOR_PRE9` code path exist for the 8.6-9.2.2 target range in
  "Toolchain And Target Platform" above; keep them enabled and verify them
  on an 8.6 install rather than assuming 9.x coverage is sufficient.
- Replace QuickDraw window/event code in verified stages (window/event loop,
  then menu/splash, then cursor, then credits/options, then cinematics
  presentation) per the migration plan; do not attempt a single
  unverifiable rewrite of `ja2_mac.c`.

## Rendering And Performance

The PPC port must remain practical on a PowerPC 603e or 604 running at
100-120 MHz and 32 MB of RAM with SVGA (800x600). This target is comparable
to the original x86 requirements:

- Windows 95 or 98
- Intel Pentium 133 MHz
- 32 MB RAM
- SVGA graphics adapter
- 373 MB free disk space

The following QuickDraw-level guidance applies to the SDL2 backend fork's
internals and to the Movie Toolbox GWorld interop in `Cinematics.c`, not to
new game-code rendering call sites — see "Rendering Backend (SDL2)" above
for the game-code interface and the fixed 16-bit color-depth policy.

Prefer the smallest redraw and decode work that preserves original behavior.
Avoid repeatedly opening archives, decoding full-screen STCI images, or
allocating frame-sized buffers in response to hover, checkbox, or button
changes. Decode static backgrounds once into an off-screen GWorld and use the
cache for the screen's lifetime. Render only the affected controls after the
first presentation.

On the System 9/Infinite Mac path, a full GWorld-to-window `CopyBits` works
for initial presentation, but clipped or small window `CopyBits` redraws can
corrupt the destination to white even when the source GWorld is valid. Do not
use clipped window `CopyBits` to restore checkbox or button regions. Instead,
redraw the changed control locally with an opaque backing that matches its
panel before rendering its frame and selected state. Keep cached GWorld pixels
immutable after they are decoded and prepared.

After any GWorld blit, explicitly select the destination color window with
`SetGWorld((CGrafPtr)gWindow, GetMainDevice())` before drawing QuickDraw text
or controls. Use a transparent text transfer mode such as `TextMode(srcOr)`
for overlays, restore the expected transfer mode afterward, and reset the
foreground color after helpers that change it. Add diagnostics for cache
creation, archive decode, GWorld/window pixel samples, blits, redraw paths,
and Toolbox allocation or locking failures when adding a new screen.

Cursor handling follows the documented Cursor Manager lifecycle. Call
`InitCursor` once when establishing the normal system cursor. Every
`HideCursor` call must have exactly one matching `ShowCursor`; do not hide the
cursor repeatedly from a frame or mouse-poll function. `SetCursor` accepts the
classic one-bit `Cursor` structure and cannot reproduce a red game cursor.
For a colored JA2 cursor, follow the original game's mouse-buffer model:
decode the cursor sprite into an off-screen cursor buffer, composite it with
the frame, and restore the previous cursor area from a valid backing surface.
Do not paint a software cursor directly into the window and do not restore it
by raw writes into a window PixMap. Consult the local `Quickdraw.h` Cursor
Manager documentation and `pdf/animated_cursor.pdf` before changing cursor
ownership or animation.

## Verification

- Build the affected Retro68 target after each feature change.
- Prefer the German debug configuration for current PPC validation:
  `retro68/build-german-debug/` with `JA2_LANGUAGE=german` and
  `JA2_DEBUG_BUILD=ON`.
- After running a debug build on the original Mac through LaunchAPPL, use the
  `classic-mac-hardware` MCP `download_file` tool to retrieve
  `JA2PPC-Debug.log` from the Mac before diagnosing the result. Inspect the
  downloaded log for startup, asset lookup, rendering, input, and shutdown
  diagnostics. The log is written beside the application by debug builds.
- Before every debug deployment and LaunchAPPL run, delete the existing remote
  `JA2PPC-Debug.log` so each run starts with a bounded, unambiguous log. Do not
  diagnose from an appended log containing earlier runs.

### Cam Link Visual Inspection

When a PPC Macintosh is connected to the host through an Elgato Cam Link 4K,
use the capture device to inspect the actual display output rather than relying
only on LaunchAPPL status or debug logs. This applies to the PowerBook G3 and
any other connected PPC Macintosh.

1. Identify the capture node and supported formats:

   ```sh
   v4l2-ctl --list-devices
   v4l2-ctl --all -d /dev/video0
   ```

   The Cam Link commonly exposes `/dev/video0` for video and `/dev/video1` for
   metadata, but always use the node reported by `v4l2-ctl`.

2. Confirm that the video node is not already owned by OBS or another capture
   application:

   ```sh
   fuser -v /dev/video0
   ```

   Do not terminate an unrelated capture process. Have it release the device
   before starting a direct `ffmpeg` capture.

3. Start `ffmpeg` and LaunchAPPL at the same time so the recording includes the
   startup, splash, menus, and any user-selected screen transitions:

   ```sh
   ffmpeg -y -f v4l2 -framerate 30 -video_size 1024x768 \
     -i /dev/video0 -t 30 -an -c:v libx264 -preset ultrafast -crf 18 \
     /tmp/opencode/powerbook-capture.mp4
   ```

   Run the capture command in parallel with
   `classic-mac-hardware_execute_binary`, using the PPC binary and machine under
   test. Adjust `-t` when a longer interaction sequence is required.

4. Extract inspection frames from the recording without changing the source
   application output:

   ```sh
   mkdir -p /tmp/opencode/capture-frames
   ffmpeg -y -i /tmp/opencode/powerbook-capture.mp4 -vf fps=1/5 \
     /tmp/opencode/capture-frames/frame-%02d.png
   ```

   Read the PNG frames to inspect splash-to-menu transitions, mirrored-display
   output, video frames, Credits, New Game, and other affected screens. Compare
   visible output with the downloaded `JA2PPC-Debug.log` timestamps and state
   transitions.

- Add diagnostic logging for feature entry, state transitions, asset lookup,
  allocation, decoding, Toolbox errors, and failure or fallback paths.
- Keep generated build artifacts out of source changes unless explicitly
  requested.
- Do not claim a migrated subsystem is complete when the PPC implementation is
  still a placeholder; expose the boundary and its status in diagnostics.
