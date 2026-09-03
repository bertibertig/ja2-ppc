# JA2 PPC Port History

This file records the implementation history from the AI-assisted porting
sessions. Read it before changing the Retro68 target so that previous
diagnostics and failed renderer approaches are not repeated.

## Project Goal

Port the historical Jagged Alliance 2 source to a native classic PowerPC Mac
application that runs under Mac OS 9, with German as the primary language.
The target toolchain is Retro68's classic PowerPC target, not Carbon:

```text
$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
```

Testing is performed with InfiniteMac using a Power Mac G3 B&W / Mac OS 9
configuration. The source repository is an old VC6 Win32/DirectX project and
is not a direct source-compatible Mac application.

## Data Installation

The legally installed GOG version is located at:

```text
$JA2_DATA_DIR/
```

It contains all 22 base `.slf` archives required by the source and the GOG
`Ja2.ini`. The archive names use mixed capitalization. The German archive is
`German.slf` or `GERMAN.SLF`, depending on the copied tree. The game data is
copyrighted and must not be committed or redistributed with the source.

## Build Configuration

`retro68/CMakeLists.txt` currently defaults to:

```cmake
JA2_LANGUAGE=german
JA2_DEBUG_BUILD=ON
```

`JA2_DATA_DIR` points to the installation root and causes the complete `Data/`
directory plus `Ja2.ini` to be copied to a generated `JA2PPC Bundle/`. Build
directories are ignored by `retro68/.gitignore`.

Primary German debug build:

```sh
cmake -S retro68 -B retro68/build-german-debug \
  -DCMAKE_TOOLCHAIN_FILE=$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake \
  -DJA2_DATA_DIR="$JA2_DATA_DIR" \
  -DJA2_LANGUAGE=german \
  -DJA2_DEBUG_BUILD=ON
cmake --build retro68/build-german-debug --clean-first
```

Use `JA2PPC.bin` or `JA2PPC.dsk` in InfiniteMac. The raw `.APPL` output is a
PPC data fork and is not a reliable standalone transfer format because
classic Mac resource metadata may be lost.

## Implemented Milestones

### Initial PPC shell

Added `retro68/ja2_mac.c` and `retro68/ja2_mac.r`:

- Native classic PowerPC APPL through InterfaceLib
- QuickDraw window and event loop
- 640x480 native JA2 client area
- German diagnostic labels
- MacBinary, APPL, and HFS disk outputs

### Data bundling

Added CMake language and data options:

- `JA2_DATA_DIR`
- `JA2_LANGUAGE=english|german`
- `JA2_DEBUG_BUILD=ON|OFF`

The build validates required archives and copies the complete selected data
tree into the deployment bundle.

### SLF reader

Added `retro68/ja2_slf.c` and `retro68/ja2_slf.h`:

- Reads the legacy little-endian SLF directory on big-endian PPC
- Opens HFS-relative and POSIX-style relative data paths
- Handles exact, lowercase, uppercase, and title/mixed-case archive names
- Finds entries by case-insensitive name
- Reads archive data by offset and length

The earlier hard-coded scanner was replaced for the menu asset path by this
reusable reader.

### STI/ETRLE menu asset

The menu asset was identified as:

```text
Data/LoadScreens.slf: MAINMENUBACKGROUND.STI
```

Its observed format is:

- STCI indexed image
- 640x480 pixels
- 256 palette entries
- ETRLE compression
- 310560 stored image bytes
- 311408 byte archive entry

The PPC target parses the header and ETRLE stream and displays temporary
German menu labels over the decoded background.

### Debug diagnostics

Debug builds open a separate `JA2 Debug` window and write:

```text
JA2PPC-Debug.log
```

The log includes startup state, language, archive opening, entry sizes, STCI
dimensions, display-depth results, allocation failures, and malformed ETRLE
runs. Repaint-time diagnostics must not invalidate the diagnostics window or
the application enters an update recursion loop.

### Memory and display setup

The `SIZE` resource was increased from 640 KB to 32 MB after:

```text
NewGWorld failed error=-108
```

`-108` is `MemFullErr`. The application requests 16-bit display depth and the
G3/InfiniteMac test reports:

Display depth request: 16-bit result=0
```

The result `0` means the request succeeded.

## Renderer Investigation

The supplied `Mac_Game_Programming_2002.pdf` was reviewed. The relevant
references are printed pages 78-103, especially:

- 16-bit color pixels, pages 81-82
- PixMaps, pages 83-88
- Offscreen GWorlds, pages 92-100
- `CopyBits()`, pages 102-103
- The PDF index begins at page 857

The book states that pre-Carbon Color QuickDraw uses the following form:

```c
CopyBits((BitMap *)&(theSource->portPixMap),
         (BitMap *)&(theDestination->portPixMap),
         &sourceRect, &destinationRect, srcCopy, nil);
```

The detailed reference notes are in:

```text
resources/Mac_Game_Programming_2002_QuickDraw_Notes.md
```

### Failed approaches

Do not repeat these without a specific reason:

1. Writing directly through `((CWindowPtr)gWindow)->portPixMap` as if it were
   a stable color-window framebuffer. In the Retro68 classic headers,
   `NewCWindow()` returns the legacy `WindowPtr` type. This caused screen
   memory corruption, image placement outside the window, and broken window
   movement.
2. Treating `gWindow->portBits` as a color destination. This produced the
   dithered/corrupted output seen in several InfiniteMac screenshots.
3. Logging from repaint-time functions and invalidating the diagnostics
   window. This caused thousands of repeated log lines and prevented startup
   from progressing.
4. Rendering with one `SetCPixel()` call per visible pixel. It caused visible
   progressive scanlines and was too slow for a final renderer.

The latest committed renderer uses a 16-bit offscreen GWorld and the documented
pre-Carbon `portPixMap` `CopyBits()` form, but it still needs verification in
InfiniteMac for final color fidelity and window movement.

## Commit Milestones

The private repository is:

```text
https://github.com/bertibertig/ja2-ppc
```

Relevant commits, oldest to newest:

- `9838837` Add German Retro68 PPC port target
- `a9e3c72` Document Retro68 port build variants
- `da4b5bb` Add reusable PPC SLF reader
- `9a2bdef` Add German main menu controls
- `404e269` Fix debug window update recursion
- `5472ce5` Log QuickDraw offscreen failures
- `de9c99f` Increase classic Mac application memory
- `a4e933f` Cache rendered German menu background
- `75967e8` Render STI data through QuickDraw palette
- `ec7c66a` Add direct QuickDraw image fallback
- `6a59fec` Request 16-bit classic Mac display
- `a3120c1` Render menu image in 16-bit offscreen buffer
- `dc0a455` Copy menu image to color window pixmap
- `4b5c654` Handle case variants in SLF archives
- `bcd91e8` Handle mixed-case JA2 archive names
- `2fb6fa1` Write menu pixels to native 16-bit framebuffer
- `01cbc39` Restore safe classic window rendering
- `8a97ef2` Map menu framebuffer to movable window
- `9a48012` Fix RGB555 byte order and blit origin
- `c0d3f25` Handle classic Mac window dragging
- `ce8611f` Apply documented classic Color QuickDraw blit

The later renderer corrections may supersede earlier experiments; inspect the
current source and the most recent commit before relying on this list.

## Current Status

The application can start as a native German PPC diagnostic/menu shell and can
read the real menu background archive. It does not yet boot the complete JA2
engine.

Remaining work:

- Establish a correct, tested Classic Color QuickDraw framebuffer transfer
- Add repaint-safe cached rendering after window movement
- Load original menu button graphics and `MainMenu.edt`
- Port the original menu state machine and actions
- Port SMK/Smacker playback for the intro videos in `Intro.slf`
- Add intro sequencing and keyboard/mouse skipping
- Connect initialization to intro completion and `MAINMENU_SCREEN`
- Port audio, timers, file paths, save games, and remaining Win32 APIs

## Working Method

Keep German debug builds as the default while diagnosing. For every meaningful
milestone:

1. Build with `JA2_DEBUG_BUILD=ON`.
2. Test the resulting `.bin` or `.dsk` in InfiniteMac.
3. Record the relevant log and screenshot result.
4. Commit with a concise milestone message.
5. Push to the `private` remote.

Do not commit generated build directories, GOG data, debug logs, or emulator
artifacts.
