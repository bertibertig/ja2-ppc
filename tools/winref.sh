#!/usr/bin/env bash
set -euo pipefail

# Windows reference: runs the GOG ja2.exe (German 1.12.1, the build closest to
# ja2-master) under Wine on a private X server, so it never covers the
# user's desktop, and drives it with xdotool. The GOG install itself is never
# written to: the game runs from a symlink copy whose writable parts (Ja2.ini,
# Ja2.set, Data/Temp, SavedGames, logs) are real files in WINREF_DIR.
#
#   tools/winref.sh start             set up (once) and launch the game
#   tools/winref.sh shot [file.png]   screenshot of the game screen only
#   tools/winref.sh click X Y [btn]   click at game coordinates (640x480)
#   tools/winref.sh move X Y          move the mouse
#   tools/winref.sh key KEY...        xdotool key names (Escape, Return, F1, ctrl+s)
#   tools/winref.sh type TEXT         type text
#   tools/winref.sh view              live read-only window for watching
#   tools/winref.sh status|stop|reset
#
# Variables: WINREF_DIR (default $EMU_WORK_DIR/winref), WINREF_DISPLAY
# (default :77), WINREF_AUDIO=1 to keep Wine sound on (default off),
# 'view' opens a read-only live window of the private display on the desktop
# (ffplay, no input; close it any time, the game keeps running).

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$ROOT_DIR/tools/local-env.sh"
GAME_SRC="${JA2_DATA_DIR:?set JA2_DATA_DIR in local.env (see local.env.example)}"
WORK="${WINREF_DIR:-${EMU_WORK_DIR:?set EMU_WORK_DIR in local.env}/winref}"
DISP="${WINREF_DISPLAY:-:77}"
OUT_DIR="${WINREF_OUT_DIR:-$ROOT_DIR/.winref-out}"
PREFIX="$WORK/prefix"
GAME="$WORK/game"
export WINEPREFIX="$PREFIX" WINEDEBUG="${WINEDEBUG:--all}"
# Wine's own DirectDraw; the bundled DDrawCompat is a Windows-only shim.
export WINEDLLOVERRIDES="ddraw=b;dsound=b;mscoree,mshtml="

die() { echo "winref: $*" >&2; exit 1; }
xd() { DISPLAY="$DISP" xdotool "$@"; }
xvfb_pid() { cat "$WORK/xvfb.pid" 2>/dev/null || true; }
running() { local p; p=$(xvfb_pid); [[ -n "$p" ]] && kill -0 "$p" 2>/dev/null; }

setup_game() {
    [[ -f "$GAME_SRC/ja2.exe" ]] || die "no ja2.exe in $GAME_SRC"
    mkdir -p "$GAME/Data" "$GAME/Data/Temp" "$GAME/SavedGames"
    local f n
    for f in "$GAME_SRC"/*; do
        n=$(basename "$f")
        case "$n" in
            Data|Ja2.ini|Ja2.set|unins*|goggame-*|goglog.ini|*.lnk) ;;
            *) [[ -e "$GAME/$n" ]] || ln -s "$f" "$GAME/$n" ;;
        esac
    done
    # Writable copies; the GOG Ja2.ini says CD=c, so no drive is needed.
    for n in Ja2.ini Ja2.set; do
        [[ -e "$GAME/$n" || ! -f "$GAME_SRC/$n" ]] || cp "$GAME_SRC/$n" "$GAME/$n"
    done
    for f in "$GAME_SRC/Data"/*; do
        n=$(basename "$f")
        [[ "$n" == Temp || -e "$GAME/Data/$n" ]] || ln -s "$f" "$GAME/Data/$n"
    done
}

setup_prefix() {
    [[ -f "$PREFIX/system.reg" ]] && return 0
    echo "winref: creating Wine prefix $PREFIX"
    DISPLAY="$DISP" wineboot -i >"$WORK/wineboot.log" 2>&1
    DISPLAY="$DISP" wineserver -w
}

set_audio() {
    local drv='""'
    [[ "${WINREF_AUDIO:-0}" == 1 ]] && drv='"pulse,alsa"'
    cat >"$WORK/audio.reg" <<EOF
REGEDIT4

[HKEY_CURRENT_USER\\Software\\Wine\\Drivers]
"Audio"=$drv
EOF
    DISPLAY="$DISP" wine regedit /S "$WORK/audio.reg" >/dev/null 2>&1 || true
}

start_x() {
    running && return 0
    # 640x480 is JA2's only mode, so the game fills the private screen 1:1.
    Xvfb "$DISP" -screen 0 640x480x24 -nolisten tcp >"$WORK/xvfb.log" 2>&1 &
    echo $! >"$WORK/xvfb.pid"
    for _ in $(seq 50); do xd getmouselocation >/dev/null 2>&1 && return 0; sleep 0.1; done
    die "Xvfb on $DISP did not come up (see $WORK/xvfb.log)"
}

cmd_start() {
    mkdir -p "$WORK" "$OUT_DIR"
    start_x
    setup_game
    setup_prefix
    set_audio
    if pgrep -f '[Jj]a2\.exe' -U "$UID" >/dev/null; then
        echo "winref: ja2.exe already running on $DISP"; return 0
    fi
    (cd "$GAME" && DISPLAY="$DISP" setsid wine ja2.exe >"$WORK/wine.log" 2>&1 &)
    echo "winref: started on $DISP (log $WORK/wine.log); main menu after ~10 s."
    echo "        Use 'shot' to see the screen, 'view' to watch live."
}

cmd_shot() {
    running || die "not running"
    local file="${1:-$OUT_DIR/shot-$(date +%H%M%S).png}"
    mkdir -p "$(dirname "$file")"
    import -display "$DISP" -window root "$file"
    echo "$file"
}

cmd_view() {
    running || die "not running"
    setsid ffplay -loglevel error -window_title "JA2 Windows-Referenz (nur Ansicht)" \
        -f x11grab -framerate 15 -video_size 640x480 -i "$DISP" \
        >/dev/null 2>&1 &
    echo "winref: viewer started (pid $!)"
}

cmd_stop() {
    pkill -f "^ffplay .*-i $DISP\$" 2>/dev/null || true
    WINEPREFIX="$PREFIX" wineserver -k 2>/dev/null || true
    local p; p=$(xvfb_pid)
    [[ -n "$p" ]] && kill "$p" 2>/dev/null || true
    rm -f "$WORK/xvfb.pid"
    echo "winref: stopped"
}

case "${1:-}" in
    start) cmd_start ;;
    shot) shift; cmd_shot "$@" ;;
    click) running || die "not running"
           xd mousemove --sync "$2" "$3" click "${4:-1}" ;;
    move) running || die "not running"; xd mousemove --sync "$2" "$3" ;;
    key) running || die "not running"; shift; xd key --delay 80 "$@" ;;
    type) running || die "not running"; shift; xd type --delay 60 -- "$*" ;;
    view) cmd_view ;;
    status) if running; then echo "Xvfb $DISP up"; pgrep -af '[Jj]a2\.exe' || echo "ja2.exe not running"; else echo "stopped"; fi ;;
    stop) cmd_stop ;;
    reset) cmd_stop; rm -rf "$GAME"; echo "winref: game copy removed (prefix kept)" ;;
    *) sed -n '4,23p' "$0" | sed 's/^# \{0,1\}//'; exit 1 ;;
esac
