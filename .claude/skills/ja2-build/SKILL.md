---
name: ja2-build
description: Configure and build the JA2 PPC Retro68 targets (German/English, debug/release) and locate the deployable JA2PPC.bin, .dsk and bundle. Use before any hardware or emulator test, after every feature change, and when switching toolchains.
---

# Building JA2 PPC

All paths relative to the repo root. Machine-specific paths (toolchain, game
data, emulator images) live in the git-ignored `local.env` (template:
`local.env.example`); read it for the actual values and never write them
into tracked files.

## Toolchain

`RETRO68_TOOLCHAIN` in `local.env` is the toolchain root (the 2026-10
rebuild with universal interfaces); the toolchain file is
`$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake`. An
older fallback toolchain is noted in `local.env`. A build dir is bound to the toolchain it was configured with;
switching toolchain means a fresh build dir (or delete `CMakeCache.txt`).

The current toolchain is Retro68 `0cbd3cb` (upstream head as of
2026-10-28) with GCC 16.1. Updating it: `git -C ~/Retro68 pull`, then in
the toolchain's build dir run `../Retro68/build-toolchain.bash --universal
--ninja` (add `--skip-thirdparty` when only Retro68's own tools changed;
needs the `boost` package). Rebuild JA2 in fresh build dirs afterwards.

SDL2 (`retro68/third_party/sdl2macos9`, submodule) is copied into each
build dir (`<build>/sdl2macos9`) and built there with `-O2`, so build dirs
never share SDL objects. Port-local fixes to the fork live as patches in
`retro68/patches/sdl2macos9/` and are applied to that copy on every build;
never edit the submodule. `-DJA2_SDL2_DEBUG=ON` re-enables the fork's
`MAC_DEBUG` stderr tracing (slow: file I/O on every event poll).

Data: `$JA2_DATA_DIR` from `local.env` (needs `Ja2.ini` + `Data/`).

## Configurations

| Dir | Flags |
|---|---|
| `retro68/build-german-debug` (default for validation) | `-DJA2_LANGUAGE=german -DJA2_DEBUG_BUILD=ON` |
| `retro68/build-german-release` | `-DJA2_LANGUAGE=german -DJA2_DEBUG_BUILD=OFF` |
| `retro68/build-english-debug` | `-DJA2_LANGUAGE=english -DJA2_DEBUG_BUILD=ON` |
| `retro68/build-profile` | German + `-DJA2_PROFILE=ON` (see `ja2-profile` skill) |

```sh
. tools/local-env.sh
TC=$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
cmake -S retro68 -B retro68/build-german-debug -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$TC \
  -DJA2_DATA_DIR="$JA2_DATA_DIR" \
  -DJA2_LANGUAGE=german -DJA2_DEBUG_BUILD=ON
cmake --build retro68/build-german-debug
```

`tools/build-german-debug.sh all` does the same (it reads `local.env`;
`RETRO68_TOOLCHAIN_FILE` overrides the toolchain file).

Avoid `--clean-first` unless needed: it re-copies the full `Data/` tree.

Fast logic check on the host (after touching game or SGP code):
`tools/host_run.sh [--gdb]` builds the same sources as a 32-bit headless
Linux program (needs `gcc -m32`) and runs it against the bundle;
`SGP_HOST_SCRIPT="600 click 318 286;900 shot gio"` drives it (frame numbers,
~10 ms each), `SGP_HOST_OUT` receives PPM screenshots. Then the real test:
`tests/autotest/phase2-menus.txt` via `tools/machw.py autotest` (PowerBook)
or `EMU_AUTOTEST=` with `tools/os9-emu.sh` (QEMU when no real Mac is up).

Smacker decoder check on the host (after touching `retro68/Smacker/`):
`cc -O2 -I retro68/Smacker -o /tmp/smktest tools/smktest.c
retro68/Smacker/smacker.c`, run it on an SMK extracted from `Intro.slf` and
`cmp` its output with `ffmpeg -i X.SMK -f rawvideo -pix_fmt rgb24` /
`-f s16le` (must be identical; see the header of `tools/smktest.c`).

## Outputs

- `JA2PPC.bin` — MacBinary; the file to pass to LaunchAPPL / `execute_binary`.
- `JA2PPC.dsk` — HFS image for emulators.
- `JA2PPC Bundle/` — app + `Ja2.ini` + `Data/` + `options.cfg`.
- `JA2PPC.APPL` — raw data fork only; never transfer it on its own.

## Checks after a build

- The build must finish without new warnings in touched files.
- `JA2PPC.bin` timestamp must be newer than the edited sources.
- Never commit build dirs, GOG data, logs or emulator images.
