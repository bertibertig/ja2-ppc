#!/usr/bin/env bash
set -euo pipefail

# Build, stage, and smoke-test the current PPC application. DingusPPC boots a
# classic Mac disk image, so the runtime test is separate from bundle staging.

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SOURCE_DIR="$ROOT_DIR/retro68"
BUILD_DIR="${JA2_BUILD_DIR:-$SOURCE_DIR/build-dingus-debug}"
DEPLOY_DIR="${JA2_DEPLOY_DIR:-$ROOT_DIR/.dingusppc/deploy}"
LANGUAGE="${JA2_LANGUAGE:-german}"
DATA_DIR="${JA2_DATA_DIR:-}"
TOOLCHAIN_FILE="${RETRO68_TOOLCHAIN_FILE:-$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake}"
CONFIG_FILE="$ROOT_DIR/opencode.json"

if [[ -n "${DINGUS_ROOT:-}" ]]; then
    :
elif command -v jq >/dev/null 2>&1; then
    DINGUS_ROOT=$(jq -r '.references.dingusppcRepo.path // empty' "$CONFIG_FILE")
else
    DINGUS_ROOT=$DINGUS_ROOT
fi

DINGUS_BIN="${DINGUS_BIN:-$DINGUS_ROOT/build/bin/dingusppc}"
DINGUS_BOOTROM="${DINGUS_BOOTROM:-}"
DINGUS_HDD_IMAGE="${DINGUS_HDD_IMAGE:-}"
DINGUS_TIMEOUT="${DINGUS_TIMEOUT:-30}"

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

build() {
    [[ -f "$TOOLCHAIN_FILE" ]] || die "Retro68 toolchain not found: $TOOLCHAIN_FILE"
    local configure_args=(
        -S "$SOURCE_DIR" -B "$BUILD_DIR"
        "-DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE"
        "-DJA2_LANGUAGE=$LANGUAGE" -DJA2_DEBUG_BUILD=ON
    )
    if [[ -n "$DATA_DIR" ]]; then
        configure_args+=("-DJA2_DATA_DIR=$DATA_DIR")
    fi
    cmake "${configure_args[@]}"
    cmake --build "$BUILD_DIR" --clean-first
}

deploy() {
    local bundle="$BUILD_DIR/JA2PPC Bundle"
    [[ -d "$bundle" ]] || die "debug bundle was not generated: $bundle"
    [[ -f "$BUILD_DIR/JA2PPC.bin" ]] || die "JA2PPC.bin was not generated"
    [[ -f "$BUILD_DIR/JA2PPC.dsk" ]] || die "JA2PPC.dsk was not generated"

    rm -rf "$DEPLOY_DIR"
    mkdir -p "$DEPLOY_DIR"
    cp -a "$bundle" "$DEPLOY_DIR/JA2PPC Bundle"
    cp -a "$BUILD_DIR/JA2PPC.bin" "$DEPLOY_DIR/JA2PPC.bin"
    cp -a "$BUILD_DIR/JA2PPC.dsk" "$DEPLOY_DIR/JA2PPC.dsk"
    printf 'deployed debug build to %s\n' "$DEPLOY_DIR"
}

resolve_bootrom() {
    if [[ -n "$DINGUS_BOOTROM" ]]; then
        [[ -f "$DINGUS_BOOTROM" ]] || die "DingusPPC BootROM not found: $DINGUS_BOOTROM"
        return
    fi
    local candidate
    shopt -s nullglob
    local candidates=("$DINGUS_ROOT/build/bin"/*.ROM "$DINGUS_ROOT/build/bin"/*.rom)
    shopt -u nullglob
    [[ ${#candidates[@]} -gt 0 ]] || die "set DINGUS_BOOTROM to a DingusPPC BootROM file"
    DINGUS_BOOTROM=${candidates[0]}
}

run_emulator() {
    [[ -x "$DINGUS_BIN" ]] || die "DingusPPC executable not found: $DINGUS_BIN"
    [[ -n "$DINGUS_HDD_IMAGE" ]] || die "set DINGUS_HDD_IMAGE to a bootable Mac OS disk image"
    [[ -f "$DINGUS_HDD_IMAGE" ]] || die "DingusPPC disk image not found: $DINGUS_HDD_IMAGE"
    resolve_bootrom
    mkdir -p "$DEPLOY_DIR/logs"
    printf 'starting DingusPPC for %s seconds\n' "$DINGUS_TIMEOUT"
    set +e
    timeout --signal=INT --kill-after=5s "${DINGUS_TIMEOUT}s" \
        "$DINGUS_BIN" -r -b "$DINGUS_BOOTROM" -m "${DINGUS_MACHINE:-pmg3dt}" \
        --workingdir "$DEPLOY_DIR" --hdd_img "$DINGUS_HDD_IMAGE" \
        --log-to-stderr --log-no-uptime 2>&1 | tee "$DEPLOY_DIR/logs/dingusppc.log"
    local status=${PIPESTATUS[0]}
    set -e
    if [[ $status -ne 0 && $status -ne 124 && $status -ne 130 ]]; then
        die "DingusPPC exited with status $status"
    fi
    printf 'DingusPPC smoke test completed; log: %s\n' "$DEPLOY_DIR/logs/dingusppc.log"
}

test_host() {
    [[ -x "$DINGUS_BIN" ]] || die "DingusPPC executable not found: $DINGUS_BIN"
    "$DINGUS_BIN" list machines >/dev/null
    [[ -f "$BUILD_DIR/JA2PPC.bin" && -f "$BUILD_DIR/JA2PPC.dsk" ]] || \
        die "build artifacts are missing; run '$0 build' first"
    printf 'host validation passed\n'
    if [[ -n "$DINGUS_HDD_IMAGE" ]]; then
        run_emulator
    else
        printf 'runtime smoke test skipped; set DINGUS_HDD_IMAGE to enable it\n'
    fi
}

case "${1:-all}" in
    build) build ;;
    deploy) deploy ;;
    test) test_host ;;
    run) run_emulator ;;
    all) build; deploy; test_host ;;
    *) die "usage: $0 [build|deploy|test|run|all]" ;;
esac
