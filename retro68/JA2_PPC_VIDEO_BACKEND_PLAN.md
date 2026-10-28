# JA2 PPC Video Backend

Status (2026-10-28): implemented as a native Smacker player. This replaces
the earlier plan of converting the movies to MPEG-1 on the host and playing
them with QuickTime, which ran at 5-7 fps on the PowerBook G3 (MPEG-1 has no
15 fps mode, so the conversion also raised the frame rate to 25 fps) and
changed image quality.

## How it works

- The x86 game used RAD's Smacker SDK (`smack.h`, not part of the source
  release) from `Utils/Cinematics.c`: `SmackOpen` on the file inside
  `Intro.slf`, `SmackToBuffer` into the 16-bit frame buffer, `SmackDoFrame`,
  `SmackNextFrame`, `SmackWait`, with audio played through MSS.
- `retro68/Smacker/smacker.c` decodes SMK2/SMK4 video (palette updates,
  mono/full/skip/fill blocks, the three-entry Huffman cache) and the packed
  DPCM audio tracks. It is endian-neutral and has no Mac dependencies.
- `tools/smktest.c` builds the decoder on the host and writes RGB24 frames
  and PCM; for all four intro movies the output is byte-identical to
  FFmpeg's Smacker decoder (video and audio).
- `retro68/ja2/Build/Utils/Cinematics.c` reads the movie from `Intro.slf`
  with a 128 KB sequential read-ahead (SLF files use a 64 KB stdio buffer),
  decodes frames on a `Microseconds()` clock at the movie's own rate,
  decodes but does not show frames when it falls behind, writes the shown
  frame into the window surface through a 256-entry `SDL_MapRGB` lookup and
  streams each frame's audio to a Sound Manager channel (`bufferCmd` plus
  `callBackCmd` per frame, 48 buffers).
- `JA2PPC-Debug.log` gets one `Video: stats` line per movie (frames shown,
  late frames, effective fps x10, dropped audio buffers); profile builds
  record the `movie` (decode incl. file reads), `blit` (palette conversion)
  and `present` zones.

## Measured on the PowerBook G3 333 MHz (Mac OS 9.2.2)

| Movie | QuickTime MPEG-1 | Smacker native |
|---|---|---|
| REBEL_CR (187 frames) | 7.3 fps | 14.4 fps |
| OMERTA (671) | ~5 fps | 14.5 fps |
| PRAGUE_CR (185) | - | 14.9 fps |
| PRAGUE (1928) | 5.5 fps | 14.8 fps |

Source rate is 15 fps; no audio buffers were dropped. Per shown frame:
decode ~9 ms (median), palette conversion ~11 ms, present ~19 ms.

## Open points

- The 604/120 minimum target needs about 3x less work per frame: candidates
  are converting only changed 4x4 blocks and presenting dirty rectangles.
- The endgame movies and any other SMK users go through the same decoder
  once the original `Cinematics.c`/`Intro.c` run on the Mac SGP.
