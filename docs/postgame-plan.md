# The post-game Battle Zone, and the Hall -- PLAN ONLY (draft 1, 2026-11-04)

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

- **Gate: every gym badge of every region** (owner: a Kanto player has the
  rest of the game to play first). **OPEN:** on either difficulty, or hard?
  Every League seat too, or badges only?
- **The secret code** opens it once from the Memory game, like the hall codes,
  with **no marks** of any kind, and records earned that way are not marked
  either (owner). Suggested sequence `1 1 3 3 1 1 3 3` (no clash with the five
  existing codes). Developers only, like every code.
- **Cheat toggles may apply inside the zone** (owner: single player, and the
  player and creatures are already marked for using them). Hidden SETTINGS:
  **OPEN** -- same answer?
- **Landing page**, laid out like a Game Corner: BP balance, the records, and a
  list of formats in the training-menu style -- TOURNAMENT, GAUNTLET, PRIZES
  (and later RENTAL). Finger-height rows, rim-paged. Entered from the gym
  tile's region chooser as a row like LAN.
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
- **The player chooses their replacement after a faint** -- needed before any
  AI switches. **OPEN:** everywhere (gyms, wild, League too), or the zone only?
  LAN would need a protocol change (today the host decides both sides,
  v3.57); suggest leaving LAN as it is.
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
  - **OPEN**: a resume window (a run saved for a week?), and whether a saved run
    blocks starting another.

## 4. The Hall (moved out of the post-game, owner)

- **Part of normal play, not the zone.** The LIVE pet alone, 1 on 1, one battle
  against each of the 18 types, like Mortal Kombat's challenge ladder.
- **Starts below the pet** -- the first rung's foe has less training, lower IVs
  and the simple AI -- and each rung raises training, IVs or the AI.
- **Changing the live pet ends the run.** Saved between rungs (rung, types
  done, PP used), like the gauntlet.
- **OPEN**: where it lives (a row on the gym chooser? the training list?), what
  it pays (money? bond? a medal for all 18?), and whether each battle costs the
  pet energy (proposed: yes, as a gym fight does -- a tired pet naps and
  continues).

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

1. The player picks their replacement after a faint; the AI picks its
   replacement by matchup.
2. The zone shell: gate, code, landing page, BP, entry fees, prize counter,
   records, the zone PP ledger, no items, SKETCH not kept.
3. AI switching (upper tiers only), measured with `league_sim`.
4. The gauntlet: the team generator, rules, scaling, daily, saving.
5. The tournament: the draw, seeding, fill, background matches, its PP.
6. The Hall (normal play).
7. The new prizes: signature moves, PP Max, Bottle Cap.
8. Later: rental teams.
