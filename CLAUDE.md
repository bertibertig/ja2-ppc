# JA2 PPC — Claude Code

@AGENTS.md
@resources/PORT_HISTORY.md

`MAC_OS9_GAME_DEVELOPMENT.md` (git-ignored, local only) is the condensed
Toolbox reference; read it before using an unfamiliar Toolbox API.

## Architecture decision (2026-10-28): original game code on a Mac SGP

The goal is a port that plays exactly like the Windows game, German first,
English second. Hand-reimplementing screens in `ja2_mac.c` does not scale to
the ~270k lines of original game code, so the port moves to this model:

- The original game sources (`../ja2-master/ja2/Build/`) are copied into
  `retro68/` at their mapped paths (see "Porting Rule" in AGENTS.md) and
  compiled with minimal, `#ifdef JA2_PPC`-guarded changes. `ja2-master/`
  itself is read-only reference and is never edited.
- The Standard Gaming Platform is reimplemented for Mac OS 9 under
  `retro68/Standard Gaming Platform/` (same file names as the original SGP):
  video/surfaces on SDL2 RGB555, input via SDL events, files and SLF
  archives, timers, Sound Manager audio.
- Every on-disk format (SLF, STCI, maps, `.edt` text, save games) is
  little-endian and was read as raw structs on x86. Each load/save site gets
  explicit byte swapping; never rely on struct layout or `#pragma pack`
  matching MSVC. Build with `-fshort-wchar` (JA2 strings are 16-bit).
- The existing hand-written screens (main menu, credits, laptop) stay until
  the original screen runs on the new SGP, then are removed.

## Tooling

- Skills in `.claude/skills/`: `ja2-build`, `mac-hw-test`, `os9-emu`,
  `ja2-profile`.
- MCP `classic-mac-hardware` (`.mcp.json`) deploys to the real PowerBook G3
  Lombard (Mac OS 9.2.2). Emulators are for functional smoke tests only;
  every feature and every performance number is accepted on the real Mac.
- Unattended hardware tests: `tools/machw.py autotest tests/autotest/<x>.txt`
  replays a script inside the app (`retro68/ja2_autotest.c`) and collects
  logs and screenshots; prefer it over asking the user to click on the Mac.
- Commit author email: sascha.bertleff@proton.me (set in the repo config).
