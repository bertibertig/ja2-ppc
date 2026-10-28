#!/usr/bin/env bash
set -euo pipefail

# Builds the ported Standard Gaming Platform core and its self-test
# (retro68/ja2_sgp_selftest.c) for the build host and runs it against a
# JA2 bundle (default: retro68/build-german-debug/JA2PPC Bundle). The test
# does the same checks the "sgptest" autotest command does on the Mac.

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUNDLE="${1:-$ROOT_DIR/retro68/build-german-debug/JA2PPC Bundle}"
OUT="${SGP_HOST_OUT:-${TMPDIR:-/tmp}/ja2-sgp-host}"
SGP="$ROOT_DIR/retro68/Standard Gaming Platform"

mkdir -p "$OUT"
python3 "$ROOT_DIR/tools/gen_case_includes.py" "$OUT/case" "$SGP" "$ROOT_DIR/retro68" >/dev/null
cat >"$OUT/main.c" <<'MAIN'
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "ja2_sgp_selftest.h"
void *LockFrameBuffer(unsigned int *pitch);
void UnlockFrameBuffer(void);
void DebugLog(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    fputs("log: ", stderr);
    vfprintf(stderr, format, arguments);
    fputc('\n', stderr);
    va_end(arguments);
}
static void Report(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    vprintf(format, arguments);
    putchar('\n');
    va_end(arguments);
}
int main(void)
{
    int failures = JA2SGPSelfTest(Report);
    failures += JA2SGPGraphicsSelfTest(Report);
    if (getenv("SGP_DUMP")) {
        /* Raw 640x480 RGB555 frame buffer for tools/sgp_reference.py --compare. */
        unsigned int pitch, row;
        unsigned char *fb = LockFrameBuffer(&pitch);
        FILE *out = fopen(getenv("SGP_DUMP"), "wb");
        for (row = 0; out && row < 480; ++row)
            fwrite(fb + row * pitch, 2, 640, out);
        if (out) fclose(out);
        UnlockFrameBuffer();
    }
    return failures != 0;
}
MAIN
cc -std=gnu99 -fshort-wchar -fcommon -w -O1 -DJA2 -DJA2_PPC -DJA2_LANGUAGE_GERMAN \
    -iquote "$SGP" -iquote "$ROOT_DIR/retro68" -iquote "$OUT/case" \
    -o "$OUT/sgptest" "$OUT/main.c" "$ROOT_DIR/retro68/ja2_sgp_selftest.c" \
    "$SGP/sgp_win32.c" "$SGP/FileMan.c" "$SGP/LibraryDataBase.c" "$SGP/Container.c" \
    "$SGP/Random.c" "$SGP/timer.c" "$SGP/sgp_ddraw.c" "$SGP/sgp_video_game.c" \
    "$SGP/himage.c" "$SGP/STCI.c" "$SGP/impTGA.c" "$SGP/Compression.c" "$SGP/shading.c" \
    "$SGP/vobject.c" "$SGP/vobject_blitters.c" "$SGP/vsurface.c" "$SGP/video.c" \
    "$SGP/DirectDraw Calls.c" "$SGP/DirectX Common.c" "$SGP/Mutex Manager.c" \
    -x c "$SGP/DEBUG.C" "$SGP/PCX.C"
SGP_ROOT="$BUNDLE" "$OUT/sgptest"
