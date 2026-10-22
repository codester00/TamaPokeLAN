#!/usr/bin/env python3
"""Move-expansion report: which real moves to add, by the owner's rules (2026-10-22).

Usage: python3 -I tools/move_report.py <pokeapi_csv_dir> <repo> <out.md> <out.json>

The CSV dir holds PokeAPI's data/v2/csv files (pokemon_moves, moves, move_meta,
move_meta_stat_changes, move_names, move_effect_prose, pokemon_species), fetched
from raw.githubusercontent.com. Writes docs/move-expansion-report.md.
"""
import csv, json, os, re, sys, unicodedata
from collections import defaultdict, Counter

CSV, REPO, OUT, OUTJ = sys.argv[1:5]
def rd(n): return list(csv.DictReader(open(os.path.join(CSV, n + '.csv'), encoding='utf-8')))

MAX_NAME, MAX_LOCAL = 12, 23
BUDGET = int(os.environ.get('BUDGET', 150))
LANG = {'9': 'EN', '7': 'ES', '5': 'FR', '6': 'DE', '8': 'IT'}
STAT_X = {7:'ACC',8:'EVA'}
TYPES = {1:'normal',2:'fighting',3:'flying',4:'poison',5:'ground',6:'rock',7:'bug',8:'ghost',
         9:'steel',10:'fire',11:'water',12:'grass',13:'electric',14:'psychic',15:'ice',
         16:'dragon',17:'dark',18:'fairy'}
# The owner's calls (2026-10-22): stat changes on damaging moves YES, accuracy
# and evasion stages NO, the pre-Gen-6 spellings YES, 150 of the 165 free
# slots. Each can be flipped from the environment to re-run the comparison.
STATHIT = os.environ.get('STATHIT', '1') == '1'
ACCEVA = os.environ.get('ACCEVA', '0') == '1'
OLDNAME = os.environ.get('OLDNAME', '1') == '1'
STAT = {2:'ATK',3:'DEF',4:'SPA',5:'SPD',6:'SPE'}           # 1 hp, 7 acc, 8 eva
AIL = {1:'PARA',2:'SLEEP',3:'FREEZE',4:'BURN',5:'POISON',6:'CONFUSE'}

moves = {int(r['id']): r for r in rd('moves') if int(r['id']) < 10000}
meta = {int(r['move_id']): r for r in rd('move_meta')}
stchg = defaultdict(list)
for r in rd('move_meta_stat_changes'): stchg[int(r['move_id'])].append((int(r['stat_id']), int(r['change'])))
prose = {int(r['move_effect_id']): r['short_effect'].replace('\n', ' ')
         for r in rd('move_effect_prose') if r['local_language_id'] == '9'}
names = defaultdict(dict)
for r in rd('move_names'):
    if r['local_language_id'] in LANG: names[int(r['move_id'])][LANG[r['local_language_id']]] = r['name']

def norm(s):
    s = s.replace('’', "'").replace('ß', 'SS').replace('œ', 'OE').replace('Œ', 'OE').replace('æ', 'AE')
    s = ''.join(c for c in unicodedata.normalize('NFKD', s) if not unicodedata.combining(c))
    return s.upper()

# ---------------------------------------------------------------- the engine
# Exactly what battle.cpp can express (see dex_moves.py's EF_* and AIL_*):
# a damaging move carries at most ONE effect, plus an optional secondary
# ailment; a status move either heals a % or applies ONE stage delta to a mask.
REGULAR = {1, 104, 379, 406}
ABSENT_MECHANIC = {       # the extra effect concerns something this game does not have
    150: 'hits a target mid-Fly', 148: 'hits a target mid-Dig', 258: 'hits a target mid-Dive',
    208: 'hits a target mid-Fly', 153: 'hits a target mid-Fly', 261: 'perfect accuracy in hail',
    334: 'hits a target mid-Fly / weather accuracy', 126: 'user thaws itself',
    133: 'heal varies with weather', 382: 'heal varies with weather', 224: 'hits through Protect',
    273: 'hits through Protect',
}
AIL_EFFECTS = {5, 7, 77, 3, 6, 261, 153, 334, 330, 126, 372}
MULTI = {30, 361}
NEVER = {18, 79, 381}
CHARGE_OPEN = {152, 332, 333}
CHARGE_HIDE = {156, 257, 256, 264, 273}
RECHARGE = {81}
FIXED_LVL = {88}
FIXED = {42: 40, 131: 20}
DRAIN = {4, 349}
RECOIL = {49: 4, 199: 3, 270: 2, 263: 3, 254: 3}
HEAL = {33, 133, 382}
STAGE_EFFECTS = {11, 12, 51, 52, 53, 55, 54, 322, 329, 61, 59, 63, 62, 19, 20, 60, 365,
                 157, 209, 213, 207, 212, 291, 317, 328, 206}

# Why a move was discarded -> the engine feature that would bring it back
def classify(mid):
    m, mt = moves[mid], meta.get(mid)
    if not m['effect_id']: return None, 'no battle data'
    eff = int(m['effect_id'])
    tid = int(m['type_id'])
    if tid not in TYPES: return None, 'type the game does not have'
    if mt is None: return None, 'no battle data'
    cat = int(m['damage_class_id'])          # 1 status 2 phys 3 spec
    power = int(m['power'] or 0)
    acc = int(m['accuracy'] or 0)
    pri = int(m['priority'])
    if int(mt['crit_rate']): return None, 'high critical-hit ratio'
    if int(mt['flinch_chance']): return None, 'flinch chance'
    ail = int(mt['meta_ailment_id'] or 0)
    ach = int(mt['ailment_chance'] or 0)
    sc = stchg.get(mid, [])
    note = ABSENT_MECHANIC.get(eff)
    enc = dict(type=TYPES[tid], cat={1:'STATUS',2:'PHYS',3:'SPEC'}[cat], power=power, acc=acc,
               effect='NONE', param=0, mask=[], stages=0, target='FOE', ail='NONE', ach=0)
    if cat == 1:
        if pri: return None, 'priority on a status move'
        if eff in HEAL:
            enc.update(effect='HEAL', param=int(mt['healing']) or 50, target='SELF')
            return enc, note
        if eff in STAGE_EFFECTS:
            if any(s not in (({**STAT, **STAT_X}) if ACCEVA else STAT) for s, _ in sc) or not sc:
                return None, 'accuracy/evasion stages'
            deltas = {c for _, c in sc}
            if len(deltas) != 1: return None, 'different stage amounts in one move'
            tgt = int(m['target_id'])
            enc.update(effect='STAGE', mask=[{**STAT, **STAT_X}[s] for s, _ in sc], stages=deltas.pop(),
                       target='SELF' if tgt in (7, 13, 4, 5) else 'FOE', acc=acc)
            return enc, note
        if eff in (24, 25, 17, 109, 278, 323) and not ACCEVA: return None, 'accuracy/evasion stages'
        if eff in (24, 25, 17, 109, 278, 323):
            ok = {**STAT, **STAT_X}
            deltas = {c for _, c in sc}
            if not sc or any(x not in ok for x, _ in sc) or len(deltas) != 1: return None, 'mixed stages'
            tgt = int(m['target_id'])
            enc.update(effect='STAGE', mask=[ok[x] for x, _ in sc], stages=deltas.pop(),
                       target='SELF' if tgt in (7, 13, 4, 5) else 'FOE', acc=acc)
            return enc, note
        if ail: return None, 'status move that only inflicts a status'
        return None, 'unique status effect'
    # damaging
    hitstat = None
    if int(mt['stat_chance']) or sc:
        if not STATHIT: return None, 'stat change on a damaging move'
        ok = {**STAT, **STAT_X} if ACCEVA else STAT
        deltas = {c for _, c in sc}
        if not sc or any(x not in ok for x, _ in sc) or len(deltas) != 1:
            return None, 'stat change on a damaging move (mixed/acc)'
        hitstat = dict(mask=[ok[x] for x, _ in sc], stages=deltas.pop(),
                       who='SELF' if mt['meta_category_id'] == '7' else 'FOE',
                       chance=int(mt['stat_chance']) or 100)
    if ail and ail not in AIL: return None, 'unsupported status (bad poison, trap, ...)'
    if eff in (78, 45, 105, 450): return None, 'fixed number of hits'
    if eff in (43, 262): return None, 'trapping damage over turns'
    if eff == 203: return None, 'unsupported status (bad poison, trap, ...)'
    if eff == 37: return None, 'random status'
    slot = None
    if eff in REGULAR or eff in ABSENT_MECHANIC and eff not in AIL_EFFECTS | CHARGE_HIDE | HEAL:
        slot = 'NONE'
    if eff in AIL_EFFECTS: slot = 'NONE'
    if eff in MULTI: slot = 'MULTI'
    if eff in NEVER: slot = 'NEVER_MISS'; enc['acc'] = 0
    if eff in CHARGE_OPEN: slot = 'CHARGE'; enc['param'] = 0
    if eff in CHARGE_HIDE: slot = 'CHARGE'; enc['param'] = 1
    if eff in RECHARGE: slot = 'RECHARGE'
    if eff in FIXED_LVL: slot = 'FIXED_LVL'
    if eff in FIXED: slot = 'FIXED'; enc['param'] = FIXED[eff]
    if eff in DRAIN: slot = 'DRAIN'; enc['param'] = int(mt['drain'])
    if eff in RECOIL: slot = 'RECOIL'; enc['param'] = RECOIL[eff]
    if slot is None and hitstat and eff in {73,71,74,70,72,205,141,396,69,140,219,230,277,21,397,139,183,272,296,297,331,335,359,375,405,469,358}:
        slot = 'NONE'
    if slot is None:
        return None, 'unique effect: ' + prose.get(eff, '?')[:60]
    if pri:
        if slot != 'NONE': return None, 'priority plus another effect'
        slot = 'PRIORITY'; enc['param'] = pri
    if eff in AIL_EFFECTS or (ail and ach):
        if ail not in AIL: return None, 'unsupported status (bad poison, trap, ...)'
        enc.update(ail=AIL[ail], ach=ach or 100)
    if power > 255: return None, 'power above 255'
    enc['effect'] = slot
    if hitstat: enc['hitstat'] = hitstat
    return enc, note

# ------------------------------------------------------------- learnsets
VG = {1: 7, 2: 4, 3: 6, 4: 9, 5: 14, 6: 16, 7: 18, 8: 20, 9: 25}   # last main game of its generation
species = {int(r['id']): r for r in rd('pokemon_species')}
def gen_of(d): return int(species[d]['generation_id'])
DEXN = max(i for i in species if i < 10000)
lv = defaultdict(lambda: defaultdict(dict))   # dex -> vg -> move -> level
tm = defaultdict(set)
for r in csv.DictReader(open(os.path.join(CSV, 'pokemon_moves.csv'))):
    p = int(r['pokemon_id'])
    if p > DEXN: continue
    meth = r['pokemon_move_method_id']
    mid, vg = int(r['move_id']), int(r['version_group_id'])
    if meth == '1':
        L = max(1, int(r['level'] or 0))
        old = lv[p][vg].get(mid)
        lv[p][vg][mid] = L if old is None else min(old, L)
    elif meth == '4':
        tm[p].add(mid)
def levelset(d):
    pref = VG[gen_of(d)]
    if lv[d].get(pref): return lv[d][pref], pref
    for vg in sorted(lv[d], key=lambda v: -v):     # newest game that has it
        if vg not in (19, 24, 30, 31, 32) and lv[d][vg]: return lv[d][vg], vg
    for vg in sorted(lv[d], key=lambda v: -v):
        if lv[d][vg]: return lv[d][vg], vg
    return {}, None

# ------------------------------------------------------------- our table
src = open(os.path.join(REPO, 'tools/dex_moves.py')).read()
have_slugs = set(re.findall(r'^\s*\("[^"]+",\s*"([a-z0-9-]+)"', src, re.M))
slug2id = {m['identifier']: i for i, m in moves.items()}
have = {slug2id[s] for s in have_slugs if s in slug2id}
have_dmg = {slug2id[sl] for sl, c in re.findall(r'^\s*\("[^"]+",\s*"([a-z0-9-]+)",\s*\'[a-z]+\',\s*(MC_[A-Z]+)', src, re.M)
            if c != 'MC_STATUS' and sl in slug2id}
mh = open(os.path.join(REPO, 'moves.h')).read()
rows = re.findall(r'\{ "([^"]+)", (T_[A-Z]+), (MC_[A-Z]+), (\d+),[^}]*\},\s*//\s*(\d+)', mh)
our_dmg = {int(i) for n, t, c, p, i in rows if c != 'MC_STATUS' and n != 'STRUGGLE'}
ofs = [int(x) for x in re.findall(r'\d+', mh.split('LEARN_OFS[DEX_COUNT + 2] = {')[1].split('};')[0])]
body = re.sub(r'//[^\n]*', '', mh.split('static const LearnEntry LEARN_TBL[')[1].split('= {', 1)[1].split('};')[0])
tbl = [(int(a), int(b)) for a, b in re.findall(r'\{\s*(\d+),\s*(\d+)\s*\}', body)]
dexh = open(os.path.join(REPO, 'dex.h')).read()
dexname = ['?'] + re.findall(r'\{ "([^"]+)", \d+, \d+, R_', dexh)
def our_first(d):
    ls = [l for m, l in tbl[ofs[d]:ofs[d + 1]] if l > 0 and m in our_dmg]
    return min(ls) if ls else None

# ------------------------------------------------------------- classify all
cand, discard = {}, defaultdict(list)
learners_lv, learners_tm = Counter(), Counter()
for d in range(1, DEXN + 1):
    ls, _ = levelset(d)
    for mid in ls: learners_lv[mid] += 1
    for mid in tm[d]: learners_tm[mid] += 1
name_fail = []
for mid in sorted(moves):
    if mid in have: continue
    if learners_lv[mid] == 0 and learners_tm[mid] == 0: continue
    enc, why = classify(mid)
    if enc is None:
        discard[why].append(mid); continue
    nm = {k: norm(v) for k, v in names[mid].items()}
    if OLDNAME and len(nm.get('EN', '')) > MAX_NAME and int(moves[mid]['generation_id']) <= 5 \
            and len(nm['EN'].replace(' ', '')) <= MAX_NAME:
        nm['EN'] = nm['EN'].replace(' ', '')
    long_ = [k for k, v in nm.items() if len(v) > (MAX_NAME if k == 'EN' else MAX_LOCAL)]
    missing = [k for k in ('EN', 'ES', 'FR', 'DE', 'IT') if k not in nm]
    if long_ or missing:
        name_fail.append((mid, nm, long_, missing)); continue
    enc['names'] = nm; enc['note'] = why
    cand[mid] = enc

# ------------------------------------------------------------- choose
real_first = {}
for d in range(1, DEXN + 1):
    ls, _ = levelset(d)
    dm = [(L, mid) for mid, L in ls.items() if mid in have_dmg or (mid in cand and cand[mid]['cat'] != 'STATUS')]
    real_first[d] = min(dm)[0] if dm else None
need = {}
for d in range(1, DEXN + 1):
    rf, of = real_first[d], our_first(d)
    if rf is None: continue
    if of is None or of > rf:
        ls, _ = levelset(d)
        opts = {mid for mid, L in ls.items() if L <= rf and mid in cand and cand[mid]['cat'] != 'STATUS'}
        if opts: need[d] = opts
chosen, why_chosen = [], {}
left = dict(need)
while left:
    cnt = Counter(m for o in left.values() for m in o)
    best = max(cnt, key=lambda m: (cnt[m], learners_lv[m], -m))
    fixed = [d for d, o in left.items() if best in o]
    chosen.append(best); why_chosen[best] = fixed
    for d in fixed: del left[d]
required = list(chosen)
rest = sorted((m for m in cand if m not in chosen), key=lambda m: (-learners_lv[m], -learners_tm[m], m))
for m in rest:
    if len(chosen) >= BUDGET: break
    chosen.append(m)
cut = [m for m in rest if m not in chosen]

# after: who is still attackless?
def first_after(d):
    ls, _ = levelset(d)
    ours = our_first(d)
    news = [L for mid, L in ls.items() if mid in chosen and cand[mid]['cat'] != 'STATUS']
    xs = [x for x in [ours] + news if x is not None]
    return min(xs) if xs else None
still = [d for d in range(1, DEXN + 1) if first_after(d) is None]
before_none = [d for d in range(1, DEXN + 1) if our_first(d) is None]
late_before = sum(1 for d in range(1, DEXN + 1) if (our_first(d) or 999) > 10)
late_after = sum(1 for d in range(1, DEXN + 1) if (first_after(d) or 999) > 10)

# ------------------------------------------------------------- report
def mname(m): return names[m].get('EN', moves[m]['identifier'])
def desc(e):
    if e['cat'] == 'STATUS':
        if e['effect'] == 'HEAL': return 'heal %d%%' % e['param']
        return '%s %s%+d' % (e['target'].lower(), '+'.join(e['mask']), e['stages'])
    bits = []
    if e['effect'] == 'NONE': pass
    elif e['effect'] == 'MULTI': bits.append('2-5 hits')
    elif e['effect'] == 'RECOIL': bits.append('recoil 1/%d' % e['param'])
    elif e['effect'] == 'DRAIN': bits.append('drain %d%%' % e['param'])
    elif e['effect'] == 'CHARGE': bits.append('charge' + (' (hidden)' if e['param'] else ''))
    elif e['effect'] == 'PRIORITY': bits.append('priority %+d' % e['param'])
    elif e['effect'] == 'FIXED': bits.append('%d damage' % e['param'])
    elif e['effect'] == 'FIXED_LVL': bits.append('damage = level')
    else: bits.append(e['effect'].lower().replace('_', ' '))
    if e['ail'] != 'NONE': bits.append('%d%% %s' % (e['ach'], e['ail'].lower()))
    return ', '.join(bits) or '-'
def row(m):
    e = cand[m]
    pw = str(e['power']) if e['power'] else '-'
    return '| %s | %s | %s | %s | %s | %d | %d |' % (e['names']['EN'], e['type'], e['cat'].lower(), pw,
                                                     desc(e), learners_lv[m], learners_tm[m])

L = []
P = L.append
P('# Move expansion report')
P('')
P('Generated %s from PokeAPI\'s CSV dump, against the move table on the branch. **Nothing in the game has '
  'changed yet**; this is the list to approve before the build.' % '2026-10-22')
P('')
P('## The rules applied')
P('')
P('- **Budget: %d new moves.** Every saved creature stores a move as one byte, so the table tops out at 255; '
  '90 are in use. Going past that is a save-format change for every creature on every board.' % BUDGET)
P('- **First, moves that fix attackless creatures.** A species needs a fix when it has a real attacking '
  'level-up move, but our table gives it none, or gives it its first one later than the real game does. '
  'Moves are chosen to fix the most such species first.')
P('- **Then, the moves learned by the most species** by level-up, until the budget is spent.')
P('- **Discarded outright: any effect the battle engine cannot reproduce** (listed below with what would bring '
  'each group back), and **any name too long** for the firmware: English over %d characters, or Spanish, '
  'French, German or Italian over %d after accents are dropped.' % (MAX_NAME, MAX_LOCAL))
P('- Levels come from each species\' own generation\'s last main game (FireRed, Crystal, Emerald, Platinum, '
  'Black 2/White 2, Omega Ruby, Ultra Sun, Sword, Scarlet), falling back to the newest game that has it.')
P('')
P('## The result')
P('')
P('| | before | after |')
P('|---|---|---|')
P('| species with no attacking level-up move | %d | %d |' % (len(before_none), len(still)))
P('| species whose first attack comes after level 10 | %d | %d |' % (late_before, late_after))
P('| moves in the table | 90 | %d |' % (90 + len(chosen)))
P('')
P('Fixing every creature whose first attack was missing or late takes **%d moves**; the other **%d** are the '
  'most widely learned of what is left. **%d** acceptable moves did not fit the budget.'
  % (len(required), len(chosen) - len(required), len(cut)))
P('')
if still:
    P('Still without an attacking level-up move after this (their real attacks were all discarded, or they '
      'have none in the games): ' + ', '.join(dexname[d] for d in still) + '.')
    P('')
fixed_now = [d for d in before_none if d not in still]
P('Fixed outright: ' + ', '.join(dexname[d] for d in fixed_now) + '.')
P('')
real_none, game_attack = [], []
for d in still:
    ls, _ = levelset(d)
    (game_attack if any(int(moves[m]['damage_class_id'] or 1) in (2, 3) for m in ls) else real_none).append(dexname[d])
P('**Decision for you.** Of the %d left, %d have no attack in the real games either (%s) -- the same kind of '
  'case as Magikarp, so your rule is met. The other **%d do attack in the games** (%s), but only with moves this '
  'rule discards. None of them is a NEW case -- all %d are without an attack in the game today too -- but they '
  'are cases the games did not have. Two engine features would close most of the gap: **a stat change on a '
  'damaging move** (Struggle Bug, for Wimpod and Blipbug, plus 60 other moves) and **flinch** (Astonish, for '
  'Applin, plus 20 others). Counter, Mirror Coat, Bide and Hidden Power are one-offs.'
  % (len(still), len(real_none), ', '.join(real_none), len(game_attack), ', '.join(game_attack), len(still)))
P('')
if still:
    P('### Why each of these is still without an attack')
    P('')
    P('| Creature | Its real attacking level-up moves | Why none can be added |')
    P('|---|---|---|')
    for d in still:
        ls, _ = levelset(d)
        atk = sorted(((lvl, mid) for mid, lvl in ls.items()
                      if int(moves[mid]['damage_class_id'] or 1) in (2, 3)), key=lambda x: x[0])
        if not atk:
            P('| %s | none -- it has no attack in the games either | -- |' % dexname[d]); continue
        whys = []
        for lvl, mid in atk[:4]:
            enc, why = classify(mid)
            if enc is not None:
                nmv = [n for n in name_fail if n[0] == mid]
                why = 'name too long' if nmv else 'not chosen'
            whys.append('%s (L%d): %s' % (mname(mid), lvl, why))
        P('| %s | %s | %s |' % (dexname[d], ', '.join(mname(m) for _, m in atk[:4]), '; '.join(whys)))
    P('')
P('## Part 1: moves that fix attackless or late creatures (%d)' % len(required))
P('')
P('In the order chosen. "Fixes" counts the species whose first attack this move supplies.')
P('')
P('| Move | Type | Kind | Power | Effect | Level-up learners | TM learners | Fixes |')
P('|---|---|---|---|---|---|---|---|')
for m in required:
    P(row(m) + ' %d |' % len(why_chosen[m]))
P('')
P('## Part 2: the most widely learned (%d)' % (len(chosen) - len(required)))
P('')
P('| Move | Type | Kind | Power | Effect | Level-up learners | TM learners |')
P('|---|---|---|---|---|---|---|')
for m in chosen[len(required):]:
    P(row(m))
P('')
notes = [m for m in chosen if cand[m]['note']]
if notes:
    P('## Kept, with an effect that does not exist in this game')
    P('')
    P('Their extra effect involves something our battles never have (weather, Protect, a target mid-Fly), '
      'so they play exactly as they would in the games here. The table already treats EARTHQUAKE, SURF and '
      'THUNDER this way. Say if you want these dropped too.')
    P('')
    for m in notes: P('- %s: %s' % (cand[m]['names']['EN'], cand[m]['note']))
    P('')
P('## Acceptable but over budget (%d)' % len(cut))
P('')
P(', '.join('%s (%d)' % (cand[m]['names']['EN'], learners_lv[m]) for m in cut[:80]) + (' ...' if len(cut) > 80 else ''))
P('')
P('## Discarded: effect the engine cannot reproduce')
P('')
P('Grouped by what is missing. The count is how many moves each engine feature would bring back, and in '
  'brackets the most widely learned of them.')
P('')
groups = sorted(discard.items(), key=lambda kv: -sum(learners_lv[m] for m in kv[1]))
P('| Missing feature | Moves | Most learned |')
P('|---|---|---|')
uniq = []
for why, ms in groups:
    if why == 'no battle data': continue
    if why.startswith('unique effect'):
        uniq.extend(ms); continue
    top = sorted(ms, key=lambda m: -learners_lv[m])[:6]
    P('| %s | %d | %s |' % (why, len(ms), ', '.join(mname(m) for m in top)))
top = sorted(uniq, key=lambda m: -learners_lv[m])[:10]
P('| a one-off effect of its own | %d | %s |' % (len(uniq), ', '.join(mname(m) for m in top)))
P('')
nobd = sorted(discard.get('no battle data', []), key=lambda m: -learners_lv[m])
P('## Not judged: PokeAPI has no battle data for these (%d)' % len(nobd))
P('')
P('Not an engine limit: PokeAPI has not yet filled in effect data for most Scarlet/Violet moves, so the '
  'rules above could not be applied. Several are plainly replicable (WAVE CRASH is recoil, AQUA CUTTER plain '
  'damage) and could be judged by hand from the games if you want them. Level-up learners in brackets.')
P('')
P(', '.join('%s (%d)' % (mname(m), learners_lv[m]) for m in nobd))
P('')
P('## Discarded: name too long (%d)' % len(name_fail))
P('')
P('Many of these are old moves whose modern English name gained a space: the games themselves spelled them '
  'THUNDERSHOCK, DYNAMICPUNCH, ANCIENTPOWER until Gen 6, inside the same 12 characters, which is how our table '
  'already names THUNDERSHOCK and ANCIENTPOWER. Allowing those older official spellings would bring back the '
  'ones marked *.')
P('')
P('| Move | Too long in | Level-up learners |')
P('|---|---|---|')
for mid, nm, long_, missing in sorted(name_fail, key=lambda x: -learners_lv[x[0]]):
    if missing and not long_: what = 'no official name in ' + ', '.join(missing)
    else: what = ', '.join('%s %d' % (k, len(nm[k])) for k in long_)
    star = ' *' if long_ == ['EN'] and len(nm['EN'].replace(' ', '')) <= MAX_NAME and int(moves[mid]['generation_id']) <= 5 else ''
    P('| %s%s | %s | %d |' % (names[mid].get('EN', moves[mid]['identifier']), star, what, learners_lv[mid]))
P('')
P('## Still to do in the build')
P('')
P('- **Portuguese names.** PokeAPI has no Portuguese; the table today uses Pokemon GO\'s Brazilian names '
  'where they exist and reviewed translations otherwise. Every new move needs one, at most %d characters.' % MAX_LOCAL)
P('- Effects and powers are the modern games\' (as the table already is); a move\'s Gen 1 value is not used.')
P('- New moves go at the END of the table, so no saved moveset changes.')
open(OUT, 'w').write('\n'.join(L) + '\n')
json.dump({'chosen': [dict(id=m, slug=moves[m]['identifier'], required=m in required, **cand[m]) for m in chosen],
           'still_attackless': [dexname[d] for d in still]}, open(OUTJ, 'w'), indent=1)
print('required', len(required), 'chosen', len(chosen), 'cut', len(cut), 'name_fail', len(name_fail),
      'discard', sum(len(v) for v in discard.values()))
print('before none', len(before_none), 'after none', len(still), [dexname[d] for d in still])
print('late>10 before', late_before, 'after', late_after)
