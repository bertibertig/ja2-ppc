#!/usr/bin/env bash
set -euo pipefail

# Functional smoke tests of a JA2 PPC build in a Mac OS 9 emulator. The build
# is copied with hfsutils into the boot volume's Startup Items folder of a
# private working copy of the OS image, so the game starts by itself after
# boot. After the run, debug log and profiler CSV are copied back out.
# Emulator results never replace the real-hardware test (mac-hw-test skill).
#
#   tools/os9-emu.sh [prepare|deploy|run|collect|all|reset]
#
# Engines (EMU_ENGINE):
#   dingus  DingusPPC, Beige G3 (pmg3dt), Old World ROM, Mac OS 9.1 image
#   qemu    QEMU mac99 (New World), needs a Mac OS 9.2.2 image; QEMU also
#           writes guest screenshots via its monitor (EMU_SCREENSHOTS)

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ENGINE="${EMU_ENGINE:-dingus}"
BUILD_DIR="${JA2_BUILD_DIR:-$ROOT_DIR/retro68/build-german-debug}"
WORK_DIR="${EMU_WORK_DIR:-$EMU_WORK_DIR}"
OUT_DIR="${EMU_OUT_DIR:-$ROOT_DIR/.emu-out}"
TIMEOUT="${EMU_TIMEOUT:-240}"
HFS_BIN="${HFS_BIN:-$RETRO68_TOOLCHAIN/bin}"
DINGUS_ROOT="${DINGUS_ROOT:-$DINGUS_ROOT}"
DINGUS_BIN="${DINGUS_BIN:-$DINGUS_ROOT/build/bin/dingusppc}"
DINGUS_BOOTROM="${DINGUS_BOOTROM:-$DINGUS_ROOT/build/bin/1997-11 - 78F57389 - Power Mac G3 (v3).ROM}"
QEMU_BIN="${QEMU_BIN:-qemu-system-ppc}"
EMU_SCREENSHOTS="${EMU_SCREENSHOTS:-20}"

case "$ENGINE" in
    dingus) BASE_IMAGE="${EMU_BASE_IMAGE:-$VM_DIR/Powermac G3/macos91.img}" ;;
    qemu) BASE_IMAGE="${EMU_BASE_IMAGE:-$EMU_WORK_DIR/base/macos922.img}" ;;
    *) printf 'error: EMU_ENGINE must be dingus or qemu\n' >&2; exit 1 ;;
esac
WORK_IMAGE="$WORK_DIR/$ENGINE-work.img"
DATA_STAMP="$WORK_DIR/$ENGINE-data.stamp"
# hfsutils keeps its "current volume" state here; keep it out of $HOME.
export HOME_HFS_STATE="$WORK_DIR/.hfs"

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

hfs() {
    local tool=$1
    shift
    HOME="$HOME_HFS_STATE" "$HFS_BIN/$tool" "$@"
}

mount_work() {
    mkdir -p "$HOME_HFS_STATE"
    hfs hmount "$WORK_IMAGE" >/dev/null
}

unmount_work() {
    hfs humount >/dev/null 2>&1 || true
}

# The Startup Items folder is localized ("Startobjekte" on German systems).
startup_folder() {
    local system name
    for system in "System Folder" "Systemordner"; do
        for name in "Startup Items" "Startobjekte"; do
            # hls exits 0 even for a missing path; it prints the path only
            # when it exists.
            if [[ -n "$(hfs hls -d ":$system:$name" 2>/dev/null)" ]]; then
                printf ':%s:%s' "$system" "$name"
                return
            fi
        done
    done
    die "no Startup Items folder found on $WORK_IMAGE"
}

prepare() {
    [[ -f "$BASE_IMAGE" ]] || die "base OS image not found: $BASE_IMAGE"
    mkdir -p "$WORK_DIR"
    if [[ ! -f "$WORK_IMAGE" ]]; then
        printf 'copying %s -> %s\n' "$BASE_IMAGE" "$WORK_IMAGE"
        cp --sparse=always "$BASE_IMAGE" "$WORK_IMAGE"
        rm -f "$DATA_STAMP"
    fi
}

copy_tree() {
    local source=$1 target=$2 entry name
    hfs hmkdir "$target" 2>/dev/null || true
    for entry in "$source"/*; do
        name=$(basename -- "$entry")
        if [[ -d "$entry" ]]; then
            copy_tree "$entry" "$target:$name"
        else
            hfs hcopy -r "$entry" "$target:$name"
        fi
    done
}

deploy() {
    local bundle="$BUILD_DIR/JA2PPC Bundle" folder
    [[ -f "$WORK_IMAGE" ]] || die "run '$0 prepare' first"
    [[ -f "$BUILD_DIR/JA2PPC.bin" ]] || die "missing $BUILD_DIR/JA2PPC.bin; build first"
    [[ -d "$bundle/Data" ]] || die "missing $bundle/Data; configure with JA2_DATA_DIR"
    mount_work
    trap unmount_work EXIT
    folder=$(startup_folder)
    hfs hdel "$folder:JA2PPC" "$folder:JA2PPC-Debug.log" "$folder:JA2PROF.csv" \
        "$folder:options.cfg" 2>/dev/null || true
    hfs hcopy -m "$BUILD_DIR/JA2PPC.bin" "$folder:JA2PPC"
    hfs hcopy -r "${EMU_OPTIONS:-$bundle/options.cfg}" "$folder:options.cfg"
    hfs hcopy -r "$bundle/Ja2.ini" "$folder:Ja2.ini" 2>/dev/null || \
        { hfs hdel "$folder:Ja2.ini"; hfs hcopy -r "$bundle/Ja2.ini" "$folder:Ja2.ini"; }
    if [[ ! -f "$DATA_STAMP" || "$bundle/.bundle-complete" -nt "$DATA_STAMP" ]]; then
        printf 'copying Data/ into the image (once per data change)\n'
        hfs hdel -r "$folder:Data" 2>/dev/null || true
        copy_tree "$bundle/Data" "$folder:Data"
        touch "$DATA_STAMP"
    fi
    unmount_work
    trap - EXIT
    printf 'deployed %s into %s%s\n' "$BUILD_DIR/JA2PPC.bin" "$WORK_IMAGE" "$folder"
}

run_dingus() {
    [[ -x "$DINGUS_BIN" ]] || die "DingusPPC not found: $DINGUS_BIN"
    [[ -f "$DINGUS_BOOTROM" ]] || die "BootROM not found: $DINGUS_BOOTROM"
    timeout --signal=INT --kill-after=10s "${TIMEOUT}s" \
        "$DINGUS_BIN" -r -b "$DINGUS_BOOTROM" -m "${DINGUS_MACHINE:-pmg3dt}" \
        --workingdir "$WORK_DIR" --hdd_img "$WORK_IMAGE" \
        --log-to-stderr --log-no-uptime >"$OUT_DIR/emulator.log" 2>&1 || true
}

run_qemu() {
    local monitor="$WORK_DIR/qemu-monitor.sock" pid shot interval
    command -v "$QEMU_BIN" >/dev/null || die "$QEMU_BIN not installed (pacman -S qemu-system-ppc)"
    rm -f "$monitor"
    "$QEMU_BIN" -M mac99,via=pmu -m 512 -boot c \
        -drive "file=$WORK_IMAGE,format=raw,media=disk" \
        -device usb-kbd -device usb-mouse -g 800x600x32 \
        -display "${QEMU_DISPLAY:-none}" -monitor "unix:$monitor,server,nowait" \
        >"$OUT_DIR/emulator.log" 2>&1 &
    pid=$!
    interval=$(( TIMEOUT / (EMU_SCREENSHOTS > 0 ? EMU_SCREENSHOTS : 1) ))
    for (( shot = 1; shot <= EMU_SCREENSHOTS; ++shot )); do
        sleep "$interval"
        kill -0 "$pid" 2>/dev/null || break
        printf 'screendump %s/screen-%02d.ppm\n' "$OUT_DIR" "$shot" | \
            python3 -I -c 'import socket,sys; s=socket.socket(socket.AF_UNIX); s.connect(sys.argv[1]); s.sendall(sys.stdin.buffer.read()); s.settimeout(2)
try: s.recv(4096)
except OSError: pass' "$monitor" || true
    done
    (( EMU_SCREENSHOTS > 0 )) || sleep "$TIMEOUT"
    kill -INT "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    if command -v ffmpeg >/dev/null; then
        for shot in "$OUT_DIR"/screen-*.ppm; do
            [[ -f "$shot" ]] || continue
            ffmpeg -loglevel error -y -i "$shot" "${shot%.ppm}.png" && rm -f "$shot"
        done
    fi
}

run() {
    [[ -f "$WORK_IMAGE" ]] || die "run '$0 prepare' and '$0 deploy' first"
    rm -rf "$OUT_DIR"
    mkdir -p "$OUT_DIR"
    printf 'booting %s for %ss (game starts from Startup Items)\n' "$ENGINE" "$TIMEOUT"
    case "$ENGINE" in
        dingus) run_dingus ;;
        qemu) run_qemu ;;
    esac
}

collect() {
    local folder file
    mkdir -p "$OUT_DIR"
    mount_work
    trap unmount_work EXIT
    folder=$(startup_folder)
    for file in JA2PPC-Debug.log JA2PROF.csv; do
        if hfs hcopy -r "$folder:$file" "$OUT_DIR/$file" 2>/dev/null; then
            printf 'collected %s\n' "$OUT_DIR/$file"
        fi
    done
    unmount_work
    trap - EXIT
    [[ -f "$OUT_DIR/JA2PPC-Debug.log" ]] || \
        printf 'no JA2PPC-Debug.log: the game did not start or did not get that far\n'
}

case "${1:-all}" in
    prepare) prepare ;;
    deploy) deploy ;;
    run) run ;;
    collect) collect ;;
    all) prepare; deploy; run; collect ;;
    reset) rm -f "$WORK_IMAGE" "$DATA_STAMP"; prepare ;;
    *) die "usage: $0 [prepare|deploy|run|collect|all|reset]" ;;
esac
