---
name: os9-emu
description: Run a JA2 PPC build headlessly in a Mac OS 9 emulator (QEMU mac99 or DingusPPC) via tools/os9-emu.sh - deploy into Startup Items of a private OS image copy, boot, collect JA2PPC-Debug.log and QEMU screenshots. Use for quick functional smoke tests when the PowerBook is off; never for performance numbers or final acceptance.
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
4. `collect` copies `JA2PPC-Debug.log` and `JA2PROF.csv` to `.emu-out/`.
   No log means the game never started (or the OS never finished booting).

Variables: `EMU_ENGINE=qemu|dingus`, `JA2_BUILD_DIR`, `EMU_OPTIONS=<cfg>`
(e.g. with `skip_intros=1`), `EMU_TIMEOUT`, `EMU_BASE_IMAGE`,
`EMU_SCREENSHOTS` (QEMU only: N evenly spaced PNGs in `.emu-out/`; read them
to see the guest screen), `EMU_INSTALL_CD`, `QEMU_DISPLAY`.

Interactive (QEMU only, the user drives the window): `install` boots the
install CD with the base image attached; `gui` boots the base image for
one-off guest settings, then run `reset` so the working copy picks them up.
Both use the SDL UI when `qemu-ui-sdl` is installed (GTK loses the pointer
under KDE Wayland). Never take host screenshots of these windows (the user's
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
| `dingus` (pmg3dt, Beige G3 ROM) | `$VM_DIR/Powermac G3/macos91.img`, German 9.1 | Abandoned 2026-10-28: "Busfehler (1)" at the 9.1 welcome screen, also with all ATI extensions disabled. Interpreter only (no JIT), no 3D. Use QEMU. |

Recreating the QEMU base image (once, interactive): move the old image away,
`EMU_ENGINE=qemu tools/os9-emu.sh install` (creates an empty 4 GB image).
In Drive Setup initialize it as **Mac OS Standard** (hfsutils cannot write
Mac OS Extended), install, shut down from the Finder. Then with `gui`: set
Monitors to 1024x768 thousands and turn off the unclean-shutdown warning in
General Controls; delete the assistant aliases from Startup Items.

## Rules

- An emulator pass is a smoke test only. Acceptance and every performance
  number come from the PowerBook G3 (`mac-hw-test`, `ja2-profile`).
- The hfsutils steps need the emulator stopped; never deploy while it runs.
- `run` stops QEMU hard, so the next boot runs a silent disk check first;
  allow extra time.
