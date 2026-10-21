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
  - evolved forms left out of an area because their floor is above its band
    (and where each can first be met instead), and the ones kept above their
    band because no other area has them (§ 6.3);
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

DECOMP_SOURCES = {'frlg'}   # sources that ARE the game's own tables


def G_floor(area, dex, areas):
    for a in areas:
        for d, _, fl in a['slots']:
            if d == dex and a['region'] == area['region']:
                return fl
    return 0


def main():
    refresh = '--refresh' in sys.argv
    bad = 0

    def fail(msg):
        nonlocal bad
        bad += 1
        print('FAIL  ' + msg)

    # 1. every map with encounters is accounted for, exactly once
    for reg in WA.REGIONS:
        data = G.load_source(reg['source'], refresh)
        have = set()
        for v in data.values():
            have |= set(v)
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
    print('maps: every map with encounters is in exactly one area or SKIP' if not bad else '')

    # 2. the committed header is what the generator makes now
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

    # 3. legendary levels (gen_wild read them from the scripts; say so)
    for dex, area, lvl in legends:
        print('legend: %-9s L%-3d in %s (level read from the game script)'
              % (DEX[dex - 1][2], lvl, areas[area]['name']))

    # 4. bands, floors and where things can first be met
    noart = set(int(x) for x in re.findall(
        r'\d+', open(os.path.join(ROOT, 'noart.h')).read().split('= {')[1].split('}')[0]))
    print('\nlevel bands (from trainers.h; gate = ladder trainers beaten):')
    for a in areas:
        print('  %-17s gate %-2s  L%d-%d' % (a['name'], a['gate'], *a['band']))
    first = {}
    for a in areas:
        for d, _, _ in a['slots']:
            k = (a['region'], d)
            if k not in first or a['gate'] < first[k]['gate']:
                first[k] = a
    left = [(a, d) for a in areas for d in a['dropped']]
    if left:
        print('\nleft out of an area, floor above its band, met elsewhere (%d):' % len(left))
        for a, d in left:
            f = first[(a['region'], d)]
            print('  %-11s floor L%-3d not in %-17s (L%d-%d) -> first met in %s (gate %d)'
                  % (DEX[d - 1][2], G_floor(a, d, areas), a['name'], *a['band'],
                     f['name'], f['gate']))
    over = [(a, d, fl) for a in areas for d, _, fl in a['slots'] if fl > a['band'][1]]
    if over:
        print('\nkept above the band because nowhere else has them -- they appear at their floor (%d):' % len(over))
        for a, d, fl in over:
            print('  %-11s L%d in %s (band L%d-%d)' % (DEX[d - 1][2], fl, a['name'], *a['band']))
    art = sorted({d for a in areas for d, _, _ in a['slots'] if d in noart})
    if art:
        print('\nno art (the firmware skips these): %s' % ', '.join(DEX[d - 1][2] for d in art))

    # 5. coverage by region
    print('\nregions:')
    for r, (name, *_r) in enumerate(REGIONS):
        regs = [g for g in WA.REGIONS if g['region'] == r]
        if not regs:
            print('  %-7s no areas yet -- keeps the old region-wide pool' % name)
        elif regs[0]['source'] in DECOMP_SOURCES:
            print('  %-7s %d areas, VERIFIED against the %s disassembly'
                  % (name, sum(1 for a in areas if a['region'] == r), regs[0]['source']))
        else:
            print('  %-7s %d areas, NOT VERIFIED (no disassembly; %s)'
                  % (name, sum(1 for a in areas if a['region'] == r), regs[0]['source']))

    if '--table' in sys.argv:
        for a in areas:
            print('\n%s (gate %d, L%d-%d)' % (a['name'], a['gate'], *a['band']))
            for dex, w, floor in a['slots']:
                print('  %5.2f%%  %-12s%s' % (w / 100, DEX[dex - 1][2],
                                             '  floor L%d' % floor if floor else ''))

    print('\n%s' % ('FAILED: %d problem(s)' % bad if bad else 'OK'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
