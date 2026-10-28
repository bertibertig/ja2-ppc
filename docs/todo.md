# To do

Sorted by priority. Evidence in brackets; details in findings.md.

## Now (finish Phase 2)

- [ ] **Run Phase 2 on the PowerBook**: `tests/autotest/phase2-menus.txt`
      via `tools/machw.py`; listen for menu music and button clicks; record
      startup time, menu fps and intro fps. Phase 2 was tested in QEMU only.
- [ ] **Startup time**: ~11 s in `LoadAllItemNames` (QEMU), which opens
      the item text file three times per item. Profile on the PowerBook;
      make FileMan's open path cheap (real-file existence check before the
      library lookup) rather than changing the original loop.
- [ ] Compare the options screen with an identical `Ja2.set` on both
      sides, so the volume sliders can be checked too.

## Phase 3: laptop

- [ ] Port `Laptop/*.c` (57 files) and the strategic pieces they need:
      `Strategic/Game Init.c` (`InitNewGame`), `Game Clock.c`,
      `Game Events.c`/`Game Event Hook.c`, `Tactical/Soldier Profile.c`
      (`LoadMercProfiles`, byte-swapped `prof.dat`), `Laptop/finances.c`,
      e-mail, files, history.
- [ ] Byte-swap every laptop data load (e-mail text, AIM/MERC data,
      profiles); wide text from `.edt` files via the existing helpers.
- [ ] Remove the matching groups from `ja2_ppc_unported.c`.
- [ ] Autotest: new game → laptop → e-mail → AIM; compare with winref.

## Sound

- [ ] **IMA ADPCM** (format 0x11) in `sgp_mss.c` for speech and battle
      sounds: `Speech.slf`, `Npc_Speech.slf`, most of `BattleSnds.slf`.
      Decode to PCM in `AIL_set_named_sample_file` / stream buffers.
- [ ] Check `SoundPlayRandom`/ambience once tactical maps load.
- [ ] Music streams share the SLF library handle with FileMan; confirm no
      read interferes (absolute seeks are in place).

## Text

- [ ] Sync `_GermanText.c`/`_Ja25GermanText.c` with the German 1.12.1
      exe: a tool that lists literals missing from `ja2.exe` and maps them
      to the exe's string at the same array position (86 candidates,
      most only line breaks).
- [ ] Verify German keyboard input (umlauts, `ß`, `#`, `+`) in a text
      field against Windows; the virtual-key table in `sgp_sdl.c` follows
      the German Windows layout, `English.c`'s translation table is the
      source's.

## Graphics

- [ ] 75 blitters still log "not ported yet" (Z-buffer, tactical,
      8-bit destination). Port each when its first caller arrives
      (Phase 4), against the assembly.
- [ ] `Compression.c` is a stub (no STI uses zlib so far); revisit if a
      compressed STI appears.

## Performance (target 604/120)

- [ ] Present only dirty rectangles; today every frame is a full 640x480
      RGB555 → window blit plus `SDL_UpdateWindowSurface`.
- [ ] Movies: frames now go through the frame buffer and the normal
      present path (original behaviour); measure on the PowerBook against
      the Phase 1 direct-to-window path (14.4–14.9 fps).
- [ ] Profile the menus with `menu-idle.txt` on the PowerBook.

## Platform details

- [ ] Control-click is the right button (one-button trackpad); the game
      still sees Control held. Decide whether to hide Control during the
      click.
- [ ] `MessageBox` only logs; `ShutdownWithErrorBox` paths end silently.
      Show a Mac alert with the text.
- [ ] `skip_intros` in `options.cfg` has no effect any more; remove it
      or map it to the original flow.
- [ ] `WinMain` returns an uninitialised `Message.wParam` when the loop
      ends through `gfProgramIsRunning` (cosmetic, log only).

## Later phases

- [ ] Phase 4 tactical, Phase 5 strategic, Phase 6 save games (Windows
      save compatibility), Phase 9 English build and Mac OS 8.6 test
      (see plan.md).
- [ ] VGA-out audio quirk on the PowerBook (no sound with VGA attached,
      also on the headphone jack): unresolved, user-side checks pending.

## Done in Phase 2 (for reference)

- [x] Original game loop, screen table and menu screens on the Mac SGP.
- [x] Win32 messages/hooks/timers, Miles PCM subset, Smacker SDK, MSVC
      wide strings.
- [x] `.edt` text and `Ja2.set` byte order; pixelate blitter.
- [x] Old shell, SGP self-test and legacy laptop removed.
- [x] Host debugging harness `tools/host_run.sh`.
