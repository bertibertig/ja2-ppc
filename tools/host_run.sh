#!/usr/bin/env bash
set -euo pipefail

# Builds the port's original sources (the same list as retro68/CMakeLists.txt)
# as a 32-bit headless Linux program with tools/host/sgp_host.c instead of
# the SDL platform layer, and runs it against a bundle. A debugging aid for
# crashes and logic errors (gdb, fast turnaround); the Mac stays the
# reference. See tools/host/sgp_host.c for the SGP_HOST_* variables.
#
#   tools/host_run.sh [--gdb] [BUNDLE]

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
GDB=0
if [ "${1:-}" = "--gdb" ]; then GDB=1; shift; fi
BUNDLE="${1:-$ROOT/retro68/build-german-debug/JA2PPC Bundle}"
OUT="${SGP_HOST_BUILD:-${TMPDIR:-/tmp}/ja2-host}"
P="$ROOT/retro68"
S="$P/Standard Gaming Platform"
mkdir -p "$OUT/obj"
python3 "$ROOT/tools/gen_case_includes.py" "$OUT/case" "$P" >/dev/null

INC=()
for d in "Standard Gaming Platform" "" Utils TileEngine Tactical Strategic TacticalAI Laptop Editor; do
    INC+=(-iquote "$P/$d")
done
INC+=(-iquote "$OUT/case" -iquote "$P/win32-include" -idirafter "$P/win32-include")
FLAGS=(-m32 -g -O0 -std=gnu99 -fshort-wchar -fcommon -finput-charset=ISO-8859-1 -w
       -Wno-error=implicit-int -Wno-error=incompatible-pointer-types -Wno-error=int-conversion
       -Wno-error=return-mismatch -DJA2 -DJA2_PPC -DJA2_LANGUAGE_GERMAN -DJA2_DEBUG_BUILD)

# Source lists from CMakeLists.txt (one quoted or bare path per line).
mapfile -t SGP < <(sed -n '/^set(JA2_SGP_PORT_SOURCES/,/^)/p;/^set(JA2_SGP_ORIGINAL_SOURCES/,/^)/p' "$P/CMakeLists.txt" \
    | grep JA2_SGP_DIR | sed 's/.*JA2_SGP_DIR}\/\(.*\)"/\1/' | grep -v sgp_sdl.c)
mapfile -t GAME < <(sed -n '/^set(JA2_GAME_SOURCES/,/^)/p' "$P/CMakeLists.txt" \
    | sed '1d;$d;/^ *#/d;s/^ *//;s/"//g')

OBJS=()
compile() { # source object extra-flags...
    local src=$1 obj=$2; shift 2
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ] || [ -n "$(find "$P" -name '*.h' -newer "$obj" -print -quit)" ]; then
        gcc "${FLAGS[@]}" "$@" "${INC[@]}" -x c -c "$src" -o "$obj"
    fi
    OBJS+=("$obj")
}
for f in "${SGP[@]}"; do compile "$S/$f" "$OUT/obj/sgp_$(echo "$f" | tr ' /' '__').o"; done
for f in "${GAME[@]}"; do compile "$P/$f" "$OUT/obj/game_$(echo "$f" | tr ' /' '__').o" -DPRECOMPILEDHEADERS; done
compile "$P/Smacker/smacker.c" "$OUT/obj/smacker.o"
compile "$ROOT/tools/host/sgp_host.c" "$OUT/obj/sgp_host.o"
gcc -m32 -g -o "$OUT/ja2host" "${OBJS[@]}" -lm

if [ "$GDB" = 1 ]; then
    exec env SGP_ROOT="$BUNDLE" gdb -batch -ex run -ex bt -ex 'info registers eip' --args "$OUT/ja2host"
fi
exec env SGP_ROOT="$BUNDLE" "$OUT/ja2host"
