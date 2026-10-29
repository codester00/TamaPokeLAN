# Explore specialty zones -- BUILD PLAN (2026-10-29)

Status: **a plan, nothing built, waiting on the owner's answers in § 0.** It
turns `docs/explore-specialty-plan.md` (the proposal: what and why) into
phases with files, callers, tests and hardware checks, in the shape of
`docs/build-plan.md`. Where the two differ, the owner's answers decide, then
this file.

Every number below comes from a pret disassembly, ported, never remembered.
The sources were checked as fetchable from this container on 2026-10-29:

| mechanic | source |
|---|---|
| Kanto Safari (BAIT / ROCK, flee, ball count) | `pokered` `engine/battle/core.asm` (`wSafariBaitFactor`, `wSafariEscapeFactor`, `wNumSafariBalls`), `engine/items/item_effects.asm` (the ball) |
| Hoenn Safari (POKEBLOCK / GO NEAR) | `pokeemerald` `src/safari_zone.c`, `src/battle_script_commands.c` (`safariCatchFactor * 1275 / 100`) |
| Great Marsh (MUD / BAIT) | `pokeplatinum` `src/battle/` (battle controller, Safari branch) |
| Bug-Catching Contest | `pokecrystal` `engine/events/bug_contest/contest.asm`, `judging.asm`, `data/wild/bug_contest_mons.asm` |
| Pokemon Tower's ghosts | `pokered` `engine/battle/core.asm` (`IsGhostBattle`, `PrintGhostText`) |

## 0. Decisions for the owner (blocking)

Recommendation first in each.

1. **Safari catch maths.** (a) *Recommended:* port each game's own catch
   formula for the Safari ball, which needs each species' real catch rate --
   a new generated `DEX_CATCH_RATE[]` (1 byte x 1025, ~1 KB flash; PokeAPI's
   `capture_rate`, which `gen_dex_data.py` already reads). BAIT and ROCK only
   mean what they meant in the game on top of the game's formula.
   (b) Apply bait/rock as multipliers on today's `wildCaptureChance()`. That is
   cheaper but not the games' odds.
2. **What ends a session.** (a) *Recommended:* the games' rule -- 30 Safari
   Balls or 500 real pedometer steps, whichever comes first, with each EXPLORE
   tap inside the session one encounter. Someone who never walks can still use
   all 30 balls. The emulator has no pedometer, so its sessions end on balls
   only (and `safari_test` drives steps through `Pet::addSteps()`).
   (b) A fixed number of encounters (for example 30).
3. **Hoenn and Sinnoh menus.** (a) *Recommended:* each game's own menu (Kanto
   BALL/BAIT/ROCK/RUN, Emerald BALL/POKEBLOCK/GO NEAR/RUN, Platinum
   BALL/MUD/BAIT/RUN). There are no Pokeblocks here, so Emerald's POKEBLOCK
   throws a plain block with the game's "no flavour match" effect.
   (b) Kanto's menu everywhere.
4. **Safari Balls stay out of the bag** (*recommended*, as in the games): a
   session counter, and anything left over is lost when you leave.
5. **The entry fee** is $500 from step money, as in every game. Refused with
   less than $500.
6. **Pokemon Tower's gate.** The Tower opens today at gate 4, which is also
   where the Silph Scope fits (Celadon), so a ghost gate there would never be
   seen. *Recommended:* open the Tower at gate 3 (Lavender is reachable after
   Lt. Surge and Rock Tunnel, as in Red) and identify the ghosts at gate 4.
   Secret like the Super Rod, or visible? (*Recommended: visible* -- an
   unidentified GHOST is the point of the mechanic. Only the fact that the
   badge fixes it goes unsaid.)
7. **Bug-Catching Contest prizes.** Evolution stones do not exist here.
   *Recommended:* 1st place gets a random TM the player does not own (the
   Mart's own "not owned" rule), 2nd gets money, 3rd coins. Days: Tuesday,
   Thursday and Saturday as in Crystal. With no clock set, never (fail safe,
   like the weather).
8. **Fixed encounters: which ones, and which gates.** The list in § 4 with a
   gate for each. The red Gyarados is always shiny (the game's rule).
   Lapras is Fridays only (no clock = never).
9. **Swarms show on the row** (`SWARM`) -- *recommended*. The alternative is a
   hidden swarm, which in practice nobody would ever find.
10. **Fuchsia City and Pastoria City.** Today the SAFARI ZONE area holds
    Fuchsia's fishing and the GREAT MARSH holds Pastoria's. In Safari mode
    those maps would turn into Safari encounters. *Recommended:* give each city
    its own map in a neighbouring area (Fuchsia's with ROUTES 19-21's sea if
    the rod gates agree, Pastoria's with Route 213), checked with
    `verify_wild.py`.

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
  reuse existing bytes. Safari and contest sessions are RAM only.
- **`wild.cpp` stays the only includer of `wild_areas.h`.**
- **Every new guard is negative-checked.** The test must also prove the
  mechanism engaged (trap 3's "vacuously true" shape), for example that a
  Safari session really did skip `buildSquad()`, not just that the squad
  looks untouched.
- **Each phase is one version**: `FW_VERSION`, README badge, the README
  manual's numbers, `docs/release-notes/vX.Y.md` for players, a CLAUDE.md
  "Done" entry, screenshots regenerated (`make_screens.sh`, the three lists
  reconciled). Strings appended in all six languages, ASCII only.

## 1. v3.75 -- the Safari engine, Kanto (M)

**Generator.** A `'mode'` on the area tuple in `tools/wild_areas.py`
(`'safari'`); `gen_wild.py` emits `WildArea::mode` (`WILD_MODE_NORMAL`,
`WILD_MODE_SAFARI`) and refuses a Safari area that has a legendary home or a
hidden rod/water gate (no legendary lives in one today; the assertion keeps
it that way). Re-check that the other regions come out byte-identical apart
from the new field. § 0.10's city split happens here.

**Firmware.**
- `wild.h/.cpp`: `wildAreaMode()`; the pure Safari rules as testable
  functions with every roll passed in, like the rest of wild.cpp:
  `safariCatchChance(dex, baitState, rockState, roll)`,
  `safariFleeChance(...)`, `safariApplyBait/Rock(...)`. Kanto's values come
  from `core.asm`/`item_effects.asm`, and the test pins them at their
  boundaries.
- `safari.h` (Arduino-free, like `surf.cpp`): `SafariSession { region, balls,
  stepsLeft, active }`, `safariStart()` (charge $500 through the wallet,
  `pet.saveNow()`), `safariEnd()`, `safariStepTick()` fed from
  `Pet::addSteps()`'s caller.
- Sketch: tapping EXPLORE on a Safari area starts or continues the session.
  `startWildBattle()` branches on `wildAreaMode()`: **no `buildSquad()`**, no
  energy cost, no PP, `btlYou` is a stand-in that draws the player's avatar
  from behind (`avatars.h`) and never acts. A `btlSafari` flag picks the
  menu, through `btlMainOpt()`. BALL throws a Safari Ball (the session counter,
  not `bag.consume()`), and a catch goes through `wildStoreCaught()` ->
  `storeNewCreature()` -- the ONE catch path. BAIT/ROCK change the foe's
  state. Each turn the foe watches or flees by the game's odds. The foe never
  attacks (as in the games). RUN always works. Out of balls or steps: the
  game's message, then the session ends.
- Explore screen: while a session runs, the area row reads the balls and
  steps left. Leaving Explore ends the session (as leaving the gate does).
- i18n: BAIT, ROCK, SAFARI BALL x n, the out-of-balls and session-over lines,
  the fee line.

**Tests.** `safari_test` through the real tap handlers: the fee is charged
once and refused under $500. No squad is built, and the live pet's energy and
PP are unchanged; this is proved by a probe that fails if `buildSquad()` ran.
Balls count down per throw and never touch the bag. A catch lands in party,
then the PC, then the box. The session ends at 0 balls and at 500 steps.
BAIT/ROCK odds are pinned to the disassembly's constants. A non-Safari area
is unchanged (Route 1 still builds a squad). `flush_test` covers the Safari
menu, `hit_test` the four cells (they are the existing cells, so finger-sized
by construction), and `explore_test` the row text. Negative checks: the squad
built anyway, a ball taken from the bag, the session not ending, bait ignored.

**On a board:** the step count against a real walk, and the session row.

## 2. v3.76 -- Hoenn's and Sinnoh's Safari menus (S-M)

Mark Hoenn's SAFARI ZONE and SAFARI EXTENSION and Sinnoh's GREAT MARSH
`'safari'`. Per-game rule sets behind the same session: `SafariRules` chosen
by region (Emerald: `safariCatchFactor`, `safariEscapeFactor`, GO NEAR,
POKEBLOCK; Platinum: MUD, BAIT). Port each from its source and pin each in
`safari_test`. If § 0.3 is answered (b), this phase reduces to marking the
areas.

## 3. v3.77 -- Pokemon Tower's ghosts (S)

`'ghost_gate': 4` on the Tower area (and its gate moved to 3, per § 0.6).
`wildGhostHidden(area, pet)` is the one answer, built on `wildGateOpen()`.
While it holds: the foe's plate reads GHOST and draws a silhouette
(`drawThumbInk()`-style), balls "dodge" (the game's text, and the ball is
spent, as in Red), the foe's turns are the game's "GET OUT..." with no
damage, and RUN always works (`core.asm` `.canEscape`). Nothing is registered
as seen. `ghost_test`: hidden before the badge, identified after it on either
difficulty, a ball never catches while hidden, run always escapes, and other
Kanto areas are unaffected. Negative-check each.

## 4. v3.78 -- fixed encounters (S-M)

A second generated list, `WILD_FIXED` (`dex, area, level, gate, flags:
shiny, weekday mask`), with its own saved MET bits (`FIXED_BYTES`,
APPEND-ONLY order in `tools/wild_areas.py`, new legacy key, player record
tail). A species on it stays hatchable: **`wildIsExploreLegend()` must NOT
read this list.** `wildFixedFor(area, pet, weekday)` comes before the
legendary roll in `startWildBattle()`. It is a one-time guaranteed meeting
with no rare repeat (in the games they were one-offs). A fixed encounter
that runs or faints is gone, as in the games: MET is set as the fight starts,
same as the legendaries.

Candidates (levels from the map scripts): SNORLAX x2 (Routes 12 and 16, L30),
ELECTRODE x2 (Power Plant, L43), SUDOWOODO (Route 36, L20), LAPRAS (Union Cave
B2F, Fridays, L20), red GYARADOS (Lake of Rage, shiny, L30), KECLEON (Routes
119-120, L30). `fixed_test`: once and only once, the weekday rule, a forced
shiny, still in the egg pool, and saved and reloaded through each half of the
save.

## 5. v3.79 -- swarms (S-M)

`gen_wild.py` stops dropping each game's swarm tables and emits them as
`WILD_SWARMS` (region, area, species, level). `wildSwarmToday(region, pet)`
picks one area per region per day with the same day-hash as
`wildAreaWeather()` (salted by `Pet::daySalt`, so the existing re-roll cheat
covers it). With no clock set, no swarm. On the swarm day the area's draw
gives the swarm species the game's swarm share (Crystal: the swarm table
replaces the map's; Platinum: slots 0-1). The row reads SWARM.
`areaStatusText()` remains the one status line, and must still fit 20
characters (`explore_test` checks). `swarm_test`: one area a day, steady all
day, every candidate comes round, the share, and no swarm without a clock.

## 6. v3.80 -- the Bug-Catching Contest (S-M, after § 1)

NATIONAL PARK gets `'contest'` mode, live on contest days (§ 0.7) and normal
on other days. A session like the Safari's: 20 Park Balls, ONLY the lead
creature (a squad of one, the team picker's first), the bug table from
`bug_contest_mons.asm`. The one creature you keep is scored by `judging.asm`'s
formula against the game's NPC scores. Prizes per § 0.7. You may swap your
catch for a new one, as in the game. `contest_test`: the day rule, the bug
table, a squad of one, the score pinned to the disassembly, a prize once a
day.

## 7. v3.81 -- the Ruins of Alph puzzle (M)

A 4x4 sliding puzzle screen (`SCR_PUZZLE`, art from `pokecrystal`
`gfx/unown_puzzle/` via a `tools/gen_*.py`). Until it is solved the RUINS OF
ALPH row opens the puzzle instead of an encounter. One saved bit. It needs
`flush_test`, `hit_test` (finger-sized tiles) and `uiRimTarget()` only if it
pages. Tanoby and Solaceon reuse are left to the owner.

## 8. Later -- the Kalos Friend Safari (M)

After the champion, each `RivalRecord` opens a FRIEND SAFARI row whose type
and three species derive from that rival's MAC (a stable hash, only species
with art and an installed pack). This adds no new wire format. Design it
once phases 1-6 have been on a board.

## Not in this plan

Max Raids and Tera Raids (they need the battle league's systems), separate
FISH / ROCK SMASH buttons (they would reveal the rod secret), SOS chaining,
Hidden Grottos and tides. The reasons are in the proposal.
