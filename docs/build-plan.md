# Build and execution plan: training, secret codes, Bill's PC

Agreed with the owner 2026-10-23/24. This is the plan to build from; the
decisions behind it are summarised in CLAUDE.md § TODO "agreed with the owner
2026-10-23". Nothing here is built yet. Where this file and an older note
disagree, this file wins.

**The secret codes are developer-only.** Their sequences go in CLAUDE.md, never
in the README, the release notes or the installer page.

## Ground rules for every phase

- One version per phase, on `ccr-151c604c-659yz2`. **Merge to `main` only when
  the owner says so**, then rebuild the installer (`tools/build_web.sh`).
- `FW_VERSION`, the README badge and `docs/release-notes/vX.Y.md` move together.
  Release notes are for players and never mention a secret.
- Every rule is answered by ONE function, and every caller asks it (CLAUDE.md
  trap 1). Each phase below lists the function and its callers.
- Every new guard is negative-checked: break it on purpose, watch a test fail.
- Saves are append-only: new player-record fields go on the tail with a
  `PLAYER_VERSION` bump, a legacy key, a `SAVE_FIELDS` entry and a `begin()`
  reset; `PartyMon` keeps its size unless a phase says otherwise.
- The full suite runs before each push that changes firmware; the firmware
  must compile under `arduino-cli` (run.sh does that first).
- Anything that changes what goes over the radio bumps `LINK_PROTO` (and the
  trade protocol's version if trade messages change).

| Phase | Version | What | Size |
|---|---|---|---|
| 1 | v3.58 | Training scales with level, in battle; leader training | small |
| 2 | v3.59 | Code entry, the three minigame codes, hidden SETTINGS, the rules mark | medium |
| 3 | v3.60 | CHEATS menu, the cheats mark, creature marks, marks shown in LAN | medium |
| 4 | v3.61 | Bill's PC storage layer, migration, backup, every caller moved | large |
| 5 | v3.62 | Bill's PC screens: species grid, species ring, the 7th rule | medium |
| 6 | v3.63 | OWNED in the Pokedex, GO TO #, missing-only filter | small |
| -- | later | Post-game battle league | not designed |

Phases 4-6 can only ship in order; 1-3 are independent of them.

---

## Phase 1 -- v3.58: training in battle scales with level

**Player-visible rule.** Training stays 0-100 on the training screen, in the
minigames and in every gain message. The card's stat numbers show what the
creature fights with. Easy leaders have no training (like the games' story
leaders, which have no EVs); hard leaders have full training (like the games'
post-game facilities). Wild creatures have none.

**Formula.** `stat = base + level + iv * level / 100 + tr * level / 100`.
Full training is still +100 at level 100, +73 at 73, +14 at 14. HP is
unchanged (its fixed +10 is not training).

### Steps

1. **One stat function.** Today `pet.cpp` and `party.cpp` each have a
   `calcStat()` (party.cpp's comment says "mirrors"). Move it to one place
   (`stats.h`, `inline uint16_t battleStat(base, iv, lvl, tr)` plus the HP
   variant) and make both call it. Commit this first, with no behaviour change,
   and prove the suite is unchanged.
2. **Scale training** inside that one function.
3. **Leader training.** `foeFromSpecies()` takes the training value: easy 0,
   hard `Pet::trMaxFor(HARD_IV)` (= 100) on attack, defence and speed. Wild
   foes stay 0. The team-select caps and IVs are unchanged.
4. **LAN.** `LinkMon` carries stats computed by the sender, so two versions
   would fight with different maths: bump `LINK_PROTO` 4 -> 5.
5. **Re-measure.** A small simulation (a test binary, not a script that
   restates the formula) plays the ladder on easy and hard at levels 14, 40,
   73 and 100 with a trained and an untrained creature. Put the before/after
   table in front of the owner BEFORE shipping, then update the ladder table in
   CLAUDE.md and the README manual.

### Callers that must ask the one function

`Pet::atkStat/defStat/spaStat/spdStat/speStat/vitStat`, `Party::atkOf..vitOf`,
`combatantFromPet`, `combatantFromParty`, `linkMonFrom*`, the card
(`renderCardStats`), the party sheet, `STATS` on the console.

### Tests

- `stats_test`: the formula at L1/L14/L50/L100 with training 0 and 100;
  `Pet` and `Party` give identical numbers for the same creature
  (negative-check: give party.cpp its own copy back -- must fail).
- Leaders: easy foe training 0, hard 100, wild 0, read off a real
  `startTrainerBattle()` / `startWildBattle()` combatant.
- The card prints the battle number; the training screen prints 0-100.
- `lan_test`: a proto-4 peer is refused.

### Docs

README "Game manual": the stat formula and the leader rule. CLAUDE.md: the
ladder table and this entry. Release notes: battles fairer, card numbers changed
for young creatures and why.

---

## Phase 2 -- v3.59: code entry, minigame codes, hidden SETTINGS

### Code entry

- **Where:** the Memory game (`MG_SIMON`), during the WATCH phase.
  `simonPress()` returns early while `simonShowing`; that branch records the
  berry index into an 8-entry ring instead.
- **Time to enter:** the first watch phase is short (one berry plus
  `SIMON_PAUSE_MS`). While taps keep arriving, the showing PAUSES; it resumes
  after 1.5 s without a tap. A code can therefore be entered calmly, and a
  normal player who never taps during the watch phase sees no change.
- **Codes:** 8 berries each (4^8 = 65,536 possibilities), defined in one table
  `CODES[]` in a new `codes.h`. Matching is on the last 8 taps.
- **On a match:** a short jingle, the round ends with NO pay (joy/bond), and the
  code's action runs. A mismatch does nothing.
- **One function** `codeApply(id)`, also reachable as `CODE <name>` on the
  serial console for tests.

### The five codes

| Code | Action | Saved |
|---|---|---|
| CELADON | open Celadon Game Corner directly, once | coins / high score earned |
| GOLDENROD | open Goldenrod Game Corner directly, once | coins earned |
| SURF | open Surfing Pikachu directly, once | coins / high score / bond earned |
| SETTINGS | open the hidden SETTINGS screen | the toggles; the rules mark |
| CHEATS | open the hidden CHEATS screen (phase 3) | -- |

**One-time entry:** the code opens the hall/ride screen with a flag
`hallViaCode`; it bypasses `hallUnlocked()` / `surfSecretOpen()` only for that
visit, and is cleared when the screen closes from any of its exits (the same
closing point `hallMusicSync()` already relies on). The halls do NOT appear in
the area list afterwards. No mark.

### Hidden SETTINGS

A paged toggle list (rim-paged: add to `uiRimTarget()` and `swipe_test`).
Each toggle is a bit in `pet.settings` (u16) and is checked INSIDE the one
function that already owns the rule:

| Toggle | Checked in |
|---|---|
| Farming lockout off | `Pet::legendLockedFor()` |
| Safety net after 5 eggs | the safety-net threshold (one accessor replacing `LEGEND_SAFETY_EGGS` at its call site) |
| Story legendaries hatch from eggs | `eggCanHold()` |
| ALL region in Explore | `exploreRegionAllowed()` |
| No gym level cap | `startTrainerBattle()`'s cap (one helper `gymLevelCap()`) |
| No hard team-size cap | `squadCap()` |
| Revived companions age and evolve | `Pet::evoInfo()` and the ageing check on `frozen` |
| No daily bond cap | `Pet::bondCappedToday()` and the bond gain path |
| No runaways | `Pet::startRunaway()`'s automatic trigger (the console `RUN` still works) |
| Fair slots | `slotDrawOdds()` returns the bare strips' return |
| Necrozma needs Ultra Beasts met, not caught | `wildLegendLive()` |
| Classic stats (training not scaled by level) | `battleStat()` from phase 1 |

**The rules mark.** Turning ANY toggle on sets `pet.rulesUsed` bit for that
toggle (u16), permanent. The player card shows a "modified rules" mark while
`rulesUsed != 0`, forever. Turning a toggle off restores the rule, not the mark.

**Settings never apply in LAN battles or trades.** LAN fights are built from
`lan.mine` and resolved without these rules; the trade path does not read them.
Classic stats would change `LinkMon` numbers, so LAN combatants are always built
with the scaled formula regardless of the toggle.

### Save

Player record v8 tail: `settings` (u16), `rulesUsed` (u16). Legacy keys `stng`,
`rlus`; `SAVE_FIELDS`; `begin()` resets. `tpsave.py` decodes them.

### Tests

- `codes_test`: a code entered in the watch phase fires; the same taps outside
  the watch phase play the game normally; a near-miss (7 of 8) does nothing;
  a fired code pays nothing.
- Each minigame code opens its screen, a second visit through Explore is still
  gated, and leaving keeps the coins and high score.
- Each SETTINGS toggle changes its rule through the real caller (e.g. a
  locked-out release egg is legendary-eligible with the toggle on), and the
  rules mark appears and survives turning it off and a reload (checkpoint and
  legacy key, as `legendnet_test` does).
- Negative checks: a toggle checked outside its one function; the mark cleared
  on toggle-off.

---

## Phase 3 -- v3.60: CHEATS, marks, and marks in LAN

### The CHEATS screen

Same paged-list screen type as SETTINGS. One-shot actions and saved toggles:

| Cheat | Kind | Checked in / does |
|---|---|---|
| +1,000 coins | one-shot | `pet.coins`, capped as today |
| +1 Master Ball | one-shot | `bag.add(IT_MASTERBALL)` |
| +$100,000 | one-shot | `pet.wallet`, capped at $999,999 |
| All TMs | one-shot | `giveTm()` for every `TM_MOVES` entry |
| Unlimited PP | toggle | the PP spend in `battleAct()`, player side only, never LAN |
| All badges, both difficulties | one-shot | `winBadge()` per region and trainer |
| All Explore areas and secret gates open | toggle | `gateOpen()` in wild.cpp (areas, legends, rod, water, story gates) |
| Meet a chosen legendary | one-shot | `wildLegendFor()` forced for the next encounter |
| Max training | one-shot | live pet's training to `trMax*()` -- marks the creature |
| Hatch the egg now | one-shot | the hatch path -- marks the creature |
| Next egg shiny | one-shot | `hatch()` shiny roll -- marks the creature |
| Re-roll today's weather / Mart TMs | one-shot | a day salt read by `wildAreaWeather()` and `martTmToday()` |

### Marks

- **Trainer:** using any cheat sets `pet.cheatsUsed` bit for it (u32),
  permanent. The player card shows a cheats mark, distinct from the rules mark.
- **Creature:** a creature directly changed by a cheat gets `PMON_CHEATED`
  (bit 1 of `PartyMon::flags`; the same bit in the pet checkpoint's flags
  byte). Teaching a move from a TM that the player owns only through the cheat
  also marks (TMs given by the cheat are remembered in a `tmsCheated` bitset).
  Money, coins and balls are fungible: they mark the trainer only.
- **Audit every Pet <-> PartyMon conversion** so the flag survives banking,
  swapping, evolving, reviving and trading: `switchTo()`, `toPartyMon()`,
  `reviveFrom()`, `focusSwap()`, `wildToPartyMon()`, `prizeToPartyMon()`, the
  trade apply. `markTraded()` must keep bit 1.
- **Shown:** the card and party sheet show the creature mark; the player card
  shows both trainer marks.

### LAN and trades

- `LINK_PROTO` 5 -> 6: the hello carries the trainer marks (rules, cheats); each
  `LinkMon` carries the creature's flags byte. The pairing screen shows the
  rival's marks; the battle plates show a marked creature.
- Trades already carry the whole `PartyMon`, so the creature mark travels.
  `tradeMonValid()` must accept bit 1; bump the trade protocol version so an
  older board refuses rather than rejecting a marked creature mid-trade.
- Toggles (unlimited PP, all areas) never apply in a LAN fight.

### Save

Player record v9 tail: `cheatsUsed` (u32), `tmsCheated` (TM bitset, same size as
the owned bits). Legacy keys `chts`, `tmch`. No `PartyMon` size change.

### Tests

`cheats_test`: each cheat through `codeApply()`/the screen; the trainer mark is
permanent across a reload; the creature mark survives bank -> swap -> evolve ->
trade; unlimited PP does nothing in a LAN fight (`lan_test`); a proto-5 peer is
refused; `trade_test` passes a marked creature. Negative checks for each
"survives" path.

---

## Phase 4 -- v3.61: Bill's PC, the storage layer

### Decisions it implements

- Up to **6 of each species**, 1,025 species: 6,150 creatures.
- **OWNED** = held in the PC, the party, or as the live pet, right now.
  Releasing the last one removes the species from the collection. SEEN and
  REGISTERED are unchanged.
- PC deposit restores PP (as the games' PC heals): `ppForget(uid)`.

### Where it lives

The board's `ffat` partition (9.875 MB at 0x610000) is unused; 6,150 x 64 bytes
is 394 KB. The web installer's four-part install does not write it; "Erase
device" does, exactly as it erases the save.

- **Mounting:** `FFat.begin(false)`. Format ONLY when the partition has never
  held a filesystem (first boot after this version), never on an ordinary
  mount failure: formatting on any failure would wipe a collection over a
  transient error. A mount failure after the first format sets the existing
  save warning (`saveHealthy()`) and the PC reads as unavailable, not empty.

### File layout

- 65 groups of 16 species. Each group is ONE record: header (magic, version,
  group number, generation counter, length) + 16 x 6 `PartyMon` slots + CRC.
  About 6 KB.
- **Two files per group, A and B, alternating by generation**, newest valid
  one wins -- the same scheme as the `petA`/`petB` checkpoints. FAT has no
  atomic rename over an existing file, so "write a temp, rename" would leave a
  window; A/B does not.
- At boot, read every group's newest valid record into a 1,025-byte count index
  in RAM (about 400 KB of flash reads). Only the open group is kept whole.

### API (`pcstore.h`) -- the ONE way in

`pcBegin()`, `pcCount(dex)`, `pcGet(dex, i)`, `pcPut(mon) -> slot or PC_FULL`,
`pcTake(dex, i)`, `pcReplace(dex, i, mon)`, `pcFind(uid, &dex, &i)`, each write
returning whether it LANDED (the trade board needs that, as it does for NVS
today). `ownedCount(dex)` = PC + party + live pet, in one function.

### Every caller moves onto it (grep `party.box`, `BOX_SLOTS`, `boxAdd`)

`storeNewCreature()` (catches, prizes), `swapPartyBox()`, the farewell chooser,
`release`, `FwTradeBoard` (`where()`, `slotAt()`, `put()`), the trade picker,
the Linking Cord partner pick, `backfillUids()`, `save.cpp` (`MAX_VAL`, the
`box` field), `tools/debugger/tpsave.py`, and every test that reads the box.
`Party::box[]` is removed from RAM use once every caller is moved.

### Migration

On the first boot of v3.61: if the PC holds no migration marker, copy every
creature from the NVS `box` key into the PC, then write the marker. The NVS
`box` key is left as it is (a downgrade reads it), and is no longer written.
Documented consequence: downgrading after migrating shows the box as it was on
the day of migration.

### Backup ships in the SAME release

A storage layer the backup cannot carry is not allowed to ship.

- `EXPORT` gains a PC section of occupied slots only (sparse), with its own
  byte count in the header; `IMPORT` validates the whole PC section before
  writing anything, like the NVS half.
- `web/savefile.js`: new format version, PC section parsed and verified both
  directions (`verifyBackup()`); `tools/check_savefile.mjs` updated; the golden
  fixture regenerated from the firmware with a few PC creatures in it.
- Worst case, a full PC, is about 790 KB of hex: roughly a minute over USB.

### The emulator

An FFat stand-in over a directory beside the save file (`--save x.nvs` ->
`x.pc/`), with fault injection for a torn write (like `nvsFailWritesAfter()`).

### Interim screen

Until phase 5, the existing box screen pages over the PC (by species, six to a
ring), so nothing becomes unreachable.

### Tests

`pc_test`: put/take/full at 6; a power cut at every write keeps either the old
or the new group, never neither (A/B); a mount failure is not a format;
migration copies an 18-creature box once and only once; `ownedCount()` counts
the live pet; the PC index rebuilt at boot matches the files. `trade_test` /
`trade_ui_test` over the PC. `save_test` + `check_savefile.mjs` round-trip a PC.
Negative checks: format on failure, a single file instead of A/B, the backup
without its PC section.

---

## Phase 5 -- v3.62: Bill's PC screens

- **Entry:** the party screen's hub button opens BILL'S PC (it opens the box
  today).
- **Species grid:** the Pokedex grid and its region chooser, rim-paged
  (`uiRimTarget()`, `swipe_test`). A cell is COLOURED if the PC holds one,
  OUTLINED if only the party or the live pet does, a grey silhouette otherwise,
  with the PC count (1-6) in a corner.
- **Species ring:** tapping a cell opens the familiar ring of six for that
  species: view a creature's sheet, deposit from / withdraw to the party
  (through `pcPut`/`pcTake`), release (behind the existing confirmation).
- **A 7th of a species** (catch, prize, farewell, trade) opens the existing
  chooser over the six held plus the incoming one; one must go.
  A prize with no room stays greyed ("NO ROOM") as it does today.
- Tap targets stay `UI_TAP_FINGER`; `hit_test` measures them.
- Screenshots: add the PC grid and ring to `make_screens.sh`, the emulator's
  `--shot` names and the README, and reconcile all three lists.

Tests: `pc_ui_test` drives the grid, a deposit, a withdraw, a release and the
7th chooser through `onTap()`; `swipe_test` pages the grid; `flush_test` covers
both screens.

---

## Phase 6 -- v3.63: OWNED, GO TO #, missing-only

- The Pokedex shows OWNED alongside SEEN and REGISTERED (a per-species mark
  and an owned count per region on the chooser), all from `ownedCount()`.
- **GO TO #:** a number pad (the touch keyboard's digits) shared by the
  Pokedex and the PC; one function `dexJump(n)` turns a number into a page and
  a highlighted cell for either screen.
- **Missing only:** a filter on the PC grid that skips species already owned.

Tests: `dex_test`/`pc_ui_test` for the three states, `dexJump()` landing on the
right page in both screens, and the filter.

---

## Later: the post-game battle league

Fully trained opponent teams, like the games' Battle Tower / Frontier / PWT /
Battle Tree. Not designed; plan it with the owner after phase 3.

## Hardware checks the owner should do

| After | Check on a board |
|---|---|
| v3.58 | A hard gym feels fair; card numbers make sense on a young creature |
| v3.59 | Entering a code in the Memory game by finger is comfortable |
| v3.60 | Two boards: marks appear on the LAN pairing screen and in battle |
| v3.61 | **EXPORT first.** First boot formats the PC area once and migrates the box; reboot several times; pull the power mid-deposit; export and restore through the web page |
| v3.62 | The PC grid scrolls and taps well by finger |
