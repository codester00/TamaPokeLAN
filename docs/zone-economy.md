# The Battle Zone economy (v4.09 tuning pass, 2026-11-05)

Measured with `tools/emu/zone_sim.cpp` (500 runs a row, the firmware's own
rules: `gauntletChallenger()`, `tourDraw()`/`tourFoe()`, `bhallFoe()`, the real
battle engine). Rebuild and re-run it after any change to a zone number:

```bash
eval "$(grep -E '^CORE=' tools/emu/tests/run.sh | sed 's/\$ROOT/./g; s/\$EMU/tools\/emu/g')"
g++ -std=c++17 -O2 -w -Itools/emu -I. tools/emu/zone_sim.cpp "${CORE[@]}" -o /tmp/zone_sim
ZS_N=500 /tmp/zone_sim        # ZS_ONLY=gaunt|tour|money|hall for one section
```

## The owner's intent (2026-11-05)

- **PP MAX and the BOTTLE CAP** take one gauntlet run of about 40 wins, or 3-4
  casual runs.
- **The Master Ball** about a week, which is 6-7 PP MAXes.
- **Walking is supplementary**; replaying gyms is an income source.
- A steady-state end-game player has **somewhere to spend money**, but nothing
  frustratingly or unnecessarily blocks them from playing.
- **PP is the gauntlet's real barrier**, and PP UPs are what get a team past it.

`zonetune_test` pins the first, second and fourth of these as RULES (on the
prices and `gauntletBp()`), so a later change that breaks the intent fails a
test rather than quietly shipping.

## Profiles

| profile | team |
|---|---|
| CASUAL | random final forms, base stats 470-540, IV 16, training 50, the four strongest level-up attacks |
| GOOD | one of the 12 strongest final forms by 1v1 wins (they include Lunala, Manaphy and the Paradox creatures), IV 31, training 100, TMs |
| +PPUP | the same with three PP UPs on every move slot |

Both sides of the sim use the smart AI; the player's side never switches by
choice. A real player who switches and picks counters does better than CASUAL
and somewhat better than GOOD.

## Gauntlet (any type, no cap, all forms, level 100)

| profile | team | wins p10 / p50 / p90 | BP a run (1x) | PP left when the run ended |
|---|---|---|---|---|
| CASUAL | 3 | 6 / 10 / 14 | 31 | 63% |
| CASUAL +PPUP | 3 | 7 / 11 / 15 | 33 | 76% |
| GOOD | 3 | 19 / 19 / 28 | 71 | 31% (12% nearly dry) |
| GOOD +PPUP | 3 | 19 / 26 / 37 | 87 | 44% |
| GOOD | 6 | 19 / 22 / 28 | 73 | 28% (19% nearly dry) |
| GOOD +PPUP | 6 | 19 / 30 / 39 | 94 | 39% |

- A casual team loses on STRENGTH (two thirds of its PP left); a good team
  loses on PP. **PP UPs add 5-8 wins to a good team and almost nothing to a
  casual one** -- they are the reward for a team worth investing in.
- **The first boss (the 20th challenger) is the wall**: half of good teams
  without PP UPs stop exactly there (p10 = p50 = 19). With PP UPs most pass.
  The second boss (40th) is the very best's: about 1 run in 10.
- BP for exactly W wins at 1x: 10 wins 30, 20 wins 70, 30 wins 100, **40 wins
  140**, 50 wins 170.

## Tournament (500 entries a row)

| profile | team | champion | BP an entry | battles an entry |
|---|---|---|---|---|
| CASUAL | 3 | 0% | 3.2 | 2.1 |
| CASUAL | 6 | 6% | 6.6 | 2.9 |
| GOOD | 3 | 23% | 9.5 | 3.2 |
| GOOD +PPUP | 3 | 33% | 11.2 | 3.3 |
| GOOD | 6 | 99% | 22.0 | 4.0 |

A full good team wins almost every time: ladder rosters at their own level
cannot hold a curated six. Tried and NOT adopted (owner's call to make):
full training for every opponent (GOOD 6: 93%), flattening every roster to its
best level (93%), the hard ladder's team cap (81%, and casual teams fall to 0%).
As it stands the tournament is the **money-to-BP** route: $2,500 for up to 22
BP (~$115 a BP) against the gauntlet's $1,500 for ~70-90 BP (~$20 a BP).

## Battle Hall (fully trained pet alone)

| pet level | clears all 18 | $ a run | battles a run | a full clear pays |
|---|---|---|---|---|
| 50 | 2% | $4,100 | 9.2 | $11,700 |
| 100 | 13% | $12,400 | 11.9 | $23,400 |

Energy for a clear is 108 (6 a fight). The clear rate is the Hall's own
difficulty, not changed here: with the foes' training ramping to 50 instead of
100 the clears are 7% / 28% -- an option for the owner.

## Money

| source | amount |
|---|---|
| walking | $1 a step |
| hard rematch, a region's 8 gyms | $10,200-14,450 |
| hard rematch, a League run (5 fights) | $13,950-19,800 |
| easy rematches | half of hard |
| Battle Hall, a level-100 attempt | ~$12,400 |

| sink | cost |
|---|---|
| gauntlet entry | $1,500 |
| tournament entry | $2,500 |
| PP UP | $10,000 (12 a creature for every slot at 3) |
| vitamins | $9,800 |
| Mart TMs | $1,500-5,000 |

## The prices (v4.09)

| prize | BP | why |
|---|---|---|
| PP MAX | 120 | one 40-win run (140) or four casual runs (~30) |
| BOTTLE CAP | 120 | the same |
| Master Ball | 800 | 6.7 PP MAXes |
| PP UP | 45 | $10,000 at ~$220 a BP; three (135) stay dearer than a PP MAX |
| Protein / Iron / Carbos | 40 | $9,800 |
| Linking Cord | 40 | $9,800 |
| evolution stones | 10 | $2,100 (Black Augurite 14, $3,000) |
| Ether | 4 | sold nowhere |
| ordinary TMs | Mart price / 250 | 6-20 |
| signature TMs | 48 (legendary), 32 (starter ultimate) | |

Gauntlet: 3 BP a win, +10 for a boss, doubled for the day's daily run.
Tournament: 3 a win, +10 for the title.
