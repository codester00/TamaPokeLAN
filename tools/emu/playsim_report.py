#!/usr/bin/env python3
"""Summarise playsim runs (tools/emu/playsim.sh) into one JSON report.

    python3 tools/emu/playsim_report.py casual.jsonl regular.jsonl ... > report.json

Each input is one profile's event stream (the lines starting with '{'). The
report gives, per profile: when each region's easy and hard ladders were
finished, the gym attempts and losses per leader, the fight and catch counts,
the stones, the money, the acquisition timeline and how each species came, the
slowest species, and whatever was never reached.
"""
import json
import os
import re
import sys
from collections import Counter, defaultdict

REGIONS = ['Kanto', 'Johto', 'Hoenn', 'Sinnoh', 'Unova', 'Kalos', 'Alola', 'Galar', 'Paldea']
STONES = ['Fire', 'Water', 'Thunder', 'Leaf', 'Moon', 'Sun', 'Shiny', 'Dusk', 'Dawn', 'Ice',
          'Black Augurite']


sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from dex_data import DEX  # noqa: E402

NAME = {row[0]: row[2] for row in DEX}
# Evolution-only species, from the generated dex: runs made before the bot told
# a hatch from an evolution label both 'evolve', and this sorts them.
_DEX_H = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'dex.h')).read()
EVO_ONLY = {int(n) for n in re.findall(r'R_EVO, [^}]*\},\s*// (\d+) ', _DEX_H)}


def region_of(d):
    from dex_data import REGIONS as R
    for i, row in enumerate(R):
        lo, hi = row[1], row[2]
        if lo <= d <= hi:
            return REGIONS[i] if i < len(REGIONS) else str(i)
    return '?'


def t(e):
    return e['day'] + e['h'] / 24.0


def summarise(path):
    evs = [json.loads(l) for l in open(path) if l.startswith('{')]
    start = next(e for e in evs if e['ev'] == 'start')
    weeks = [e for e in evs if e['ev'] == 'week']
    last = weeks[-1] if weeks else {}
    out = {'profile': start['profile'], 'seed': start['seed'], 'target': start['target'],
           'done': any(e['ev'] == 'done' for e in evs),
           'days': max((e['day'] for e in evs if 'day' in e), default=0),
           'crashed': not any(e['ev'] in ('end', 'done') for e in evs)}
    # ladders: the day each region's 13th badge came, per difficulty
    wins = defaultdict(set)
    ladder = {}
    attempts = Counter()
    losses = Counter()
    for e in evs:
        if e['ev'] != 'gym':
            continue
        k = (e['r'], e['i'], e['hard'])
        attempts[k] += 1
        if e['won']:
            wins[(e['r'], e['hard'])].add(e['i'])
            if len(wins[(e['r'], e['hard'])]) == 13 and (e['r'], e['hard']) not in ladder:
                ladder[(e['r'], e['hard'])] = round(t(e), 1)
        else:
            losses[k] += 1
    out['ladders'] = [{'region': REGIONS[r], 'easy': ladder.get((r, 0)), 'hard': ladder.get((r, 1))}
                      for r in range(len(REGIONS))]
    out['all_easy_day'] = max(ladder.get((r, 0)) or 1e9 for r in range(9))
    out['all_hard_day'] = max(ladder.get((r, 1)) or 1e9 for r in range(9))
    for k in ('all_easy_day', 'all_hard_day'):
        if out[k] >= 1e9:
            out[k] = None
    out['hardest_leaders'] = [
        {'region': REGIONS[r], 'seat': i + 1, 'hard': bool(h), 'attempts': attempts[(r, i, h)],
         'losses': n}
        for (r, i, h), n in losses.most_common(12)]
    out['gym_losses'] = sum(losses.values())
    # the acquisition timeline
    got = [e for e in evs if e['ev'] == 'got']
    for e in got:
        if e['how'] in ('evolve', 'hatch'):
            e['how'] = 'evolve' if e['dex'] in EVO_ONLY else 'hatch'
    out['how'] = Counter(e['how'] for e in got)
    out['curve'] = [[round(t(e), 2), start['target'] - e['left']] for e in got]
    out['last'] = [{'dex': e['dex'], 'name': e['name'], 'how': e['how'], 'day': round(t(e), 1)}
                   for e in got[-15:]]
    out['milestones'] = {}
    for frac in (0.5, 0.75, 0.9, 0.95, 0.99, 1.0):
        need = start['target'] * frac
        hit = next((p[0] for p in out['curve'] if p[1] >= need), None)
        out['milestones'][str(frac)] = hit
    # stones
    found = [e for e in evs if e['ev'] == 'stone_found']
    out['stones_found'] = Counter(STONES[e['stone']] for e in found)
    out['stones_bought'] = Counter(STONES[e['stone']] for e in evs if e['ev'] == 'buy_stone')
    out['first_stone_day'] = round(t(found[0]), 1) if found else None
    out['catch_fail'] = sum(1 for e in evs if e['ev'] == 'catch_fail')
    out['catch_fail_legend'] = sum(1 for e in evs if e['ev'] == 'catch_fail' and e['legend'] >= 0)
    out['stuck'] = sum(1 for e in evs if e['ev'] == 'stuck_fight')
    out['refused'] = sum(1 for e in evs if e['ev'] == 'gym_refused')
    out['evolve_why'] = Counter(e['why'] for e in evs if e['ev'] == 'evolve')
    out['totals'] = {k: last.get(k) for k in (
        'reg', 'left', 'badges_easy', 'badges_hard', 'fights', 'wild', 'catch', 'miss', 'gym',
        'gymwin', 'blackouts', 'farewells', 'evolves', 'stones_found', 'stones_bought', 'cords',
        'swaps', 'wallet', 'in', 'out', 'in_steps', 'in_prize', 'in_sold', 'out_balls', 'out_stones',
        'out_cords', 'out_x', 'out_vit', 'out_ppup', 'out_heal', 'sold', 'vit', 'vit_maxed', 'x_used',
        'ppup', 'heal_used', 'ui_miss')}
    # the day the wallet first reached the cap, if it did
    out['cap_day'] = next((w['day'] for w in weeks if w['wallet'] >= 999000), None)
    out['weekly'] = [{k: w.get(k) for k in ('day', 'left', 'badges_easy', 'badges_hard', 'wallet', 'fights',
                                            'in_steps', 'in_prize', 'in_sold', 'out')}
                     for w in weeks]
    # what never came: the goal is in the stream ('target', since the bot
    # learned to print it) or in --target FILE, a 'target' line saved apart
    goal = next((e['dex'] for e in evs if e['ev'] == 'target'), TARGET)
    if goal:
        have = {e['dex'] for e in got}
        miss = [d for d in goal if d not in have]
        out['missing'] = [{'dex': d, 'name': NAME.get(d, '?'), 'region': region_of(d)} for d in miss]
        out['missing_by_region'] = Counter(region_of(d) for d in miss)
    return out


TARGET = None


if __name__ == '__main__':
    args = sys.argv[1:]
    if args[:1] == ['--target']:
        TARGET = json.load(open(args[1]))['dex']
        args = args[2:]
    json.dump([summarise(p) for p in args], sys.stdout, indent=1, default=dict)
    print()
