# QuickDraw Renderer Notes

Reference: `Mac_Game_Programming_2002.pdf` in this directory.

## Relevant Sections

- Chapter 4, “Introduction to Macintosh Graphics”, printed pages 78-103
- “16-Bit Color Pixels”, printed pages 81-82
- “Pixel Maps”, printed pages 83-88
- “Offscreen GWorlds”, printed pages 92-100
- “The CopyBits() Function”, printed pages 102-103
- The index begins at PDF page 857 and includes entries for `CopyBits`,
  `GWorld`, `LockPixels`, `PixMap`, and `SetGWorld`.

## Porting Rules Applied

1. `RGBColor` stores each component as a 16-bit value. QuickDraw takes the
   highest five bits of each component for a 16-bit direct-color pixel.
2. A 16-bit color pixel is RGB555: bits 11-15 red, bits 6-10 green, bits 1-5
   blue, with the low bit unused. This is the “Thousands” display mode used
   by the System 9 test machine.
3. An offscreen GWorld has its own graphics port and local coordinate system.
   The recommended sequence is to save the current port, call `SetGWorld`,
   lock the GWorld PixMap when accessing pixels, draw, unlock, restore the
   previous port, and then copy the completed image to the window.
4. In pre-Carbon QuickDraw, `CopyBits()` takes `BitMap *` parameters even for
   color ports. The book explicitly shows casting the source and destination
   `portPixMap` fields to `BitMap *`. Passing a legacy `GrafPort.portBits`
   field for a color window can produce dithered or corrupted output.
5. `CopyBits()` should use `srcCopy` for an unmodified copy. Source and
   destination rectangles are local to their respective ports.
6. The window manager owns window movement. Application mouse handling must
   pass an `inDrag` hit to `DragWindow()` rather than drawing at global screen
   coordinates.

## Application Findings

The JA2 menu asset is an indexed STCI image with ETRLE compression:

- `640x480` pixels
- `256` palette entries
- `310560` stored image bytes
- Image file: `MAINMENUBACKGROUND.STI`
- Archive: `Data/LoadScreens.slf`

The PPC port must therefore keep the STI palette and image decoding separate
from the QuickDraw destination. It must not assume that the x86 Windows
surface layout or `GrafPort.portBits` is equivalent to a color `PixMap`.

## Current Implementation Decision

The renderer uses a 16-bit offscreen GWorld, renders the decoded image in
that local port, and copies it to the color window through its `portPixMap`.
The window is created at the native JA2 resolution of `640x480`; the title
bar and frame are outside the client rectangle. The German debug build also
requests 16-bit display depth and logs the selected mode and asset stages.
