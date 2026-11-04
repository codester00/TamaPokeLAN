#!/bin/bash
# A negative check: break the code on purpose, run a suite, expect it to FAIL,
# then restore the file. CLAUDE.md trap 3: "if an assertion cannot fail, it is
# not a test." Usage (from the repo root):
#   bash tools/emu/tests/neg.sh . <test filter> <file> '<exact old text>' '<new text>'
# The old text must occur exactly once. Prints NEG CAUGHT (good) or NEG MISSED.
ROOT=$1; T=$2; F=$3; OLD=$4; NEW=$5
BAK=$(mktemp); cp "$ROOT/$F" "$BAK"
python3 - "$ROOT/$F" "$OLD" "$NEW" <<'PY'
import sys
p,a,b=sys.argv[1:4]; s=open(p).read()
if s.count(a)!=1: sys.exit('anchor count %d' % s.count(a))
open(p,'w').write(s.replace(a,b))
PY
rc=$?
if [ $rc -ne 0 ]; then cp "$BAK" "$ROOT/$F"; rm -f "$BAK"; echo "NEG BAD ANCHOR"; exit 2; fi
out=$(bash "$(dirname "$0")/prun.sh" "$ROOT" "$T" 2>&1)
cp "$BAK" "$ROOT/$F"; rm -f "$BAK"
if echo "$out" | grep -q "failed: 0"; then echo "NEG MISSED: $OLD"; else echo "NEG CAUGHT"; echo "$out" | grep "^FAIL" | head -3; fi
