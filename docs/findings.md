# Findings

What was learned while porting, grouped by area. Each entry: symptom →
cause → resolution (file). Measurements name the machine.

## Toolchain and build

- **GCC 16 defaults to C23**, where `f()` means `f(void)`. The original
  declares many functions with `()` and calls them with arguments
  ("too many arguments to InitializeFileDatabase"). → All original and
  port files build with `-std=gnu99` (`retro68/CMakeLists.txt`).
- **GCC 14+ turns MSVC-tolerated code into errors**: implicit `int`,
  pointer/int conversions, incompatible pointer types, `return` without
  value. → `-Wno-error=` for exactly these four in original files; implicit
  function declarations stay errors, because a missing prototype on PPC can
  truncate a returned pointer.
- **Precompiled headers**: the x86 projects defined `PRECOMPILEDHEADERS`
  and compiled every file through "JA2 All.h"/"Utils All.h". The non-PCH
  include lists in the sources had rotted. → Game files build with
  `-DPRECOMPILEDHEADERS`.
- **Case-insensitive includes**: the sources spell headers in several
  cases and even with Windows paths (`"LAPTOP\files.h"`,
  `"\JA2\Build\utils\Font Control.h"`). → `tools/gen_case_includes.py`
  creates symlink aliases, including names that contain backslashes.
- **Headers from ja2-master shadowing ported ones**: an original header
  includes its siblings from its own directory first. → All 312 game and
  SGP headers are copied to `retro68/`; only `retro68/` is on the include
  path.
- **Latin-1 sources**: `_GermanText.c` is ISO-8859-1; GCC assumes UTF-8.
  → `-finput-charset=ISO-8859-1`; `wchar_t` is 16-bit (Retro68 default,
  `-fshort-wchar` on hosts).
- **`INT16 x[] = L"..."`**: MSVC accepted wide literals for signed short
  arrays; GCC does not (`wchar_t` is unsigned). → These arrays are guarded
  as `CHAR16` (`GameVersion.c/.h`, `_GermanText.c`, `_EnglishText.c`,
  `Text.h`).
- **Duplicate `main`**: SDL2main defines `main()` and calls `SDL_main`;
  `sgp.c` does not include `SDL.h`, so its `main` clashed. Retro68's
  XCOFF linker then crashed with SIGSEGV inside `multiple_definition`
  instead of reporting it (found with gdb on `powerpc-apple-macos-ld`).
  → `sgp.c` defines `SDL_main` under JA2_PPC.
- **Win32 macros vs. Toolbox**: `sgp_win32.h` defines `ShowCursor` and
  `ShowWindow` as macros; QuickDraw/MacWindows declare functions of the
  same names. → Port files include Mac/SDL headers before `sgp_win32.h`.
- `gzCreditNames` & co. are defined in a header and in `_GermanText.c`
  with different sizes; `-fcommon` merges them as MSVC did (linker
  warning only).

## Win32 semantics the game depends on

- **MSVC `swprintf(buffer, format, ...)`** has no size argument; in wide
  formats `%s` is a wide string and `%S` a narrow one (1,226 calls).
  glibc/newlib follow C99. → `sgp_wchar.c` implements the MSVC behaviour;
  numbers go through `vsnprintf`.
- **`rand()`**: `RAND_MAX` 0x7fff and MSVC's LCG; `Random()` overflows
  with newlib's 2^31 range. → `sgp_platform.h` (Phase 1).
- **Input** arrives through `SetWindowsHookEx` keyboard and mouse hooks;
  `KeyChange` tells numeric-keypad keys from the navigation block by the
  scan code in `lParam`. → `sgp_sdl.c` produces Windows virtual-key codes
  and PC set-1 scan codes (German layout) and calls the hooks.
- **The game clock** (`guiBaseJA2Clock`) is advanced by a 10 ms
  multimedia timer callback on its own thread (`CALLBACKTIMER`). Menus wait
  on it (main menu: 4 s splash). → `timeSetEvent` is a Time Manager task
  on the Mac. A SIGALRM version on the host ran at a tenth of real time
  (`usleep` swallowed signals); the host now services timers from the
  message pump.
- **Disabled buttons** are drawn by `Blt16BPPBufferPixelateRectWithColor`.
  The assembly compares bytes (`cmp [esi+ebx],0`) and resets the pattern
  index to 0 at each line start, so the first pixel of every line reads
  `Pattern[0][0]`. Verified against the Windows screen (checkerboard).
- **CD checks**: the GOG build has none; `NOCDCHECK` is already defined
  for non-Russian builds. The CD-location code in `GameSettings.c` is
  replaced behind JA2_PPC.

## Byte order (big-endian PPC vs. little-endian data)

Swapped so far: SLF library header and directory, STCI headers, ETRLE
offsets, 16-bit STCI pixels, PCX header (Phase 1); `.edt` text
(`Encrypted File.c`, `Text Utils.c`), `Ja2.set` (`GameSettings.c`), 16-bit
WAV samples (`soundman.c`, `sgp_mss.c` streams) in Phase 2.
- Symptom of a missed site: credits text rendered as "AAAA…" on the Mac,
  correct on the little-endian host build.
- The GCC/PPC layout of the on-disk structs matches MSVC's default
  packing (natural alignment); only the byte order differs.

## Data and text

- **The German GOG 1.12.1 exe has newer text than the source release**
  (Build 01.20.17 vs. "Build 04.12.02"): "Ironman" instead of "Mit
  Zeitlimit", German Ironman messages where the source has English.
  86 of 2,227 German literals are not found verbatim in the exe (most
  differ only in line breaks or are demo texts). → Visible ones replaced
  behind JA2_PPC; full sync open (todo).
- **Sound formats**: music (`Music.slf`) is PCM 22 kHz stereo 16-bit;
  `Sounds.slf` PCM 8/16-bit, 11–44 kHz; `Speech.slf`, `Npc_Speech.slf`
  and most of `BattleSnds.slf` are **IMA ADPCM** (format 0x11, 4-bit,
  22 kHz). The Miles subset handles PCM only so far.
- Music is streamed by Miles from the SLF library through a raw Win32
  handle (`"\\\\<handle>"` filename trick in `SoundPlayStreamedFile`).
  The handle is shared with FileMan; `sgp_mss.c` seeks absolutely for
  every read.
- `Intro.slf` movies are Smacker SMK2/SMK4 at 15 fps; the port's decoder
  is bit-identical to FFmpeg (`tools/smktest.c`).

## Platform (Mac OS 9, Retro68, SDL fork)

- Retro68 stdio refills only 1 KB per File Manager call; a 64 KB
  `setvbuf` on library files cut the menu music load from 10.4 s to
  1.2 s and enabled full-rate movies (Phase 0/1).
- The SDL fork's event pump called `WaitNextEvent(sleep=1)` per poll
  (50 ms per frame). Fixed by `retro68/patches/sdl2macos9/0001-*.patch`
  (0.04 ms).
- LaunchAPPL's `execute_binary` returns after 45 s while the app keeps
  running; `tools/machw.py` polls `JA2AUTOTEST.log`.
- QEMU boots sometimes stop with "ATI Video Accelerator, Fehler 41" before
  the game starts; `tools/os9-emu.sh reset` and a rerun fix it.

## Original-code pitfalls found in Phase 2

- `InitializeGameVideoObjects` (save buffer, extra buffer) is called from
  `InitTacticalEngine` in `Tactical/Overhead.c`. A stand-in that skipped
  it made the main menu crash in `RestoreExternBackgroundRect` (NULL
  buffers). Stand-ins must run the ported parts of the original body.
- `InitNewGame` drives the screen flow (`SetPendingNewScreen(LAPTOP_SCREEN)`,
  `gubScreenCount`); without it the game stays on INIT_SCREEN after the
  intro.
- `LoadAllItemNames` opens `BINARYDATA\ITEMDESC.EDT` three times per item;
  this is the bulk of the startup time (below).

## Measurements

| What | Where | Result |
|---|---|---|
| Main menu event pump before/after SDL patch | PowerBook | 50 ms → 0.04 ms per frame |
| Intro movies, native Smacker (Phase 1 shell) | PowerBook | 14.4–14.9 fps of 15, no audio drops |
| Intro movie, original Cinematics.c on SGP | QEMU | 14.2 fps of 15, no audio drops |
| Startup to splash | QEMU | ~2.3 s to frame 1, then ~11 s in `InitializeJA2` (item names) |
| Full present (blit + update) | PowerBook | 11–21 ms + 19–37 ms per frame (Phase 0) |

## Comparison with Windows (Phase 2)

`tools/compare_screens.py`, 5 bits per channel, one step tolerance:

| Screen | Match | Remaining difference |
|---|---|---|
| Main menu, hover, return | 100 % (host) | – |
| Game options | 100 % (QEMU) | – |
| Options | 98.4 % | volume sliders (the reference's saved `Ja2.set`) |
| Credits | 96–99 % | scroll position at capture time |
