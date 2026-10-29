# Explore: zones with their own mechanics -- PROPOSAL (2026-10-29)

Status: **proposed to the owner, nothing decided, nothing built.** Written
after v3.74. Every Explore area today works the same way: pick it, meet one
creature, fight or throw balls with your own team. The games gave some places
rules of their own. This is the list of the ones worth bringing over, grouped
by mechanic, since one engine usually serves several zones.

Sizes: **S** = data plus one rule, part of a version. **M** = about a version
(the size of Card Flip). **L** = several versions, or a new battle system.

Whatever is built: port the rules from the pret disassemblies (as the v3.73
slot engines were), never from memory, and verify the numbers against the
source before shipping.

## Worth doing

### 1. The Safari engine -- M

Zones: **SAFARI ZONE** (Kanto), **SAFARI ZONE** + **SAFARI EXTENSION**
(Hoenn), **GREAT MARSH** (Sinnoh). All three are already wild areas.

The games: no team of your own; $500 buys 30 Safari Balls and 500 steps; the
foe can flee every turn. Menus differ by game:

| game | menu |
|---|---|
| Red/FireRed (Kanto) | BALL / BAIT / ROCK / RUN |
| Emerald (Hoenn) | BALL / POKEBLOCK / GO NEAR / RUN |
| Platinum (Great Marsh) | BALL / MUD / BAIT / RUN |

Proposal: one Safari engine with each game's menu and odds. Tapping the area
charges $500 and starts a session: 30 Safari Balls (a session counter, not bag
items), no team picker, a foe that may flee. Catches go through the existing
catch path (`wildStoreCaught()` / `storeNewCreature()`). Nothing new is saved:
leaving ends the session.

Shape: a WildArea mode flag from the generator (the area stays a WILD area, so
`exploreAreaOf()`, `wildAreaState()` and the gates are untouched); a safari
branch in the battle screen's menu cells; a multiplier hook in
`wildCaptureChance()` for bait/rock; flee odds per turn. Tests: a
`safari_test` through the real tap handlers.

**Open questions for the owner:**
- The session limit: real pedometer steps (500, as in the games; the emulator
  has no pedometer), an encounter count, or both.
- Hoenn: its real menu (Pokeblocks, GO NEAR) or Kanto's for all three.
- Safari Balls kept apart from the bag, as in the games (recommended).

### 2. Bug-Catching Contest -- M (S-M once #1 exists)

Zone: **NATIONAL PARK** (Johto). Crystal: Tuesday, Thursday, Saturday; 20
Park Balls; only your lead creature; the park's table switches to a bug-only
list; the catch is judged on level, HP and stats (pokecrystal
`engine/events/bug_contest/`). 1st prize was a Sun Stone. Reuses #1's session
frame and the clock's weekday. Evolution stones do not exist here, so prizes
become money, coins or a TM -- the owner's call.

### 3. Pokemon Tower's ghosts -- S

Zone: **POKEMON TOWER** (Kanto). Before the Silph Scope every ghost is an
unidentified GHOST that dodges balls; you can only run. Proposal: a hidden
gate like the Super Rod's (the badge the Scope fits on the ladder); before it,
a Tower encounter shows "GHOST" as a silhouette, balls miss, RUN is the only
way out. One rule, asked in one function.

### 4. Fixed (static) encounters -- S-M

Snorlax (Routes 12 and 16, Poke Flute), Sudowoodo (Route 36), Lapras on
FRIDAYS (Union Cave B2F), the red Gyarados (Lake of Rage, always shiny),
Electrode (Power Plant), Kecleon (Routes 119-120). Proposal: the legendaries'
"guaranteed first meeting" with a SEPARATE list and its own saved bits --
these must stay hatchable, and `wildIsExploreLegend()` would take a species
out of the egg pool. Each needs a location and gate picked; the rest is
generator data. Bits APPEND-ONLY, like `WILD_LEGENDS`.

### 5. Ruins of Alph's puzzle -- M

Zone: **RUINS OF ALPH** (Johto). In Crystal no Unown appear until the first
sliding-tile puzzle is solved. Proposal: a 4x4 sliding puzzle with pret's
puzzle art; solving it opens the ruins (one saved bit). TANOBY RUINS and
SOLACEON RUINS could reuse it. No Unown letter forms exist in this dex, so it
only unlocks the area.

### 6. Swarms / outbreaks -- S-M

Crystal's phone swarms (Dunsparce, Yanma, Qwilfish, Remoraid, Marill), Hoenn's
TV outbreaks, Platinum's daily swarms, TROPHY GARDEN's daily creature. These
were deliberately LEFT OUT by `gen_wild.py`. Proposal: one area a region a day
has a swarm species, picked by the same day-hash as `wildAreaWeather()`; the
row reads SWARM. Brings back species no area offers today (Remoraid).

### 7. Kalos Friend Safari, from LAN rivals -- M

The game: three species of one type per friend code, after the champion.
Proposal: each rival in the LAN head-to-head record (`RivalRecord`, keyed by
MAC) opens a Friend Safari whose type and three species derive from that
board's MAC. A lasting reward for LAN battles.

## Probably not, or later

| zones | why |
|---|---|
| Galar Max Raid Dens, Paldea Tera Raids | Need Dynamax/Terastal and a multi-creature boss fight: a new battle system (L). A fit for the post-game battle league. |
| Separate FISH / ROCK SMASH / HEADBUTT buttons on routes | Would give away the Super Rod secret (and Galar's water mode). Only if the owner drops that secrecy. |
| Alola SOS chaining (shiny odds along a chain) | SOS creatures already appear as rare encounters; chaining needs allies summoned mid-fight: M-L for little. |
| Unova Hidden Grottos, Hoenn Shoal Cave tides | Grottos existed for hidden abilities (not in this game); tides mostly change items. |

## Suggested order

1. Safari engine (#1): three regions at once, and the base for the contest.
2. Pokemon Tower's ghosts (#3) and the fixed encounters (#4).
3. Swarms (#6).
4. Ruins puzzle (#5).
5. Friend Safari (#7). Raids wait for the battle league.
