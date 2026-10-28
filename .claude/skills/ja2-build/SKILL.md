---
name: ja2-build
description: Configure and build the JA2 PPC Retro68 targets (German/English, debug/release) and locate the deployable JA2PPC.bin, .dsk and bundle. Use before any hardware or emulator test, after every feature change, and when switching toolchains.
---

# Building JA2 PPC

All paths relative to the repo root `<repo root>`.

## Toolchain

| Toolchain | Path |
|---|---|
| Current (2026-10 rebuild, universal interfaces) | `$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake` |
| Fallback (2026-09) | `$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake` |

Use the current toolchain once `$RETRO68_BUILD/build.log` ends with
`EXIT=0`. A build dir is bound to the toolchain it was configured with;
switching toolchain means a fresh build dir (or delete `CMakeCache.txt`).

Data: `$JA2_DATA_DIR` (needs `Ja2.ini` + `Data/`).

## Configurations

| Dir | Flags |
|---|---|
| `retro68/build-german-debug` (default for validation) | `-DJA2_LANGUAGE=german -DJA2_DEBUG_BUILD=ON` |
| `retro68/build-german-release` | `-DJA2_LANGUAGE=german -DJA2_DEBUG_BUILD=OFF` |
| `retro68/build-english-debug` | `-DJA2_LANGUAGE=english -DJA2_DEBUG_BUILD=ON` |
| `retro68/build-profile` | German + `-DJA2_PROFILE=ON` (see `ja2-profile` skill) |

```sh
TC=$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
cmake -S retro68 -B retro68/build-german-debug -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$TC \
  -DJA2_DATA_DIR="$JA2_DATA_DIR" \
  -DJA2_LANGUAGE=german -DJA2_DEBUG_BUILD=ON
cmake --build retro68/build-german-debug
```

`tools/build-german-debug.sh all` does the same with the fallback toolchain
unless `RETRO68_TOOLCHAIN_FILE` is set.

Avoid `--clean-first` unless needed: it re-runs the intro video conversion
(FFmpeg) and re-copies the full `Data/` tree.

## Outputs

- `JA2PPC.bin` — MacBinary; the file to pass to LaunchAPPL / `execute_binary`.
- `JA2PPC.dsk` — HFS image for emulators.
- `JA2PPC Bundle/` — app + `Ja2.ini` + `Data/` + `options.cfg`.
- `JA2PPC.APPL` — raw data fork only; never transfer it on its own.

## Checks after a build

- The build must finish without new warnings in touched files.
- `JA2PPC.bin` timestamp must be newer than the edited sources.
- Never commit build dirs, GOG data, logs or emulator images.
