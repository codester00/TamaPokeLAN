#!/bin/bash
# Fast, parallel version of tools/emu/tests/run.sh for development: NO
# arduino-cli compile (run.sh does that first, and takes over an hour in a cloud
# container). The shared sources compile once to objects, then each suite links
# and runs in its own directory, 4 at a time. ~3 minutes for all of them.
#   bash tools/emu/tests/prun.sh .            # every suite
#   bash tools/emu/tests/prun.sh . cleanup    # suites whose name contains it (one word)
# Prints only failures, then "suites passed: N, failed: M". KEEP=dir keeps the output.
ROOT="$(cd "$1" && pwd)"; FILTER="${2:-}"
EMU="$ROOT/tools/emu"; HERE="$EMU/tests"
SPRITE_DIR="$ROOT/tools/sdcard/mons"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
bash "$EMU/build.sh" >/dev/null || { echo "EMULATOR BUILD FAILED"; exit 1; }
eval "$(grep -E '^(CORE=|needs_sketch\(\)|standalone\(\)|needs_host\(\))' "$HERE/run.sh")"
FLAGS=(-std=c++17 -O1 -w -I"$EMU" -I"$ROOT" -DSPRITE_DIR="\"$SPRITE_DIR\"")
SK=("$EMU/sketch.cpp" "$EMU/host_impl.cpp" "$EMU/font.cpp" "$EMU/clock.cpp")
STD=("$ROOT/gbsynth.cpp" "$ROOT/musicplay.cpp" "$ROOT/surf.cpp")
mkdir -p "$OUT/obj"; export OUT
all=($(printf '%s\n' "${CORE[@]}" "${SK[@]}" | sort -u))
obj() { echo "$OUT/obj/$(echo "$1" | md5sum | cut -c1-12).o"; }
printf '%s\n' "${all[@]}" | xargs -P4 -I{} sh -c 'g++ '"$(printf '%q ' "${FLAGS[@]}")"' -c "{}" -o "'"$OUT"'/obj/$(echo "{}" | md5sum | cut -c1-12).o" 2>>"'"$OUT"'/obj.log" || echo "OBJ FAIL {}"'
runone() {
  src="$1"; name="$(basename "$src" .cpp)"; d="$OUT/$name"; mkdir -p "$d"
  if [ "$name" = sprite_test ] && [ ! -d "$SPRITE_DIR" ]; then echo "SKIP $name" > "$d/res"; return; fi
  if standalone "$name"; then srcs=("${STD[@]}"); link=("${srcs[@]}")
  else
    link=(); for f in "${CORE[@]}"; do link+=("$(obj "$f")"); done
    if needs_sketch "$name"; then for f in "${SK[@]}"; do link+=("$(obj "$f")"); done
    elif needs_host "$name"; then link+=("$(obj "$EMU/host_impl.cpp")" "$(obj "$EMU/font.cpp")"); fi
  fi
  if ! g++ "${FLAGS[@]}" -o "$d/$name" "$src" "${link[@]}" 2>"$d/log"; then echo "NOCOMPILE $name" > "$d/res"; return; fi
  if (cd "$d" && "./$name" > out.txt 2>&1); then echo "PASS $name" > "$d/res"; else echo "FAIL $name" > "$d/res"; fi
}
export -f runone obj standalone needs_sketch needs_host; export ROOT EMU SPRITE_DIR
export FLAGS_STR="$(printf '%q ' "${FLAGS[@]}")"
# arrays do not export: re-create them inside the worker
for src in "$HERE"/*_test.cpp; do
  name="$(basename "$src" .cpp)"; [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]] && continue
  echo "$src"
done | xargs -P4 -I{} bash -c 'eval "$(grep -E "^(CORE=|needs_sketch\(\)|standalone\(\)|needs_host\(\))" "'"$HERE"'/run.sh")"; FLAGS=('"$(printf '%q ' "${FLAGS[@]}")"'); SK=('"$(printf '%q ' "${SK[@]}")"'); STD=('"$(printf '%q ' "${STD[@]}")"'); runone "{}"'
p=0; f=0
for r in "$OUT"/*/res; do
  read -r st nm < "$r"
  case $st in PASS) p=$((p+1));; SKIP) ;; *) f=$((f+1)); echo "=== $st $nm"; grep -E "^FAIL|failed|FAILED" "$OUT/$nm/out.txt" 2>/dev/null | head -8; tail -3 "$OUT/$nm/log" 2>/dev/null;; esac
done
echo "suites passed: $p, failed: $f"
[ -n "$KEEP" ] && cp -r "$OUT" "$KEEP"
[ "$f" -eq 0 ]
