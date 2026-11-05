# The Battle Zone economy (v4.09 tuning pass, 2026-11-05; Hall v4.11)

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

## Tournament (v4.10: six at level 100, they bring as many as you)

Every seat: its roster at final forms + fillers of its type, level 100; IVs
4-24, training 0-80 and the smart AI from the middle, by story strength.
MID = a typical endgame team: final forms of base stats 500-600 (no
legendaries), IV 20, training 75, TMs. 500 entries a row.

| profile | team | champion | BP an entry |
|---|---|---|---|
| CASUAL | 3 / 6 | 8% / 3% | 6.8 / 6.0 |
| MID | 3 / 6 | 54% / 60% | 15.3 / 16.3 |
| MID +PPUP | 3 / 6 | 55% / 63% | 15.3 / 16.7 |
| GOOD (perfect, legendaries) | 3 / 6 | 99% / 100% | 22 |

A perfect team with legendaries is above the ceiling the owner's levers allow
(a champion at IV 24, training 80, smart): measured even at IV 31 / training
100 / strongest fillers it wins 93%+. Before v4.10 (story rosters, 2-6
creatures): GOOD 6 99%, GOOD 3 27%, CASUAL 6 8%.

## Battle Hall (v4.11)

The types come worst matchup for the pet first, best last (`bhallTypeAt()`,
`bhallMatchup()`). Foes from the 6 species of the rung's type nearest 90% of
the pet's base stats, 3 levels under closing to level at the last rung, IV
4-16, training 0-90, smart from rung 11. A fully trained pet (training 100,
TMs) of any of 426 final forms of base stats 470+ (legendaries included),
1,500 runs a cell:

| pet IV | L50 | L75 | L100 | losses in rungs 1-6 / 7-12 / 13-18 | losses to a counter |
|---|---|---|---|---|---|
| 10 | 4% | 4% | 6% | 53-59 / 27-30 / 12-15% | 44-50% |
| 20 | 8% | 11% | 13% | 45-51 / 29-33 / 18-21% | 37-44% |
| 31 | 14% | 19% | 23% | 41-46 / 28-32 / 24-26% | 34-40% |

The ordering alone (v4.10's 80%) raised IV 20 to 16-22%: a counter met early
faces a weak foe, and the strong late foes are the types the pet beats. 85%
gave 11-15%, 95% 5-7%; 90% was adopted. v4.10 (shuffled order, 80%): IV 10
2-3%, IV 20 6-9%, IV 31 11-17%.

The 12 strongest species clear 26% (L50) / 45% (L100). Raising IV and
training levers or refilling PP every rung moved these by a point or two; the
parlay of 18 and the type matchups decide, and the 80% species pick was the
lever that moved them.

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
