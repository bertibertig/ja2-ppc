# JA2 PPC Mac target

This directory contains the first native platform slice for a System 9
PowerPC port. It builds a classic `APPL` using Retro68's
`powerpc-apple-macos` toolchain and replaces the original Win32/DirectDraw
window and event loop with QuickDraw and `WaitNextEvent`.

The PPC sources are kept in the same subsystem boundaries as the original
source where they have been migrated: `ja2_mac.c` contains the platform shell
and event loop, `ja2_game_init_options.c` contains the game-start options
screen, and `ja2_new_game_init.c` contains the first PPC `InitNewGame(FALSE)`
boundary. Their headers expose only the small interfaces needed between these
subsystems.

The checked-in game source is not yet a complete Mac port. The historical
source calls Win32, DirectDraw, DirectSound, Miles Sound System, registry,
and other APIs throughout the SGP and game modules. The original game data
files are also not part of this repository. Consequently, this target is a
runnable native platform shell, not a claim that the full game has already
been migrated.

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
the same true-value syntax is accepted for enabling it. The measured value and
the active setting are recorded in `JA2PPC-Debug.log`.

When `JA2_DATA_DIR` is configured, the build extracts the four beginning-game
Smacker files from `Data/Intro.slf` and converts them to cached 640x480 MOV
files under `Data/Intro/` in the bundle. FFmpeg is required for this step.
The cache is reused by normal incremental builds; use `cmake --build
retro68/build-german-debug --clean-first` only when a deliberate full rebuild
is needed.

## Remaining port work

- Replace DirectDraw surfaces with the QuickDraw GWorld/frame-buffer path.
- Replace Win32 input, timers, file paths, and registry settings.
- Replace DirectSound/Miles audio and Smacker video playback.
- Remove Win32 types from shared headers and fix 32-bit PPC ABI assumptions.
- Supply the separately licensed JA2 data files and test on real System 9
  hardware or an emulator.
