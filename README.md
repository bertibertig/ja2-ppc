# Jagged Alliance 2 PPC Mac Port

This repository is focused on porting the historical Jagged Alliance 2 source
to a native classic PowerPC Macintosh application for Mac OS 9. The target is
built with [Retro68](https://github.com/autc04/Retro68), using InterfaceLib,
QuickDraw, and the classic PowerPC PEF format.

The original source is a VC6 Win32/DirectX project. The Mac target is being
migrated incrementally from the original Standard Gaming Platform layer. The
current target provides a native PPC application shell, German-focused data
bundling, SLF archive inspection, and initial STCI/ETRLE image support.

## Port Attribution

This port is being implemented with assistance from an AI coding agent. The
AI-generated changes are reviewed and tested in the project environment, but
the port is experimental and is not an official Strategy First or Jagged
Alliance 2 release.

## Source and Data

The historical source is included for educational purposes under the terms of
`SFI Source Code license agreement.txt`.

The original JA2 game data is not included in this repository. Use a legally
owned installation, such as the GOG release. The build expects the installation
root to contain `Ja2.ini` and `Data/`. Do not commit or redistribute the
copyrighted data archives.

## Retro68 Prerequisites

- Retro68 with the `powerpc-apple-macos` target built
- CMake 3.9 or later
- GNU Make
- A legally owned JA2 installation for data bundling
- InfiniteMac, SheepShaver, or a Power Macintosh running Mac OS 9 for testing

The examples below use this local toolchain:

```text
$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
```

Change the path if Retro68 is installed elsewhere.

## German Debug Build

German is the default language and diagnostics are enabled by default. This
build opens a separate `JA2 Debug` window and writes `JA2PPC-Debug.log` beside
the application when run on the Mac.

```sh
cmake -S retro68 -B retro68/build-german-debug \
  -DCMAKE_TOOLCHAIN_FILE=$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake \
  -DJA2_DATA_DIR="$JA2_DATA_DIR" \
  -DJA2_LANGUAGE=german \
  -DJA2_DEBUG_BUILD=ON
cmake --build retro68/build-german-debug --clean-first
```

## German Release Build

The release build keeps the German language but disables the diagnostics
window and log:

```sh
cmake -S retro68 -B retro68/build-german-release \
  -DCMAKE_TOOLCHAIN_FILE=$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake \
  -DJA2_DATA_DIR="$JA2_DATA_DIR" \
  -DJA2_LANGUAGE=german \
  -DJA2_DEBUG_BUILD=OFF
cmake --build retro68/build-german-release --clean-first
```

## English Debug Build

English is available explicitly for comparison testing:

```sh
cmake -S retro68 -B retro68/build-english-debug \
  -DCMAKE_TOOLCHAIN_FILE=$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake \
  -DJA2_DATA_DIR="$JA2_DATA_DIR" \
  -DJA2_LANGUAGE=english \
  -DJA2_DEBUG_BUILD=ON
cmake --build retro68/build-english-debug --clean-first
```

## English Release Build

```sh
cmake -S retro68 -B retro68/build-english-release \
  -DCMAKE_TOOLCHAIN_FILE=$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake \
  -DJA2_DATA_DIR="$JA2_DATA_DIR" \
  -DJA2_LANGUAGE=english \
  -DJA2_DEBUG_BUILD=OFF
cmake --build retro68/build-english-release --clean-first
```

## Build Results

Each data-enabled build creates:

```text
retro68/<build-directory>/JA2PPC.bin
retro68/<build-directory>/JA2PPC.APPL
retro68/<build-directory>/JA2PPC.dsk
retro68/<build-directory>/JA2PPC Bundle/
```

Use `JA2PPC.bin` or `JA2PPC.dsk` with InfiniteMac. The raw `.APPL` file is a
PPC data fork and may fail when transferred without its classic Mac resource
metadata. The `JA2PPC Bundle` contains the application, `Ja2.ini`, and the
complete selected `Data/` tree for deployment.

## Current Port Status

The current application is not yet the complete game. Remaining work includes:

- General portable SLF file lookup and streaming reads
- Complete STCI image decoding and QuickDraw rendering
- Intro SMK/Smacker video playback and skipping
- Startup state sequencing from initialization through the intro
- German main-menu text and button rendering
- Main-menu mouse and keyboard handling
- Audio, timers, file paths, save games, and remaining Win32 dependencies

The original VC6 projects remain available under `ja2/Build/` for reference.
