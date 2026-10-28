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

Machine-specific paths are kept out of the repository. Copy
`local.env.example` to `local.env` (git-ignored) and set the Retro68
toolchain root, the JA2 data directory, and the emulator paths there; the
scripts in `tools/` read it automatically. The examples below use these
variables, for example the toolchain file:

```text
$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
```

Run `. tools/local-env.sh` in your shell to load them for manual commands.

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

## DingusPPC Debug Workflow

The repository's `opencode.json` identifies the local DingusPPC checkout. Use
the wrapper below to configure and build the German debug target, deploy the
bundle to `.dingusppc/deploy/`, validate the emulator installation, and run a
bounded emulator smoke test when a bootable Mac OS disk is configured:

```sh
JA2_DATA_DIR="/path/to/Jagged Alliance 2" \
DINGUS_HDD_IMAGE="/path/to/System-9.dsk" \
DINGUS_BOOTROM="$DINGUS_ROOT/build/bin/1997-11 - 78F57389 - Power Mac G3 (v3).ROM" \
./tools/dingusppc-ja2.sh all
```

`DINGUS_BOOTROM` is optional when exactly one ROM is present in the configured
DingusPPC `build/bin` directory. `DINGUS_HDD_IMAGE` must be a bootable Mac OS
image because DingusPPC cannot boot a host directory or a raw `.APPL` file.
The generated `JA2PPC.dsk` and complete `JA2PPC Bundle` are staged for
transfer, but the wrapper does not modify the supplied OS disk image. Use
`build`, `deploy`, `test`, or `run` for individual steps. The emulator stops
after 30 seconds by default; override this with `DINGUS_TIMEOUT`.

## Real PowerBook G3 Workflow

The repository includes `.mcp.json` and a machine configuration template for
the `classic-mac-hardware-mcp` deployment/logging workflow. See
`CLASSIC_MAC_HARDWARE_MCP.md` for modern-machine setup, RumpusFTP and
LaunchAPPLServer installation on the G3, network safety, and the exact run
sequence. This provides remote deployment and log retrieval, not source-level
breakpoints or register debugging.

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
