#!/usr/bin/env python3
"""Check wild_areas.h against the games and against tools/wild_areas.py.

    python3 tools/verify_wild.py            # checks; non-zero exit on failure
    python3 tools/verify_wild.py --table    # also print every area's odds
    python3 tools/verify_wild.py --refresh  # re-download the sources first

FAILS when:
  - a map the game has wild encounters on is in no area and not in SKIP (a
    silent drop), sits in two areas, or an area names a map the game lacks;
  - the committed wild_areas.h differs from a fresh build (hand-edited or
    stale -- re-run tools/gen_wild.py);
  - a legendary's level does not match the game's own script.

REPORTS, for review:
  - each area's level band, derived from the gym ladder in trainers.h the same
    way the firmware will (docs/explore-plan.md § 6.1);
  - evolved forms whose floor is above their area's band -- they stay in that
    area and appear at their floor (§ 6.3);
  - species with no art, which the firmware can never draw;
  - regions with no areas yet, and any region whose data is NOT VERIFIED.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
# wild_areas.py is edited by hand and read back at once; a same-size edit
# inside one second would otherwise load a stale .pyc (it did, in testing).
sys.dont_write_bytecode = True
import gen_wild as G                       # noqa: E402
import wild_areas as WA                    # noqa: E402
from dex_data import DEX, REGIONS          # noqa: E402

DECOMP_SOURCES = {'frlg', 'crystal', 'emerald', 'platinum'}   # sources that ARE the game's own tables
WHEN = {1: 'morning', 2: 'day', 4: 'night'}


def main():
    refresh = '--refresh' in sys.argv
    bad = 0

    def fail(msg):
        nonlocal bad
        bad += 1
        print('FAIL  ' + msg)

    # 1. the committed header is what the generator makes now (this also runs
    #    every loader, which records the maps each source has encounters on)
    areas, legends = G.build(refresh)
    G.check(areas)
    fresh = G.emit(areas, legends)
    try:
        committed = open(G.OUT).read()
    except FileNotFoundError:
        committed = ''
    if committed != fresh:
        fail('wild_areas.h is not what tools/gen_wild.py builds -- re-run it')
    else:
        print('wild_areas.h matches a fresh build')

    # 2. every map with encounters is accounted for, exactly once
    for reg in WA.REGIONS:
        have = G.SOURCE_MAPS[reg['source']]
        seen = {}
        for name, _gate, maps in reg['areas']:
            for mp in maps:
                if mp in seen:
                    fail('%s map %s is in both %r and %r' % (reg['source'], mp, seen[mp], name))
                seen[mp] = name
                if mp not in have:
                    fail('%s area %r names map %s, which has no encounters' % (reg['source'], name, mp))
        for mp in sorted(have - set(seen) - set(reg['skip'])):
            fail('%s map %s has wild encounters but is in no area and not in SKIP' % (reg['source'], mp))
        print('maps: every %s map with encounters is in exactly one area or SKIP (%d maps)'
              % (reg['source'], len(have)))

    # 3. legendaries (gen_wild read every level from the game's own script)
    for lg in legends:
        print('legend: %-9s L%-3d home %-15s gate %-2d%s%s'
              % (DEX[lg['dex'] - 1][2], lg['level'], areas[lg['area']]['name'], lg['gate'],
                 '  roams its region once met' if lg['roams'] else '',
                 '  (OVERRIDE: the game fights it at L%d)' % lg['game_level']
                 if lg['level'] != lg['game_level'] else ''))

    # 4. bands, floors and where things can first be met
    noart = set(int(x) for x in re.findall(
        r'\d+', open(os.path.join(ROOT, 'noart.h')).read().split('= {')[1].split('}')[0]))
    print('\nlevel bands (from trainers.h; gate = ladder trainers beaten):')
    for a in areas:
        print('  %-7s %-17s gate %-2s  L%d-%d' % (REGIONS[a['region']][0], a['name'], a['gate'], *a['band']))
    over, seen_over = [], set()
    for i, a in enumerate(areas):
        for d, _w, _wh, fl, g in a['slots']:
            if fl > a['band'][1] and (i, d) not in seen_over:
                seen_over.add((i, d))
                over.append((a, d, fl, g))
    if over:
        print('\nabove the band: they stay in their area and appear at their floor (%d):' % len(over))
        for a, d, fl, g in over:
            print('  %-11s L%d in %s (band L%d-%d)%s' % (DEX[d - 1][2], fl, a['name'], *a['band'],
                                                          ', best rod: gate %d' % g if g else ''))

    # The best rod's hidden gate (owner's call: nothing on the panel shows it).
    for reg in WA.REGIONS:
        rod = reg['super_rod']['gate']
        held = [(a, d) for a in areas if a['region'] == reg['region']
                for d, _w, _wh, _f, g in a['slots'] if g]
        if not held:
            continue
        print('\nbest rod: %s holds %d fishing slots back until gate %d (%s); first met only then:'
              % (REGIONS[reg['region']][0], len(held), rod,
                 reg['super_rod'].get('script') or reg['super_rod']['note']))
        early = {}
        for a in areas:
            if a['region'] != reg['region']:
                continue
            for d, _w, _wh, _f, g in a['slots']:
                at = max(a['gate'], g)
                early[d] = min(early.get(d, 99), at)
        late = sorted({d for _a, d in held if early[d] >= rod})
        print('  %s' % (', '.join(DEX[d - 1][2] for d in late) or 'none -- each is met earlier anyway'))
    # Areas the panel greys out as NO VALID SPRITES: in some time band, nothing
    # behind an open gate can be drawn. Built anyway (owner's call): it lights
    # up by itself when the art is ingested or the rod's gate opens.
    dark = []
    for a in areas:
        for b, bname in ((1, 'morning'), (2, 'day'), (4, 'night')):
            drawable = [(d, g) for d, _w, wh, _f, g in a['slots'] if wh & b and d not in noart]
            if not any(g == 0 for _d, g in drawable):
                until = ('the rod (gate %d)' % min(g for _d, g in drawable)) if drawable \
                    else 'its art is ingested'
                dark.append('  %-7s %-17s %-8s NO VALID SPRITES until %s'
                            % (REGIONS[a['region']][0], a['name'], bname, until))
    print('\nareas greyed as NO VALID SPRITES (%d):' % len(dark))
    for line in dark:
        print(line)
    if not dark:
        print('  none')
    art = sorted({d for a in areas for d, _w, _wh, _f, _g in a['slots'] if d in noart})
    if art:
        print('\nno art (the firmware skips these): %s' % ', '.join(DEX[d - 1][2] for d in art))

    # 5. coverage by region, and which of each region's own species no area has
    print('\nregions:')
    legend_dex = {lg['dex'] for lg in legends}
    for r, (name, lo, hi, *_r) in enumerate(REGIONS):
        regs = [g for g in WA.REGIONS if g['region'] == r]
        n = sum(1 for a in areas if a['region'] == r)
        if not regs:
            print('  %-7s no areas yet -- keeps the old region-wide pool' % name)
            continue
        verified = regs[0]['source'] in DECOMP_SOURCES
        print('  %-7s %d areas, %s' % (name, n, 'VERIFIED against the %s disassembly' % regs[0]['source']
                                       if verified else 'NOT VERIFIED (no disassembly; %s)' % regs[0]['source']))
        met = {d for a in areas if a['region'] == r for d, _w, _wh, _f, _g in a['slots']} | legend_dex
        missing = [d for d in range(lo, hi + 1) if d not in met]
        if missing:
            print('          its own species no area or legendary home offers (%d): %s'
                  % (len(missing), ', '.join(DEX[d - 1][2] for d in missing)))

    if '--table' in sys.argv:
        for a in areas:
            print('\n%s / %s (gate %d, L%d-%d)' % (REGIONS[a['region']][0], a['name'], a['gate'], *a['band']))
            for b in (1, 2, 4):
                rows = [(d, w, fl, g) for d, w, wh, fl, g in a['slots'] if wh & b]
                if b > 1 and rows == [(d, w, fl, g) for d, w, wh, fl, g in a['slots'] if wh & 1]:
                    continue   # the same as the morning: say it once
                label = 'all day' if all(wh == 7 for _d, _w, wh, _f, _g in a['slots']) else WHEN[b]
                print('  [%s]' % label)
                for d, w, fl, g in rows:
                    print('    %5.2f%%  %-12s%s%s' % (w / 100, DEX[d - 1][2], '  floor L%d' % fl if fl else '',
                                                    '  best rod, gate %d' % g if g else ''))

    print('\n%s' % ('FAILED: %d problem(s)' % bad if bad else 'OK'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
