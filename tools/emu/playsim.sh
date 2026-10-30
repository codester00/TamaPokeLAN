#!/bin/bash
# Builds tools/emu/playsim.cpp against the real sketch and runs it:
#   bash tools/emu/playsim.sh <child|walker|adult> <seed> <days>
# The JSON lines go to stdout; tools/emu/playsim_report.py turns them into a report.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; EMU="$ROOT/tools/emu"
OUT="${PLAYSIM_BIN:-/tmp/playsim}"
if [ ! -x "$OUT" ] || [ -n "$PLAYSIM_REBUILD" ]; then
  bash "$EMU/build.sh" >/dev/null
  eval "$(grep -E '^CORE=' "$EMU/tests/run.sh")"
  g++ -std=c++17 -O2 -w -I"$EMU" -I"$ROOT" -DSPRITE_DIR='""' "$EMU/playsim.cpp" "${CORE[@]}" \
      "$EMU/sketch.cpp" "$EMU/host_impl.cpp" "$EMU/font.cpp" "$EMU/clock.cpp" -o "$OUT"
fi
exec "$OUT" "${1:-walker}" "${2:-1}" "${3:-365}"
