# The post-game Battle Zone, and the Hall -- PLAN ONLY (draft 4, 2026-11-04)

Agreed with the owner in conversation on 2026-11-04. Nothing here is built.
Where this and CLAUDE.md differ, this wins for the post-game. Open questions
are marked **OPEN** and are the owner's to answer before the phase that needs
them.

## 0. Ground rules (owner)

- **No new battle mechanics.** Everything works inside the engine that exists:
  singles, the current moves and effects, stages, ailments, PP, TRANSFORM and
  SKETCH. No inverse typing, no weather, no held items, no abilities.
- **Single player only.** No LAN ghosts. A rental-team mode is "later".
- **A money sink.** Money (steps, gym and League rematches) goes in; Battle
  Points (BP) come out of wins; BP buy prizes. BP is its own currency, separate
  from Game Corner coins.
- **No level shifting of the player's team.** The player brings what they have
  raised. Difficulty comes from the opponents' training, IVs and AI.

## 1. The zone

- **Gate: every region's champion beaten, on EITHER difficulty** (owner,
  draft 2: the badges are already needed to reach each champion; easy counts).
  A legitimate unlock therefore needs every sprite pack; the code covers
  anyone without them.
- **The secret code** opens it once from the Memory game, like the hall codes,
  with **no marks** of any kind, and records earned that way are not marked
  either (owner). Suggested sequence `1 1 3 3 1 1 3 3` (no clash with the five
  existing codes). Developers only, like every code.
- **Cheat toggles and the hidden SETTINGS both apply inside the zone** (owner:
  single player, and the player and creatures are already marked for using them).
- **Landing page**, laid out like a Game Corner: BP balance, the records, and a
  list of formats in the training-menu style -- TOURNAMENT, GAUNTLET, PRIZES
  (and later RENTAL). Finger-height rows, rim-paged.
- **A new tile on the tile axis** (owner, draft 2: LAN on the gym chooser is
  odd). GYM keeps only the region ladders. The new tile lists LAN BATTLE, LAN
  TRADE (today a button inside the LAN screen), BATTLE HALL and BATTLE ZONE
  (greyed "REQUIRES EVERY CHAMPION" until open, per the locked-never-hidden
  rule). **Named PLAZA** (owner, draft 3).
- **The tile axis is reordered** (owner, draft 4): admin to the LEFT of the
  pet, battles to the RIGHT, the most used nearest the pet:
  **`PLAYER . DEX . MART . PC . [PET] . EXPLORE . GYM . PLAZA`** (built in
  v4.01) -- the owner's order
  once the Mart is easy to swipe past (PC, then the Mart, then the Pokedex,
  going left). PC is today's PARTY tile (the party ring, its hub opens Bill's
  PC). **The Mart swipe is fixed (v4.00)**: the cause was the rim, as
  suspected and as the owner guessed -- the WHOLE ring outside `RIM_R0` 186 is
  the rim (not only the scrollbar's flank), so a right-to-left swipe begun
  near the right edge above or below the middle swept through a scroll's angle
  and paged. Flipping the scrollbar to the other side would not have helped,
  since the rim is every side. A rim drag that comes `RIM_LEAVE_PX` (28) in
  now puts back any pages it turned and resolves as a swipe (`rimswipe_test`).
  The PLAYER card stays leftmost, past the Pokedex (owner, 2026-11-04).
  `TILE[]`, `uiTileGo()`, `uiRimTarget()`, the pet screen's neighbour names,
  `swipe_test` and the tile dots all follow the one table. `TILE[]` and
  `uiTileGo()`/`uiRimTarget()` are the one-table edit the gesture notes promise;
  `swipe_test` and the tile dots follow.
- **Entry fees in Pokedollars**, BP for wins. Prices and pay set by the
  playthrough bot, so the zone drains money rather than creating it.
- **No items in zone battles.** Full HP heal between battles.
- **PP is the run's resource**: a zone run starts from FULL PP (each move's max,
  PP Ups counted -- they are a permanent upgrade), carries PP used from battle
  to battle through the whole tournament or gauntlet, and never touches the PP
  book. On leaving the run, the creatures are exactly as they entered (the real
  PP book was never written). Opponents' PP carries between their battles too.
- **SKETCH learned in the zone is not kept** (`sketchKeep()` must not run for a
  zone fight). TRANSFORM needs nothing: it is per battle already.
- **The player chooses their replacement after a faint, in EVERY mode** (owner,
  draft 2): gyms, the League, wild, the zone, the Hall (1v1, so nothing to
  pick) and LAN. Offline it is a picker on the same POKEMON screen the switch
  uses, opened when the faint line is dismissed, costing no turn.
  **LAN is moderate, not hard** (no one else runs this code, so a protocol
  bump is free): today the host decides both sides at resolution and ships
  `hostNext/guestNext` (v3.57). Instead the result says "pick pending" for a
  side that fainted; each side picks on its own screen; the guest's pick rides
  the action message that already carries a switch (`LINK_ACT_SWITCH_TO`),
  stamped with the turn and resent until the host's next result echoes it; the
  host latches its own pick; the next turn opens once both are in. A player
  who walks away is the existing idle timeout. `LINK_PROTO` bump,
  `lossy_test` (a lost pick) and `lanfight_test` (both faint in one turn, each
  picks a different creature) cover it.
- **Battle style: SHIFT outside the zone, SET in the zone and in LAN** (owner,
  draft 3; the games' own split -- shift in the story, set in the Battle
  Frontier and link battles). Shift: when the foe's creature faints, the screen
  names what it sends next and offers the player a free switch before it comes
  in; set: no free switch. A fixed rule, not a setting: a settings toggle with
  "LAN follows the host" was weighed and is not recommended -- it doubles the
  combinations to test, needs a wire field, and surprises the guest with the
  other board's rule. If a toggle is ever wanted, it fits as a hidden SETTING.
  One function answers it (`battleStyleShift()`, asked by the faint flow).
- **AI switching only in the zone, and only in its upper tiers** (later
  gauntlet rounds, later tournament rounds). Rules to make it sane: switch only
  when a teammate clearly does better against the creature in front of it, at
  most once every few turns, never into a creature that takes a big hit coming
  in. Tuned and measured with `league_sim`.

## 2. Tournament

- **16 contestants, 4 rounds, single elimination.** The player plus 15 drawn
  from EVERY existing opponent -- the 72 gym leaders and 45 League seats of the
  regions whose sprite packs are installed -- with seeding so the strongest
  (champions, then the Elite Four) usually meet the player late.
- **Fill with generated trainers** when fewer than 15 are available: a player
  who entered by code without every pack installed.
- **Opponents keep their own teams** at their roster levels, trained by
  `HARD_TRAINING` or stronger, smart AI, switching in the later rounds.
- **Other matches are fought in the background by the real engine**, AI
  against AI. **Not deterministic**: the engine rolls crits, damage spread,
  accuracy and ailments, so the same pairing can go either way. (A seeded RNG
  could make a bracket replayable; not needed unless the owner wants it.)
- **Records**: tournaments won (player record, saved).
- Opponents' movesets are what they are today: derived from species and level
  by `relearnFromLevel()` -- the same every time, not random -- not the games'
  hand-picked sets (the rosters store species and level only). Real movesets
  from the decompilations would be a separate, larger job.

## 3. Gauntlet

- **Challengers in succession until the player loses**, generated from the
  stock trainer avatars (19 classes) with names, teams drawn from the art-ful
  species pool under the run's rules.
- **Rules**: one type, a level cap, an evolution tier (unevolved, middle,
  final, no legendaries, a base-stat cap), team size 1-6. The challenger obeys
  the same rules. Inverse typing is OUT (owner).
- **Daily challenge**: random rules from the day hash; **double BP only for the
  first successful run of that day's challenge**. Custom rules: 1x BP.
  **The 2x belongs to the day the run STARTED** (owner, draft 2): a daily run
  begun Monday pays 2x until it ends, even on Wednesday; the next run started
  gets the day's own challenge then. "Successful" means
  **the first daily run that beats at least one challenger claims the day's 2x**
  (a run that loses its first battle leaves the bonus unclaimed), and that run
  keeps it for its whole life (owner agreed, draft 3).
- **Pacing (owner)**: no wall where a player always wins rounds 1-2 and always
  loses round 3, and no 50-win cakewalk. A run should take **up to ~30 minutes**
  before a loss. So: the challenger's strength rises a little every round
  (training, then IVs, then the smart AI, then switching), tuned so a well-built
  team's chance of losing rises gradually -- a run ends at varied points, not a
  fixed one. Carried PP is a second, natural pressure: it runs low over a long
  run, and PP Ups matter. Both tuned with `league_sim` against the OPTIMAL and
  CASUAL teams already defined there.
- **Records**: best streak (trainers beaten), all-time and today's daily.
- **Saving a run (owner wants it: "mom says put it away")**. Proposed:
  - Saved **between battles only**, after each win: round number (the next
    challenger's strength), the rules, the daily flag, the RNG seed for the next
    challenger, the team (creature IDs, in order) and each slot's PP used.
    About 60 bytes in its own NVS key, in `SAVE_FIELDS`.
  - **Turning the screen off mid-battle is safe**: the board keeps running and
    the fight is still there when it comes back on.
  - **A reboot or power cut mid-battle counts as the loss** (as in the games),
    otherwise pulling the power is a free retry of any battle about to be lost.
  - A saved run ends if any of its creatures is no longer held (released,
    traded, deposited where it cannot be found) -- checked on resume.
  - **A saved run lasts indefinitely, and only one gauntlet run exists at a
    time** (owner, draft 2): starting another needs the current one lost or
    quit (QUIT asks twice, like the League's GIVE UP). The save holds the day
    it started and whether it carries the 2x.
  - **The tournament saves between rounds the same way**, one at a time, with
    the same mid-battle-reboot-is-a-loss rule (owner agreed, draft 3).

## 4. The Hall (moved out of the post-game, owner)

- **Part of normal play, not the zone.** The LIVE pet alone, 1 on 1, one battle
  against each of the 18 types, like Mortal Kombat's challenge ladder.
- **Starts below the pet** -- the first rung's foe has less training, lower IVs
  and the simple AI -- and each rung raises training, IVs or the AI.
- **Changing the live pet ends the run.** Saved between rungs (rung, types
  done, PP used), like the gauntlet.
- **It lives on the new tile** (BATTLE HALL, beside LAN; owner, draft 2).
- **Pay: a little money per rung**, like a route trainer in the games, less
  than a gym leader. Proposed: `trainerPrize()`'s Gen 3 rule (4 x the foe's
  level x a class value) with a route-trainer class (~8, against a leader's 25),
  rising with the rung. Tuned with the playthrough bot.
- **Clearing all 18 marks the creature for good**: an icon on its card that
  travels with it (banked, PC, traded). It fits without growing the record --
  a new bit in `PartyMon::flags` (`PMON_HALL` 0x04; six bits are free) and the
  pet checkpoint's flags byte, through `monFlags()`/`setMonFlags()` like the
  cheat mark; `TRADE_PROTO` bump so `tradeMonValid()` accepts it, and the
  `LinkMon` flags so a LAN rival sees it.
- **Each battle costs energy, and a run spans sittings** (owner, draft 3). The
  run (rung, types done) is saved between battles; a tired pet rests and
  continues. **The Hall uses the real PP book**, as a gym fight does, so a nap
  between sittings refills it (owner, draft 4); the zone's separate PP rule is
  for the zone only.

## 5. Prize counter

- **Every existing item** (Master Ball included -- never sold at the Mart), PP Up,
  vitamins, stones, the Linking Cord, Ether, the TMs the Mart has not offered.
- **New items with existing mechanics only**: PP Max (three PP Ups at once),
  a Bottle Cap (one IV to 31, the games' Hyper Training).
- **Legendary signature moves as BP TMs**, only those the engine already
  expresses: e.g. ROAR OF TIME / ETERNABEAM (recharge), SHADOW FORCE (charge,
  hidden), SEARING SHOT / BLUE FLARE / BOLT STRIKE (an ailment chance), LAND'S
  WRATH (plain damage), plus the starter ultimates BLAST BURN / HYDRO CANNON /
  FRENZY PLANT. **There are 14 free move indices** (`MOVE_COUNT` 242, a move is
  one byte), APPEND-ONLY. A signature move is learnable only by its species.
  SPACIAL REND / AEROBLAST would be plain damage (no high-crit support).
- **Cosmetic**: Frontier-symbol style marks on the player card for the gauntlet
  bosses and the first tournament win.

## 6. Build order (proposed)

0. **DONE (v4.01)** The tile axis: the new order, and the PLAZA tile (GYM keeps the ladders;
   LAN BATTLE / LAN TRADE / BATTLE HALL (stub) / BATTLE ZONE (locked) move to
   it).
1. **DONE (v4.02)** The player picks their replacement after a faint, everywhere including LAN
   (protocol bump); the AI picks its replacement by matchup.
2. **DONE (v4.03)** The zone shell: gate, code, landing page, BP, entry fees, prize counter,
   records, the zone PP ledger, no items, SKETCH not kept.
3. **DONE (v4.05)** AI switching (upper tiers only). NOT yet measured with `league_sim`: the rule's numbers are placeholders.
4. **DONE (v4.04)** The gauntlet: the team generator, rules, scaling, daily, saving.
5. **DONE (v4.06)** The tournament: the draw, seeding, fill, background matches, its PP.
6. **DONE (v4.07)** The Hall (normal play).
7. **DONE (v4.08)** The new prizes: signature moves (ten, BP TMs, their species only, never
   an opponent's), PP Max, Bottle Cap, five counter TMs a day, the zone's symbols.
   **Tuned (v4.09)** to the owner's intent of 2026-11-05: PP MAX / BOTTLE CAP = one
   ~40-win gauntlet run or 3-4 casual runs, the Master Ball 6-7 of them (about a
   week), walking supplementary, gym rematches an income source, money worth
   spending but never a wall, and PP the gauntlet's barrier that PP UPs answer.
   Measurements and prices: `docs/zone-economy.md`.
8. Later: rental teams.
