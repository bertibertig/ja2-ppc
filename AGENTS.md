# JA2 PPC Development Instructions

These instructions apply to every interaction that changes or evaluates the
JA2 PPC codebase.

## Porting Rule

The PPC port must preserve the original x86 source organization 1:1.

- Keep the original directory hierarchy, file names, and subsystem boundaries.
- Port code into the PPC counterpart of the original source file instead of
  accumulating game logic in a central platform or shell file.
- Preserve the original ownership of state, call flow, and lifecycle functions.
- Add a PPC source file only when there is no suitable original counterpart;
  document the platform-specific reason and keep the file narrowly scoped.
- Keep platform-specific Mac OS 9, QuickDraw, Sound Manager, file, and event
  code behind the smallest practical platform interface.

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

## Rendering And Performance

The PPC port must remain practical on a PowerPC 603e or 604 running at
100-120 MHz and 32 MB of RAM with SVGA (800x600) with 32-bit colors (Millions of colors). This target is comparable to the original x86 requirements:

- Windows 95 or 98
- Intel Pentium 133 MHz
- 32 MB RAM
- SVGA graphics adapter
- 373 MB free disk space

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

## Verification

- Build the affected Retro68 target after each feature change.
- Prefer the German debug configuration for current PPC validation:
  `retro68/build-german-debug/` with `JA2_LANGUAGE=german` and
  `JA2_DEBUG_BUILD=ON`.
- Add diagnostic logging for feature entry, state transitions, asset lookup,
  allocation, decoding, Toolbox errors, and failure or fallback paths.
- Keep generated build artifacts out of source changes unless explicitly
  requested.
- Do not claim a migrated subsystem is complete when the PPC implementation is
  still a placeholder; expose the boundary and its status in diagnostics.
