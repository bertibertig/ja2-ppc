# JA2 PPC Mac target

This directory contains the Mac OS 8.6-9.2.2 PowerPC port of JA2, built
with Retro68's `powerpc-apple-macos` toolchain. The original game sources
are copied here at their mapped paths (`ja2/Build/X` -> `retro68/X`) and
compiled with changes behind `#ifdef JA2_PPC`, on a Mac reimplementation of
the Standard Gaming Platform (`Standard Gaming Platform/`, with the
`sgp_*.c` platform files). Modules not ported yet are logged stand-ins in
`ja2_ppc_unported.c`. See `../AGENTS.md` ("Port Architecture") for the
details. The game data is not part of this repository.

## Build

From a Retro68 build environment:

```sh
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
make
```

For runtime diagnostics, add `-DJA2_DEBUG_BUILD=ON`. The debug application
opens a separate `JA2 Debug` window and writes `JA2PPC-Debug.log` beside the
application. The log records archive lookup, STCI header, image allocation,
and ETRLE decoding failures.

To bundle a legally installed GOG data set, configure the installation root
and language. The root must contain `Ja2.ini` and `Data/`:

```sh
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake \
  -DJA2_DATA_DIR="$JA2_DATA_DIR" \
  -DJA2_LANGUAGE=german
make
```

German is the default. Use `-DJA2_LANGUAGE=english` only when an English
bundle is specifically needed. The build creates
`JA2PPC Bundle/`, containing `JA2PPC.bin`, `JA2PPC.APPL`, `Ja2.ini`, and the required
archives and loose files under `Data/`. The complete `Data/` directory is
copied so that files outside the SLF archives are retained as well. The
bundle is the directory to transfer to System 9. English builds omit the
language archive; German builds include `German.slf` and define the German
language compile flag.

The generated `JA2PPC.bin` can be copied to a PowerPC Mac running System 9.
Use `JA2PPC.bin` or `JA2PPC.dsk` for InfiniteMac and other transfers that do
not preserve classic Mac resource forks. `JA2PPC.APPL` is the raw PPC data
fork and is not a safe standalone transfer format. The same binary can be
launched with Retro68's `LaunchAPPL -e classic` on a suitable host. For the
data probe, copy the game's `Data` folder beside the application, preserving
the archive names.
The window reports the number of required `.slf` archives it can open.
It also reads the little-endian `Data.slf` directory on PPC without relying
on x86 struct layout and reports the number of valid indexed entries.

## New Game test slice

In the German debug build, select `Neues Spiel` from the main menu. The PPC
shell follows the first-run branch of the original `InitNewGame(FALSE)` flow
through its `LAPTOP_SCREEN` transition: it records the laptop and strategic
initialization boundary, queues the five opening emails, selects medium
difficulty with 35,000 starting cash, selects A9 Omerta, sets the screen-count
transition to 1, and marks the campaign started. The first Laptop slice now
composes the SDL2 RGB555 screen from the original `Laptop.slf` and
`Interface.slf` assets using the original 640x480 coordinates. It
shows the laptop frame, desktop, taskbar, navigation buttons, lights, status
values, and the initial Welcome help overlay without the legacy FPS counter.

The laptop program pages, email records, strategic subsystem, and full original
help text database remain future migration work. The active boundary and asset
fallbacks are explicitly reported in the diagnostics.

The debug build writes `JA2PPC-Debug.log` beside the application and mirrors
the messages in the `JA2 Debug` window. For a New Game failure, capture the
log from `German main menu: new game selected` through the final `New Game:`
phase message. The log includes the selected difficulty, starting cash,
opening-email count, initial sector, game-started flag, and transition target.

## Runtime options

`options.cfg` is copied beside the application in the generated bundle. Set
`skip_intros=1` to skip the four intro videos during debugging. The accepted
true values are `1`, `true`, `yes`, and `on`; the default is `0`, preserving
the normal intro flow. The option is loaded once at startup and recorded in
`JA2PPC-Debug.log`.

The runtime FPS overlay is enabled by default. Set `show_fps=0` to disable it;
the same true-value syntax is accepted for enabling it. The value counts
successful window presents in the current shell, not simulation or tactical
game frames. The measured value and the active setting are recorded in
`JA2PPC-Debug.log`.

The intro movies are played directly from the original `Data/Intro.slf`
Smacker files by `Smacker/smacker.c` (native 15 fps, original audio); no host
conversion step and no QuickTime are involved.

## Remaining port work

- Replace DirectDraw surfaces with the QuickDraw GWorld/frame-buffer path.
- Replace Win32 input, timers, file paths, and registry settings.
- Replace DirectSound/Miles audio and Smacker video playback.
- Remove Win32 types from shared headers and fix 32-bit PPC ABI assumptions.
- Supply the separately licensed JA2 data files and test on real System 9
  hardware or an emulator.
