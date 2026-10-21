# Explore rework: the plan (revision 3, CONFIRMED by the owner 2026-10-21)

Revision 3, 2026-10-21. Revision 2 folded in the owner's answers to revision
1, each checked against the games' data; revision 3 records the owner's answers
to revision 2's five open questions (§ 11). **The plan is confirmed.** Phase 1
(the data pipeline and Kanto's data, no firmware change) is the first work.

## 1. What exists today

- EXPLORE is a tile beside the pet. It has a `< REGION >` selector (installed
  packs only), an EASY/HARD toggle and one big button.
- `startWildBattle()` rolls a rarity tier (70/22/7/1 %), then picks any
  species of that tier in the region, all equally likely.
- Levels are the pet ±5 on normal and 1–100 on hard. Shiny is 1/4096.
- Nothing about Explore is saved (`exploreRegion` is RAM only).

## 2. Decisions so far

| # | Question | Decision |
|---|---|---|
| 1 | Kanto data source | **FireRed**: our Kanto gym ladder already uses FireRed/LeafGreen's teams, so wild areas from the same game keep one world (see § 3) |
| 2 | Version exclusives | **Combine both versions** in every area |
| 3 | Levels | **Tied to the gym ladder** (§ 6). Evolved forms follow the games' own tolerance (§ 6.3) |
| 4 | Water | **Water areas are ordinary areas.** No Surf badge or rod needed |
| 5 | Legendaries | **One guaranteed encounter** at a named location, then the area's normal spawns plus a small chance of the legendary again until it is caught (§ 7) |
| 6 | Side and postgame areas | Included (pros and cons in § 5.4) |
| 7 | Easy and hard badges | **Both count** toward a gate |
| 8 | ALL in Explore | **Disabled with a compile-time switch** (`EXPLORE_ALLOW_ALL 0`). The code stays in place and still compiles |
| 9 | Mythicals | None of the 23 can be found in the wild in the main story, so **all are rare egg spawns** under the existing egg bonuses (§ 8) |

About question 1: there are two ways to get Kanto's wild data, the original
Red (1996) and the FireRed remake (2004). They put somewhat different
creatures in the same places. Our Kanto gym ladder already uses FireRed's
leaders and levels, so using FireRed for the wild areas means a route and the
gym next to it come from the same game.

## 3. Data sources

| Region | Games (combined) | Source | Status |
|---|---|---|---|
| Kanto | FireRed + LeafGreen | `pret/pokefirered` `src/data/wild_encounters.json` | VERIFIED |
| Johto | Crystal (+ Gold/Silver exclusives, Q-A) | `pret/pokecrystal` `data/wild/*.asm` | VERIFIED |
| Hoenn | Emerald (+ Ruby/Sapphire exclusives, Q-A) | `pret/pokeemerald` `src/data/wild_encounters.json` | VERIFIED |
| Sinnoh | Platinum (+ Diamond/Pearl, Q-A) | `pret/pokeplatinum` `res/field/encounters/encounters_*.json` | VERIFIED |
| Unova | Black 2 + White 2 | PokeAPI CSV | NOT VERIFIED |
| Kalos | X + Y | PokeAPI CSV | NOT VERIFIED |
| Alola | Ultra Sun + Ultra Moon | PokeAPI CSV | NOT VERIFIED |
| Galar | Sword + Shield | PokeAPI CSV | NOT VERIFIED |
| Paldea | Scarlet + Violet | Bulbapedia / Serebii (PokeAPI has 0 rows) | NOT VERIFIED |

**Q-A, for Johto, Hoenn and Sinnoh.** Each of these "third" games is
effectively already a combined edition, and the pret decomps contain only that
one game. Combining with the pair before it (Gold/Silver, Ruby/Sapphire,
Diamond/Pearl) would have to come from PokeAPI, outside what the decomp can
verify. The recommendation is to use the third game alone for those three,
since it already holds nearly every species the pair had.

## 4. Areas

- **Several maps become one area**, through a hand-maintained table
  (Mt. Moon 1F/B1F/B2F becomes MT. MOON). Kanto came out at 36. Names are
  upper-case ASCII, 16 characters or fewer.
- **All encounter methods go into one area pool**: walking, surfing, the best
  fishing rod and Rock Smash, each weighted by its own in-game rates. (Q-B:
  fishing included. Without it Horsea, Goldeen, Magikarp, Krabby and others
  can't be caught at all in Kanto.) Special methods are left out: swarms,
  Poke Radar, honey trees and hidden grottos.
- **Sea routes are areas too.** Kanto's Routes 19–21, for example, come in as
  areas of their own, gated like any other.
- **Time of day**: each slot can be MORN, DAY or NITE where the source says
  so. The bands are 04–10, 10–18 and 18–04, and a board whose clock was never
  set counts as DAY. Galar uses its normal-weather tables only.

## 5. Badge gates

### 5.1 Rule

An area opens when the player holds at least `gate` badges in that region,
counting easy and hard together (popcount of `easy | hard`, bits 0–7). The
other gates are `GATE_E4`, meaning all eight badges with the Elite 4 still
ahead, and `GATE_CHAMPION`, meaning ladder bit 12 is set. There is one
function, `Pet::areaUnlocked()`. The list, the tap handler and the tests all
ask it.

### 5.2 Display

A locked area reads **`REQUIRES BOULDER BADGE`**: the actual badge, by name
(owner's call, Q-C). The row is greyed and never hidden. This needs a badge-name
table, which does not exist yet (the names are only comments in `badges.h`):
`BADGE_NAMES[GYM_REGIONS][TRAINER_GYMS]`, facts from Bulbapedia, ASCII.

- Regions without badges in the games use what the games award instead,
  e.g. Alola's island trials. If no name exists for a slot, the row falls back
  to the leader's name (`REQUIRES <LEADER>`), never to a bare number.
- `GATE_CHAMPION` reads `REQUIRES CHAMPION`.
- `Requires %s` is one translated string in all six languages; badge names are
  proper nouns and stay in English, as the trainer names already do.

### 5.3 Kanto worked example (bands from § 6, using the ladder as it is today)

| Gate | Areas | Normal levels |
|---|---|---|
| 0 | Route 1, Route 22, Route 2, Viridian Forest | 1–12 |
| 1 (beat Brock) | Route 3, Mt. Moon, Route 4, Route 24, Route 25 | 11–19 |
| 2 (Misty) | Route 5, Route 6, Route 11, Diglett's Cave | 14–22 |
| 3 (Surge) | Route 9, Route 10, Rock Tunnel, Route 7, Route 8 | 19–27 |
| 4 (Erika) | Pokemon Tower, Routes 12–18, Safari Zone | 33–41 |
| 5 (Koga) | Seafoam Islands, Routes 19–21, Power Plant, Pokemon Mansion | 33–41 |
| 7 (Blaine) | Sevii 1–3: Kindle Road, Treasure Beach, Mt. Ember, Berry Forest | 40–48 |
| 8 (Giovanni) | Route 23, Victory Road | 44–52 |
| Champion | Cerulean Cave, Sevii 4–7 | 60–70 |

The other regions get the same table, built from each game's progression and
reviewed one region per commit. Paldea is open world, so its areas are gated
by their level band: an area opens at the first badge whose band covers it.

### 5.4 Side and postgame areas: pros and cons

These are places like FireRed's Sevii Islands, Emerald's Battle Frontier
surroundings, Platinum's Battle Zone (Routes 225–230, Stark Mountain) and
Crystal's Mt. Silver.

- **For:** they hold the strongest wild creatures, so they give a reason to
  keep exploring after the champion. Some species are only found there
  (FireRed's Sevii Islands have Johto species such as Sentret, Hoothoot and
  Togepi). Several legendaries live there (Moltres on Mt. Ember; Heatran and
  the Regigigas line at Stark Mountain and Snowpoint Temple).
- **Against:** they add 5–15 more rows per region to the list. Some hold
  species from other regions, so they need that region's sprite pack, and
  with it missing they read NEEDS PACK. They are more work to gate correctly.

The recommendation is to include them, with each one's gate matching the game
(Sevii 1–3 after Blaine, the rest at Champion).

## 6. Levels

### 6.1 Normal

The level band comes from **the next gym battle's level cap**, which is the
highest level on that leader's team. That is the same cap the ladder already
enforces, and it is derived from `trainers.h` at runtime, never copied out:

- **Gate 0:** level 1 to (first leader's cap − 2).
- **Gate g (g badges held):** the next battle is ladder slot g, and the band
  is (its cap − 10) to (its cap − 2). At `GATE_E4` the next battle is the
  first Elite 4 member.
- **Champion:** (champion's cap − 5) to (champion's cap + 5), since there is
  no next battle.
- **Bands never go down.** Each cap is the running maximum along the ladder.
  This matters where a ladder dips (Johto's Pryce is weaker than Chuck), so
  the next set of areas is never easier than the last.

Example, Kalos: Viola's team in our ladder tops out at **12** (Vivillon L12,
matching X/Y), not 14. So Kalos's first areas are **1–10**. Once Viola is
beaten, the next battle is Grant (cap 25), so those areas are **15–23**.

### 6.2 Hard: the AI, not the levels (owner's call, Q-D)

**Easy and hard use the same level band.** Raising the levels would make the
fight harder, but it would also hand the player stronger creatures than the
games intended for the next gym, and the games' own balance is the point.
Hard means the wild creature **fights smart**: it already passes the difficulty
to the battle AI (`aiChooseMove(btlFoe, btlYou, btlHard)`), so hard needs no
new mechanism. Hard keeps its existing reward (two item drops instead of one).
`wildLevelMin()`/`wildLevelMax()`'s 1-100 hard range is removed.

### 6.3 Evolved forms: do the games allow them below their evolution level?

**Yes, and it's common, but never by much.** Every wild slot in FireRed was
compared against PokeAPI's level-up evolution data. 13 evolved species appear
below the level their pre-evolution evolves at:

| Species | Lowest wild level | Evolves at | Where |
|---|---|---|---|
| Kakuna / Metapod | 4 / 5 | 7 | Viridian Forest |
| Gyarados | 15 | 20 | fishing |
| Haunter | 20 | 25 | Pokemon Tower |
| Seaking | 20 | 33 | Safari Zone fishing |
| Dragonair | 25 | 30 | Safari Zone fishing |
| Golduck, Dewgong, Weezing, Rapidash, Seadra, Poliwhirl, Magcargo | 1–13 levels under | | various |

So the rule copies the game rather than inventing a stricter one. **An evolved
species' floor is the lower of (its evolution level) and (the lowest level the
source game has it wild).** A Metapod can be L5; a Venusaur, never wild in
FireRed, can never be below L32.

**When the floor is above the area's band** (found by Phase 1, see § 14), the
evolved form is **left out of that area if another area of the region has it
inside its own band**, and **kept at its floor if not**. FireRed's best rod
puts a level 15+ Gyarados and a level 20+ Poliwhirl on Route 22, which opens
before Brock (cap 14): both are left out there and first met on Route 4 and
Routes 5–6. Dugtrio is only ever in Diglett's Cave, so it stays there at L26
in a 14–22 band, because a species must never become unobtainable to tidy a
band. `verify_wild.py` lists both kinds.

Evolve-up (an early-route roll becoming the evolved form) is **dropped**.
Bands now follow the ladder, so an area's own species list already fits its
levels.

## 7. Legendaries

### 7.1 The model

1. **A named location with one guaranteed encounter.** Once its gate is met,
   the location's row shows a star. The first EXPLORE there is the legendary,
   at its in-game level (Articuno 50, Mewtwo 70), on both difficulties; hard
   only changes how it fights.
2. That guaranteed encounter is **spent once the battle ends, however it ends**
   (caught, fled, fainted or knocked out).
3. **After that**, the location gives its normal spawns, plus a **small chance
   (proposed 3 %) of the legendary again**, until it is caught.
4. Once **caught**, it never appears in Explore again.

### 7.2 Is there always a fallback area? Mostly, not always

I checked against the decomps:

- **Kanto:** Seafoam Islands, the Power Plant and Cerulean Cave have their own
  wild tables. **Mt. Ember's summit has none**, so it falls back to Mt. Ember.
- **Johto:** Tin Tower and Whirl Islands (including Lugia's chamber) have
  tables. No gaps.
- **Hoenn: six gaps.** Terra Cave, Marine Cave, Southern Island and the three
  Regi chambers (Desert Ruins, Island Cave, Ancient Tomb) have no table. Each
  gets a hand-picked parent: the route it opens from (Route 111's desert for
  Desert Ruins, Route 105 for Island Cave, Route 120 for Ancient Tomb, the
  surrounding sea routes for the others).
- **Sinnoh:** the lakes, Turnback Cave, Stark Mountain, Snowpoint Temple and
  Iron Island have tables. **Spear Pillar and Fullmoon Island have none**, so
  they fall back to Mt. Coronet and the sea route nearby.
- **Unova onward:** the generator fails if a legendary location has neither a
  table nor a named parent, so a gap can't ship unnoticed.

### 7.3 Roamers

Johto's beasts, Hoenn's Latias/Latios, Sinnoh's Mesprit and Cresselia, and
FireRed's beasts all wander in the games. Each roamer is guaranteed once
where the game first shows it (Burned Tower for the Johto beasts). After that
it has the same small chance in **every unlocked area of its region**,
instead of one place, until it is caught.

### 7.4 Which legendaries count

All species PokeAPI marks `is_legendary`, at the place each game puts them.
Ones that need an event item in the games (Ho-Oh's Rainbow Wing, the
Eon Ticket) get the gate where the item is normally earned, usually Champion.

## 8. Mythicals: eggs only

PokeAPI lists 23: Mew, Celebi, Jirachi, Deoxys, Phione, Manaphy, Darkrai,
Shaymin, Arceus, Victini, Keldeo, Meloetta, Genesect, Diancie, Hoopa,
Volcanion, Magearna, Marshadow, Zeraora, Meltan, Melmetal, Zarude, Pecharunt.
In the main games they came from events, gifts or other games, or from DLC
outside our maps (Pecharunt is in Kitakami). None is a normal wild encounter
or a story catch in a base game. **So all 23 follow your egg rule** and never
appear in Explore. The egg bonuses already exist and stay as they are:

- the legendary egg tier unlocks at 25 registered species;
- it is 3 %, or **10 % after a farewell** (the "blessed" egg), plus a care
  bonus.

That is the "rare and incentivized" path, with no new mechanism.

**Story legendaries must be caught, not hatched** (owner's call, Q-E). They
leave the egg pool, so the egg legendary tier holds only mythicals: the rate
is unchanged, and every legendary egg is a mythical.

**This is switched per region, in the same phase that region's legendaries go
live in Explore.** Removing them from eggs everywhere at once would leave, say,
Unova's legendaries unobtainable until Unova's areas exist. A region whose
egg tier would be empty (no mythical with art in that region) falls back to
rare, the same way `startWildBattle()` already falls back when a tier is
empty.

## 9. UI

- The EXPLORE screen keeps `< REGION >` and EASY/HARD. The big button reads
  `EXPLORE <AREA>`, your last area in that region, for one-tap repeats. An
  **AREAS** button opens the list.
- **The area list (`SCR_AREA`)** shows three finger-height rows a page. Each
  row shows the name and level band, or the greyed gate (§ 5.2), or NEEDS
  PACK, plus a star for a legendary that is still uncaught. Tapping an open
  row picks it and goes back to EXPLORE; it never starts a fight on its own.
  Swipe down goes back.
- The list pages by rim drag through `uiRimTarget()`. It is added to
  `swipe_test`, `flush_test`, `SCREEN_NAME[]` and the screenshot lists.
- ALL is disabled behind `EXPLORE_ALLOW_ALL 0`. The selector skips ALL while
  the switch is off.
- New UI strings (AREAS, BADGE %u or BEAT %s, NEEDS PACK reused, a legendary
  marker) go in all 6 language rows at the same index, ASCII only.

## 10. Save impact: now one addition

The guaranteed encounter and "until caught" both have to survive a reboot, so
the player record gains **two bitmasks**: `legendMet` and `legendCaught`, each
`uint8_t[16]` (128 legendaries of room).

- Appended to the `plyA`/`plyB` tail under the append-only rule, with its
  dimension in the header like the other arrays, plus a legacy key, a
  `SAVE_FIELDS` entry and an update to `tpsave.py`.
- **The bit index is the legendary's position in a generated `WILD_LEGENDS[]`
  list, and that list is APPEND-ONLY**, the same rule as moves and avatars.
  A test pins the first entries by name.
- Downgrading is safe: an older firmware ignores the tail.
- `save_test`, `powerloss_test` and `upgrade_test` cover the new field.

Nothing else is stored. The chosen area stays RAM only.

## 11. Revision 2's open questions: the owner's answers

- **Q-A:** Johto, Hoenn and Sinnoh use Crystal, Emerald and Platinum alone, so
  they stay verifiable against the decomps. *As recommended.*
- **Q-B:** fishing (the best rod) and Rock Smash go into each area's pool.
  *As recommended.* How the methods share an area is in § 12.
- **Q-C:** locked rows read `REQUIRES BOULDER BADGE` (§ 5.2).
- **Q-D:** easy and hard share the level band; hard is the smarter AI (§ 6.2).
- **Q-E:** story legendaries are caught, not hatched (§ 8).

## 12. Data pipeline and tests

```
tools/fetch_wild.py    raw sources -> tools/wild_cache/ (gitignored)
tools/wild_areas.py    HAND-WRITTEN: area merges, gates, legend locations + parents
tools/gen_wild.py      -> wild_areas.h (areas, slots, WILD_LEGENDS[]); idempotent
tools/verify_wild.py   diff against the decomps; NOT VERIFIED list; floor overshoots;
                       legendary locations with no table and no parent = failure
```

**How encounter methods share one area.** Each method's own slot rates are kept
inside it; between methods, walking counts 1.0, surfing 0.5, fishing 0.3 and
Rock Smash 0.2, renormalised over the methods the area has. So a route with
grass and water is mostly grass, and a sea route is mostly surfing with some
fishing. FireRed and LeafGreen are averaged, so a version exclusive comes in
at half the weight. The weights live in `tools/wild_areas.py`, beside the
area table, and `verify_wild.py` prints each area's final percentages.

`wild_areas.h` holds `WildSlot { int16_t dex; uint16_t weight; uint8_t when, floor; }`
(weights in parts of 10000, so a 1 % fishing slot is never rounded away),
`WildArea { name, region, gate, first, count, lvMin, lvMax }` and
`WildLegend { int16_t dex; uint8_t area, level }`. **The band is computed by
the generator from `trainers.h` and emitted per area**, so the firmware reads
one number instead of re-deriving a formula; `verify_wild.py` fails if the
ladder changes and the header is not regenerated, and Phase 2's firmware test
checks the emitted bands against `TRAINER_SETS` itself. Kanto is 435 slots,
about 3.5 KB. Roamer flags arrive with Phase 3.

**Tests** (each guard negative-checked):

- **New `wildarea_test`:**
  - Gates are enforced through `onTap()`, and a locked row draws greyed.
  - Bands are derived from the ladder, and the band never goes down.
  - The evolved-form floor holds.
  - Only installed packs are drawn from, and the test also proves some slots
    *were* dropped.
  - Time bands work, and an unset clock counts as day.
  - The legendary is guaranteed exactly once, then rare, then never after
    it's caught, across a simulated reboot.
  - A roamer appears in other areas after its first encounter.
  - Mythicals never appear in Explore.
- **`explore_test`:** the area chosen in the list is where `wildDex` comes
  from.
- **`region_test`:** mythicals are still reachable from eggs, and the
  blessed egg bonus still applies.
- **Screenshots:** an `area` shot, with the three screenshot lists
  reconciled.

## 13. Phases

| Phase | Content | Version |
|---|---|---|
| 1 | Pipeline + Kanto data, no firmware change | — |
| 2 | Firmware: areas, `SCR_AREA`, gates, ladder bands, floors, badge names, hard = AI only, ALL switched off; Kanto live, other regions keep the old pool | 3.39 |
| 3 | Legendaries (save bitmasks, guaranteed encounter, roamers); Kanto's legendaries leave the egg pool | 3.40 |
| 4 | Johto, Hoenn, Sinnoh | 3.41 |
| 5 | Unova, Kalos, Alola, Galar | 3.42 |
| 6 | Paldea; the 70/22/7/1 tier roll is removed once every region has areas | 3.43 |

Every firmware phase updates the README's wild encounters section,
`FW_VERSION`, the README badge and the player release notes.

## 14. Progress

**Phase 1: DONE (2026-10-21).** `tools/wild_areas.py` (the hand table: 36
Kanto areas, gates, 4 legendaries), `tools/gen_wild.py` (fetch, merge, weigh,
floors, bands; deterministic) and `tools/verify_wild.py`. `wild_areas.h` is
generated and committed but **not yet included by the firmware**, so nothing
on a board changes. Verify: every one of FireRed/LeafGreen's 132 maps with
encounters is in exactly one area; the header matches a fresh build; the four
legendary levels are read from the games' own scripts (Articuno, Zapdos,
Moltres 50, Mewtwo 70). Negative-checked: a dropped map, a map in two areas, a
hand-edited header, a reordered legend list, a legendary at the wrong script
and a ladder edit without a regeneration each fail.

**Next: Phase 2**, the firmware half (§ 13).
