---
name: winref
description: Run the Windows reference build of JA2 (GOG ja2.exe, German 1.12.1) under Wine on a private, invisible X display via tools/winref.sh, and drive it with clicks/keys/screenshots to check how the original behaves or looks. Use whenever a port feature must be compared with the original Windows game; never run the Windows game fullscreen on the user's desktop.
---

# Windows reference (GOG 1.12.1 under Wine)

`tools/winref.sh start|shot|click|move|key|type|view|status|stop|reset`

Why GOG: its `ja2.exe` ("Release v1.12.1", Build 01.20.17) is the build
closest to `../ja2-master` ("Release v1.12"), needs no CD and ships the same
data as the German CD + patch 1.05 except about 20 files (Scully's salary,
three NPC scripts, a few sprites and sounds). The user's CD install (1.05,
LaserLok, CD check) is not used as reference.

## How it runs

- The game runs on its own Xvfb server (`WINREF_DISPLAY`, default `:77`,
  640x480x24). Nothing appears on the user's desktop, nothing takes focus,
  and Wine sound is off (`WINREF_AUDIO=1` turns it on). Fullscreen mode
  changes only affect the private display.
- The GOG install (`JA2_DATA_DIR` from `local.env`) is never written to. The
  game runs from `$EMU_WORK_DIR/winref/game`, a symlink copy with real
  `Ja2.ini`, `Ja2.set`, `Data/Temp` and `SavedGames`; the Wine prefix is
  `$EMU_WORK_DIR/winref/prefix` (Heroic's prefix is not touched).
- Wine's built-in `ddraw` is forced; GOG's bundled DDrawCompat is not used.
- `start` takes ~4 s; the main menu is up about 10 s later. The first run
  creates the prefix (~1 min).

## Driving it

Coordinates are game coordinates (the screen is exactly 640x480).

```bash
tools/winref.sh start
tools/winref.sh shot .winref-out/menu.png     # then Read the PNG
tools/winref.sh click 318 397                 # main menu "CREDITS"
tools/winref.sh key Escape
tools/winref.sh type "Hallo"
tools/winref.sh stop
```

- Always take a `shot` and Read it before clicking somewhere new; wait 1-2 s
  after a click before the next shot (screen transitions fade).
- Main menu buttons (German): NEUES SPIEL ~(318,286), SPIEL LADEN (318,323),
  EINSTELLUNGEN (318,359), CREDITS (318,397), BEENDEN (318,435).
- `click X Y 3` is a right click; `key` takes xdotool key names
  (`Return`, `space`, `F1`, `ctrl+s`, `alt+x`).
- Screenshots default to `.winref-out/` (git-ignored). Compare them with
  port screenshots from `machw.py`/`os9-emu.sh` at the same 640x480 size.
- The intro videos play after "Neues Spiel" (INTRO_BEGINING in `Intro.c`),
  not at program start.
- `view` opens a read-only live window (ffplay of the private display) if
  the user wants to watch; it takes no input and can be closed any time.
  Only start it when the user asks.
- `reset` deletes the game copy (save games, settings) but keeps the prefix.
- Call `stop` when done so Wine and Xvfb do not keep running.
