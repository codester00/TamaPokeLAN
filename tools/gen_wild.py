#!/usr/bin/env python3
"""Build wild_areas.h -- Explore's per-area encounter tables -- from the games.

    python3 tools/gen_wild.py            # fetch (cached) + emit wild_areas.h
    python3 tools/gen_wild.py --refresh  # re-download the sources first

Two inputs, kept apart on purpose:

- tools/wild_areas.py, HAND-WRITTEN: which maps make an area, its name, its
  badge gate, where each legendary lives.
- the pret disassemblies, FETCHED: every species, encounter rate, level and
  time of day, and every legendary's level. Nothing a game decides is typed
  by hand, so nothing can be misremembered.

The raw sources are cached in tools/wild_cache/ (gitignored). Output is
deterministic: two runs over the same sources give the same bytes, which is
what lets verify_wild.py compare the committed header against a fresh build.

How an area's weights are made (docs/explore-plan.md § 12), once for EACH time
band (morning, day, night -- Crystal's own bands; a game with no times of day
gives three identical answers):
  1. per version, per encounter method: average the method's species
     distribution over the area's maps that have that method in this band,
     using each slot's in-game rate;
  2. mix the methods by METHOD_WEIGHT, renormalised over the methods present;
  3. average the versions, so an exclusive comes in at half weight;
  4. scale to WILD_WEIGHT_TOTAL, never rounding a present species to zero.
A species whose weight is the same in several bands is emitted ONCE with those
bands' bits, so a game without times of day costs no more than before.

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

MORN, DAY, NITE = 1, 2, 4
ANY = MORN | DAY | NITE
BANDS = (MORN, DAY, NITE)

FRLG = 'https://raw.githubusercontent.com/pret/pokefirered/master/'
CRYSTAL = 'https://raw.githubusercontent.com/pret/pokecrystal/master/'
EMERALD = 'https://raw.githubusercontent.com/pret/pokeemerald/master/'
# The Gen 3 decomps share one JSON shape and one script language.
GEN3 = {'frlg': FRLG, 'emerald': EMERALD}

SLUG = {slug: n for n, slug, *_ in DEX}

# source -> every map that source has wild encounters on, filled in by the
# loaders for verify_wild.py's "nothing silently dropped" check.
SOURCE_MAPS = {}

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
    """SPECIES_MR_MIME / MR__MIME -> 122. An unknown species is a hard error."""
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


# ---------------------------------------------------------------------------
# Sources. Each loader returns
#   {version: {map: {method: [(dex, level, rate, when), ...]}}}
# with `rate` the slot's in-game percentage within its method and `when` the
# time bands it applies to.

def load_gen3(src, refresh=False):
    """FireRed/LeafGreen or Emerald: src/data/wild_encounters.json. FRLG keeps
    one table per version (the label ends in FireRed or LeafGreen); Emerald is
    one game. A map listed twice (Emerald's Altering Cave: nine sets, the
    first being what the game uses until a Mystery Event changes it) keeps
    its FIRST table, which is what the engine reads by default."""
    d = json.loads(fetch(GEN3[src] + 'src/data/wild_encounters.json',
                         '%s_wild_encounters.json' % src, refresh))
    group = d['wild_encounter_groups'][0]
    rates = {f['type']: f['encounter_rates'] for f in group['fields']}
    fish = next(f for f in group['fields'] if f['type'] == 'fishing_mons')
    # Only the best rod counts (plan § 4); its slots are read from the
    # file's own groups where it has them (Emerald), FRLG's being 5-9 too.
    super_rod = set(fish.get('groups', {}).get('super_rod', range(5, 10)))
    keys = {'land_mons': 'land', 'water_mons': 'water',
            'fishing_mons': 'fishing', 'rock_smash_mons': 'rock_smash'}
    out = {'FireRed': {}, 'LeafGreen': {}} if src == 'frlg' else {'Emerald': {}}
    for e in group['encounters']:
        ver = next((v for v in out if e['base_label'].endswith(v)), None)
        if src == 'emerald':
            ver = 'Emerald'
        if ver is None:
            sys.exit('gen_wild: encounter %s belongs to no version' % e['base_label'])
        mp = e['map'].replace('MAP_', '')
        if mp in out[ver]:
            continue
        m = out[ver][mp] = {}
        for key, method in keys.items():
            if key not in e:
                continue
            slots = list(enumerate(e[key]['mons']))
            if method == 'fishing':
                slots = [(i, s) for i, s in slots if i in super_rod]
            m[method] = [(dex_of(s['species']), s['min_level'], rates[key][i], ANY)
                         for i, s in slots]
    SOURCE_MAPS[src] = set().union(*(set(v) for v in out.values()))
    return out


def camel_to_const(name):
    """Crystal's map file names -> its map constants: MountMortar1FOutside ->
    MOUNT_MORTAR_1F_OUTSIDE. Every result is checked against the encounter
    tables' own constants by the caller, so a bad split fails loudly."""
    s = re.sub(r'(?<=[a-z])(?=[A-Z0-9])', '_', name)
    s = re.sub(r'(?<=[A-Z])(?=[A-Z][a-z])', '_', s)
    return s.upper()


def crystal_prob(text, label):
    """A cumulative percentage table from probabilities.asm -> per-slot rates."""
    block = text.split(label + ':')[1].split('assert_table_length')[0]
    cum = [int(x) for x in re.findall(r'mon_prob\s+(\d+),', block)]
    return [b - a for a, b in zip([0] + cum, cum)]


def crystal_wild(text, kind, rates):
    """johto_grass/kanto_grass (kind 'grass': morn/day/nite blocks of 7) or the
    water files (kind 'water': one block of 3) -> {map: [(dex, lvl, rate, when)]}."""
    out = {}
    for mp, body in re.findall(r'def_%s_wildmons (\w+)(.*?)end_%s_wildmons' % (kind, kind),
                               text, re.S):
        mons = re.findall(r'db\s+(\d+),\s*(\w+)', body)
        if kind == 'grass':
            n = len(rates)
            if len(mons) != 3 * n:
                sys.exit('gen_wild: %s has %d grass slots' % (mp, len(mons)))
            slots = []
            for b, when in enumerate(BANDS):
                for i in range(n):
                    lvl, sp = mons[b * n + i]
                    slots.append((dex_of(sp), int(lvl), rates[i], when))
        else:
            slots = [(dex_of(sp), int(lvl), rates[i], ANY) for i, (lvl, sp) in enumerate(mons)]
        out[mp] = slots
    return out


def crystal_fish(fish, maps_asm, water_maps):
    """The SUPER rod's table for each map with water, by its map's fish group.
    Time groups are day (Crystal's day covers morning too) or night."""
    group_of = {}
    for camel, grp in re.findall(r'^\s*map (\w+),.*?(FISHGROUP_\w+)\s*$', maps_asm, re.M):
        group_of[camel_to_const(camel)] = grp
    order = re.findall(r'fishgroup [^,]+,\s*\.(\w+)_Old', fish)
    time_rows = re.findall(r'db (\w+),\s*(\d+),\s*(\w+),\s*(\d+) ; \d+',
                           fish.split('TimeFishGroups:')[1])
    names = {}
    for i, base in enumerate(order):
        names['FISHGROUP_' + camel_to_const(base)] = base
    out = {}
    for mp in water_maps:
        grp = group_of.get(mp)
        if not grp or grp == 'FISHGROUP_NONE':
            continue
        base = names.get(grp)
        if base is None:
            sys.exit('gen_wild: %s uses fish group %s, not in fish.asm' % (mp, grp))
        block = fish.split('.%s_Super:' % base)[1].split('\n.')[0]
        prev, slots = 0, []
        for line in block.splitlines():
            m = re.match(r'\s*db\s+(\d+) percent(?: \+ 1)?,\s*(.*)$', line)
            if not m:
                continue
            cum, rest = int(m.group(1)), m.group(2)
            rate, prev = cum - prev, cum
            tg = re.match(r'time_group (\d+)', rest)
            if tg:
                dsp, dlv, nsp, nlv = time_rows[int(tg.group(1))]
                slots.append((dex_of(dsp), int(dlv), rate, MORN | DAY))
                slots.append((dex_of(nsp), int(nlv), rate, NITE))
            else:
                sp, lv = [x.strip() for x in rest.split(',')]
                slots.append((dex_of(sp), int(lv), rate, ANY))
        out[mp] = slots
    return out


def crystal_trees(treemons, set_name):
    """A headbutt tree set -> slots. Common and rare trees depend on the player's
    ID, so each counts half."""
    block = treemons.split('TreeMonSet_%s:' % set_name)[1].split('\nTreeMonSet_')[0]
    tables = block.split('db -1')
    slots = []
    lists = [t for t in tables if re.search(r'db\s+\d+,\s*\w+,\s*\d+', t)]
    for t in lists:
        for pct, sp, lv in re.findall(r'db\s+(\d+),\s*(\w+),\s*(\d+)', t):
            slots.append((dex_of(sp), int(lv), int(pct) / len(lists), ANY))
    return slots


def load_crystal(refresh=False, needed=()):
    """Crystal's grass, surf, super rod, Headbutt and Rock Smash tables, for the
    maps the area table names (`needed`)."""
    g = lambda p: fetch(CRYSTAL + p, 'crystal_' + p.replace('/', '_'), refresh)
    prob = g('data/wild/probabilities.asm')
    grass_r = crystal_prob(prob, 'GrassMonProbTable')
    water_r = crystal_prob(prob, 'WaterMonProbTable')
    grass, water = {}, {}
    for f in ('johto', 'kanto'):
        grass.update(crystal_wild(g('data/wild/%s_grass.asm' % f), 'grass', grass_r))
        water.update(crystal_wild(g('data/wild/%s_water.asm' % f), 'water', water_r))
    maps_asm = g('data/maps/maps.asm')
    fish = crystal_fish(g('data/wild/fish.asm'), maps_asm, set(water))
    treemons = g('data/wild/treemons.asm')
    # Two tables in one file, read the way the engine reads them: Headbutt
    # looks a map up in TreeMonMaps, Rock Smash in RockMonMaps (GetTreeMonSet
    # in engine/events/treemons.asm takes the first entry for the map).
    tm = g('data/wild/treemon_maps.asm')
    def first_sets(section):
        block = tm.split(section + ':')[1].split('\n\n')[0]
        sets = {}
        for mp, st in re.findall(r'treemon_map (\w+),\s*TREEMON_SET_(\w+)', block):
            sets.setdefault(mp, st)
        return sets
    tree_of, rock_of = first_sets('TreeMonMaps'), first_sets('RockMonMaps')
    out = {}
    for mp in needed:
        m = {}
        if mp in grass:
            m['land'] = grass[mp]
        if mp in water:
            m['water'] = water[mp]
        if mp in fish:
            m['fishing'] = fish[mp]
        if tree_of.get(mp, 'NONE') != 'NONE':
            m['headbutt'] = crystal_trees(treemons, tree_of[mp].capitalize())
        if rock_of.get(mp, 'NONE') != 'NONE':
            m['rock_smash'] = crystal_trees(treemons, rock_of[mp].capitalize())
        if not m:
            sys.exit('gen_wild: Crystal map %s has no encounters of any kind' % mp)
        out[mp] = m
    # Every map the source has encounters on, for verify_wild.py's coverage
    # check: the Johto tables, Johto's trees and rocks, and the Kanto-file maps
    # that are Johto's road to its Elite Four.
    johto = set(crystal_wild(g('data/wild/johto_grass.asm'), 'grass', grass_r))
    johto |= set(crystal_wild(g('data/wild/johto_water.asm'), 'water', water_r))
    johto |= {k for k, v in tree_of.items() if v != 'NONE' and not re.match(r'ROUTE_(\d|1\d|2[0-5])$', k)}
    johto |= {k for k, v in rock_of.items() if v != 'NONE'}
    SOURCE_MAPS['crystal'] = johto | {'ROUTE_26', 'ROUTE_27', 'ROUTE_28', 'TOHJO_FALLS', 'VICTORY_ROAD'}
    return {'Crystal': out}


def legend_level(src, leg, refresh=False):
    """The level a legendary is fought at, read from the game's own script."""
    dex = leg['dex']
    if src == 'emerald' and leg.get('roams'):
        # Latias and Latios roam from src/roamer.c, created at one level.
        text = fetch(EMERALD + 'src/roamer.c', 'emerald_src_roamer.c', refresh)
        names = re.findall(r'ROAMER->species = (SPECIES_\w+);', text)
        lvl = re.search(r'CreateMon\([^,]+, ROAMER->species, (\d+),', text)
        if lvl and dex in [dex_of(n) for n in names]:
            return int(lvl.group(1))
        sys.exit('gen_wild: #%d is not one of roamer.c\'s %s' % (dex, names))
    if 'script' not in leg and not leg.get('roams'):
        sys.exit('gen_wild: legend #%d needs the script it is fought in' % dex)
    if src in GEN3:
        text = fetch(GEN3[src] + 'data/maps/%s/scripts.inc' % leg['script'],
                     '%s_%s_scripts.inc' % (src, leg['script']), refresh)
        pat = r'setwildbattle\s+(SPECIES_\w+),\s*(\d+)'
    elif leg.get('roams'):
        # Raikou and Entei start roaming from InitRoamMons at one shared level.
        text = fetch(CRYSTAL + 'engine/overworld/wildmons.asm',
                     'crystal_engine_overworld_wildmons.asm', refresh)
        init = text.split('InitRoamMons:')[1].split('\n\n\n')[0]
        names = re.findall(r'ld a, (\w+)\s*\n\s*ld \[wRoamMon\dSpecies\]', init)
        lvl = re.search(r'; level\s*\n\s*ld a, (\d+)', init)
        if lvl and dex in [dex_of(n) for n in names]:
            return int(lvl.group(1))
        sys.exit('gen_wild: #%d is not one of InitRoamMons %s' % (dex, names))
    else:
        text = fetch(CRYSTAL + 'maps/%s.asm' % leg['script'],
                     'crystal_maps_%s.asm' % leg['script'], refresh)
        pat = r'loadwildmon\s+(\w+),\s*(\d+)'
    for const, lvl in re.findall(pat, text):
        if dex_of(const) == dex:
            return int(lvl)
    sys.exit('gen_wild: no wild battle for #%d in %s' % (dex, leg['script']))


def super_rod_gate(reg, refresh=False):
    """The ladder position the region's best rod is handed out at, after
    checking that the named script really is where the game gives it."""
    rod = reg['super_rod']
    if reg['source'] in GEN3:
        text = fetch(GEN3[reg['source']] + 'data/maps/%s/scripts.inc' % rod['script'],
                     '%s_%s_scripts.inc' % (reg['source'], rod['script']), refresh)
        pat = r'(?:give|add)item\s+ITEM_SUPER_ROD\b'
    else:
        text = fetch(CRYSTAL + 'maps/%s.asm' % rod['script'],
                     'crystal_maps_%s.asm' % rod['script'], refresh)
        pat = r'giveitem\s+SUPER_ROD\b'
    if not re.search(pat, text):
        sys.exit('gen_wild: %s does not give the Super Rod' % rod['script'])
    if rod['gate'] not in tuple(range(9)) + (WA.GATE_CHAMPION,):
        sys.exit('gen_wild: super rod gate %r' % rod['gate'])
    return rod['gate']


def load_source(reg, refresh=False):
    if reg['source'] in GEN3:
        return load_gen3(reg['source'], refresh)
    if reg['source'] == 'crystal':
        needed = [mp for _n, _g, maps in reg['areas'] for mp in maps]
        return load_crystal(refresh, needed)
    sys.exit('gen_wild: unknown source %r' % reg['source'])


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


def mix_band(data, versions, maps, when, rod_gate=0):
    """One time band's distribution over an area (steps 1-3), keyed by
    (species, gate): the best rod's share carries rod_gate, so a species both
    walked into and fished up splits into an open slot and a gated one."""
    mix = {}
    for v in versions:
        per_method = {}
        for method in WA.METHOD_WEIGHT:
            dists = []
            for mp in maps:
                slots = [s for s in data[v].get(mp, {}).get(method, []) if s[3] & when]
                if not slots:
                    continue
                tot = sum(s[2] for s in slots)
                dist = {}
                for dex, _lv, r, _w in slots:
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
            gate = rod_gate if method == 'fishing' else 0
            for dex, p in dist.items():
                key = (dex, gate)
                mix[key] = mix.get(key, 0) + p * WA.METHOD_WEIGHT[method] / wsum / len(versions)
    return mix


def build(refresh=False):
    """-> (areas, legends) in emit order. Each area is a dict with name,
    region, gate, maps, band and slots [(dex, weight, when, floor)]; each legend
    a dict with dex, area (index), level, gate and roams."""
    pre = pre_evolutions()
    caps = ladder_caps()
    areas, legends = [], []
    for reg in WA.REGIONS:
        data = load_source(reg, refresh)
        rod = super_rod_gate(reg, refresh)
        versions = sorted(data)
        src_min = {}   # the lowest level each species is wild at in this game
        for v in versions:
            for methods in data[v].values():
                for slots in methods.values():
                    for dex, lv, _r, _w in slots:
                        src_min[dex] = min(src_min.get(dex, 255), lv)

        def floor_of(dex):
            lvl = pre.get(dex)
            if lvl:
                return min(src_min[dex], lvl)
            return src_min[dex] if dex in pre else 0

        names = set()
        for name, gate, maps in reg['areas']:
            if name in names:
                sys.exit('gen_wild: area %r twice' % name)
            names.add(name)
            # A rod handed out at or before the area opens holds nothing back.
            rod_gate = rod if rod > gate else 0
            per_band = {}
            for when in BANDS:
                mix = mix_band(data, versions, maps, when, rod_gate)
                if not mix:
                    sys.exit('gen_wild: area %r has no encounters at time %d' % (name, when))
                if not any(g == 0 for _d, g in mix):
                    # The rod gate is SECRET (owner's call): an area that held
                    # nothing until it opened would read as empty on the panel.
                    sys.exit('gen_wild: area %r has only rod encounters at time %d'
                             % (name, when))
                per_band[when] = dict(scale(mix))
            # One slot per (species, weight, gate) with the bands that share it.
            slots = {}
            for when in BANDS:
                for key, w in per_band[when].items():
                    k = key + (w,)
                    slots[k] = slots.get(k, 0) | when
            ordered = sorted(slots.items(),
                             key=lambda kv: (-kv[0][2], kv[0][0], kv[0][1], kv[1]))
            areas.append({'name': name, 'region': reg['region'], 'gate': gate,
                          'maps': maps, 'band': band(caps[reg['region']], gate),
                          'per_band': per_band,
                          'slots': [(dex, w, when, floor_of(dex), g)
                                    for (dex, g, w), when in ordered]})
        for leg in reg['legends']:
            if leg['area'] not in names:
                sys.exit('gen_wild: legend #%d names unknown area %r' % (leg['dex'], leg['area']))
            idx = next(i for i, a in enumerate(areas) if a['name'] == leg['area']
                       and a['region'] == reg['region'])
            gate = leg.get('gate', areas[idx]['gate'])
            if gate not in tuple(range(9)) + (WA.GATE_CHAMPION,) or gate < areas[idx]['gate']:
                sys.exit('gen_wild: legend #%d gate %r is not at or after its area\'s'
                         % (leg['dex'], gate))
            legends.append({'dex': leg['dex'], 'area': idx, 'gate': gate,
                            'roams': bool(leg.get('roams')),
                            'level': legend_level(reg['source'], leg, refresh)})
    order = [lg['dex'] for lg in legends]
    if order != WA.LEGEND_ORDER:
        sys.exit('gen_wild: legends %s do not follow LEGEND_ORDER %s -- that '
                 'list is APPEND-ONLY, it indexes the save' % (order, WA.LEGEND_ORDER))
    return areas, legends


def scale(mix):
    """Probabilities -> integer weights summing to WEIGHT_TOTAL, largest
    remainder first, and never a present species rounded down to zero."""
    total = sum(mix.values())
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
        d = max(out, key=lambda k: (out[k], tuple(-x for x in k)))
        out[d] -= 1
        left += 1
    return sorted(out.items(), key=lambda kv: (-kv[1], kv[0]))


WHEN_NAME = {ANY: 'WILD_WHEN_ANY', MORN: 'WILD_WHEN_MORN', DAY: 'WILD_WHEN_DAY',
             NITE: 'WILD_WHEN_NITE', MORN | DAY: 'WILD_WHEN_MORN | WILD_WHEN_DAY',
             MORN | NITE: 'WILD_WHEN_MORN | WILD_WHEN_NITE',
             DAY | NITE: 'WILD_WHEN_DAY | WILD_WHEN_NITE'}


def emit(areas, legends):
    n_regions = len(DEX_REGIONS)
    o = []
    o.append('// GENERATED by tools/gen_wild.py -- do not edit. The hand-written half\n'
             '// is tools/wild_areas.py; the species, rates and floors come from the\n'
             '// games\' own data (the pret disassemblies). tools/verify_wild.py proves\n'
             '// this file still matches both. Design: docs/explore-plan.md.\n'
             '#pragma once\n#include <stdint.h>\n\n')
    o.append('#define WILD_WEIGHT_TOTAL %d   // each time band\'s weights in an area sum to this\n' % WEIGHT_TOTAL)
    o.append('#define WILD_GATE_CHAMPION %d  // ladder positions: 0 open, 1-8 badges, 13 champion\n' % WA.GATE_CHAMPION)
    o.append('#define WILD_WHEN_MORN 1\n#define WILD_WHEN_DAY 2\n#define WILD_WHEN_NITE 4\n'
             '#define WILD_WHEN_ANY 7\n')
    o.append('#define WILD_LEGEND_ROAMS 1   // WildLegend::flags: met anywhere in its region\n')
    o.append('#define WILD_REGIONS %d       // real regions, REGIONS order (no ALL)\n\n' % n_regions)
    o.append('struct WildSlot {\n'
             '  int16_t dex;       // NOT uint8_t -- see CLAUDE.md trap 2\n'
             '  uint16_t weight;   // share of WILD_WEIGHT_TOTAL in each band it is in\n'
             '  uint8_t when;      // WILD_WHEN_* bits\n'
             '  uint8_t floor;     // lowest level it may appear at, 0 = none\n'
             '  uint8_t gate;      // ladder position that lets it appear: the best rod, 0 = open\n'
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
             '  uint16_t area;     // its home, into WILD_AREAS -- NOT uint8_t: nine regions pass 255\n'
             '  uint8_t level;     // the level the game fights it at, read from its script\n'
             '  uint8_t gate;      // ladder position that lets it appear (at or after its area\'s)\n'
             '  uint8_t flags;     // WILD_LEGEND_ROAMS\n'
             '};\n\n')
    o.append('static const WildSlot WILD_SLOTS[] = {\n')
    first = 0
    spans = []
    for a in areas:
        o.append('  // %s\n' % a['name'])
        for dex, w, when, floor, gate in a['slots']:
            o.append('  { %d, %d, %s, %d, %d },  // %s%s\n'
                     % (dex, w, WHEN_NAME[when], floor, gate, DEX[dex - 1][2],
                        ', best rod' if gate else ''))
        spans.append((first, len(a['slots'])))
        first += len(a['slots'])
    o.append('};\n#define WILD_SLOT_COUNT %d\n\n' % first)
    o.append('static const WildArea WILD_AREAS[] = {\n')
    for a, (f, c) in zip(areas, spans):
        if c > 255:
            sys.exit('gen_wild: area %r has %d slots, past uint8_t' % (a['name'], c))
        o.append('  { "%s", %d, %d, %d, %d, %d, %d },\n'
                 % (a['name'], a['region'], a['gate'], f, c, a['band'][0], a['band'][1]))
    if len(areas) >= 0xFFFF:
        sys.exit('gen_wild: %d areas -- 0xFFFF is the no-area sentinel' % len(areas))
    o.append('};\n#define WILD_AREA_COUNT %d\n\n' % len(areas))
    o.append('// Each region\'s slice of WILD_AREAS (first, count); count 0 = no areas yet,\n'
             '// and that region keeps the old region-wide pool.\n')
    o.append('static const uint16_t WILD_REGION_AREAS[WILD_REGIONS][2] = {\n')
    for r in range(n_regions):
        idx = [i for i, a in enumerate(areas) if a['region'] == r]
        if idx and idx != list(range(idx[0], idx[0] + len(idx))):
            sys.exit('gen_wild: region %d areas are not contiguous' % r)
        o.append('  { %d, %d },  // %s\n' % (idx[0] if idx else 0, len(idx), DEX_REGIONS[r][0]))
    o.append('};\n\n')
    o.append('static const WildLegend WILD_LEGENDS[] = {\n')
    for lg in legends:
        o.append('  { %d, %d, %d, %d, %s },  // %s, %s%s\n'
                 % (lg['dex'], lg['area'], lg['level'], lg['gate'],
                    'WILD_LEGEND_ROAMS' if lg['roams'] else '0',
                    DEX[lg['dex'] - 1][2], areas[lg['area']]['name'],
                    ', roams' if lg['roams'] else ''))
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
        for when in BANDS:
            if sum(w for _d, w, wh, _f, _g in a['slots'] if wh & when) != WEIGHT_TOTAL:
                sys.exit('gen_wild: area %r band %d does not sum to %d' % (n, when, WEIGHT_TOTAL))


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
