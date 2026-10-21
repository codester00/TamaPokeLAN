#!/usr/bin/env python3
"""Build wild_areas.h -- Explore's per-area encounter tables -- from the games.

    python3 tools/gen_wild.py            # fetch (cached) + emit wild_areas.h
    python3 tools/gen_wild.py --refresh  # re-download the sources first

Two inputs, kept apart on purpose:

- tools/wild_areas.py, HAND-WRITTEN: which maps make an area, its name, its
  badge gate, where each legendary lives.
- the pret disassemblies, FETCHED: every species, encounter rate and level.
  Nothing a game decides is typed by hand, so nothing can be misremembered.

The raw sources are cached in tools/wild_cache/ (gitignored). Output is
deterministic: two runs over the same sources give the same bytes, which is
what lets verify_wild.py compare the committed header against a fresh build.

How an area's weights are made (docs/explore-plan.md § 12):
  1. per version, per encounter method: average the method's species
     distribution over the area's maps that have that method, using each
     slot's in-game rate;
  2. mix the methods by METHOD_WEIGHT, renormalised over the methods present;
  3. average the two versions, so an exclusive comes in at half weight;
  4. scale to WILD_WEIGHT_TOTAL, never rounding a present species to zero.
Levels are NOT taken from the source. Each area's BAND comes from the gym
ladder in trainers.h (§ 6.1): gate 0 is 1 .. first cap - 2, gate g is the
next battle's cap - 10 .. cap - 2, the champion's areas cap - 5 .. cap + 5,
caps being running maxima so a band never goes down. What the source does
decide is each species' FLOOR: the lowest level an evolved form may appear
at, the lower of our own evolution level and the lowest level the game itself
has it wild (§ 6.3).

Every species STAYS in the area the game puts it in (owner's call). An evolved
form whose floor is above its area's band -- FireRed's best rod puts a level
15+ Gyarados on Route 22, before Brock's cap of 14 -- simply appears at its
floor, above the band. That is the same edge the gym ladder already handles
for an over-raised pet: the ladder caps every creature's level in the fight,
so a strong early catch is handicapped there anyway.
"""
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
# wild_areas.py is edited by hand and read back at once; a same-size edit
# inside one second would otherwise load a stale .pyc (it did, in testing).
sys.dont_write_bytecode = True
from dex_data import DEX, REGIONS as DEX_REGIONS, EEVEE_BRANCHES   # noqa: E402
from dex_trade import TRADE_EVOS                                   # noqa: E402
import wild_areas as WA                                            # noqa: E402

CACHE = os.path.join(HERE, 'wild_cache')
OUT = os.path.join(ROOT, 'wild_areas.h')
WEIGHT_TOTAL = 10000

SOURCES = {
    'frlg': {
        'json': 'https://raw.githubusercontent.com/pret/pokefirered/master/src/data/wild_encounters.json',
        'script': 'https://raw.githubusercontent.com/pret/pokefirered/master/data/maps/%s/scripts.inc',
        'versions': ('FireRed', 'LeafGreen'),
        # Only the best rod counts (plan § 4): FRLG's fishing table is
        # old rod 0-1, good rod 2-4, super rod 5-9.
        'fishing_slots': range(5, 10),
    },
}

METHOD_KEYS = {'land_mons': 'land', 'water_mons': 'water',
               'fishing_mons': 'fishing', 'rock_smash_mons': 'rock_smash'}

SLUG = {slug: n for n, slug, *_ in DEX}

# trainers.h's ladders, in REGIONS order.
LADDER_ARRAYS = ['TRAINERS_KANTO', 'TRAINERS_JOHTO', 'TRAINERS_HOENN',
                 'TRAINERS_SINNOH', 'TRAINERS_UNOVA', 'TRAINERS_KALOS',
                 'TRAINERS_ALOLA', 'TRAINERS_GALAR', 'TRAINERS_PALDEA']


def ladder_caps():
    """region -> running-max team level of ladder slots 0..12."""
    text = open(os.path.join(ROOT, 'trainers.h')).read()
    caps = {}
    for r, arr in enumerate(LADDER_ARRAYS):
        block = text.split(arr + '[TRAINER_COUNT] = {')[1].split('};')[0]
        run, out = 0, []
        for line in block.splitlines():
            lv = [int(x) for x in re.findall(r'\{\d+,\s*(\d+)\}', line)]
            if lv:
                run = max(run, max(lv))
                out.append(run)
        if len(out) != 13:
            sys.exit('gen_wild: %s has %d trainers, expected 13' % (arr, len(out)))
        caps[r] = out
    return caps


def band(caps, gate):
    """(lo, hi) for an area of this gate, from one region's ladder caps."""
    if gate == WA.GATE_CHAMPION:
        return caps[12] - 5, min(100, caps[12] + 5)
    if gate == 0:
        return 1, caps[0] - 2
    return caps[gate] - 10, caps[gate] - 2


def dex_of(const):
    """SPECIES_MR_MIME -> 122. A species the dex does not know is a hard error."""
    s = re.sub(r'[^a-z0-9]', '', const.replace('SPECIES_', '').lower())
    if s not in SLUG:
        sys.exit('gen_wild: unknown species %s' % const)
    return SLUG[s]


def fetch(url, name, refresh=False):
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, name)
    if refresh or not os.path.exists(path):
        # curl, not urllib: urllib has no cert store in some environments
        # this project runs in. -f makes a 404 fail instead of caching a page.
        data = subprocess.run(['curl', '-sSfL', url], check=True,
                              capture_output=True).stdout
        with open(path, 'wb') as f:
            f.write(data)
    with open(path, encoding='utf-8') as f:
        return f.read()


def load_source(src, refresh=False):
    """-> {version: {map: {method: [(dex, min_level, rate), ...]}}}"""
    cfg = SOURCES[src]
    d = json.loads(fetch(cfg['json'], src + '_wild_encounters.json', refresh))
    group = d['wild_encounter_groups'][0]
    rates = {f['type']: f['encounter_rates'] for f in group['fields']}
    out = {v: {} for v in cfg['versions']}
    for e in group['encounters']:
        ver = next((v for v in cfg['versions'] if e['base_label'].endswith(v)), None)
        if ver is None:
            sys.exit('gen_wild: encounter %s belongs to no version' % e['base_label'])
        m = out[ver].setdefault(e['map'].replace('MAP_', ''), {})
        for key, method in METHOD_KEYS.items():
            if key not in e:
                continue
            slots = list(enumerate(e[key]['mons']))
            if method == 'fishing':
                slots = [(i, s) for i, s in slots if i in cfg['fishing_slots']]
            m[method] = [(dex_of(s['species']), s['min_level'], rates[key][i])
                         for i, s in slots]
    return out


def legend_level(src, script_map, dex, refresh=False):
    """The level a static legendary is fought at, read from the game's script."""
    text = fetch(SOURCES[src]['script'] % script_map,
                 '%s_%s_scripts.inc' % (src, script_map), refresh)
    for const, lvl in re.findall(r'setwildbattle\s+(SPECIES_\w+),\s*(\d+)', text):
        if dex_of(const) == dex:
            return int(lvl)
    sys.exit('gen_wild: no setwildbattle for #%d in %s' % (dex, script_map))


def pre_evolutions():
    """species -> the level its pre-evolution evolves at in THIS game, or None
    when the route there is not a level (pure trade evolutions)."""
    trade = {(a, b) for a, b, _ in TRADE_EVOS}
    pre = {}
    lvl_of = {n: lvl for n, _s, _d, _t, _e, lvl in DEX}
    for n, _s, _d, _t, evo, lvl in DEX:
        if evo:
            pre[evo] = None if (n, evo) in trade else lvl
    for b in EEVEE_BRANCHES:
        pre[b] = lvl_of[133]
    for a, b, _ in TRADE_EVOS:
        pre.setdefault(b, None)
    return pre


def build(refresh=False):
    """-> (areas, legends) in emit order. Each area is a dict with name,
    region, gate, maps and slots [(dex, weight, floor)]."""
    pre = pre_evolutions()
    caps = ladder_caps()
    areas, legends = [], []
    for reg in WA.REGIONS:
        data = load_source(reg['source'], refresh)
        versions = SOURCES[reg['source']]['versions']
        # The lowest level each species is wild at anywhere in this game.
        src_min = {}
        for v in versions:
            for methods in data[v].values():
                for slots in methods.values():
                    for dex, lv, _ in slots:
                        src_min[dex] = min(src_min.get(dex, 255), lv)
        names = set()
        for name, gate, maps in reg['areas']:
            if name in names:
                sys.exit('gen_wild: area %r twice' % name)
            names.add(name)
            mix = {}
            for v in versions:
                per_method = {}
                for method in WA.METHOD_WEIGHT:
                    dists = []
                    for mp in maps:
                        slots = data[v].get(mp, {}).get(method)
                        if not slots:
                            continue
                        tot = sum(r for _, _, r in slots)
                        dist = {}
                        for dex, _, r in slots:
                            dist[dex] = dist.get(dex, 0) + r / tot
                        dists.append(dist)
                    if dists:
                        avg = {}
                        for dist in dists:
                            for dex, p in dist.items():
                                avg[dex] = avg.get(dex, 0) + p / len(dists)
                        per_method[method] = avg
                if not per_method:
                    continue
                wsum = sum(WA.METHOD_WEIGHT[m] for m in per_method)
                for method, dist in per_method.items():
                    for dex, p in dist.items():
                        share = p * WA.METHOD_WEIGHT[method] / wsum / len(versions)
                        mix[dex] = mix.get(dex, 0) + share
            if not mix:
                sys.exit('gen_wild: area %r has no encounters' % name)
            areas.append({'name': name, 'region': reg['region'], 'gate': gate,
                          'maps': maps, 'mix': mix,
                          'band': band(caps[reg['region']], gate)})
        mine = [a for a in areas if a['region'] == reg['region']]

        def floor_of(dex):
            lvl = pre.get(dex)
            if lvl:
                return min(src_min[dex], lvl)
            return src_min[dex] if dex in pre else 0

        for a in mine:
            a['slots'] = [(d, w, floor_of(d)) for d, w in scale(a['mix'])]
        for dex, area, script in reg['legends']:
            if area not in names:
                sys.exit('gen_wild: legend #%d names unknown area %r' % (dex, area))
            idx = next(i for i, a in enumerate(areas) if a['name'] == area
                       and a['region'] == reg['region'])
            legends.append((dex, idx, legend_level(reg['source'], script, dex, refresh)))
    order = [d for d, _, _ in legends]
    if order != WA.LEGEND_ORDER[:len(order)] or len(order) != len(WA.LEGEND_ORDER):
        sys.exit('gen_wild: legends %s do not follow LEGEND_ORDER %s -- that '
                 'list is APPEND-ONLY, it indexes the save' % (order, WA.LEGEND_ORDER))
    return areas, legends


def scale(mix):
    """Probabilities -> integer weights summing to WEIGHT_TOTAL, largest
    remainder first, and never a present species rounded down to zero."""
    total = sum(mix.values())   # < 1 once an area has left a species out
    items = sorted(mix.items(), key=lambda kv: (-kv[1], kv[0]))
    raw = [(d, p / total * WEIGHT_TOTAL) for d, p in items]
    out = {d: max(1, int(x)) for d, x in raw}
    left = WEIGHT_TOTAL - sum(out.values())
    for d, x in sorted(raw, key=lambda dx: (-(dx[1] - int(dx[1])), dx[0])):
        if left <= 0:
            break
        out[d] += 1
        left -= 1
    while left < 0:   # only possible when many species were lifted to 1
        d = max(out, key=lambda k: (out[k], -k))
        out[d] -= 1
        left += 1
    return sorted(out.items(), key=lambda kv: (-kv[1], kv[0]))


def emit(areas, legends):
    n_regions = len(DEX_REGIONS)
    o = []
    o.append('// GENERATED by tools/gen_wild.py -- do not edit. The hand-written half\n'
             '// is tools/wild_areas.py; the species, rates and floors come from the\n'
             '// games\' own data (the pret disassemblies). tools/verify_wild.py proves\n'
             '// this file still matches both. Design: docs/explore-plan.md.\n'
             '#pragma once\n#include <stdint.h>\n\n')
    o.append('#define WILD_WEIGHT_TOTAL %d   // every area\'s weights sum to this\n' % WEIGHT_TOTAL)
    o.append('#define WILD_GATE_CHAMPION %d  // ladder positions: 0 open, 1-8 badges, 13 champion\n' % WA.GATE_CHAMPION)
    o.append('#define WILD_WHEN_MORN 1\n#define WILD_WHEN_DAY 2\n#define WILD_WHEN_NITE 4\n'
             '#define WILD_WHEN_ANY 7\n')
    o.append('#define WILD_REGIONS %d       // real regions, REGIONS order (no ALL)\n\n' % n_regions)
    o.append('struct WildSlot {\n'
             '  int16_t dex;       // NOT uint8_t -- see CLAUDE.md trap 2\n'
             '  uint16_t weight;   // share of WILD_WEIGHT_TOTAL\n'
             '  uint8_t when;      // WILD_WHEN_* bits\n'
             '  uint8_t floor;     // lowest level it may appear at, 0 = none\n'
             '};\n'
             'struct WildArea {\n'
             '  const char *name;\n'
             '  uint8_t region;    // REGIONS index\n'
             '  uint8_t gate;      // ladder position that opens it\n'
             '  uint16_t first;    // into WILD_SLOTS\n'
             '  uint8_t count;\n'
             '  uint8_t lvMin, lvMax;  // the band, from the gym ladder (trainers.h)\n'
             '};\n'
             '// APPEND-ONLY: the save stores a legendary by its index here.\n'
             'struct WildLegend {\n'
             '  int16_t dex;\n'
             '  uint8_t area;      // into WILD_AREAS\n'
             '  uint8_t level;     // the level the game fights it at\n'
             '};\n\n')
    o.append('static const WildSlot WILD_SLOTS[] = {\n')
    first = 0
    spans = []
    for a in areas:
        o.append('  // %s\n' % a['name'])
        for dex, w, floor in a['slots']:
            o.append('  { %d, %d, WILD_WHEN_ANY, %d },  // %s\n'
                     % (dex, w, floor, DEX[dex - 1][2]))
        spans.append((first, len(a['slots'])))
        first += len(a['slots'])
    o.append('};\n#define WILD_SLOT_COUNT %d\n\n' % first)
    o.append('static const WildArea WILD_AREAS[] = {\n')
    for a, (f, c) in zip(areas, spans):
        if c > 255:
            sys.exit('gen_wild: area %r has %d slots, past uint8_t' % (a['name'], c))
        o.append('  { "%s", %d, %d, %d, %d, %d, %d },\n'
                 % (a['name'], a['region'], a['gate'], f, c, a['band'][0], a['band'][1]))
    o.append('};\n#define WILD_AREA_COUNT %d\n\n' % len(areas))
    o.append('// Each region\'s slice of WILD_AREAS (first, count); count 0 = no areas yet,\n'
             '// and that region keeps the old region-wide pool.\n')
    o.append('static const uint8_t WILD_REGION_AREAS[WILD_REGIONS][2] = {\n')
    for r in range(n_regions):
        idx = [i for i, a in enumerate(areas) if a['region'] == r]
        if idx and idx != list(range(idx[0], idx[0] + len(idx))):
            sys.exit('gen_wild: region %d areas are not contiguous' % r)
        o.append('  { %d, %d },  // %s\n' % (idx[0] if idx else 0, len(idx), DEX_REGIONS[r][0]))
    o.append('};\n\n')
    o.append('static const WildLegend WILD_LEGENDS[] = {\n')
    for dex, area, lvl in legends:
        o.append('  { %d, %d, %d },  // %s, %s\n' % (dex, area, lvl, DEX[dex - 1][2], areas[area]['name']))
    o.append('};\n#define WILD_LEGEND_COUNT %d\n' % len(legends))
    return ''.join(o)


def check(areas):
    for a in areas:
        n = a['name']
        if len(n) > WA.NAME_MAX or not n.isascii() or n != n.upper():
            sys.exit('gen_wild: area name %r must be upper-case ASCII, <= %d'
                     % (n, WA.NAME_MAX))
        if a['gate'] not in tuple(range(9)) + (WA.GATE_CHAMPION,):
            sys.exit('gen_wild: area %r has gate %r' % (n, a['gate']))
        if sum(w for _, w, _ in a['slots']) != WEIGHT_TOTAL:
            sys.exit('gen_wild: area %r weights do not sum to %d' % (n, WEIGHT_TOTAL))


def main():
    refresh = '--refresh' in sys.argv
    areas, legends = build(refresh)
    check(areas)
    text = emit(areas, legends)
    with open(OUT, 'w') as f:
        f.write(text)
    print('wrote %s: %d areas, %d slots, %d legendaries'
          % (os.path.relpath(OUT, ROOT), len(areas),
             sum(len(a['slots']) for a in areas), len(legends)))


if __name__ == '__main__':
    main()
