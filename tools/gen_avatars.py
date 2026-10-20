#!/usr/bin/env python3
"""Player avatars: pret/pokered overworld sprites -> avatars.h

    python3 tools/gen_avatars.py

Downloads 16x16 overworld sprites from the pokered disassembly, takes the
first (front-facing) frame of each sheet and packs them 8bpp with a four-entry
RGB565 palette; index 0xFF is transparent.

Two lists. PICKS are the player avatars: the player, the rival and every Gen 1
trainer class that has an overworld sprite of its own (v3.38; which sprite each
class uses was read from pokered's map objects, not guessed from file names).
NPCS are characters the game draws but nobody plays as: MOM, who greets you
after a blackout, and OAK, kept for later.

The sheets are 2bpp greyscale, six frames stacked vertically. Shade 3 is the
white background and becomes transparent; 0 is the outline, 1 the large fill,
2 the highlight. Each avatar gets an accent colour on shade 1 -- the same thing
Pokemon Yellow did when it colourised these sprites for the GBC -- because eight
identical grey silhouettes are unreadable at a glance in the picker.

SOURCE AND LICENCE -- read CREDITS.md before publishing anything built from
this. These are Nintendo / Game Freak / Creatures artwork, taken from a
disassembly. They are NOT licensed for redistribution the way the PMD sprites
(CC BY-NC) or the badges (CC BY 3.0) are. They are here because the project
owner asked for the real sprites, and CREDITS.md records that honestly rather
than leaving it undocumented.
"""
import os
import struct
import sys
import subprocess
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'avatars.h')
BASE = 'https://raw.githubusercontent.com/pret/pokered/master/gfx/sprites/%s.png'
SIZE = 16

# (file, label shown in the UI, accent for shade 1)
# APPEND-ONLY. The save stores the avatar as its INDEX in this list, so
# reordering it or inserting into the middle silently changes every player's
# avatar -- the same rule as move indices. New ones go at the end.
# Labels are unaccented ASCII: the bitmap font has nothing else.
PICKS = [
    ('red',           'RED',      0xEA87),   # the classic player
    ('blue',          'BLUE',     0x4C98),   # the rival
    ('girl',          'GIRL',     0xF454),
    ('cooltrainer_m', 'ACE M',    0x5DCD),
    ('cooltrainer_f', 'ACE F',    0xC2F0),
    ('hiker',         'HIKER',    0xB3C8),
    # NOT swimmer: it is drawn low in its cell (the character is in water),
    # which reads as a misaligned sprite once it is out of context.
    ('sailor',        'SAILOR',   0x7E3D),
    ('youngster',     'YOUNGSTER',0xFECB),
    # v3.38: the remaining trainer classes.
    ('beauty',        'BEAUTY',     0xFB56),
    ('biker',         'BIKER',      0x6B4D),
    ('channeler',     'CHANNELER',  0xA254),
    ('fisher',        'FISHER',     0x3D7F),
    ('gambler',       'GAMBLER',    0xC9A6),
    ('gentleman',     'GENTLEMAN',  0x4A49),
    ('rocker',        'ROCKER',     0xFC60),
    ('rocket',        'ROCKET',     0x39E7),
    ('scientist',     'SCIENTIST',  0xB5B6),
    ('super_nerd',    'SUPER NERD', 0x867F),
    # The swimmer is drawn low in its cell (the character is in the water) and
    # read as misaligned, which is why it was left out of the first eight. It
    # is centred vertically now instead, see CENTRE.
    ('swimmer',       'SWIMMER',    0x2D9F),
]

# Not playable: drawn by the game itself. Their own list and their own array,
# so they can never be picked or shift a player's stored index.
NPCS = [
    ('mom',           'MOM',        0xE30C),   # the blackout screen
    ('oak',           'OAK',        0xA514),   # unused for now
]

# Sheets whose figure sits off-centre in its cell and is moved to the middle.
CENTRE = {'swimmer'}

OUTLINE = 0x18C4     # same ink as the rest of the UI
HIGHLIGHT = 0xEF7D   # a soft off-white, so it reads on the cream card


def read_png_2bpp(data, path):
    """pokered's sprites are 2-bit greyscale; the shared decoder in
    gen_backs.py only handles 8-bit channels, so this one is separate."""
    pos, idat, w, h, bd, ct = 8, b'', 0, 0, 0, 0
    while pos < len(data):
        ln = struct.unpack('>I', data[pos:pos + 4])[0]
        typ = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + ln]
        if typ == b'IHDR':
            w, h, bd, ct = struct.unpack('>IIBB', chunk[:10])
        elif typ == b'IDAT':
            idat += chunk
        pos += 12 + ln
    if bd != 2 or ct != 0:
        raise SystemExit('%s: expected 2-bit greyscale, got bd=%d ct=%d'
                         % (path, bd, ct))
    raw = zlib.decompress(idat)
    bpr = (w * bd + 7) // 8
    rows, prev, i = [], bytearray(bpr), 0
    for _ in range(h):
        f = raw[i]; i += 1
        line = bytearray(raw[i:i + bpr]); i += bpr
        for x in range(len(line)):
            a = line[x - 1] if x >= 1 else 0
            b = prev[x]
            c = prev[x - 1] if x >= 1 else 0
            if f == 1:   line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 255
        rows.append(bytes(line)); prev = line
    px = []
    for line in rows:
        px.append([(line[x // 4] >> ((3 - (x % 4)) * 2)) & 3 for x in range(w)])
    return w, h, px


def emit(out, fn, accent):
    """Fetch one sheet, take its front-facing frame, append its arrays."""
    url = BASE % fn
    sys.stderr.write('fetching %s\n' % url)
    # curl rather than urllib: the stock python.org build on macOS ships
    # without a usable CA bundle and fails to verify github.com.
    r = subprocess.run(['curl', '-fsSL', url], capture_output=True)
    if r.returncode != 0:
        raise SystemExit('fetch failed (%s): %s'
                         % (url, r.stderr.decode().strip()))
    w, h, px = read_png_2bpp(r.stdout, fn)
    if w != SIZE or h % SIZE:
        raise SystemExit('%s: expected a %dpx-wide sheet, got %dx%d'
                         % (fn, SIZE, w, h))
    frame = [row[:] for row in px[:SIZE]]     # frame 0 only: front-facing
    if fn in CENTRE:
        used = [y for y in range(SIZE) if any(v != 3 for v in frame[y])]
        top, bot = used[0], used[-1]
        shift = (SIZE - 1 - bot - top) // 2   # equal margins above and below
        blank = [3] * SIZE
        frame = [frame[y - shift] if 0 <= y - shift < SIZE else blank
                 for y in range(SIZE)]
    pal = [OUTLINE, accent, HIGHLIGHT]
    idx = []
    for y in range(SIZE):
        for x in range(SIZE):
            v = frame[y][x]
            idx.append(0xFF if v == 3 else v)   # 3 = white = transparent
    sym = 'AV_' + fn.upper()
    out.append('static const uint16_t %s_PAL[3] = { %s };'
               % (sym, ', '.join('0x%04X' % c for c in pal)))
    out.append('static const uint8_t %s_IDX[%d] = {' % (sym, SIZE * SIZE))
    for r in range(SIZE):
        row = idx[r * SIZE:(r + 1) * SIZE]
        out.append('  ' + ' '.join('0x%02X,' % v for v in row))
    out.append('};')
    out.append('')
    return sym


def main():
    out = ['// GENERATED by tools/gen_avatars.py from pret/pokered - do not edit',
           '#pragma once',
           '#include <stdint.h>',
           '',
           '// Player avatars: Gen 1 overworld sprites, 16x16, 8bpp indexed with',
           '// a four-entry RGB565 palette. Index 0xFF is transparent.',
           '//',
           '// See CREDITS.md: this is Nintendo / Game Freak artwork from a',
           '// disassembly, not licensed material like the PMD sprites are.',
           '']
    picks = [emit(out, fn, accent) for fn, _, accent in PICKS]
    npcs = [emit(out, fn, accent) for fn, _, accent in NPCS]

    out.append('struct AvatarArt {')
    out.append('  const uint8_t *idx;')
    out.append('  const uint16_t *pal;')
    out.append('  const char *name;')
    out.append('};')
    out.append('#define AVATAR_PX %d' % SIZE)
    out.append('#define AVATAR_COUNT %d' % len(picks))
    out.append('static const AvatarArt AVATARS[AVATAR_COUNT] = {')
    for sym, (fn, label, _) in zip(picks, PICKS):
        out.append('  { %s_IDX, %s_PAL, "%s" },   // %s' % (sym, sym, label, fn))
    out.append('};')
    out.append('')
    out.append('// Characters the game draws but nobody plays as.')
    out.append('enum : uint8_t { %s, NPC_COUNT };'
               % ', '.join('NPC_' + fn.upper() for fn, _, _ in NPCS))
    out.append('static const AvatarArt NPC_ART[NPC_COUNT] = {')
    for sym, (fn, label, _) in zip(npcs, NPCS):
        out.append('  { %s_IDX, %s_PAL, "%s" },   // %s' % (sym, sym, label, fn))
    out.append('};')
    out.append('')
    open(OUT, 'w').write('\n'.join(out))
    sys.stderr.write('wrote %s (%d avatars, %d npcs)\n' % (OUT, len(picks), len(npcs)))


if __name__ == '__main__':
    main()
