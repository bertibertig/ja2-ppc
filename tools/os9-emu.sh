#!/usr/bin/env bash
set -euo pipefail

# Functional smoke tests of a JA2 PPC build in a Mac OS 9 emulator. The build
# is copied with hfsutils into the boot volume's Startup Items folder of a
# private working copy of the OS image, so the game starts by itself after
# boot. After the run, debug log and profiler CSV are copied back out.
# Emulator results never replace the real-hardware test (mac-hw-test skill).
#
#   tools/os9-emu.sh [prepare|deploy|run|collect|all|reset|install|gui]
#
# Engines (EMU_ENGINE):
#   qemu    QEMU mac99 (New World), Mac OS 9.2.2, mirrors the PowerBook G3
#   dingus  DingusPPC Power Mac 7600/120 (Old World, 604), Mac OS 8.6: the
#           low-end target and the only emulator that boots 8.x
# Both write guest-only screenshots (EMU_SCREENSHOTS): QEMU via its monitor,
# DingusPPC by capturing its own XWayland window, never the host desktop.

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
# Machine-specific paths come from the git-ignored local.env.
. "$ROOT_DIR/tools/local-env.sh"
ENGINE="${EMU_ENGINE:-qemu}"
BUILD_DIR="${JA2_BUILD_DIR:-$ROOT_DIR/retro68/build-german-debug}"
WORK_DIR="${EMU_WORK_DIR:?set EMU_WORK_DIR in local.env (see local.env.example)}"
OUT_DIR="${EMU_OUT_DIR:-$ROOT_DIR/.emu-out}"
TIMEOUT="${EMU_TIMEOUT:-240}"
HFS_BIN="${HFS_BIN:-${RETRO68_TOOLCHAIN:?set RETRO68_TOOLCHAIN in local.env}/bin}"
DINGUS_ROOT="${DINGUS_ROOT:-}"
DINGUS_BIN="${DINGUS_BIN:-$DINGUS_ROOT/build/bin/dingusppc}"
DINGUS_BASE_DIR="${DINGUS_BASE_DIR:-$WORK_DIR/base86}"
DINGUS_BOOTROM="${DINGUS_BOOTROM:-$DINGUS_BASE_DIR/pm7600.rom}"
# Low-end target: Power Mac 7600/120 (604 at 120 MHz), 64 MB, onboard
# "control" video with 2 MB VRAM (the smallest DingusPPC emulates; AGENTS.md
# asks for 1 MB) at 800x600 thousands (set in the guest's Monitors panel and
# kept in PRAM). DingusPPC interprets instructions, so timing means nothing.
# The pm8500 is avoided: its extra AV video card opens a second window.
DINGUS_MACHINE=(-m pm7600 --cpu 604 --rambank1_size 64 --mon_id VGA-SVGA
    --gfxmem_size 2)
QEMU_BIN="${QEMU_BIN:-qemu-system-ppc}"
EMU_SCREENSHOTS="${EMU_SCREENSHOTS:-20}"
INSTALL_CD="${EMU_INSTALL_CD:-}"
# Mirror the PowerBook G3 Lombard test machine as far as mac99 allows:
# G3 (750, no AltiVec), PMU, 384 MB, 1024x768 at 16-bit (the LCD panel).
# Not reproducible: 333 MHz clock, ATI Rage LT Pro (QEMU VGA instead),
# internal ADB keyboard/trackpad (USB devices instead; Lombard has USB too).
QEMU_MACHINE=(-M mac99,via=pmu -cpu 750 -m 384 -g 1024x768x16
    -device usb-kbd -device usb-mouse)

case "$ENGINE" in
    dingus) BASE_IMAGE="${EMU_BASE_IMAGE:-$DINGUS_BASE_DIR/macos86.img}" ;;
    qemu) BASE_IMAGE="${EMU_BASE_IMAGE:-$WORK_DIR/base/macos922.img}" ;;
    *) printf 'error: EMU_ENGINE must be dingus or qemu\n' >&2; exit 1 ;;
esac
WORK_IMAGE="$WORK_DIR/$ENGINE-work.img"
DATA_STAMP="$WORK_DIR/$ENGINE-data.stamp"
# DingusPPC keeps PRAM/NVRAM (screen resolution, startup disk) in its
# working directory; test runs use a copy of the base image's state.
DINGUS_STATE="$WORK_DIR/dingus-state"
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
        if [[ "$ENGINE" == dingus ]]; then
            rm -rf "$DINGUS_STATE"
            mkdir -p "$DINGUS_STATE"
            cp "$(dirname "$BASE_IMAGE")"/{nvram,pram}.bin "$DINGUS_STATE"/ 2>/dev/null || true
        fi
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

# Screenshots written by an autotest script (<name>.bmp beside the app).
guest_shots() {
    hfs hls -1 "$1" 2>/dev/null | grep -i '\.bmp$' || true
}

deploy() {
    local bundle="$BUILD_DIR/JA2PPC Bundle" folder options script file
    [[ -f "$WORK_IMAGE" ]] || die "run '$0 prepare' first"
    [[ -f "$BUILD_DIR/JA2PPC.bin" ]] || die "missing $BUILD_DIR/JA2PPC.bin; build first"
    [[ -d "$bundle/Data" ]] || die "missing $bundle/Data; configure with JA2_DATA_DIR"
    mount_work
    trap unmount_work EXIT
    folder=$(startup_folder)
    hfs hdel "$folder:JA2PPC" "$folder:JA2PPC-Debug.log" "$folder:JA2PROF.csv" \
        "$folder:JA2AUTOTEST.log" "$folder:options.cfg" 2>/dev/null || true
    for file in $(guest_shots "$folder"); do
        hfs hdel "$folder:$file" 2>/dev/null || true
    done
    hfs hcopy -m "$BUILD_DIR/JA2PPC.bin" "$folder:JA2PPC"
    options="${EMU_OPTIONS:-$bundle/options.cfg}"
    if [[ -n "${EMU_AUTOTEST:-}" ]]; then
        # Same scripts as tools/machw.py autotest on the PowerBook.
        script=$(basename -- "$EMU_AUTOTEST")
        hfs hdel "$folder:$script" 2>/dev/null || true
        hfs hcopy -r "$EMU_AUTOTEST" "$folder:$script"
        mkdir -p "$WORK_DIR/tmp"
        { grep -v '^autotest=' "$options"; printf 'autotest=%s\n' "$script"; } \
            >"$WORK_DIR/tmp/options.cfg"
        options="$WORK_DIR/tmp/options.cfg"
    fi
    hfs hcopy -r "$options" "$folder:options.cfg"
    hfs hcopy -r "$bundle/Ja2.ini" "$folder:Ja2.ini" 2>/dev/null || \
        { hfs hdel "$folder:Ja2.ini"; hfs hcopy -r "$bundle/Ja2.ini" "$folder:Ja2.ini"; }
    if [[ ! -f "$DATA_STAMP" || "$bundle/.bundle-complete" -nt "$DATA_STAMP" ]]; then
        printf 'copying Data/ into the image (once per data change)\n'
        hfs hdel -r "$folder:Data" 2>/dev/null || true
        copy_tree "$bundle/Data" "$folder:Data"
        touch "$DATA_STAMP"
    fi
    # The Finder opens every visible item in Startup Items at boot (text
    # files in SimpleText, folders as windows); only the app may stay visible.
    for file in options.cfg Ja2.ini Data ${script:+"$script"}; do
        hfs hattrib +i "$folder:$file" 2>/dev/null || true
    done
    unmount_work
    trap - EXIT
    printf 'deployed %s into %s%s\n' "$BUILD_DIR/JA2PPC.bin" "$WORK_IMAGE" "$folder"
}

run_dingus() {
    local pid shot interval window
    [[ -x "$DINGUS_BIN" ]] || die "DingusPPC not found: $DINGUS_BIN (set DINGUS_ROOT in local.env)"
    [[ -f "$DINGUS_BOOTROM" ]] || die "BootROM not found: $DINGUS_BOOTROM"
    mkdir -p "$DINGUS_STATE"
    # X11 (XWayland) so the window can be captured on its own.
    SDL_VIDEODRIVER=x11 "$DINGUS_BIN" -r -b "$DINGUS_BOOTROM" "${DINGUS_MACHINE[@]}" \
        --workingdir "$DINGUS_STATE" --hdd_img "$WORK_IMAGE" \
        --log-to-stderr --log-no-uptime >"$OUT_DIR/emulator.log" 2>&1 &
    pid=$!
    interval=$(( TIMEOUT / (EMU_SCREENSHOTS > 0 ? EMU_SCREENSHOTS : 1) ))
    for (( shot = 1; shot <= EMU_SCREENSHOTS; ++shot )); do
        sleep "$interval"
        kill -0 "$pid" 2>/dev/null || break
        window=$(xdotool search --pid "$pid" --name "DingusPPC Display" 2>/dev/null | head -1) || true
        [[ -n "$window" ]] && import -window "$window" \
            "$(printf '%s/screen-%02d.png' "$OUT_DIR" "$shot")" 2>/dev/null || true
    done
    (( EMU_SCREENSHOTS > 0 )) || sleep "$TIMEOUT"
    kill -INT "$pid" 2>/dev/null || true
    sleep 5
    kill -9 "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
}

run_qemu() {
    local monitor="$WORK_DIR/qemu-monitor.sock" pid shot interval
    command -v "$QEMU_BIN" >/dev/null || die "$QEMU_BIN not installed (pacman -S qemu-system-ppc)"
    rm -f "$monitor"
    "$QEMU_BIN" "${QEMU_MACHINE[@]}" -boot c \
        -drive "file=$WORK_IMAGE,format=raw,media=disk" \
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

# Interactive: boot the Mac OS install CD with the QEMU base image attached so
# the user can initialize it (Mac OS Standard/HFS, which hfsutils can write)
# and install Mac OS. Creates an empty 4 GB base image if none exists.
install_os() {
    [[ -f "$INSTALL_CD" ]] || die "install CD not found; set EMU_INSTALL_CD in local.env"
    mkdir -p "$(dirname "$BASE_IMAGE")"
    [[ -f "$BASE_IMAGE" ]] || truncate -s 4G "$BASE_IMAGE"
    run_qemu_gui -boot d \
        -drive "file=$INSTALL_CD,format=raw,media=cdrom,readonly=on"
}

# Interactive boot of the base image (no CD), for one-off guest settings.
# Delete the working copy afterwards ('reset') so tests pick the change up.
gui() {
    [[ -f "$BASE_IMAGE" ]] || die "base OS image not found: $BASE_IMAGE"
    if [[ "$ENGINE" == dingus ]]; then
        # Native SDL driver: it locks the pointer (Ctrl+G) under Wayland.
        "$DINGUS_BIN" -r -b "$DINGUS_BOOTROM" "${DINGUS_MACHINE[@]}" \
            --workingdir "$(dirname "$BASE_IMAGE")" --hdd_img "$BASE_IMAGE"
    else
        run_qemu_gui -boot c
    fi
}

# The guest only has a relative USB mouse, so QEMU must lock the pointer.
# Under KDE Wayland the GTK UI loses the pointer (natively and via XWayland);
# the SDL UI (qemu-ui-sdl) locks it properly, so prefer it when installed.
run_qemu_gui() {
    local display=gtk,grab-on-hover=on
    [[ "$ENGINE" == qemu ]] || die "install needs EMU_ENGINE=qemu"
    command -v "$QEMU_BIN" >/dev/null || die "$QEMU_BIN not installed (pacman -S qemu-system-ppc)"
    [[ -f /usr/lib/qemu/ui-sdl.so ]] && display=sdl
    "$QEMU_BIN" "${QEMU_MACHINE[@]}" "$@" \
        -drive "file=$BASE_IMAGE,format=raw,media=disk" \
        -display "${QEMU_DISPLAY:-$display}" \
        -monitor "unix:$WORK_DIR/qemu-install.sock,server,nowait"
}

collect() {
    local folder file
    mkdir -p "$OUT_DIR"
    mount_work
    trap unmount_work EXIT
    folder=$(startup_folder)
    for file in JA2PPC-Debug.log JA2PROF.csv JA2AUTOTEST.log; do
        if hfs hcopy -r "$folder:$file" "$OUT_DIR/$file" 2>/dev/null; then
            printf 'collected %s\n' "$OUT_DIR/$file"
        fi
    done
    for file in $(guest_shots "$folder"); do
        hfs hcopy -r "$folder:$file" "$OUT_DIR/$file" &&
            ffmpeg -loglevel error -y -i "$OUT_DIR/$file" "$OUT_DIR/${file%.*}.png" &&
            rm -f "$OUT_DIR/$file" && printf 'collected %s\n' "$OUT_DIR/${file%.*}.png"
    done
    unmount_work
    trap - EXIT
    [[ -f "$OUT_DIR/JA2PPC-Debug.log" ]] || \
        printf 'no JA2PPC-Debug.log: the game did not start or did not get that far\n'
    if [[ -f "$OUT_DIR/JA2AUTOTEST.log" ]]; then
        tr '\r' '\n' <"$OUT_DIR/JA2AUTOTEST.log" | tail -n 1
    fi
}

case "${1:-all}" in
    prepare) prepare ;;
    deploy) deploy ;;
    run) run ;;
    collect) collect ;;
    all) prepare; deploy; run; collect ;;
    reset) rm -f "$WORK_IMAGE" "$DATA_STAMP"; rm -rf "$DINGUS_STATE"; prepare ;;
    install) install_os ;;
    gui) gui ;;
    *) die "usage: $0 [prepare|deploy|run|collect|all|reset|install|gui]" ;;
esac
