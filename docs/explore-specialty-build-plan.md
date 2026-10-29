# Explore specialty zones -- BUILD PLAN (2026-10-29, revision 2)

Status: **revision 6 -- the owner's answers for v3.75, v3.76, v3.78 and v3.79
are in; nothing built yet.** This file turns `docs/explore-specialty-plan.md` (the proposal:
what and why) into phases with files, callers, tests and hardware checks, in
the shape of `docs/build-plan.md`. Where the two differ, the owner's answers
decide, then this file. Version numbers are labels: each phase takes the next
free version when it is built.

Every number below comes from a pret disassembly, ported, never remembered.
The sources were checked as fetchable from this container on 2026-10-29:

| mechanic | source |
|---|---|
| Kanto Safari (BALL / BAIT / ROCK) | `pokefirered` `src/battle_main.c` (`HandleAction_ThrowBait/ThrowRock/WatchesCarefully`, the factors at battle start), `src/battle_ai_script_commands.c` (`Cmd_if_random_safari_flee`), `src/battle_script_commands.c` (the ball: `safariCatchFactor * 1275 / 100`, Safari Ball x1.5) |
| Hoenn Safari (POKEBLOCK / GO NEAR) | `pokeemerald` `src/battle_util.c` (`HandleAction_ThrowPokeblock`, `HandleAction_GoNear`, `sPkblToEscapeFactor`, `sGoNearCounterTo*`), `src/battle_main.c` (escape factor starts at 3) |
| Great Marsh (BAIT / MUD) | `pokeplatinum` `src/battle/battle_controller_player.c` (`SafariBait/RockCommand`), `src/battle/battle_script.c` (`sSafariCatchRate`, 13 stages), `src/battle/battle_lib.c` (both start at stage 6) |
| Bug-Catching Contest | `pokecrystal` `engine/events/bug_contest/judging.asm` (`ContestScore`), `data/wild/bug_contest_mons.asm`, `data/events/bug_contest_winners.asm`, `maps/Route36NationalParkGate.asm` |
| Ruins of Alph puzzle | `pokecrystal` `engine/games/unown_puzzle.asm`, `data/wild/unlocked_unowns.asm` |
| Swarms | `pokecrystal` `data/wild/swarm_grass.asm`, `swarm_water.asm`; `pokeemerald` `src/tv.c` (`sPokeOutbreakSpeciesList`); `pokeplatinum` encounter JSON (`swarms`) |
| Pokemon Tower's ghosts (deferred) | `pokered` `engine/battle/core.asm` (`IsGhostBattle`) |

**Kanto ports from FireRed, not Red.** Kanto's areas are FireRed's tables
(v3.39), so its Safari is FireRed's Gen 3 Safari: BAIT and ROCK work the same
way as Red's but through Gen 3 numbers. This also puts Kanto and Hoenn on one
catch formula.

## 0. Decisions

**Decided by the owner (2026-10-29):**

1. **The Safari's odds come from the games.** Each species' real catch rate
   becomes a generated `DEX_CATCH_RATE[]` (1 byte x 1025, from PokeAPI's
   `capture_rate`), and each Safari ball runs the game's own catch formula.
2. **No entry fee and no step limit.** A Safari area works like every other
   area: each EXPLORE tap is one encounter. What changes is the battle itself:
   no team, Safari Balls, and the game's own items (Kanto: BAIT and ROCK).
3. **Each game's own menu** in Hoenn and Sinnoh (v3.76).
4. **Safari Balls stay out of the bag.** They belong to the encounter.
5. **Move the city fishing out of the Safari areas.** Fuchsia City and
   Pastoria City (§ 1).
6. **Pokemon Tower's ghosts, the Bug-Catching Contest and the Ruins of Alph
   puzzle are deferred**, to come later with the Friend Safari.
7. **Fixed encounters get their own marker** on the Explore screen, like the
   legendary's star but different (§ 4). **A missed one is never lost**: it
   comes back now and then until caught, like a legendary, and stays in the
   egg pool (§ 4).
8. **Pokeblock: one average effect**, no natures (§ 2).
9. **Swarms show on the row, like the weather** (§ 5).

**Small calls made here (say if you want them different):**

- **30 Safari Balls per encounter**, the games' number. In practice a foe
  flees long before 30 throws (§ 1), so the count is only there for the
  game's "out of Safari Balls" line.
- A catch still drops items, as every other Explore catch does.
- No energy cost, since no creature of yours fights.

10. **A marker means a GUARANTEED battle, for legendaries too.** The star
    (legendary) and the `!` (one-off) show only while the guaranteed first
    meeting is due. Once it has happened, the creature falls back to the 3%
    chance and the marker goes. This changes the legendary star, which since
    v3.40 has stayed until the legendary is caught (§ 4).

11. **Evolution stones, decided (end of this file):** stones only, no level
    route; Eevee follows the stones; a hidden 50/50 gender from the creature's
    ID, shown on its card; Meltan evolves by walking; Cosmoem splits 50/50 by ID.

**Still open:** the items in § E3, the Eevee rules in § E2, where stones come
from (§ E5), and the gender question in § E4.

## Rules that hold in every phase

- **One answer per rule (trap 1).** A zone's mode is `wildAreaMode(area)`,
  generated into `WildArea` beside `wxMask`. `startWildBattle()`, the area
  list's status line (`areaStatusText()`), the battle menu's DRAWING and its
  TAP HANDLER all ask it. The battle main menu's four labels are a function
  (`btlMainOpt(i)`) that `renderBattle()` and `battleTap()` both read, not an
  `opt[4]` in one place and cell indices in the other.
- **A specialty zone is still a WILD area.** `exploreAreaOf()`,
  `wildAreaState()`, the gates, NEEDS PACK / NO VALID SPRITES and
  `areaListEntry()` work as they do today. Nothing here is a hall.
- **Width (trap 2).** Every area index is `uint16_t`; any new list of species
  is `int16_t`; any new saved bitset has its own `*_BYTES` and an APPEND-ONLY
  order that the generator enforces, like `LEGEND_ORDER`.
- **Saved data is additive.** New bits go into the player checkpoint tail
  (`PLAYER_VERSION` 10, then later) plus a legacy key in `SAVE_FIELDS`. Never
  reuse existing bytes. Safari and contest state is RAM only.
- **`wild.cpp` stays the only includer of `wild_areas.h`.**
- **Every new guard is negative-checked.** The test must also prove the
  mechanism engaged (trap 3's "vacuously true" shape), for example that a
  Safari encounter really did skip `buildSquad()`, not just that the squad
  looks untouched.
- **Each phase is one version**: `FW_VERSION`, README badge, the README
  manual's numbers, `docs/release-notes/vX.Y.md` for players, a CLAUDE.md
  "Done" entry, screenshots regenerated (`make_screens.sh`, the three lists
  reconciled). Strings appended in all six languages, ASCII only.

## 1. v3.75 -- Safari encounters, Kanto (M)

**The rules, from FireRed:**
- At the start of each encounter: catch factor = `catchRate * 100 / 1275`
  (0..20); escape factor = the species' `safariZoneFleeRate * 100 / 1275`,
  at least 2. `safariZoneFleeRate` is a FireRed species field, so the
  generator reads it from `pokefirered` for the species Kanto's Safari holds.
- **BAIT**: catch factor halved (floor 3), bait counter + 2..6 (at most 6),
  rock counter cleared.
- **ROCK**: catch factor doubled (cap 20), rock counter + 2..6 (at most 6),
  bait counter cleared.
- **Each turn the foe flees** with probability `5 x f` %, where f is the
  escape factor x2 (cap 20) while angry, /4 (at least 1) while eating, as is
  otherwise. Otherwise it watches, and one turn comes off the counter. When the
  rock counter runs out, the catch factor resets to the species' own.
- **BALL**: Gen 3's formula, with catch rate = `factor * 1275 / 100`, ball
  x1.5, full HP and no status (a Safari foe is never hurt): caught outright at
  a >= 255, otherwise four shake checks.
- **RUN** always works.

**Generator.** A `'mode': 'safari'` on the area in `tools/wild_areas.py`;
`gen_wild.py` emits `WildArea::mode` (`WILD_MODE_NORMAL`, `WILD_MODE_SAFARI`)
and the FireRed flee rates. It refuses a Safari area that holds a legendary's
home. `gen_dex.py` emits `DEX_CATCH_RATE[]`. The other regions must come out
byte-identical apart from the new field.

**The city fishing moves.** FUCHSIA CITY leaves SAFARI ZONE for ROUTES 12-15,
and PASTORIA CITY leaves GREAT MARSH for ROUTE 213. What that changes:
- **Pastoria: nothing.** Its surf, Good Rod and Super Rod tables are the same
  as Route 213's, slot for slot (checked in pokeplatinum). The species and the
  odds are unchanged.
- **Fuchsia**: Psyduck (surfing), and Goldeen, Seaking, Gyarados and Psyduck
  (Super Rod) join ROUTES 12-15. That area's water odds shift a little, and
  Gyarados becomes catchable there. The rod gate is 4 on both sides, so the
  Super Rod secret holds no more back than before.
- **The Safari Zone keeps its own water**: Psyduck surfing, and Dratini and
  Dragonair on the Super Rod. Those become Safari encounters, as they are in
  FireRed.
- **Not SEA ROUTES 19-21.** Surfing Pikachu's secret needs every species in
  that area registered, so adding Fuchsia's catch there would quietly move the
  secret's goalposts. `surfui_test` and `wildarea_test` should prove SEA
  ROUTES 19-21 is unchanged.
- Nothing saved stores an area's contents (`exploreAreaPick[]` is RAM), so no
  save changes.

**Firmware.**
- `wild.h/.cpp`: `wildAreaMode()`; the pure Safari rules with every roll
  passed in, like the rest of wild.cpp: `safariStart(dex)`, `safariBait()`,
  `safariRock()`, `safariFleeChance()`, `safariTurnEnd()`,
  `safariBallCaught(roll...)`.
- Sketch: `startWildBattle()` branches on `wildAreaMode()`. It does **not**
  call `buildSquad()`: no energy, no PP, no team picker. `btlYou` stands in for
  the player (the player's avatar from behind, `avatars.h`) and never acts.
  `btlSafari` selects the menu through `btlMainOpt()`. A catch goes through
  `wildStoreCaught()` -> `storeNewCreature()`, the ONE catch path, so the
  party / PC / box order and the release chooser all come for free.
  Narration as in FireRed: "WILD X is eating!", "is angry!", "is watching
  carefully", "fled!". The foe never attacks (as in the games).
- Explore's area row and the area list read SAFARI ZONE as usual. The status
  line is unchanged.
- i18n: BAIT, ROCK, SAFARI BALL, the eating/angry/watching lines and
  out-of-balls, in six languages.

**Tests.** `safari_test` through the real tap handlers:
- No squad is built, and the live pet's energy and PP are unchanged (a probe
  fails if `buildSquad()` ran).
- Balls count down and never touch the bag.
- A catch lands in the party, then the PC, then the box.
- BAIT/ROCK factors, the counters and the flee chance are pinned at FireRed's
  boundaries: floor 3, cap 20, x2, /4, at least 1, the 2..6 counter, the reset
  when the rock counter runs out.
- The ball's catch odds for one species are checked against an independent
  simulation of the formula (like the slots).
- Route 1 still builds a squad.
- ROUTES 12-15 now offers Gyarados, the Safari Zone no longer offers it from
  Fuchsia, SEA ROUTES 19-21 is unchanged, and the Great Marsh and Route 213
  hold the same water species as before.

Also `flush_test` (the Safari menu) and `hit_test` (the four cells are the
existing cells). Negative checks: the squad built anyway, a ball taken from
the bag, bait ignored, the flee check not run, Fuchsia not moved.

**On a board:** it plays like every Explore tap, so only the feel needs a
check.

## 2. v3.76 -- Hoenn's and Sinnoh's Safari menus (S-M)

SAFARI ZONE and SAFARI EXTENSION (Hoenn) and GREAT MARSH (Sinnoh) get
`'mode': 'safari'` with their own rules, chosen by region (`SafariRules`).
The encounter frame from § 1 is unchanged: no team, 30 Safari Balls, the same
catch path.

**Hoenn (Emerald): BALL / POKEBLOCK / GO NEAR / RUN.**
- Escape factor starts at **3** for every species (15% a turn); the catch
  factor comes from the catch rate, as in § 1. Emerald has no per-species
  flee rate.
- **GO NEAR**: catch factor + 4, +3, +2, +1 on the first four goes (cap 20);
  escape factor + 4 each time (cap 20). After four, "can't get any closer".
  It is a gamble: easier to catch, much likelier to flee.
- **POKEBLOCK**: lowers the escape factor by an amount that depends on whether
  the foe's NATURE likes the block's flavour (`sPkblToEscapeFactor`: ignored /
  curious / enthralled) and on how many blocks have been thrown (up to 3).
  The catch is untouched. **Decided (owner): one average effect, no natures.**
  Every block is the game's "curious" row: escape factor -3, -2, then -1 for
  every later throw (floor 1). That row is the equal-odds mean of the game's
  three outcomes, rounded: (3+5+0)/3 = 2.7, (2+3+0)/3 = 1.7, (1+2+0)/3 = 1.
  The narration reads "is curious about the block". The game's BUGFIX
  comparison (`<=`) is ported, so the factor can never reach 0 (Emerald's
  original has the "Pokeblock glitch").
- Two new strings: POKEBLOCK and GO NEAR fit in 12 characters.

**Sinnoh (Platinum): BALL / BAIT / MUD / RUN.** Platinum works in STAGES, not
factors:
- A catch stage 0..12 (starting at 6) indexes `sSafariCatchRate`: x0.25,
  x0.29, x0.33, x0.4, x0.5, x0.67, x1, x1.5, x2, x2.5, x3, x3.5, x4, applied to
  the species' catch rate. An escape count also starts at 6.
- **BAIT**: catch stage +1, and in 9 cases of 10 escape count +1.
- **MUD**: escape count -1, and in 9 cases of 10 catch stage -1.
- The names in the decompile look swapped against how the games describe them
  (in Platinum, bait makes the foe stay and mud makes it easier to catch). So
  the flee check (`subscript_safari_escape` and the battle script that reads
  `BTLVAR_SAFARI_ESCAPE_COUNT`) must be read in full before porting, and the
  test pinned to it, not to these notes.
- Ball: Gen 4's formula (same shape as Gen 3) with the stage multiplier.

**Tests.** `safari_test` grows a section per region, through the same tap
handlers. Each region's numbers are pinned to its own source, the Pokeblock's
row, and GO NEAR's cap at four. Kanto's section stays
green untouched, which is the proof the three rule sets do not leak into each
other.

**Size.** The frame is built in § 1, so this phase is two rule sets, four
strings and tests.

## 3. DEFERRED -- Pokemon Tower's ghosts

Deferred by the owner (2026-10-29), to be taken up with the Friend Safari (§
8). The design stays as it was: `'ghost_gate': 4` on the Tower, the Tower
moved to gate 3, an unidentified GHOST that dodges balls and cannot be
fought, until the Silph Scope's badge.

## 4. v3.78 -- fixed encounters, with their own marker (S-M)

**The rule, as the owner set it: a one-off works like a legendary.** The
first Explore in its area after its gate is a guaranteed meeting. If it is
not caught (it fled, fainted or you ran), it is NOT gone: from then on it
turns up now and then in that area, `WILD_LEGEND_PCT` (3) of encounters, the
same rule the legendaries use, until it is caught. Unlike a legendary it
**stays in the egg pool**, so eggs bring it too.

This changes revision 2, which had a missed one-off gone for good, as in the
games. (That is what "the legendary comes back and the one-off does not"
meant there. It is withdrawn.)

**Data.** A second generated list, `WILD_FIXED` (`dex, area, level, gate,
flags: shiny, weekday mask`), with its own saved MET and CAUGHT bits
(`FIXED_BYTES` each, APPEND-ONLY order in `tools/wild_areas.py`, a new legacy
key, the player record's tail), exactly like `LEGEND_BYTES`.
`wildFixedFor(area, pet, roll)` follows `wildLegendFor()`'s shape: the
guaranteed meeting first, then the rare repeat while not caught. A legendary
of the same area wins the roll. **`wildIsExploreLegend()` must NOT read this
list**, or the species leaves the egg pool. MET is set as the fight starts and
CAUGHT in `btlThrowBall()`, the same places as the legendaries.

**The markers (owner, 2026-10-29): a marker means a guaranteed battle.**
- `* AREA *` (a legendary) and `! AREA !` (a one-off) are drawn by
  `areaCentredName()`, in the area list and on Explore's area row, while that
  area's GUARANTEED first meeting is due: open, gate passed, not yet met, art
  and pack present, and for Lapras, a Friday. If both are due, the star wins.
- Once met, the creature falls back to the 3% chance and the marker goes,
  whether or not it was caught.
- **This changes the legendary star.** Today `wildAreaHasLegend()` shows it
  until the legendary is CAUGHT. It becomes "a guaranteed meeting is due"
  (`wildAreaHasLegendDue()`, or the old name with the new rule). The callers
  are the star in `areaCentredName()` and the console's `AREA` listing; both
  move to the one answer. A roamer's guaranteed meeting is at its home, so
  that is where its star shows. Necrozma's star still waits for all seven
  Ultra Beasts, since its meeting is not due until then.
- `legendui_test` (the star's own test) changes with it: the star is on before
  the first meeting, off after it while uncaught, and stays off after a catch.
  `fixed_test` checks the same for the `!`.

**How each candidate is reachable today** (checked in `wild_areas.h` and
`dex.h`; "egg" means it hatches, or its first form does):

| candidate | the game's fixed encounter | met in Explore today | from eggs |
|---|---|---|---|
| SNORLAX | Routes 12 and 16 (FireRed) | not in Kanto-Sinnoh; wild in Kalos and Galar | via Munchlax (Sinnoh eggs) |
| ELECTRODE | Power Plant (FireRed) | Cerulean Cave, New Mauville | via Voltorb |
| SUDOWOODO | Route 36 (Crystal) | Routes 219-221 (Sinnoh) | via Bonsly (Sinnoh eggs) |
| LAPRAS | Union Cave B2F, Fridays (Crystal) | Icefall Cave, Victory Road back | hatches (rare) |
| red GYARADOS | Lake of Rage, always shiny (Crystal) | many routes, not shiny | via Magikarp |
| KECLEON | Routes 119-120, Devon Scope (Emerald) | Routes 118-123 (Hoenn) | hatches (common) |

Every candidate stays reachable by an egg, so a missed one-off never leaves
the collection incomplete. Snorlax is the one with no Kanto wild slot, which
is why its fixed encounter matters most.

Rules that follow the game: Lapras's meeting and its repeats happen on
Fridays only (no clock = never). The red Gyarados is shiny on every meeting,
repeats included, since it is the same creature. Levels are read from the map
scripts at build time.

**Tests.** `fixed_test`:
- The guaranteed meeting happens once.
- After a miss, the repeat appears at the legendaries' rate until caught, and
  never after.
- The weekday rule, and the forced shiny on the repeat too.
- The species is still in the egg pool, and still in its ordinary wild slots.
- The marker is on and off at exactly the right moments, through
  `areaCentredName()`'s own text.
- Saved and reloaded through each half of the save.

Negative checks: no repeat after a miss, a repeat after a catch, Lapras on a
Thursday, `wildIsExploreLegend()` reading the list, the marker after the
first meeting (for both markers).

## 5. v3.79 -- swarms (S-M)

**What the games did.** One place in the region holds a swarm of one species
on a given day:
- **Johto (Crystal):** a phone call from a contact starts it. The swarm tables
  are separate (`swarm_grass.asm`, `swarm_water.asm`): Dunsparce in Dark Cave
  (40% of the cave's slots in the data above) and Yanma on Route 35 in the
  grass; Qwilfish (Route 32) and Remoraid (Route 44) on the rod. Remoraid is
  met nowhere else in Crystal, so this brings it into Explore.
- **Hoenn (Emerald):** a TV news bulletin. Five outbreaks in `tv.c`: Seedot
  (Route 102, L3), Nuzleaf (Route 114, L15), Seedot (Route 117, L13), Seedot
  (Route 120, L25), Skitty (Route 116, L8). Each comes with its own moveset,
  and the species takes about half the route's encounters.
- **Sinnoh (Platinum):** Professor Oak's daughter on TV reports a swarm each
  day after the Pokedex is national. The route's grass slots 0 and 1 change
  to the swarm species; the encounter JSON carries a `swarms` pair per map,
  which must be checked against the game's swarm list before use.
- **Kanto (FireRed), Unova onward:** FireRed has none. Gen 5+ is PokeAPI's
  `swarm` method where the data has it, NOT VERIFIED like everything else
  there.

**Proposal.** Each region with swarm data gets one swarm a day:
`wildSwarmToday(region, pet)` picks one entry from that region's list with the
day hash `wildAreaWeather()` uses, salted by `Pet::daySalt`, so the existing
re-roll cheat covers it. No clock, no swarm. On that day the area's pick gives
the swarm species the game's share (Crystal: the swarm table replaces the
map's; Emerald: the outbreak's share; Platinum: slots 0-1), with the game's
level and, for Emerald, its moves. Nothing is saved: the day decides.

**Decided (owner): it shows like the weather.** `areaStatusText()`, the one
status line, appends SWARM (`Lv.12-18 SWARM`), after the weather if both
apply. The line must still fit 20 characters, which the longest band plus a
weather plus SWARM may not; `explore_test` decides, and the weather may need
its short form.

**Tests.** `swarm_test`: one swarm per region per day, steady all day, every
entry comes round over a year, the share and level, no swarm without a clock,
the status line fits 20 characters, and Remoraid reachable.

## 6. DEFERRED -- the Bug-Catching Contest (M)

Deferred by the owner (2026-10-29), with the Tower's ghosts. Kept as
designed:

**What the game did (Crystal).** Tuesday, Thursday and Saturday at National
Park:
- You enter with ONE creature (the rest are held at the gate) and 20 Park
  Balls, and have 20 minutes.
- The park's table is replaced by the contest's: Caterpie and Weedle 20%
  each; Metapod, Kakuna, Venonat and Paras 10%; Butterfree, Beedrill, Scyther
  and Pinsir 5%. Levels run 7-18; Scyther and Pinsir are 13-14.
- You may hold one catch. Catching another asks whether to swap.
- At the end, your catch is scored and judged against the NPC contestants:
  ten of them, each with three possible results and a fixed score (Bug Catcher
  Don's Kakuna 300, Cooltrainer Nick's Pinsir 368, and so on). The score is:
  4 x max HP + Attack + Defense + Speed + Sp. Atk + Sp. Def + a DV bonus +
  remaining HP / 8 + 1 if it holds an item.
- Prizes: 1st a Sun Stone, 2nd an Everstone, 3rd a Gold Berry, everyone else
  a Berry. You keep the bug.

**How it fits here.** The contest is one SESSION by nature (one entry, many
encounters, one judging), unlike your Safari decision. Proposal:
- National Park on a contest day shows CONTEST on its row. Tapping it starts
  the contest: pick ONE creature (the team picker capped at one), 20 Park
  Balls.
- Each EXPLORE tap in the park is a normal fight with that one creature (you
  weaken, then throw), drawn from the contest table.
- No timer (a 20-minute clock on a pocket device is a poor fit). The contest
  ends when you choose to end it, or when the Park Balls run out. Ask if you
  would rather have the timer.
- Judging uses Crystal's formula on **Gen 2 stats** computed from the catch
  (our IVs halved to DVs, Gen 2's stat formula). Our own stats run about 40%
  above canonical on purpose (see CLAUDE.md § "Fight length"), so scoring them
  directly would beat every NPC every time. Computed this way, the NPC scores
  mean what they meant.
- The ten NPCs each draw one of their three results, as in the game. One
  contest a contest day.
- The catch goes through `storeNewCreature()`.

**Prizes: the open question** (see "Evolution stones" below). Without
stones: 1st a TM you do not own, 2nd money, 3rd coins.

**Size.** M: a session (RAM only), the contest table from the generator, a
judging port, a results screen, about 8 strings. Saved: only "entered today".

## 7. DEFERRED -- the Ruins of Alph puzzle (M)

Deferred by the owner (2026-10-29), with the Tower's ghosts. Kept as
designed:

**What the game did (Crystal).** It is **not a sliding puzzle** (the proposal
was wrong about that):
- A 6x6 board. The 16 pieces of a 4x4 picture start on the 20 border squares
  (`PuzzlePieceInitialPositions`).
- You pick a piece up and put it down in the 4x4 centre. The picture is solved
  when every piece is in its place.
- Four puzzles (Kabuto, Omanyte, Aerodactyl, Ho-Oh), each in its own chamber.
  Each solved puzzle unlocks a set of Unown letters: A-K, L-R, S-W, X-Z
  (`unlocked_unowns.asm`).
- Until the first is solved, no Unown appears in the Inner Chamber.

**Unown today** (checked): it has art, it hatches (common tier), and it is
an ordinary wild encounter in four areas, caught like any other: Johto's
RUINS OF ALPH (gate 1, about 28% of the area, the game's Inner Chamber table
merged with the outside's Natu and Smeargle), Kanto's TANOBY RUINS
(champion), and Sinnoh's SOLACEON RUINS (gate 3) and RUIN MANIAC CAVE (gate
4). So in Johto, Unown appears from the first badge, where Crystal makes you
solve the puzzle first.

**How it fits here.**
- This dex has no Unown letter forms, so the four sets cannot mean four
  different creatures. Options:
  - (a) *recommended:* one puzzle (Kabuto, the first), and solving it lets
    Unown appear: a hidden gate on the Inner Chamber's Unown slots, like the
    rod's. The rest of RUINS OF ALPH (Natu, Smeargle outside) is unaffected.
  - (b) all four puzzles as a collection (four saved bits, a medal), with
    Unown unlocked by the first.
- Tapping RUINS OF ALPH while it is unsolved opens the puzzle instead of a
  fight (a new screen, `SCR_PUZZLE`). Tap a piece, tap a square: two taps,
  finger-sized squares (a 6x6 board on the round panel is about 58 px a
  square, under `UI_TAP_FINGER`, so the board needs `hit_test` attention or a
  zoomed centre).
- Art: pret's `gfx/unown_puzzle/` via a `tools/gen_unown_puzzle.py`, the same
  unlicensed-Nintendo-art arrangement as the avatars and the Game Corners.
- Saved: one bit (or four), the player record's tail.
- Tanoby Ruins (Kanto) and Solaceon Ruins (Sinnoh) also hold Unown. Gate them
  on the same bit, or leave them open? (Recommended: leave them open. Their
  games had no puzzle.)

## 8. LATER -- the Kalos Friend Safari, with the Tower's ghosts

Deferred by the owner. After the champion, each `RivalRecord` would open a
FRIEND SAFARI row whose type and three species derive from that rival's MAC.

## E. Evolution stones, branches, gender, Melmetal and Lunala (one version, M-L)

Decided by the owner (2026-10-29), with the open points marked. Every rule
below comes from PokeAPI's `pokemon_evolution.csv`, checked.

### E1. Stones only: no level route for a stone evolution

**Today** a stone evolver evolves by LEVEL, with a stand-in level in the
data (Pikachu at 30, Gloom at 36), and `evolvesTo` holds ONE target, so a
species with two evolutions reaches only one of them.

**Decided:** a stone evolution happens ONLY with its stone. A Pikachu, a
Gloom or an Eevee stops evolving by level, which the owner accepts as long as
stones can be had (§ E5). No save changes: a creature mid-way simply waits
for its stone.

The ten stones cover **47 evolutions from 42 species** (counted from PokeAPI,
leaving out the four that need a regional form this dex does not have:
Hisuian Electrode, Galarian Darmanitan, Alolan Sandslash and Alolan
Ninetales):

| stone | evolutions |
|---|---|
| FIRE STONE | Vulpix, Growlithe, Pansear, Capsakid, Eevee (Flareon) |
| WATER STONE | Poliwhirl (Poliwrath), Shellder, Staryu, Lombre, Panpour, Eevee (Vaporeon) |
| THUNDERSTONE | Pikachu, Magneton, Nosepass, Eelektrik, Charjabug, Tadbulb, Eevee (Jolteon) |
| LEAF STONE | Gloom (Vileplume), Weepinbell, Exeggcute, Nuzleaf, Pansage, Eevee (Leafeon) |
| MOON STONE | Nidorina, Nidorino, Clefairy, Jigglypuff, Skitty, Munna |
| SUN STONE | Gloom (Bellossom), Sunkern, Cottonee, Petilil, Helioptile |
| SHINY STONE | Roselia, Togetic, Minccino, Floette |
| DUSK STONE | Misdreavus, Murkrow, Lampent, Doublade |
| DAWN STONE | Kirlia (Gallade, male), Snorunt (Froslass, female) |
| ICE STONE | Cetoddle, Crabrawler, Eevee (Glaceon) |

- Names: the bag draws an item name at text size 2 up to 11 characters and at
  size 1 beyond. THUNDERSTONE uses the games' Gen 1-4 spelling (12
  characters), the same call as DRAGONBREATH.
- Gloom gets two stones (Leaf -> Vileplume, Sun -> Bellossom), Kirlia and
  Snorunt get the Dawn Stone by gender, and their old level route still exists
  for the OTHER branch (Kirlia -> Gardevoir at 30, Snorunt -> Glalie at 42).
  That level route stays: it is not a stone evolution.
- **The farewell must not wait for a stone.** Since v3.72,
  `evolutionAhead()` delays the farewell while an evolution is coming, and an
  unmarked trade evolver does NOT count. A stone evolution is the same case: a
  Pikachu with no Thunderstone is offered its farewell like any final form.
  `farewell_test` gets a stone case. (This is trap 1 wearing a stone.)
- The card's evolution line (`uiEvoLine()`, from `evoInfo()`) reads
  `NEEDS A THUNDERSTONE`, as it reads the trade line today.

### E2. Eevee follows the stones

Five of Eevee's eight branches are stones: Vaporeon (Water), Jolteon
(Thunder), Flareon (Fire), Leafeon (Leaf), Glaceon (Ice). Using the stone
from the bag evolves it, at any level. Today `Pet::eeveeOptions()` picks at
random, preferring an unregistered branch; that random pick goes.

**The other three, and when they happen.** In the games none of them has a
level: they happen on ANY level-up once friendship is high (Espeon by day,
Umbreon by night), and Sylveon on any level-up with high affection while
knowing a Fairy-type move. Here:
- **When:** on a level-up (one an hour), once BOND reaches a threshold. Bond
  rises at most `BOND_DAILY_MAX` (20) a day, so the threshold sets how long
  it takes: 40 is two days of full care, 60 three, and a creature is first
  offered its farewell at three days (level 73). **Decide the threshold**;
  *recommended* 40, which an attentive player reaches around level 48.
- **Which:** Sylveon if it knows a Fairy move, else Espeon by day (06:00-18:00)
  and Umbreon by night. No clock set counts as day, as Explore's time bands do.
- **No blocking needed.** Evolution here already waits for the player: the
  EVOLVE button appears and nothing happens until it is tapped, and KEEP
  (`declineEvolve()`) holds it until the next level. A player saving their Eevee
  for a stone simply does not tap EVOLVE; the stone works whenever it is used.
  The button reads which branch is on offer (for example `EVOLVE: UMBREON`), so
  nobody evolves into one by accident.

### E3. The one-off items

**Single-line items: evolve by level, done** (owner's rule). The item only
gates a creature's one evolution, so the existing level stand-in stays:
URSARING -> Ursaluna (Peat Block), KUBFU -> Urshifu (two scrolls that only
choose a form this dex does not have), DURALUDON -> Archaludon (Metal Alloy),
SINISTEA and POLTCHAGEIST (pots and teacups, forms only). The three held-item
evolutions (Happiny holds an Oval Stone by day, Sneasel a Razor Claw at night,
Gligar a Razor Fang at night) are single-line too, so they stay on level.
**Nothing in the games is held for the branching items either**: apples,
armours and the augurite are used from the bag, like stones.

**Branching items: two ways to build them (decide).**

| species | branches | the games' items | level stand-in today |
|---|---|---|---|
| APPLIN | Flapple, Appletun, Dipplin | Tart, Sweet, Syrupy Apple | 30 (Flapple only) |
| CHARCADET | Armarouge, Ceruledge | Auspicious, Malicious Armor | 30 (Armarouge only) |

- **(a) *Recommended:* a choice at the level, no items.** At its level the
  EVOLVE offer asks which branch (two or three buttons: FLAPPLE / APPLETUN /
  DIPPLIN), and KEEP holds it until the next level, like every evolution. In
  the games these items are one-off pickups with no other use, so a choice
  loses nothing a player could feel, and the Mart and the bag stay free of
  five items that do one thing each. It needs a multi-button evolution offer
  (the Eevee buttons are similar).
- **(b) Five items** (the apples and armours), used from the bag like stones,
  sold in the Mart. More faithful, more clutter, and five more names to fit
  (AUSPICIOUS ARMOR is 16 characters).

**SCYTHER (owner's design): the Black Augurite as a stone, blended with the
trade.**
- **SCIZOR by trade** (built in v3.32: a trade or a Linking Cord marks it).
  **KLEAVOR by the Black Augurite**, used from the bag like a stone.
- **Whichever comes first decides.** A Scyther already marked for its trade
  evolution refuses the augurite (the item is not spent), and a Scyther that
  used the augurite has evolved and cannot be marked. A traded Scyther still
  evolves only when it is the live pet and cared for, exactly as today.
- **Neither: it stays a Scyther**, and its farewell is not held back, as
  for an unmarked Kadabra or a Pikachu with no stone (`evolutionAhead()`).
- BLACK AUGURITE is the one special item (14 characters: measure it where the
  bag, the Mart and the battle bag draw).

### E4. Gender: hidden, from the creature's ID, shown on the card (decided)

**From the creature's ID, not the species' number.** A Pokedex number's even
or odd would make every Kirlia the same gender, so one of Gallade or Gardevoir
could never exist. The creature's own ID (`CreatureUid`: the board's MAC, a
salt and a serial) is fixed at hatching or catching, travels with trades, and
needs no save change.
- A mixed bit of the whole ID, not the serial's parity: the serial counts up
  by one per creature, so parity alone would alternate male, female, male in
  hatch order. (Clamperl's trade target uses `u.serial % n` and can stay.)
- Both boards compute the same gender for a traded creature.

**The games' fixed cases are forced (owner).** From PokeAPI's `gender_rate`,
generated into the dex:
- two genders (rate 1-7): 50/50 by the ID;
- all male (rate 0), e.g. Tauros, Hitmonlee, Nidoking;
- all female (rate 8), e.g. Chansey, Jynx, Nidoqueen;
- genderless (rate -1), e.g. Magnemite, Staryu, the legendaries: no gender,
  no icon.

**What it decides:** Kirlia's Dawn Stone (Gallade, male), Snorunt's
(Froslass, female), and Burmy at 20 (Mothim, male; Wormadam, female). Two
existing single-gender evolutions could also follow their games: COMBEE
evolves to Vespiquen only if female, SALANDIT to Salazzle only if female
(males do not evolve). Today both evolve regardless. **Decide.**

**On the card:** the font has no glyph for the gender symbols (non-ASCII is
banned), so they are drawn as small icons, blue and red as in the games, beside
the name on the creature's card and the party sheet.

### E5. Where stones come from, and gym prize money

**The Mart (decided):** all ten stones always in stock at $2,100 (the Stone
Emporium price in X/Y, and the usual price since). The Black Augurite (and the
five apples and armours if § E3 goes with items) in stock too; *recommended*
$3,000.

**Wild drops, only in the stone's own places (owner).** A stone dropping
everywhere would bury the player in spares. Instead each stone drops only in
the areas where the games place it, at a rate a player notices there:
*recommended* about one win in ten in those areas, never elsewhere. That is a
drop weight of about 9 against the table's 120 (one item a win plus a 30%
bonus roll).
- **Where:** the games' own item locations. The decompiled maps name every
  item ball and hidden item, for example FireRed's Mt. Moon holds a Moon Stone
  on 1F (an item ball) and one on B2F (hidden). `gen_wild.py` scans the maps it
  already reads (FireRed, Crystal, Emerald, Platinum) for the ten stones, maps
  each to our area, and emits a per-area stone list. A test checks every stone
  has at least one area.
- **Unova onward** has no decomp and PokeAPI has no item locations, so those
  regions either get a short hand-made list (NOT VERIFIED, like their rosters)
  or rely on the Mart. *Recommended:* the Mart only, until someone sources the
  data.

**Gym prize money (owner): an alternate source.** Every trainer win in the
games pays. Gen 3's rule (`GetTrainerMoneyToGive()` in pokefirered and
pokeemerald) is **4 x the level of the leader's LAST creature x the class
value**: 25 for gym leaders and the Elite Four, 25 for FireRed's champion and
50 for Emerald's. So:
- Brock (last Onix L14): 4 x 14 x 25 = **$1,400**
- Giovanni (L50): **$5,000**
- Lance (Elite Four, L62): **$6,200**
- Kanto's champion (L63, class 25): **$6,300**
- Emerald's champion Wallace (L58, class 50): **$11,600**
- *Recommended:* the same formula for every region's ladder, with the
  champion at 50 (Emerald's value) everywhere. The wallet's cap is $999,999.
- **Rematches:** the games pay every rematch. Here a fight already costs the
  live pet energy, which is the designed rate limit. *Recommended:* pay every
  win, on both difficulties; *alternative:* full pay on the first win of each
  leader per difficulty, a quarter after.
- No pay on a loss (the blackout takes nothing, by design), none in LAN
  fights, and none for wild wins, which drop items instead.
- A balance change, so the README manual's money section changes with it.

### E6. Branches that need no item

- **TYROGUE at 20:** higher Attack -> Hitmonlee, higher Defense ->
  Hitmonchan, equal -> Hitmontop. Exact here, since our creatures have Attack
  and Defense.
- **WURMPLE at 7:** Silcoon or Cascoon, 50/50 by the creature's ID (the
  games use a hidden value the same way).
- **NINCADA at 20 (Shedinja):** Nincada evolves into Ninjask. In the games,
  if the party has an empty slot and the bag has a Poke Ball, a second
  creature also appears: SHEDINJA, the shell Nincada left behind. The ball is
  used up (Gen 4 on), and the player now has TWO creatures, the Ninjask and a
  new Shedinja. Here: when the live pet evolves into Ninjask and a Poke Ball
  is in the bag, one ball is spent and a Shedinja (same level, a new ID) goes
  through `storeNewCreature()` (party, then the PC), as a catch does. With no
  ball or no room, only Ninjask, as in the games.

### E7. MELMETAL: walking, a nod to Pokemon GO (decided)

Meltan evolves into Melmetal after **10,000 steps walked while Meltan is the
live pet** (GO's 400 candies x 25; the owner may tune the number). Melmetal
becomes evolution-only and stops hatching.
- **Where the count lives:** the steps belong to that Meltan, so they must
  survive banking and trading. *Recommended:* one of `PartyMon`'s three
  reserved bytes, in hundreds of steps (0-100), plus the same byte appended
  to the pet checkpoint. `tradeMonValid()` refuses a non-zero reserved byte
  today, so a Meltan with progress would be refused by an older board rather
  than lose its steps. That is the right failure, and it needs a `TRADE_PROTO`
  bump.
- Fed from `Pet::addSteps()`, the pedometer's one entry point, only while
  Meltan is live.
- The card reads `7,400 STEPS TO MELMETAL` (a new `evoInfo()` status).
- The emulator has no pedometer, so the test drives `addSteps()`.

### E8. LUNALA and SOLGALEO: 50/50, separate guarantees, gone once owned

- Cosmoem evolves at 53 into Solgaleo or Lunala, 50/50 by the creature's ID
  (the games split them by version).
- Both stay catchable in Explore under the legendary rules until owned: a
  guaranteed first meeting, then 3%.
- **Once owned it no longer appears** (decided). Evolving a Cosmoem gives one
  without catching it, so the evolution sets that legendary's caught bit: one
  rule for both routes. Releasing it does not bring it back (decided).
- **The trap the owner pointed out: one must never cost the other its
  guaranteed meeting.** Each has its OWN met and caught bits in
  `WILD_LEGENDS`, and they share a home (Vast Poni Canyon).
  `wildLegendFor()` gives a home's guaranteed meeting to the FIRST legendary
  there that is not yet met and still live, so after Solgaleo is met, caught
  or obtained by evolution, the next Explore there is Lunala's guaranteed
  meeting, and the reverse. The evolution sets ONLY the evolved one's bits.
  The star (§ 4) stays while the other's guarantee is due.
- **Tests** (in `branch_test` and `legend_test`): Solgaleo caught -> the next
  Explore there is a guaranteed Lunala; Solgaleo by evolution before any
  meeting -> the next Explore is a guaranteed Lunala, and Solgaleo never
  appears again; both the other way round; the star on and off.
  Negative-check: the evolution setting both bits.
- **Option (decide):** the 50/50 could prefer the one the player does not
  own yet, as Eevee's old pick preferred an unregistered branch. A player with
  a Lunala would then always get Solgaleo from a Cosmoem. *Recommended:* yes,
  else the ID decides.
- Cosmog stays egg-only.

### E9. The 18 evolved forms that hatch, after this version

- **Fixed, they stop hatching** (their evolution now exists): Bellossom,
  Gallade, Froslass, Hitmonchan, Hitmontop, Cascoon, Shedinja, Mothim,
  Appletun, Dipplin, Ceruledge, Kleavor and Melmetal. Lunala is already out of
  the egg pool as an Explore legendary.
- **Still hatching, on purpose:** Perrserker, Runerigus, Sneasler and
  Clodsire, whose first form is a regional form this dex does not have.
  Hatching is their only way in.

### E10. Shape of the build

- **Data:** `tools/fetch_evo_branches.py` from PokeAPI emits `EVO_BRANCHES[]`
  (from, to, rule: stone/item id, stat comparison, ID split, gender, steps,
  gift, bond+time) beside `TRADE_EVOS`, and the species' gender rate.
  `gen_dex.py` adds every branch target to the evolution-only set, so "can
  evolve into" and "cannot hatch" come from ONE source, as Eevee's do.
  `EEVEE_BRANCHES` folds into it. The stone evolvers' level stand-ins go to 0.
- **Items:** the ten stones (plus the six branching items) appended after
  `IT_LINKCORD`, a new `IC_STONE` category, never usable in battle
  (`itemUsableInBattle()` states what IS allowed, so nothing to change there).
  The bag stores counts by key, so no migration.
- **The rule:** `Pet::evoInfo()` stays THE evolution rule, and gains the item,
  steps and stat/ID/gender branches. Using an item from the bag on the live
  pet goes through the Linking Cord's path (`cordUse()` -> `applyCord()`), and
  the item is spent only when it actually evolves something. A frozen
  companion, an egg or a wrong species never spends it.
- **Tests:** `branch_test` (every stone, item spent only when it works, every
  item and non-item branch, the farewell not waiting for a stone, the card's
  line, Shedinja's gift with and without a ball or room, Melmetal's steps
  through banking and a trade, Cosmoem's split and the legendary leaving
  Explore, gender fixed per creature and the same on both boards of a trade),
  `dexdata_test`'s rarity invariant over every branch target, and
  `region_test`'s egg sweep (none of the 13 hatch).
- Save: one appended byte (Meltan's steps) in the pet checkpoint and a
  reserved byte of `PartyMon`; `TRADE_PROTO` and `LINK_PROTO` bump.
- **Size:** M-L. It may split into two versions: stones + Eevee + gender, then
  the item branches + Shedinja + Melmetal + Lunala.

## Not in this plan

Max Raids and Tera Raids (they need the battle league's systems), separate
FISH / ROCK SMASH buttons (they would reveal the rod secret), SOS chaining,
Hidden Grottos and tides. The reasons are in the proposal.
