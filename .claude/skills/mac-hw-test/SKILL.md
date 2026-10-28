---
name: mac-hw-test
description: Deploy and run a JA2 PPC build on the real PowerBook G3 Lombard (Mac OS 9.2.2) through the classic-mac-hardware MCP, fetch JA2PPC-Debug.log / profiler CSVs, and record the screen with the Cam Link. Use for every acceptance test and every performance measurement; emulators never replace this step.
---

# Real-hardware test on the PowerBook G3

Machine id: `powerbook-g3` (config `~/.config/classic-mac-hardware/machines.json`,
<powerbook-host>, G3 333 MHz, 384 MB, Mac OS 9.2.2, Open Transport).
On the Mac, RumpusFTP (port 21) and LaunchAPPLServer (TCP, port 1984) must be
running. The FTP root is the folder the app runs in; `Data/`, `Ja2.ini` and
`options.cfg` live there, and debug logs are written there.

## 0. Is the Mac up?

```sh
ping -c1 -W1 <powerbook-host>
```

If it does not answer, stop and tell the user the PowerBook must be switched
on with RumpusFTP and LaunchAPPLServer running. Do not fall back to an
emulator for acceptance.

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
