# Explore rework: the plan (for review, not started)

Written 2026-10-21. This is item A of CLAUDE.md's action list, turned into a
design for the owner to review before any code is written. Nothing here is
built. Section 10 lists the questions that need an answer from the owner first.

## 1. What exists today

- EXPLORE is a tile beside the pet. It has a `< REGION >` selector (installed
  packs only), an EASY/HARD toggle and one big button (`renderExplore()`,
  `TamaPoke.ino`).
- `startWildBattle(region, hard)` rolls a **rarity tier** (`wildTierForRoll()`:
  common 70 %, evolved 22 %, rare 7 %, legendary 1 %). It then picks **any**
  species of that tier in the region, all equally likely
  (`wildPickSpecies()`). Two filters always apply: the species must have art
  and its pack must be installed.
- The level is the pet's level ±5 on normal and anywhere from 1 to 100 on hard.
  Shiny is a flat 1/4096.
- `exploreRegion` is RAM only. Nothing about Explore is saved.

The problems with this: Route 1 doesn't exist, nothing unlocks, a level 5 pet
can meet a level 1 Venusaur, a Mewtwo can turn up anywhere 1 % of the time, and
hard mode's 1–100 range makes most hard fights either trivial or unwinnable.

## 2. Goals

1. **Real areas** for each region, with the species, levels and odds taken from
   the games' own data.
2. **Areas unlock with badges**, in the same order the games open them up.
3. **Weighted species** inside an area, and evolved forms only at believable
   levels.
4. **Hard mode means the pet's level +5 to +15**, not 1–100.
5. **Legendaries live in fixed places** (Mewtwo in Cerulean Cave after the
   Elite 4), not in a 1 % roll everywhere.
6. Keep every existing guarantee: no art-less species, no species from a pack
   that isn't installed, a region with no pack stays locked (shown, never
   hidden), and the capture, escape and drop rules don't change.

On hold, by the owner's call: step-based encounters. Out of scope: clock work.

## 3. Data sources, one game per region

| Region | Game | Source | Format | Status |
|---|---|---|---|---|
| Kanto | **FireRed** (recommended, Q1) | `pret/pokefirered` `src/data/wild_encounters.json` | JSON, 12 land slots | VERIFIED (decomp) |
| Johto | Crystal | `pret/pokecrystal` `data/wild/johto_grass.asm`, `johto_water.asm` | asm, 7 slots × morn/day/nite | VERIFIED |
| Hoenn | Emerald | `pret/pokeemerald` `src/data/wild_encounters.json` | JSON, same layout as FRLG | VERIFIED |
| Sinnoh | Platinum | `pret/pokeplatinum` `res/field/encounters/encounters_*.json` | JSON per map, morning/day/night replacement slots | VERIFIED |
| Unova | Black 2 / White 2 | PokeAPI CSV `encounters.csv` + `encounter_slots.csv` + `encounter_condition_value_map.csv` | CSV, 3850 rows, 137 areas | NOT VERIFIED |
| Kalos | X / Y | PokeAPI CSV | 1470 rows, 79 areas | NOT VERIFIED |
| Alola | Ultra Sun / Ultra Moon | PokeAPI CSV, day/night conditions | 1216 rows, 123 areas | NOT VERIFIED |
| Galar | Sword / Shield | PokeAPI CSV (weather conditions included) | ~10800 rows, 165 areas | NOT VERIFIED |
| Paldea | Scarlet / Violet | **PokeAPI has 0 rows.** Bulbapedia / Serebii area pages, facts only | hand-transcribed into `tools/wild_paldea.py` | NOT VERIFIED |

I checked every path above today (all return 200, and the PokeAPI row counts
are measured, not guessed). Using the same games as the gym ladders means
Explore and the gyms describe the same world. Kanto's ladder is already
FRLG's teams, which is why FireRed is recommended over `pokered` (which
CLAUDE.md first named).

**Version exclusives (Q2).** Each region has a version pair. The recommendation
is to take the **union** of both versions for each area, adding the slots
together, so that every species a region offers can be caught on one board.
The other choice is the ladder's version only (FireRed, Emerald, Platinum,
B2, X, US, Sword, Scarlet).

### Encounter rates per slot, from the sources

- FRLG / Emerald land: 20 20 10 10 10 10 5 5 4 4 1 1. Water: 60 30 5 4 1.
- Crystal grass: 30 30 20 10 5 4 1, with a separate slot list for each time of
  day. Water: 60 30 10.
- Platinum: the Gen 4 land table, and slots 3–4 get replaced by the morning,
  day or night entry.
- PokeAPI: `encounter_slots.rarity` gives each slot its percentage directly.

## 4. Turning the data into areas

### 4.1 Merging maps into areas

Games split one place across several maps (Mt. Moon has 1F, B1F and B2F;
Lost Cave has 14 rooms). The generator merges them with a hand-maintained
`AREA_MAP` table in `tools/wild_areas.py`: area name → list of source maps.
Weights for the same species are summed, then normalised so each area's weights
total 255. Target **12–30 areas per region**, with names of 12 characters or
fewer in ASCII so they fit the panel. Area names are proper nouns and are not
translated, so they go in the generated header, not in `i18n.cpp`.

### 4.2 Encounter methods (Q4)

- **Phase 1: walking only**, meaning grass, caves and other land.
- **Phase 2, optional:** SURF as a second slot list for each area. It unlocks
  with the badge that allows Surf in that game, e.g. Kanto's Soul Badge (5)
  and Johto's Fog Badge (4). Fishing and Rock Smash are left out: they each
  need a rod or item mechanic this game doesn't have.

### 4.3 Time of day

Each slot carries `when`, a bitmask of MORN|DAY|NITE (all three if the source
doesn't care). The clock uses Crystal's bands: morning 04–10, day 10–18, night
18–04, read through one function, `wildTimeBand()`. **A board whose clock was
never set counts as DAY**, which fails safe the same way auto-sleep does.
Galar's weather tables are merged into a single "normal weather" set.

### 4.4 Generated output

New file `wild_areas.h`, emitted by `tools/gen_wild.py`:

```c
struct WildSlot { int16_t dex; uint8_t lvMin, lvMax, weight, when; };   // 6 B
struct WildArea {
  const char *name; uint8_t region; uint8_t gate;      // badges needed, or GATE_CHAMPION
  uint16_t first; uint8_t count;                       // into WILD_SLOTS[]
  int16_t legend; uint8_t legendGate;                  // fixed legendary, 0 = none
};
static const WildSlot WILD_SLOTS[] = { ... };
static const WildArea WILD_AREAS[] = { ... };
static const uint16_t WILD_AREA_OFS[REGION_COUNT + 1]; // each region's slice
```

`dex` is `int16_t`, as every dex value must be (CLAUDE.md trap 2).

**Flash budget**, an upper bound: 9 regions × 30 areas × 15 slots × 6 B ≈
24 KB, plus about 4 KB of names. That is small against a 3 MB app partition.

### 4.5 The pipeline (same shape as `verify_rosters.py`)

```
tools/fetch_wild.py    raw sources -> tools/wild_cache/ (gitignored)
tools/wild_areas.py    HAND-WRITTEN: AREA_MAP, GATES, LEGENDS, short names
tools/gen_wild.py      cache + wild_areas.py -> wild_areas.h   (idempotent)
tools/verify_wild.py   re-derives every area from the source, diffs against
                       wild_areas.h; lists Unova..Paldea as NOT VERIFIED
```

Reviewers check the hand-written table one region at a time. The fetched data
is never hand-edited.

## 5. Badge gates

### 5.1 The rule

An area opens when the player holds at least `gate` badges in **that region**.
Badges from either difficulty count (Q7): the popcount of
`badgeMask(r,false) | badgeMask(r,true)` over bits 0–7. `GATE_CHAMPION` means
ladder bit 12, the champion, beaten. Bits 8–11 are the Elite 4, using the same
layout `gymUnlocked()` reads.

There is one function, `Pet::areaUnlocked(area)`. The area list (how a row is
drawn and whether a tap is accepted), `startWildBattle()` and the tests all
ask it. That is CLAUDE.md trap 1: never a second copy of a rule.

**Locked areas are shown greyed with "BADGE n"** (or "CHAMPION"), never hidden.
That is the same lesson as NEEDS PACK.

### 5.2 Kanto, drafted in full (FireRed progression)

| Gate | Areas |
|---|---|
| 0 | Route 1, Route 22, Route 2, Viridian Forest |
| 1 Boulder | Route 3, Mt. Moon, Route 4, Route 24, Route 25 |
| 2 Cascade | Route 5, Route 6, Route 11, Diglett's Cave |
| 3 Thunder | Route 9, Route 10, Rock Tunnel, Route 7, Route 8 |
| 4 Rainbow | Pokemon Tower, Routes 12–15, Routes 16–18, Safari Zone |
| 5 Soul | Seafoam Islands, Routes 19–21 (surf), Power Plant, Pokemon Mansion |
| 7 Volcano | Sevii 1–3: Kindle Road, Mt. Ember, Berry Forest (Q6) |
| 8 Earth | Route 23, Victory Road |
| Champion | Cerulean Cave; Sevii 4–7: Icefall Cave, Lost Cave, Altering Cave (Q6) |

Legendaries: Articuno in Seafoam (5), Zapdos in the Power Plant (5), Moltres on
Mt. Ember (7), Mewtwo in Cerulean Cave (Champion).

### 5.3 The other regions, outlined

Each gets a full table like Kanto's in its own reviewed commit. The outline:

- **Johto (Crystal):**
  - 0: Routes 29–31, 46, Sprout Tower, Dark Cave (entrance)
  - 1: Route 32, Ruins of Alph, Union Cave, Route 33, Slowpoke Well
  - 2: Ilex Forest, Routes 34–37, National Park, Burned Tower
  - 3: Routes 38–39
  - 4 (Surf): Routes 40–43, Whirl Islands, Mt. Mortar, Lake of Rage, Routes 47–48
  - 7: Route 44, Ice Path
  - 8: Dragon's Den, Route 45, Routes 26–27, Tohjo Falls, Victory Road
  - Champion: Mt. Silver
  - Roaming Raikou and Entei after the Burned Tower (gate 2 or 3). Suicune,
    Lugia and Ho-Oh go wherever Crystal places them, confirmed against
    pokecrystal's event scripts.
- **Hoenn (Emerald):**
  - 0: Routes 101–104, Petalburg Woods
  - 1: Route 116, Rusturf Tunnel, Granite Cave
  - 2: Routes 110–115, Fiery Path, Meteor Falls
  - 3: Jagged Pass, Mt. Chimney
  - 4: Desert, Routes 118–123, Mt. Pyre, Safari Zone
  - 5 (Surf): the sea routes
  - 7 (Dive): Seafloor Cavern, Routes 124–134
  - 8: Victory Road
  - Champion: Sky Pillar (Rayquaza), Terra and Marine Caves (Groudon,
    Kyogre), the Regi chambers
- **Sinnoh (Platinum):** the same approach, Eterna Forest through Stark
  Mountain. The lake trio, Giratina and Dialga/Palkia go where Platinum puts
  them.
- **Unova, Kalos, Alola, Galar:** the gate is the gym count needed to reach the
  area in the ladder's version. Alola's ladder slots map onto island
  progression; Galar's Wild Area opens at gate 0 with its Wild Area levels.
- **Paldea:** open world, so it has no fixed order. Gate each named area by its
  level band against the ladder: an area opens at the first badge whose leader
  is at or above the area's minimum level −5.

### 5.4 An automatic cross-check

The Paldea rule above is also a lint for every region. `verify_wild.py` warns
when an area's minimum level is far above the next leader's level, which
catches a gate typed wrong in the hand-written table. It is a warning, not a
rule: Crystal's Johto deliberately dips (Pryce).

## 6. Rolling an encounter

1. **Pick the area.** It must be unlocked, in an installed region, and have at
   least one live slot.
2. **Live slots** are the slots whose `when` matches the time band, whose
   species has art, and whose species' own region pack is installed. Weights
   are renormalised over what's left. *Note:* Johto routes are full of Kanto
   species. With only the Johto pack, Pidgey drops out of Route 29. An area
   with no live slots shows **NEEDS PACK \<region\>** and refuses the tap.
3. **Weighted pick** over the live slots: one `random()`. The function itself
   stays pure (the roll is passed in), like the rest of `wild.cpp`, so
   `wild_test` can pin it at boundaries.
4. **Level (Q3).**
   - Normal: roll inside the slot's game range, then clamp to the pet's level
     ±5. So Route 1 is L2–5 for a new pet, and a late area never outlevels
     you by more than 5. This keeps the existing promise that a normal
     encounter can always be won.
   - Hard: the pet's level **+5 to +15** (owner's call), capped at 100.
5. **Believable forms.**
   - **Floor:** an evolved species is never below the level its pre-evolution
     evolves at (Pidgeotto ≥ 18).
   - **Evolve-up (Q3b):** if the chosen level reaches the species' own
     `evolveLevel` and the target has art and an installed pack, the
     encounter becomes the evolved form. Repeat while it still qualifies.
     This never happens for trade evolutions (`TRADE_EVOS`) or Eevee's
     branches.
   - Both rules ask `Pet::evoInfo()` and the species tables. They don't
     restate evolution levels.
6. **Legendary.** If the area has one and its `legendGate` is met, a fixed
   chance (proposed 5 %, Q5) replaces the slot pick with the legendary at its
   game level. If not, the slot pick stands. **Mythicals** (Mew, Celebi,
   Jirachi and so on) are never in Explore (Q9). Eggs are unchanged.
7. **Roamers** (Johto's beasts, the Eon duo and so on) join every unlocked
   area of their region at a low fixed weight once their gate is met.
8. Shiny, IVs, capture, escape, drops and the blackout are **unchanged**.

The 70/22/7/1 tier roll is retired for each region as soon as that region has
areas. Regions still without data use the old pool, so the game is never
partly broken mid-rollout. `R_*` rarity stays: capture odds still use it.

## 7. UI

- **The EXPLORE screen** keeps `< REGION >` and EASY/HARD. The big button
  becomes `EXPLORE <AREA>`, the area you used last in that region, so repeat
  hunting stays one tap. A new **AREAS** button opens the list.
- **The area list (`SCR_AREA`)** shows three rows a page at `UI_TAP_FINGER`
  height, with the round-panel widths from `uiSafeHalfWidthFor()`. Each row
  shows the name and level range, or `BADGE n` / `CHAMPION` / `NEEDS PACK`
  greyed out. A legendary area gets a star after the gate is met. Tapping an
  open row selects it and goes back to EXPLORE (it doesn't fight straight
  away, so a mis-tap can't start a battle). Swipe down goes back.
- **Paging is a rim drag through `uiRimTarget()`**, the one table of paged
  screens. The new screen must also go into `swipe_test`, `flush_test`,
  `SCREEN_NAME[]` and the crash breadcrumb list.
- **ALL (Q8):** recommended as a random unlocked area anywhere you have packs
  installed, with the button reading `EXPLORE ANYWHERE`.
- The chosen area per region lives in RAM (`exploreArea[REGION_COUNT]`), the
  same as `exploreRegion`.
- New UI strings (AREAS, BADGE %u, CHAMPION, ANYWHERE) go in **all 6 language
  rows at the same index**, ASCII only.

## 8. Save impact

**None, as proposed.** Encounters, the chosen area and the chosen region are
not stored, and the legendary is a chance, not a one-time event. If the owner
wants caught legendaries to be gone for good (Q5), that needs a new
player-wide bitmask. It would be appended to the `plyA`/`plyB` record's tail
under the append-only rule, with a `SAVE_FIELDS` entry and an update to
`tpsave.py`. That is the one design choice here that touches the save.

## 9. Tests (each guard negative-checked)

- **New `wildarea_test`:**
  - Every slot's dex is in 1..`DEX_COUNT` and inside its area's source data.
  - Every area has weight above zero and a gate of 0..8 or `GATE_CHAMPION`.
  - Area names are ASCII and fit the row width.
  - A locked area is refused **through `onTap()`** (the caller, not just
    `areaUnlocked()`), and is drawn greyed.
  - With only one pack installed, no species from another pack ever appears
    in 2000 rolls, and the test asserts that some *were* dropped, so the
    check can't pass vacuously.
  - Time bands: a night-only slot never appears at noon and does appear at
    night. An unset clock counts as day.
  - Hard levels always fall in pet+5..+15. Normal levels fall inside the
    clamp.
  - The evolved-form floor and evolve-up rule hold, and trade evolutions
    never evolve up.
  - A legendary appears only in its own area, and only after its gate.
- **`explore_test`:** drive the area list, then EXPLORE, and assert `wildDex`
  comes from that area.
- **`swipe_test`:** add the area list. **`flush_test`:** add the new screen.
  **`hit_test`:** rows at finger height.
- **`verify_wild.py`:** 0 differences for the four decomp regions, and a NOT
  VERIFIED section for the rest.
- **Screenshots:** add an `area` shot to `make_screens.sh`, the `--shot` list
  in `main_sdl.cpp` and the README, then run the three-list reconcile.

## 10. Questions for the owner (please answer before Phase 1)

1. **Kanto source:** FireRed, which matches the gym ladder (recommended), or
   Red as first planned?
2. **Version exclusives:** the union of both versions (recommended), or the
   ladder's version only?
3. **Normal levels:** the game range clamped to the pet's level ±5
   (recommended), pure game levels (Route 1 stays L2–5 forever), or pet ±5 as
   today?

   3b. **Evolve-up:** should a high-level roll on an early route become the
   evolved form (L60 on Route 1 gives Pidgeot)? Recommended yes.
4. **Surf** in the first release, or walking only first (recommended)?
5. **Legendaries:** a 5 % chance on every visit to their area once unlocked,
   with no save change (recommended), or one-time like the games, which
   needs a save field?
6. **Postgame and side areas:** include Sevii Islands, the Battle Frontier
   areas and so on, gated at Champion?
7. **Badges:** should easy and hard badges both count toward a gate?
   Recommended yes.
8. **ALL:** a random unlocked area anywhere (recommended), or remove ALL from
   Explore?
9. **Mythicals:** keep them out of Explore entirely (recommended)?

## 11. Phases (each one a version and a README update)

| Phase | Content | Version |
|---|---|---|
| 1 | `fetch_wild` / `gen_wild` / `verify_wild` with **Kanto** data only, no firmware change | none |
| 2 | Firmware: area model, `SCR_AREA`, gates, level and evolve rules, Kanto live; other regions keep the old pool | 3.39 |
| 3 | Johto, Hoenn, Sinnoh (decomp, verified) | 3.40 |
| 4 | Unova, Kalos, Alola, Galar (PokeAPI, NOT VERIFIED) | 3.41 |
| 5 | Paldea (Bulbapedia / Serebii, NOT VERIFIED) | 3.42 |
| 6 | Legendaries and roamers at fixed locations across all regions; the 1 % tier is removed | 3.43 |
| (7) | Surf slots, if Q4 says yes | — |

Every firmware phase is a balance change, so each one updates README § "Wild
encounters, catching and the bag" (which today documents the tier table),
`FW_VERSION`, the README badge and the release notes, and is written for
players.
