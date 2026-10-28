---
name: os9-emu
description: Run a JA2 PPC build headlessly in a Mac OS 9 emulator (QEMU mac99 or DingusPPC) via tools/os9-emu.sh - deploy into Startup Items of a private OS image copy, boot, collect JA2PPC-Debug.log and QEMU screenshots. Use for quick functional smoke tests when the PowerBook is off; never for performance numbers or final acceptance.
---

# Mac OS 9 emulator smoke tests

`tools/os9-emu.sh [prepare|deploy|run|collect|all|reset]`

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
to see the guest screen).

## Engines and status (2026-10-28)

| Engine | OS image | Status |
|---|---|---|
| `qemu` (mac99, New World) | `$EMU_WORK_DIR/base/macos922.img`, Mac OS 9.2.2 | Preferred. Not set up yet: needs `qemu-system-ppc` installed and a 9.2.2 install the user owns. Headless with screenshots via the QEMU monitor. |
| `dingus` (pmg3dt, Beige G3 ROM) | `$VM_DIR/Powermac G3/macos91.img`, German 9.1 | Boot fails with "Busfehler (1)" at the Mac OS 9.1 welcome screen (before extensions), with both the 2026-09 and the 2026-10 DingusPPC builds. Not usable until that is solved. |

Creating the QEMU 9.2.2 base image (once, interactive, the user drives the
installer): `qemu-img create -f raw macos922.img 4G`, boot
`qemu-system-ppc -M mac99,via=pmu -m 512 -boot d -cdrom <9.2.2 CD>
-drive file=macos922.img,format=raw,media=disk -device usb-kbd -device usb-mouse`,
initialize the disk as Mac OS Extended in Drive Setup, install, shut down.

## Rules

- An emulator pass is a smoke test only. Acceptance and every performance
  number come from the PowerBook G3 (`mac-hw-test`, `ja2-profile`).
- The hfsutils steps need the emulator stopped; never deploy while it runs.
- If the Mac OS volume was not shut down cleanly, the next boot runs a disk
  check first; allow extra time.
