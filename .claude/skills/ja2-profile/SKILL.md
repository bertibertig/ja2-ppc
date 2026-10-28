---
name: ja2-profile
description: Measure JA2 PPC performance on the real Mac OS 9 PowerBook with the built-in ja2prof frame profiler (JA2_PROFILE build, JA2PROF.csv, tools/ja2prof_report.py), compare runs, and judge results against the 603e/604 target budget. Use for any performance question, before and after optimizations, and when adding new screens or SGP subsystems.
---

# Profiling JA2 PPC on Mac OS 9

## Rules

- Numbers only count when measured on the real PowerBook G3 (Mac OS 9.2.2).
  Emulators (QEMU, SheepShaver, DingusPPC) do not model PPC cache, bus or
  QuickDraw timing; use them only to check that the profile build runs.
- Always compare like with like: same build type, same `options.cfg`, same
  scene, same display depth (log line `window surface format`), AC power,
  no other apps open on the Mac.
- Keep a baseline CSV per scene in `retro68/perf-baselines/` (git-ignored)
  named `<scene>-<commit>.csv`.

## Profiler pieces

- `retro68/ja2_prof.c/.h`: compiled in with `-DJA2_PROFILE=ON`; otherwise
  every call compiles away. Time base = PPC `mftb`, calibrated against
  `Microseconds()` at startup (601 falls back to `Microseconds()`).
- Zones (inclusive, nested zones count inside their caller): `events`,
  `logic`, `audio`, `video`, `render`, `blit` (format conversion to the
  window surface + overlays), `present` (`SDL_UpdateWindowSurface` →
  CopyBits), `decode` (STCI/ETRLE), `file` (SLF reads), `idle`
  (`SDL_Delay`), plus `frame` (whole main-loop iteration).
- Markers: `JA2_PROF_MARK("name")` at screen transitions (main_menu, credits,
  laptop, intro). Add one for each new screen.
- New subsystem? Wrap its per-frame entry in `JA2_PROF_BEGIN/END` with an
  existing zone, or add a zone to the enum and to `gZoneNames`.
- Memory: `FreeMem()` sampled every 30 frames; `min_free_kb` in the CSV.

## Options (`options.cfg` beside the app)

```
profile_frames=600     # quit cleanly after N frames (0 = until user quits)
profile_uncapped=1     # drop the 16 ms SDL_Delay to measure max throughput
profile_label=menu     # tag stored in the CSV
skip_intros=1          # for menu/laptop scenes
```

## Workflow

1. Build: `cmake -S retro68 -B retro68/build-profile -G Ninja
   -DCMAKE_TOOLCHAIN_FILE=$TC -DJA2_DATA_DIR=... -DJA2_LANGUAGE=german
   -DJA2_DEBUG_BUILD=ON -DJA2_PROFILE=ON && cmake --build retro68/build-profile`
2. Upload `options.cfg` with the scene's settings; delete `/JA2PROF.csv`
   and `/JA2PPC-Debug.log` on the Mac (see `mac-hw-test`).
3. `execute_binary` with `retro68/build-profile/JA2PPC.bin`. With
   `profile_frames` set the app exits by itself and writes the CSV.
4. `download_file` `/JA2PROF.csv` → scratchpad, then
   `python3 tools/ja2prof_report.py <csv> --budget-ms 33` or
   `python3 tools/ja2prof_report.py <baseline.csv> <new.csv>`.
5. Report mean/p95 per zone and the delta against the baseline.

## Budgets

The design target is a 603e/604 at 100-120 MHz; the test machine is a
G3 333 MHz (roughly 3-4x faster per clock-for-clock and frequency). Until a
slower Mac is available:

- Judge busy time (`frame` minus `idle`) with the uncapped run.
- On the G3, keep p95 busy time at or below ~8 ms per frame for menus and
  laptop and ~10 ms for strategic/tactical scenes; that leaves the target
  machine about 30 fps.
- For a closer approximation, disable the L2 cache in the PowerBook's
  control panel for a run and label it (`profile_label=menu-noL2`).

## Deeper analysis

When a zone is too slow and the cause is not obvious, add finer
`JA2_PROF_BEGIN/END` pairs around the suspect calls in that zone (temporarily
reuse a zone id) rather than guessing; remove them afterwards.
