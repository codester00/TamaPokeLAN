#!/bin/bash
# Fails if two object files in the last arduino-cli build define the same C++
# vtable with DIFFERENT sizes: two classes sharing one name, which the linker
# resolves silently by keeping one -- and the board then calls into the wrong
# class. SensorLib 0.5.0 did exactly this (see sensorlib_guard.h) and boot-looped
# the firmware while it compiled without a warning. Run after a CLEAN compile:
# objects left in the cache by a previous library version can look like a
# conflict that the current build does not have.
#
#   bash tools/check_odr.sh [build-cache-dir]
set -e
T="$(ls -d ~/.arduino15/packages/esp32/tools/esp-x32/*/bin ~/Library/Arduino15/packages/esp32/tools/esp-x32/*/bin \
        "${LOCALAPPDATA:-/nonexistent}"/Arduino15/packages/esp32/tools/esp-x32/*/bin 2>/dev/null | head -1)"
NM="$T/xtensa-esp32s3-elf-nm"
[ -x "$NM" ] || [ -x "$NM.exe" ] || { echo "check_odr: no xtensa nm found; skipping"; exit 0; }
D="${1:-$(ls -td ~/.cache/arduino/sketches/*/ ~/Library/Caches/arduino/sketches/*/ 2>/dev/null | head -1)}"
[ -d "$D" ] || { echo "check_odr: no build cache found; skipping"; exit 0; }
find "$D" -name "*.o" | while read -r o; do
  "$NM" -S -C "$o" 2>/dev/null | awk -v f="${o#$D}" '/ [VW] vtable for /{sz=$2; $1=$2=$3=""; print substr($0,4) "\t" sz "\t" f}'
done | sort -u | awk -F'\t' '
  { key=$1 SUBSEP $2; if (!(key in seen)) { seen[key]=1; kinds[$1]++ } where[$1]=where[$1] "\n    " $2 "  " $3; n[$1]=1 }
  END {
    bad=0
    for (c in kinds) if (kinds[c] > 1) { print "TWO DEFINITIONS of " c ":" where[c]; bad=1 }
    if (bad) { print "check_odr: two classes share a name -- the board would call into the wrong one"; exit 1 }
    print "check_odr: no conflicting class definitions (" length(n) " vtables)"
  }'
