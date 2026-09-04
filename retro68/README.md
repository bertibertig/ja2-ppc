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
shell now performs the first-run portion of the original `InitNewGame(FALSE)`
flow: it records the laptop and strategic initialization boundary, sets the
new-game flag, queues the five opening emails, selects medium difficulty with
35,000 starting cash, selects A9 Omerta, and marks the campaign started. The
screen displays these values as a testable result. The actual laptop and
strategic subsystems remain future migration work and are explicitly reported
as such in the diagnostics.

The debug build writes `JA2PPC-Debug.log` beside the application and mirrors
the messages in the `JA2 Debug` window. For a New Game failure, capture the
log from `German main menu: new game selected` through the final `New Game:`
phase message. The log includes the selected difficulty, starting cash,
opening-email count, initial sector, game-started flag, and transition target.

## Remaining port work

- Replace DirectDraw surfaces with the QuickDraw GWorld/frame-buffer path.
- Replace Win32 input, timers, file paths, and registry settings.
- Replace DirectSound/Miles audio and Smacker video playback.
- Remove Win32 types from shared headers and fix 32-bit PPC ABI assumptions.
- Supply the separately licensed JA2 data files and test on real System 9
  hardware or an emulator.
