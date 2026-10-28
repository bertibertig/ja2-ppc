---
name: os9-emu
description: Run a JA2 PPC build headlessly in a Mac OS 8.6/9 emulator (QEMU mac99 with 9.2.2, DingusPPC Power Mac 7600 with 8.6) via tools/os9-emu.sh - deploy into Startup Items of a private OS image copy, boot, collect JA2PPC-Debug.log and QEMU screenshots. Use for quick functional smoke tests when the PowerBook is off; never for performance numbers or final acceptance.
---

# Mac OS 9 emulator smoke tests

`tools/os9-emu.sh [prepare|deploy|run|collect|all|reset|install|gui]`

1. `prepare` copies the base OS image once to `$EMU_WORK_DIR/<engine>-work.img`.
   The base image is never modified.
2. `deploy` uses hfsutils to put `JA2PPC` (from `JA2PPC.bin`), `options.cfg`,
   `Ja2.ini` and `Data/` into the boot volume's Startup Items folder
   (`Startobjekte` on German systems), so the game launches after boot.
   `Data/` is copied only when the bundle changed.
3. `run` boots for `EMU_TIMEOUT` seconds (default 240), then stops the emulator.
4. `collect` copies `JA2PPC-Debug.log`, `JA2PROF.csv`, `JA2AUTOTEST.log`
   and autotest screenshots (as PNG) to `.emu-out/`. No log means the game
   never started (or the OS never finished booting).

Scripted runs: `EMU_AUTOTEST=tests/autotest/smoke.txt` deploys the same
autotest script as `tools/machw.py` on the PowerBook and adds
`autotest=<script>` to the options; `collect` prints the `result=` line.
The emulator still runs until `EMU_TIMEOUT`. Deploy marks everything except
the app invisible, because the Finder opens every visible Startup Items
entry at boot (SimpleText error dialogs, Finder windows).

Paths (`EMU_WORK_DIR`, `EMU_INSTALL_CD`, `DINGUS_ROOT`, `RETRO68_TOOLCHAIN`)
come from the git-ignored `local.env` (see `local.env.example`).

Variables: `EMU_ENGINE=qemu|dingus` (default qemu), `JA2_BUILD_DIR`, `EMU_OPTIONS=<cfg>`
(e.g. with `skip_intros=1`), `EMU_TIMEOUT`, `EMU_BASE_IMAGE`,
`EMU_SCREENSHOTS` (N evenly spaced guest-only PNGs in `.emu-out/`: QEMU via
its monitor, DingusPPC by capturing its own XWayland window; read them to see
the guest screen), `EMU_INSTALL_CD`, `QEMU_DISPLAY`.

Interactive (the user drives the window): `install` (QEMU only) boots the
install CD with the base image attached; `gui` boots the base image of either
engine for one-off guest settings, then run `reset` so the working copy picks
them up. QEMU uses the SDL UI when `qemu-ui-sdl` is installed (GTK loses the
pointer under KDE Wayland); DingusPPC grabs the mouse with Ctrl+G. Never take host screenshots of these windows (the user's
desktop is private); use `screendump` on `$EMU_WORK_DIR/qemu-install.sock`.

QEMU hardware mirrors the PowerBook G3 Lombard (`QEMU_MACHINE` in the
script): `mac99,via=pmu`, `-cpu 750` (G3, no AltiVec), 384 MB, 1024x768x16,
USB keyboard/mouse. Not reproducible: the 333 MHz clock, the ATI Rage LT Pro
(QEMU VGA instead) and the internal ADB keyboard/trackpad. No PPC Mac
emulator offers RAVE/OpenGL 3D; JA2 does not need it.

## Engines and status (2026-10-28)

| Engine | OS image | Status |
|---|---|---|
| `qemu` (mac99, New World) | `$EMU_WORK_DIR/base/macos922.img`, German Mac OS 9.2.2 (iMac CD), HFS Standard, 4 GB | Ready (2026-10-28). Boots to the Finder in under 30 s at 1024x768 thousands; setup/AirPort assistants removed from Startup Items, unclean-shutdown warning disabled, so hard-stopped runs reboot without a dialog. |
| `dingus` (pm7600 `--cpu 604`, 64 MB, `--mon_id VGA-SVGA`, ROM `$EMU_WORK_DIR/base86/pm7600.rom`) | `$EMU_WORK_DIR/base86/macos86.img`, German Mac OS 8.5 + 8.6 update, HFS Standard, 4 GB | Ready (2026-10-28). The minimum-spec target (Power Mac 7600/120, AGENTS.md) and the only emulator that boots 8.x. Boots to the Finder at 800x600 thousands; unclean-shutdown warning off. Interpreter: much slower than QEMU, no timing value. PRAM/NVRAM (resolution) live next to the base image and are copied by `prepare`. |

Recreating the QEMU base image (once, interactive): move the old image away,
`EMU_ENGINE=qemu tools/os9-emu.sh install` (creates an empty 4 GB image).
In Drive Setup initialize it as **Mac OS Standard** (hfsutils cannot write
Mac OS Extended), install, shut down from the Finder. Then with `gui`: set
Monitors to 1024x768 thousands and turn off the unclean-shutdown warning in
General Controls; delete the assistant aliases from Startup Items.

Why not other options for 8.6 (2026-10-28): the 8.5 CD and an 8.6 disk with a
New World `Mac OS ROM` (1.6, 2.5.1) do not boot in QEMU (OpenBIOS finds no
bootable tbxi); SheepShaver emulates no real hardware (reports a G4, no 604).
The pm8500 opens a second window for its AV video card; the default
`AppleVision1710` monitor only offers 640x480.

## Rules

- Real hardware first: always test on the PowerBook (or another real Mac
  meeting the minimum spec) via `mac-hw-test`. Use QEMU (9.2.2) only when
  no real Mac is reachable, and say so in the report. DingusPPC (8.6 on
  the 7600/120) is slow and only required before a release (and when
  touching version-gated 8.6 code paths).

- An emulator pass is a smoke test only. Acceptance and every performance
  number come from the PowerBook G3 (`mac-hw-test`, `ja2-profile`).
- The hfsutils steps need the emulator stopped; never deploy while it runs.
- `run` stops QEMU hard, so the next boot runs a silent disk check first;
  allow extra time.
