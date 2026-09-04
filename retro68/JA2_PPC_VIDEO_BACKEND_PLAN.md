# PPC Video Backend Plan

## Original Flow

The original flow is split between:

- `ja2/Build/GameInitOptionsScreen.c`
  - Commits `gGameOptions`.
  - Sets the intro type to `INTRO_BEGINING`.
  - Leaves the options screen for `INTRO_SCREEN`.
- `ja2/Build/Intro.c`
  - Disables the cursor and music for the intro.
  - Initializes Smacker playback at 640x480.
  - Plays the beginning sequence in this order:
    1. `INTRO\Rebel_cr.smk`
    2. `INTRO\Omerta.smk`
    3. `INTRO\Prague_cr.smk`
    4. `INTRO\Prague.smk`
  - Polls playback once per frame.
  - Advances when a file ends, or skips the current file on mouse/button input.
  - Returns to `INIT_SCREEN` after the final video.
- `ja2/Build/Utils/Cinematics.c`
  - Owns Smacker file handles, playback slots, frame timing, decoding, and
    video-surface output.

The PPC port must preserve these ownership and lifecycle boundaries.

## Backend Decision

The JA2 assets are `SMK2` Smacker files stored in `Intro.slf`. QuickTime Movie
Toolbox APIs documented in the local OS 9 headers, including
`NewMovieFromDataFork`, `SetMovieGWorld`, and `MoviesTask`, do not provide a
Smacker decoder. They remain a possible backend for future QuickTime media,
but must not be used as a false implementation for these intro assets.

FFmpeg on the host can decode Smacker (`smk`, `smackvideo`, and `smackaudio`).
The suggested H.264/AAC MOV conversion is not a safe System 9 target because
classic QuickTime releases do not generally include H.264 playback. The
installed FFmpeg has no Cinepak encoder, so the current conversion target is a
VCD-style MPEG program stream using MPEG-1 video and MP2 audio. The generated
files are 320x240 and are intended to use QuickTime's MPEG-1 component. They
must be generated during the host build and bundled as deployment outputs; they
must not be committed as generated source assets.

The conversion route still requires an SLF extraction step because the source
SMK files are inside `Intro.slf`. It must log source offsets, lengths, FFmpeg
status, output codec, dimensions, and output size. The PPC Movie Toolbox path
must check QuickTime availability with `Gestalt`, create the movie from its
data fork, attach it to the frame GWorld with `SetMovieGWorld`, advance it with
`MoviesTask`, and dispose the movie and data handles on every failure path.

The current PPC deployment path uses a host conversion backend with this
platform interface:

```text
Initialize(window, frameWorld)
Open(archiveName, entryName)
Poll()
Skip()
Close()
Shutdown()
```

The host conversion step extracts the four entries, invokes FFmpeg, and caches
320x240 MPEG program streams encoded as MPEG-1 video at 1,150 kbit/s with MP2
audio at 128 kbit/s and 44.1 kHz. The PPC Movie Toolbox backend will validate
QuickTime availability, create the movie from its data fork, attach it to an
off-screen 32-bit GWorld with `SetMovieGWorld`, advance it cooperatively with
`MoviesTask`, and present each complete frame without clipped window
`CopyBits` updates. The native SMK2 decoder remains the fallback if the
selected legacy QuickTime component is not available on the target System 9
installation.

## Implementation Stages

1. Probe and validate `Intro.slf` SMK2 entries without claiming playback.
2. Test the generated legacy-compatible MPEG files with the target System 9
   QuickTime installation and validate video-only playback.
3. Add a QuickTime Movie Toolbox backend in the PPC counterpart of
   `Utils/Cinematics.c`.
4. If the target QuickTime component cannot play the generated files, implement
   the SMK2 Huffman trees and frame
   chunk decoder in the PPC counterpart of `Utils/Cinematics.c`.
5. Convert decoded indexed/YUV-style Smacker frame data into the 32-bit GWorld
   format used by the PPC renderer, or copy frames produced by QuickTime into
   that GWorld.
6. Add cooperative timing using `TickCount()` and the frame rate from the SMK2
   header or Movie Toolbox time scale. Do not use a busy loop or Time Manager
   callback for decoding.
7. Add intro-screen state sequencing matching `Intro.c`, including skip input,
   cursor ownership, music shutdown, cleanup, and return to initialization.
8. Add optional Sound Manager audio-track playback only after video-only
   playback is stable.

## OS 9 Rules

- Consult `resources/os9docs-master/` and `MAC_OS9_GAME_DEVELOPMENT.md` before
  using Movies, QuickDraw, Memory Manager, Sound Manager, or timing APIs.
- Use a GWorld for decoded frames and lock pixels only while decoding or
  presenting a frame.
- Keep cached/static frame data immutable after preparation.
- Keep `WaitNextEvent` cooperative and clamp elapsed time after pauses.
- Balance cursor visibility calls exactly; do not paint a cursor into the live
  window PixMap.
- Log asset lookup, SMK2 header fields, allocations, frame decode failures,
  timing decisions, GWorld state, and shutdown paths.

## Current Boundary Status

The reusable PPC cinematics boundary begins in the source counterpart of the
original `Utils/Cinematics.c`. Until the decoder stages are complete, intro
playback must report an explicit unsupported-decoder diagnostic and must not
silently substitute a static frame or claim that the intro videos played.
