---
name: mac-hw-test
description: Deploy and run a JA2 PPC build on the real PowerBook G3 Lombard (Mac OS 9.2.2) through the classic-mac-hardware MCP, fetch JA2PPC-Debug.log / profiler CSVs, and record the screen with the Cam Link. Use for every acceptance test and every performance measurement; emulators never replace this step.
---

# Real-hardware test on the PowerBook G3

Machine id: `powerbook-g3` (config `~/.config/classic-mac-hardware/machines.json`,
which holds the Mac's LAN address; G3 333 MHz, 384 MB, Mac OS 9.2.2, Open Transport).
On the Mac, RumpusFTP (port 21) and LaunchAPPLServer (TCP, port 1984) must be
running. The FTP root is the folder the app runs in; `Data/`, `Ja2.ini` and
`options.cfg` live there, and debug logs are written there.

## 0. Is the Mac up?

```sh
ping -c1 -W1 <host from machines.json>
```

If it does not answer, stop and tell the user the PowerBook must be switched
on with RumpusFTP and LaunchAPPLServer running. Do not fall back to an
emulator for acceptance.

## Fast path: scripted autotest (preferred)

Interactive steps (clicks, keys, screen checks) are scripted instead of
asking the user to operate the Mac. `tools/machw.py` talks to the same MCP
server over stdio (works even when the MCP tools are not loaded in the
session) and does steps 1-4 below in one command:

```sh
python3 tools/machw.py autotest tests/autotest/smoke.txt \
  --build retro68/build-german-debug [--capture 75]
```

- The script format is documented at the top of `retro68/ja2_autotest.c`
  (`wait`, `frames`, `move`, `click`, `key`, `shot`, `expect`, `mark`,
  `log`, `timeout`, `quit`). Coordinates are 640x480 window coordinates.
- Outputs land in `.hw-out/`: `JA2AUTOTEST.log` (with `result=pass|...`),
  `JA2PPC-Debug.log`, `JA2PROF.csv` (profile builds), one PNG per `shot`
  (exact window surface, including the game cursor), `capture.mp4`.
- Exit code 0 only on `result=pass`. Afterwards the bundle's normal
  `options.cfg` is restored on the Mac, so a manual start is not scripted.
- Keep reusable scripts in `tests/autotest/`; write a new one per feature
  and commit it with the feature. Read the PNGs before reporting.
- `python3 tools/machw.py call TOOL '{json}'` issues raw MCP calls
  (e.g. `list_directory '{"machine":"powerbook-g3","path":"/"}'`).

Comparing with the Windows original: take the same states in the
reference (`winref` skill, same coordinates and waits; e.g.
`tests/autotest/menu-compare.txt` and `phase2-menus.txt` mirror winref
click sequences), then
`python3 tools/compare_screens.py WIN.png MAC.png OUT` reports the share of
matching pixels (5 bits per channel, one step tolerance), the differing
32x32 areas and writes OUT-side.png (Windows | Mac | differences in
magenta). Mouse cursors differ by position; everything else should match.

Only ask the user to operate the Mac for things outside the app (control
panels, Finder dialogs, hardware).

## 1. Connection

1. `list_machines`
2. `test_connection(machine="powerbook-g3", test_launchappl=true)`

## 2. Prepare a bounded run

- `delete_files(machine="powerbook-g3", path="/JA2PPC-Debug.log")` — always,
  so the log contains only this run. Same for `/JA2PROF.csv` on profile runs.
- If `options.cfg` changed, `upload_file` it to `/options.cfg`.
- Data on the Mac only changes when `Data/` changed (rare; it is large).
  Use `list_directory(path="/Data")` to check before uploading single files.

## 3. Run (with optional screen capture)

Find the capture node first: `v4l2-ctl --list-devices`; make sure it is free:
`fuser -v /dev/video0` (never kill another process holding it).

Start the recording in the background, then immediately call the MCP:

```sh
OUT=/tmp/claude-1000/ja2-capture; mkdir -p $OUT
ffmpeg -y -f v4l2 -framerate 30 -video_size 1024x768 -i /dev/video0 \
  -t 45 -an -c:v libx264 -preset ultrafast -crf 18 $OUT/run.mp4
```

`execute_binary(machine="powerbook-g3", platform="ppc",
binary_path="<abs path>/retro68/build-german-debug/JA2PPC.bin")`

The MCP timeout does not kill the app on the Mac; a hung app needs the user
(Cmd-Opt-Esc on the PowerBook).

## 4. Collect evidence

- `download_file(machine="powerbook-g3", remote_path="/JA2PPC-Debug.log",
  local_path="<scratchpad>/JA2PPC-Debug.log")` and read it before diagnosing.
- Profile runs: also `/JA2PROF.csv` (see `ja2-profile`).
- `mac://powerbook-g3/logs/latest` is LaunchAPPL's `PT_Log` (crash/exit info).
- Frames from the capture:
  `ffmpeg -y -i $OUT/run.mp4 -vf fps=1/3 $OUT/frame-%02d.png`, then read
  the PNGs and compare them with log timestamps and the Windows original.

## 5. Report

State what was verified on hardware and what was not. A feature counts as
working only when the log and the captured frames both confirm it.
