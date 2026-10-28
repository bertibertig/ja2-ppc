#!/usr/bin/env bash
set -euo pipefail

# Configure and build the German System 9 diagnostics target. Defaults are
# relative to this repository so the script can be run from any directory.

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
SOURCE_DIR="$ROOT_DIR/retro68"
# Machine-specific paths come from the git-ignored local.env.
. "$SCRIPT_DIR/local-env.sh"
BUILD_DIR="${JA2_BUILD_DIR:-$SOURCE_DIR/build-german-debug}"
DATA_DIR="${JA2_DATA_DIR:?set JA2_DATA_DIR in local.env (see local.env.example)}"
TOOLCHAIN_FILE="${RETRO68_TOOLCHAIN_FILE:-${RETRO68_TOOLCHAIN:?set RETRO68_TOOLCHAIN in local.env}/powerpc-apple-macos/cmake/retroppc.toolchain.cmake}"

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

configure() {
    [[ -f "$TOOLCHAIN_FILE" ]] || die "Retro68 toolchain not found: $TOOLCHAIN_FILE"
    [[ -d "$DATA_DIR/Data" ]] || die "JA2 data directory not found: $DATA_DIR/Data"
    [[ -f "$DATA_DIR/Ja2.ini" ]] || die "Ja2.ini not found: $DATA_DIR/Ja2.ini"

    cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" \
        "-DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE" \
        "-DJA2_DATA_DIR=$DATA_DIR" \
        -DJA2_LANGUAGE=german \
        -DJA2_DEBUG_BUILD=ON
}

build() {
    [[ -f "$BUILD_DIR/CMakeCache.txt" ]] || configure
    cmake --build "$BUILD_DIR"
}

case "${1:-all}" in
    configure) configure ;;
    build) build ;;
    all) configure; build ;;
    *) die "usage: $0 [configure|build|all]" ;;
esac

printf 'German debug application: %s\n' "$BUILD_DIR/JA2PPC.bin"
printf 'Infinite Mac data bundle: %s\n' "$BUILD_DIR/JA2PPC Bundle"
