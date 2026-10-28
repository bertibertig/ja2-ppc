# Plan

## Goal

Jagged Alliance 2 as a native PowerPC application for Mac OS 8.6–9.2.2
that plays exactly like the Windows game. German first, English second.
Reference build: GOG `ja2.exe` 1.12.1 (German), driven under Wine with the
`winref` skill.

Minimum hardware: PowerPC 604/120 (or 603e ≥160 MHz with L2), 64 MB RAM,
800x600 at thousands of colors. Acceptance machine: PowerBook G3 Lombard
(333 MHz, Mac OS 9.2.2). Emulators (QEMU 9.2.2, DingusPPC 8.6) only for
smoke tests when no real Mac is available, and for 8.6 before releases.

## Architecture (decided 2026-10-28, "Variante B")

The original sources (`../ja2-master`, ~270k lines) are compiled on a Mac
reimplementation of the Standard Gaming Platform (SGP):

- Original files are copied to their mapped paths (`ja2/Build/X` →
  `retro68/X`) and changed only behind `#ifdef JA2_PPC`. All original
  headers are copied, so no compile reads `ja2-master`, which is never
  edited.
- Windows interfaces the original uses are emulated below it instead of
  rewriting the callers: Win32 files/messages/hooks/timers
  (`sgp_win32*.c`), DirectDraw (`sgp_ddraw.c`), Miles Sound System
  (`sgp_mss.c`), Smacker SDK (`sgp_smack.c`), MSVC wide-string runtime
  (`sgp_wchar.c`). SDL2 (sdl2macos9 fork) appears only in `sgp_sdl.c`.
- Every little-endian on-disk format is byte-swapped at its load/save
  site. The x86 assembly (blitters) is ported to C that follows the
  assembly's control flow, quirks included.
- Modules not ported yet are logged stand-ins in
  `retro68/ja2_ppc_unported.c`, grouped by original file.

Details: `../AGENTS.md`, "Port Architecture".

## Phases

| Phase | Content | Status |
|---|---|---|
| 0 | Toolchain (Retro68 GCC 16.1), SDL fork patches, autotest, profiler, emulators, Windows reference | done |
| 1 | SGP core (files, SLF libraries, containers, random, timer, debug) and video (STCI/PCX/TGA, video objects/surfaces, software DirectDraw, 2D blitters) | done, accepted on the PowerBook |
| 2 | Original game loop and screens: splash, main menu, credits, options, game options, message boxes, fades, intro movies; platform layer for input, timers, sound, movies | done, tested in QEMU only (PowerBook was off) |
| 3 | Laptop: the 57 original `Laptop/*.c` files (e-mail, AIM, MERC, IMP, finances, files, history, Bobby Ray, funeral, insurance, florist) plus what they need from the strategic layer (profiles, game clock, events) | next |
| 4 | Tactical: TileEngine (world/map loading, tilesets, render world, lighting), Tactical (soldiers, animations, items, interface), TacticalAI; Z-buffer and remaining blitters | open |
| 5 | Strategic: map screen, game clock, events, towns, militia, auto-resolve, quests, meanwhile scenes | open |
| 6 | Save games (all structs swapped; files compatible with Windows saves), load screen | open |
| 7 | Sound complete: IMA ADPCM speech and battle sounds, ambience, 3D/EAX left out (as without a provider on Windows) | open |
| 8 | Performance for the 604/120 target: dirty-rect presentation, startup time, decoding hot paths | open, measure on the PowerBook first |
| 9 | English build, Mac OS 8.6 test on DingusPPC, release packaging | open |

Remaining original code by directory (files / lines):
Tactical 65 / 158k, Strategic 41 / 102k, Laptop 57 / 63k, Utils 35 / 51k
(partly ported), TileEngine 36 / 47k (2 ported), Editor 24 / 28k (not
needed for the game), TacticalAI 14 / 23k.

## Way of working

1. Read the original file first; port into its counterpart under
   `retro68/`, guards only.
2. Close the link boundary: move functions out of `ja2_ppc_unported.c`
   as their files arrive.
3. Debug logic quickly with `tools/host_run.sh` (32-bit headless Linux
   build of the same sources, gdb, PPM screenshots).
4. Script the feature in `tests/autotest/`, run it on the PowerBook
   (`tools/machw.py autotest`), QEMU only when no real Mac is up.
5. Compare with the Windows reference (`winref` skill,
   `tools/compare_screens.py`); explain every remaining difference.
6. Commit per milestone, author sascha.bertleff@proton.me, private
   remote `bertibertig/ja2-ppc` only.
