# TamaPoke — where things stand

Written 2026-08-20, updated through 2026-08-23, so work could resume after a
restart. Read this with `CLAUDE.md`, which is the permanent knowledge and is
**authoritative wherever the two disagree**.

> **§ 1g is the newest (2026-10-30, v3.84) and describes the repo as it is
> now. Read it first, then § 1f (v3.74), whose working rules still hold.** § 1e (2026-10-20) is the state before it and still holds
> the cloud-container build setup. § 1d (2026-10-12) is the state before it; everything below
> that is history, kept for the reasoning.
>
> **§ 1 below is current as of v3.22 (2026-09-08). Sections 0 and 2 to 4c are an
> August snapshot kept for the reasoning in them, and their branch names, version
> numbers and "not merged" notes are long superseded** — `feat/retire-and-release`
> landed, the dex went past Unova to 1025, and the three "failing tests" were
> fixed a dozen releases ago. § 5's pitfalls are the part that has stayed true.
> Do not read anything outside § 1 as a description of the repo today.
>
> **§ 1a is a day newer than § 1** (2026-09-10) and takes priority over it for
> anything about LAN, the save/checkpoint path, or hardware state — the first
> real two-board session happened in between and changed several of § 1's
> "not verified on hardware" answers to "verified, and broken, and now fixed."
>
> **§ 1b is later the same day** (2026-09-10) and closes the "why a checkpoint
> write fails at all" question § 1a left open — plus two more save-path bugs
> found chasing it.
>
> **§ 1c is 2026-09-11** and is unrelated to save/LAN: the Poke Mart shipped
> (steps -> Pokedollars). Its pedometer input needed a real hardware detour —
> the chip's own step-counting engine does not work, confirmed unfixable
> through several honest attempts — but a software detector on raw
> accelerometer samples does, confirmed on real hardware with a calibrated
> threshold, after a second, unrelated bug (`getDataReady()`) nearly hid that
> too.

---

## 1g. START HERE -- handoff as of v3.90 (2026-10-30)

This section is the description of the repo today; § 1f below (v3.74) is the
stretch before it and its "How the owner works", decisions, lessons and known
issues all STILL HOLD -- read them too. `CLAUDE.md` wins wherever they differ;
its § TODO "Done: ..." entries hold every version below in full, newest first.

### State

| | |
|---|---|
| Version | **3.90** in `TamaPoke.ino` and the README badge. `web/manifest.json` still says **3.74**: the installer has NOT been rebuilt for v3.75-v3.90 |
| `main` | still at v3.74 (`6abaf47`). Nothing since is merged; the installer serves v3.74 |
| Working branch | `ccr-c2040abf-fdum6b`, v3.75-v3.90 on top of `main` (one commit per version, plus tooling) |
| On a board | **None of v3.75-v3.90 has run on a board.** All emulator-only |
| Tests | 97 `*_test.cpp` suites (all pass at v3.90) (all clean under ASan + UBSan at v3.85). `run.sh` recompiles the whole sketch per suite (over an hour in a cloud container); compiling the shared sources once and the suites in parallel takes minutes |
| Formats | `LINK_PROTO` 9, **`TRADE_PROTO` 3** (v3.83), `MOVE_COUNT` 241, pet checkpoint **`PET_CORE_VERSION` 6** (v3.83), player checkpoint **`PLAYER_VERSION` 10** (v3.77), `SAVE_VERSION` 2, `SET_COUNT` 13 |
| Plan | `docs/explore-specialty-build-plan.md` revision 8 -- everything the owner decided in it is built, except what "Left" says below |

### What this stretch built (v3.75-v3.84), one line each

- **v3.75** Kanto's Safari Zone as FireRed plays it (no team, 30 Safari Balls,
  BAIT/ROCK, FireRed's catch and flee); Fuchsia's water to Routes 12-15.
- **v3.76** Hoenn's Safari (Emerald: GO NEAR, POKEBLOCK) and Sinnoh's Great
  Marsh (Platinum: BAIT/MUD, per-species flee rates).
- **v3.77** Fixed one-offs: two Snorlax, two Electrode, Sudowoodo, Friday
  Lapras, the red Gyarados (always shiny), Kecleon. Met once for certain, then
  rare until caught; stay in eggs. Row markers now mean ONLY a certain meeting
  is due (`* AREA *` legendary, `! AREA !` one-off).
- **v3.78** Swarms, read from Crystal, Emerald and Platinum; one a day per
  region, shown on the area row.
- **v3.79** Broken chains fixed (second evolution 10 levels after the first:
  Ursaluna 40, Mamoswine 43, Kingambit 62, Azumarill 35, Blissey 40, Hydrapple
  40); gym prize money (Gen 3's formula, easy half, rematch half again).
- **v3.80** Gender from the creature's ID, forced where the games force it;
  Burmy/Combee/Salandit by gender; symbols on the card and sheet.
- **v3.81** Evolution stones as items ($2,100; Black Augurite $3,000), no level
  route for stone evolutions; Eevee by stones and bond (40 + 48 h age).
- **v3.82** Branches with no items: Tyrogue by stats, Wurmple/Cosmoem split by
  ID (Cosmoem forced to the unowned legendary), Applin/Charcadet choose at
  their level, Shedinja for a spare Poke Ball.
- **v3.83** Meltan -> Melmetal by 10,000 steps walked as the live pet (saved
  on the creature, travels in trades: `TRADE_PROTO` 3).
- **v3.84** Stones drop in the wild only in the areas the four decompiled
  games leave them (37 areas, ~1 win in 10 there); Unova onward, the Ice Stone
  and the Black Augurite stay Mart-only.
- **v3.85** Stones in every region (Unova onward from Bulbapedia's item pages,
  NOT VERIFIED, the parser checked against the decomps), the Ice Stone found
  too; one win in 25 instead of ~1 in 10; Safari areas' stones come with a catch.
- **v3.86** The pre-merge audit (CLAUDE.md "Done: the pre-merge audit"): a v3.74
  save upgrades intact; EXPORT now saves first, so a backup holds unsaved
  progress (that bug is on `main`); the emulator keeps IMPORT/WIPE across its
  restart; `evoaudit_test`; `tools/emu/odds_report.cpp`.
- **v3.87** A move offer no longer outlives its creature (on `main` too). Found
  by the playthrough bot, `tools/emu/playsim.cpp` (CLAUDE.md "Done: the
  simulated playthrough"). Its report is a private artifact,
  https://claude.ai/artifact/K6zqdJr6zWcWUDEjQykzT7: nine three-year runs, all
  easy badges in 1-9 weeks, 90% of the 945-species goal in 7-18 months, 11-31
  left at three years (mostly item/trade evolutions of stored creatures, which
  the bot does not plan); hard Elite Four seats beat untrained teams every time.

### Left from the plan, and open questions for the owner

- **Deferred by the owner**: Pokemon Tower's ghosts, the Bug-Catching Contest,
  the Ruins of Alph puzzle, the Friend Safari.
- **Offered, not answered**: a second Kecleon one-off on Route 119 (Emerald
  has seven Kecleon; v3.77 placed the Route 120 one whose script sets the
  level).
- **v3.88** Wrap-safe timers (`timers.h`, every deadline); the Pokedex
  footer's garbage text (an unfilled buffer since v3.63, on `main`); a BACK
  button on every Pokedex screen, one level up; GO TO # moved to the header.
- **v3.89** The Pokedex fills a species once it has been yours (`dexObtained()`:
  registered, held now, or a legendary/one-off caught); the green dot is
  REGISTERED. No new save data.
- **v3.90** Selling at the Mart (BUY | SELL on the title pill, half price,
  `martSellOne()`); wild drops 1 + 25% easy / 50% hard (were 1-2 / 2-3).

### NEXT, agreed with the owner 2026-10-30, in this order

1. ~~Selling, and fewer drops~~ **done in v3.90.**
2. **New items** (owner's answers): vitamins usable on a STORED creature, +10,
   $9,800 (today +5, live pet only -- worse than the free training games, which
   pay up to 18 a session for energy and food); X Attack/Defense/Sp. Atk/Sp.
   Def/Speed at +2 stages (Gen 7+) and the games' prices, usable in gym fights
   (the bag is already open there, closed in LAN); PP Up stored ON THE CREATURE
   (a spare `PartyMon::reserved` byte -> `TRADE_PROTO`/`LINK_PROTO` bumps).
   NO separate special vitamins: special stats ride on Attack/Defense training.
3. **Rerun the playthrough with realistic walking**: the board counts steps
   only while powered (screen off still counts), battery life is unmeasured,
   and children leave it home for school and sports. Profiles: school child
   (~300 on school days, ~3,000 weekend days), weekend walker, daily adult
   (~4,000, upper bound). Teach the bot vitamins, X items, PP Ups and selling.
   Owner may measure one board's battery (screen off) and a week of steps
   (`STATS`) first. Then set prices.
4. **Possibly a post-game battle league** (entry fees are a money sink).
5. **The guide**: brief in `docs/guide-notes.md`.
- **From the playthrough, the owner's call**: Slowking (Linking Cord on a
  Slowpoke) and Kleavor (Black Augurite before any cord) are invisible to a
  player who does not know; the wallet caps at $999,999 by day 154-518; hard
  mode's trained Elite Four (the v3.58 re-measure).
- **Merge and installer**: v3.75-v3.87 wait for the owner's "push to main and
  update the installer" (`bash tools/build_web.sh` after the last edit, then
  fast-forward `main`). Tagging is still held on Actions minutes.
- **Board checks wanted first**: a Safari encounter by hand (the four cells,
  out of balls), the evolve-branch dialog (Applin) and the Shedinja hand-off,
  the gender symbols' colours on the panel, a trade between a v3.83+ board and
  an older one (it must refuse), and the save carrying over from v3.74
  (player checkpoint 9 -> 10, pet checkpoint 5 -> 6).

### Lessons from this stretch

- **The data surprises.** Crystal's Remoraid swarm never happens (Route 44
  fishes the POND group); Emerald places seven Kecleon but only Route 120's
  script sets a level; Kanto's only ground Leaf Stones are in the Safari Zone,
  where nothing is won. Read the source before writing the rule, and let the
  generator refuse what it cannot place (`stone_areas()`, `fixed_encounters()`).
- **Check a scraped source against a known one.** Bulbapedia's stone tables
  agree with the decomps for four regions, which is what lets them stand for the
  other five; the check found one entry that is really a gift (Mossdeep).
- **A test's earlier decline leaks into the next check.** branch_test's Nincada
  would not evolve because a KEEP had set the declined level; clear pending
  offers and raise the level past it.
- **Evolution has one exit now: `evolveInto()`** (level, stone, branch, trade);
  anything an evolution must also do -- legend caught bits, Shedinja, the save
  -- goes there, never into one caller.

---

## 1f. START HERE -- handoff as of v3.74 (2026-10-29)

This section replaces 1e as the description of the repo today. `CLAUDE.md` is
still the permanent knowledge and wins wherever the two disagree; its § TODO
"Done: ..." entries hold the full detail of every version named below, newest
first. Read § "Traps that have caught us more than once" there before touching
a rule, a dex number, a saved table or a paged screen.

### State

| | |
|---|---|
| Version | **3.74** in `TamaPoke.ino`, the README badge and `web/manifest.json` |
| `main` | at v3.74 (`6abaf47`), fast-forward from the working branch. The installer page (https://codester00.github.io/TamaPokeLAN/web/) serves `web/` from `main`, so it serves **v3.74** |
| Working branch | `ccr-151c604c-659yz2` -- everything since v3.39 was built here. It equals `main` plus this handoff (docs only) |
| Tags | The last published TAG is believed to be **v3.28** (check `git ls-remote --tags origin`). Tagging has been held all along: the owner believes the account's free Actions minutes run out, and `publish-release.yml` needs them. Release notes exist for every version through `docs/release-notes/v3.74.md`, so any of them can be tagged off `main` |
| On a board | The owner plays `main` builds on real boards: Bill's PC (v3.66), the halls (v3.67), the Kadabra farewell/moves (v3.72) and the real slots (v3.73-v3.74) were all tried on hardware. Much else is emulator-only -- see "Known issues" |
| Tests | 82 `*_test.cpp` suites under `tools/emu/tests/`, plus `node tools/check_savefile.mjs`. `bash tools/emu/tests/run.sh [name]` runs the real `arduino-cli` compile first, then the suites |
| Formats | `LINK_PROTO` 9, `TRADE_PROTO` 2, `MOVE_COUNT` 241 (SKETCH is 240), pet checkpoint v5, player checkpoint `PLAYER_VERSION` 9, backup `SAVE_VERSION` 2 (Bill's PC follows the fields), `SET_COUNT` 13 |
| Toolchain | esp32 core 3.3.0, GFX Library 1.6.9, **SensorLib 0.4.1** (0.5.0 boot-loops the board; `sensorlib_guard.h` refuses it), XPowersLib 0.3.4. Cloud-container setup: § 1e below. Build through a `TamaPoke` symlink (arduino-cli wants the folder named after the `.ino`) |

### How the owner works (follow these)

- **Develop on the branch; push to `main` only when the owner says so.** They
  usually say "push to main and update the installer": that is `bash
  tools/build_web.sh` AFTER the last source edit, commit, push the branch, then
  fast-forward `main` (`git fetch origin main && git merge-base --is-ancestor
  origin/main HEAD && git push origin HEAD:main`). Never commit on `main`.
- **"Only run required checks"** means the suites the change touches plus
  i18n/swipe where relevant -- not the full run. Still negative-check every new
  guard: break it on purpose and watch the test fail.
- **One version per change**: bump `FW_VERSION` and the README badge together,
  write `docs/release-notes/vX.Y.md` FOR PLAYERS (what changed, what it looked
  like broken, whether to do anything; no file or function names), and add a
  "Done: ..." entry at the top of CLAUDE.md § TODO.
- **Secrets stay secret.** The secret codes, hidden SETTINGS, CHEATS, the
  Super Rod / water-mode gates, Surfing Pikachu's condition, the 8-hour grace
  window, the legendary safety net and the farming lockout are DEVELOPERS ONLY:
  CLAUDE.md (and for some, the README manual -- see each entry), never the
  release notes or the installer page. v3.74's notes, for instance, call it a
  "small maintenance update".
- **Ask before design calls; make the small ones.** The owner decides game
  feel (odds, prices, what is hidden, what the games' rule is). When a choice is
  theirs, lay out the options with a recommendation and wait.
- **Port from the games, not from memory.** The pret disassemblies (pokered,
  pokecrystal, pokeemerald, pokeplatinum, pokeyellow) are the source of truth;
  PokeAPI's CSV dump on GitHub for Gen 5+. The owner wants things to feel like
  the originals.
- Commit trailers: see the session's attribution instructions. No model names
  in anything pushed.

### What this stretch built (v3.57-v3.74), one line each

Full entries in CLAUDE.md § TODO.

- **v3.57** LAN fixes from the first two-board fights (replacement decided and
  shipped by the host, results after end-of-turn ticks, next STANDING creature,
  Struggle for moveless creatures, all five on the POKEMON screen, fixed scene,
  a lost END no longer strands the guest); a gym win plays its last turn.
- **v3.58** Training scales with level in battle (`stats.h`, one formula).
- **v3.59** Secret codes in the Memory game and the hidden SETTINGS (permanent
  MODIFIED RULES mark). **v3.60** CHEATS, trainer and creature marks, marks
  over LAN.
- **v3.61** Bill's PC storage on the `ffat` partition (six of each species,
  A/B records, migration from the box, backup v2). **v3.62+3.63** PC screens,
  OWNED, GO TO #, missing-only filter, trade room check.
- **v3.64** HP drains with its own message line; LAN guest reads the host's
  narration (`LinkBeat`); the CAUGHT ball on wild foes.
- **v3.65** Emerald's indoor battle backgrounds for trainer/gym/LAN fights.
- **v3.66** Bill's PC on a real board (format a partition that never held a
  PC; the old box stands in while the PC is down).
- **v3.67** The halls after a board session: card flip's result line, Surfing
  Pikachu rides right to left and is playable by hand.
- **v3.68** Backups arrive whole (console replies wait for the host); the
  installer assembles one backup from several damaged reads.
- **v3.69** SKETCH: Smeargle copies the foe's last move for good, learned by
  level as in the games.
- **v3.70** The team picker sets the battle order (and remembers it).
- **v3.71** The battle bag pages along the rim; a faint drops with its line.
- **v3.72** Trade-only evolvers (Kadabra, Haunter...) get their farewell; the
  level bar reads MAX LEVEL at 100; an evolution teaches the new form's passed
  moves (`Pet::learnRange()` is THE way a level-up move arrives).
- **v3.73** The real slot machines: Red's engine at Celadon, Crystal's at
  Goldenrod, real payouts and lucky odds; Red's reels are 15 stops; the "fair
  slots" setting retired (bit 9 reserved).
- **v3.74** Hidden setting LUCKY SLOTS (new bit 12): every sitting is at a lucky
  machine; suspended in LAN like every setting.

### Decisions the owner made this stretch (do not reverse)

- The MOVES picker stays a free Move Reminder alongside level-up offers, as in
  Scarlet/Violet; a hidden "classic moves" setting was offered and declined.
- Slots run the games' real engines with their real payouts (the originals pay
  out more than they take, in streaks) and the games' prize prices. No rigging.
- A retired setting's bit is never reused (a board with it on would silently
  get the new rule); new settings append.
- Smeargle holds one SKETCH at a time and learns more by level (the games'
  rule), not four SKETCH slots.
- The team picker's order is the battle order; remembered by creature ID, RAM
  only.
- Dratini prizes keep each game's own price and level (Celadon L18 2,800,
  Goldenrod L10 2,100).
- Earlier, still binding: the 8-hour window and nap endings have no indicator;
  the rod and water gates stay secret; no remote storage for backups; LAN
  fights are uncapped and never black out; the guest takes the next standing
  creature (no choice) and has no bag.

### Lessons from this stretch (add to the traps when they recur)

- **A test can pass because of the layout it happened to use.** The picker's
  scrollbar check passed with no scrollbar because the FIGHT button's corner
  reaches the ring; it now reads x 420+ only. The bag's page clamp test was
  vacuous because opening the bag resets the page -- the clamp and its test
  were removed rather than kept as theatre.
- **A "never pays twice" check needs proof it could.** Enumeration showed no
  stop set on either machine lines up two lines at once, so the one-line rule
  could not be tested by a double line; the test pins that fact instead and the
  15-stop check catches the old 18-stop reels.
- **Rules that only checked AT a level miss everything a late tap skips.**
  Evolution waits for the player's tap and care, so `evolve()` must teach the
  whole range passed (`learnRange()`), not just the current level's gate.
- **Ask the RULE, not the table.** The farewell asked `evolvesTo == 0`, so every
  pure trade evolver waited forever; `evolutionAhead()` asks `evoInfo()`.
- **Fixtures inherit state.** `hatch()` keeps the age it finds, so tests set
  `ageMinutes = 0` first; a poisoned fixture died to the end-of-turn tick.
- **The emulator's generated `proto.h` sits above every type**, so a sketch
  helper whose signature names a type declared later (ItemKey, AvatarArt) must
  be `static` or the emulator build breaks while the firmware is fine.
- **Measured odds come from the emulator's 16-bit LCG**: check them against an
  independent simulation of the disassembly (both were done for the slots) and
  give tests bands, not exact figures.
- **A Python `str.replace` script can half-apply.** A failed assert after some
  files were written leaves the tree half-edited -- check `git status` before
  re-running.

### Known issues and open items

Not fixed, in rough priority:

1. **Whole-screen black flicker** (owner, sound off): seen while swiping,
   scrolling, worst in Berry Catch. Ruled out from code: blocking `flush()`,
   no black fills, brightness only on idle, no touch-controller reset. Next:
   on a board, a finger held still on a static screen vs. Berry Catch watched
   without touching.
2. **A lost REMATCH strands the guest** (LAN): `sendRematch()` is never resent;
   a kept packet needs a rematch counter so a stale one cannot rearm turn 0.
3. **`lastMove` lasts the whole fight** per creature (the games clear it on a
   switch), so SKETCH can copy a move from before a switch. Transform (Ditto) is
   absent.
4. **The gym ladder's win-rate table predates v3.58** (training scales with
   level); the re-measure (`tools/emu/ladder_sim.cpp`) waits on the owner. Hard
   mode solo drops to ~0% for a fully trained Charizard.
5. **Clock drift** (~1 min in 20 min; 3 h a week, possibly a battery change):
   on hold by the owner. The plan is CLAUDE.md § TODO item B (fix the 30 s
   cache and `applyClock()` seconds first, then measure).
6. **Unverified on hardware**: `lanLeave()` against the original
   guest-quits-mid-battle repro; the PWR long-press flush lead time; the music
   volume against the effects; Surfing Pikachu's physics on a real tilt sensor;
   the 24-48 h `HEALTH` soak; `pcfs_ffat.cpp`'s never-format-over-a-filesystem
   rule (the emulator uses `pcfs_host.cpp`).
7. ESCAVALIER, ACCELGOR, TOUCANNON have art but no route (each needs an
   art-less creature first).
8. Owner's calls, not bugs: `web/editions.json` points release lists at
   `squithy/TamaPokeLAN` and links at `reallyjustsomeguy/TamaPokeLANExp`;
   `tools/backs/` has no established licence (CREDITS.md); the avatars are
   unlicensed Nintendo art by the owner's choice.
9. Mart TM prices and the slots' feel are the owner's to retune after play.
   Latest owner feedback on v3.74 slots: "felt more like the original",
   bet 3 drained 56 coins with +15, +15, +8 -- consistent with streaky payouts.

### Proposed next work

- **Explore specialty zones** -- `docs/explore-specialty-plan.md` (proposed
  2026-10-29, nothing decided): a Safari engine for Kanto's Safari Zone,
  Hoenn's Safari Zone + Extension and Sinnoh's Great Marsh (M; three questions
  for the owner in the doc), the Bug-Catching Contest, Pokemon Tower's
  unidentified ghosts (S), fixed encounters such as Snorlax and Friday Lapras
  (S-M), the Ruins of Alph puzzle, daily swarms, a Friend Safari from LAN
  rivals. Recommended order is in the doc. Wait for the owner's answers before
  building.
- From CLAUDE.md's plan list, still unbuilt: the post-game battle league (not
  designed), and tagging a release when Actions minutes allow.

---

## 1e. Where things stood (2026-10-20)

| | |
|---|---|
| Published firmware | **v3.28** is still the last TAG. `main` is at **v3.38** and the installer page (https://codester00.github.io/TamaPokeLAN/web/) serves it as the untagged current build |
| Tagging | Held: the owner believes the free GitHub Actions minutes are spent for the month, and the release workflow needs them (the tag push itself does not). Retry after the reset |
| Repo version | **3.38** in `TamaPoke.ino`, the README badge and `web/manifest.json` (3.35 and 3.36 = 3.34 + Tilt Maze steering fixes; 3.37 slows Boulder Push and explains zero gains; 3.38 adds the trainer-class avatars and the blackout screen, on `main` since 2026-10-21; notes in `docs/release-notes/`) |
| Release notes | `docs/release-notes/v3.34.md` (everything since v3.28, for players) through `v3.38.md` are written, so tagging v3.34 or v3.38 off `main` would pass `check_release.py` |
| Next work | The Explore rework, `docs/explore-plan.md` (confirmed, revision 3). Kanto's areas (**v3.39**), the legendaries + Johto's areas (**v3.40**), the hidden best-rod gate (**v3.41**), Hoenn's areas + legendaries (**v3.42**) Sinnoh's (**v3.43**) Unova's (**v3.44**, from PokeAPI) and Kalos's with the NO VALID SPRITES catch-all (**v3.45**) Alola's (**v3.46**, Sun/Moon) and Galar's (**v3.47**, with daily weather in **v3.48**) and Paldea's (**v3.49**) -- every region -- are on branch `ccr-151c604c-659yz2`: not merged, not on a board, installer not rebuilt (`web/manifest.json` still says 3.38 -- `build_web.sh` moves it at release). Every region has areas now; next is a board. Clock drift is on hold by the owner's call |
| Tests | 56 suites, all pass (`sprite_test` skips without sprites), plus `check_savefile.mjs` |
| Firmware | esp32 core 3.3.0, GFX Library 1.6.9, **SensorLib 0.4.1**, XPowersLib 0.3.4 |
| Owner's boards | several; two had **v3.33**, and **trading worked between real boards**. Nothing from v3.34 to v3.38 has been on a board yet |

**Building in a Claude Code cloud container (2026-10-21).** The proxy blocks
`downloads.arduino.cc` and `espressif.github.io`, so `arduino-cli` cannot fetch
its indexes the normal way, but GitHub is reachable and that is enough:
`arduino-cli` 1.1.1 from its GitHub release; the ESP32 index from
`raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`,
saved locally with core 3.3.0's `dfu-util`/gdb/openocd dependencies removed
(upload-only tools) and given as a `file://` additional URL; the three
libraries cloned at their pinned versions into `~/Arduino/libraries`
(XPowersLib 0.3.4 is untagged -- commit `fea4fbb`); and Arduino's `ctags`
built from `github.com/arduino/ctags` (rename its `__unused__`/`__printf__`
macros to build against modern glibc) with
`runtime.tools.ctags.path=...` in the core's `platform.local.txt`. SDL2 is
`apt-get install libsdl2-dev`. Build through a `TamaPoke` symlink. With that,
`tools/emu/tests/run.sh` runs the real firmware compile first, as intended.

**What landed in v3.34** (all in CLAUDE.md § TODO "Done: training games..."):

- The LAN button (was LAN BATTLE) opens HOST BATTLE / JOIN BATTLE / TRADE.
- **Ten training games** in a paged list (`TRAIN_GAMES`). Seven are new: four
  hybrids that split points across two stats through `Pet::trainSplit()`, and
  three joy/bond games through `Pet::playJoyGame()`. Petting wears off
  (10/8/6/4/2/0) until a joy/bond game refreshes it.
- **Decay rework.** One drain (`Pet::drainMinute()`) for live and offline time;
  awake rates ~10/h; an **8-hour grace window** (`GRACE_MIN`) with needs
  floored at 15 and no penalties. The window restarts ONLY on full care -- all
  four needs at `CARE_FULL_PCT` 90 (`Pet::graceCheck()`).
- **Daytime naps** (`SLEEP_NAP`, 06:00-18:00): a bed by hand wakes itself at
  full energy; it never changes how much of the window is left unless it wakes
  fully cared for.
- `StrId` widened to `uint16_t` -- the string table passed 256 entries.

**Things an updater should know (also in the README's "Updating" section and
the release notes):**

- The save carries over. The pet checkpoint is now **version 5**: the 3 bytes
  v4 reserved hold `unattended` (u16) and `petStep` (u8). A v3.33 board reading
  a v5 save just ignores them, so a downgrade to 3.33 is safe. The one oddity:
  a creature napping when downgraded stays asleep until someone presses the
  light, because 3.33 does not know sleep state 3.
- An existing creature starts v3.34 with a fresh 8-hour window (`unattended`
  reads 0 from a v4 record).
- **Going below v3.31 still empties the party and box** (the record grew).
- `tools/debugger/tpsave.py` decodes the v5 fields and names sleep state 3.

**Verify on a board first, in this order:**

1. The save survives the 3.33 -> 3.34 install (`EXPORT` first).
2. **Tilt Maze**: the sensor's axes turned out to be swapped relative to the
   screen (a quarter-turn), not just reversed; v3.36's `tiltToScreen()` maps
   them from the owner's measurements. Confirm all four directions on a board.
3. Touch feel of the new games: Rock Smash's timing windows (90/180 ms), Block!'s
   thin top-edge leave strip, Memory's berry hit areas.
4. A daytime nap wakes by itself in about 15 minutes; an evening bed does not.
5. Still open from before: § 1a's `lanLeave()` fix against the original
   guest-quits-mid-battle repro, and the power-key flush.

**Decided against, by the owner (2026-10-20): no indicator of the 8-hour
protection and no cue when a nap ends.** The protection is a secret reward for
good care, not a meter to manage. Do not add an icon, a message or a sound for
either.

---

## 1d. Where things stood (2026-10-12)

| | |
|---|---|
| Published firmware | **v3.28** (the last tag, 2026-09-14). `web/` on `main` is the untagged "Current Pages build" |
| Repo version | **3.33** in `TamaPoke.ino` and the README badge -- unreleased; 3.32 was on the owner's board first |
| Dex | `DEX_COUNT` 1025, `REGION_COUNT` 10, `GYM_REGIONS` 9 |
| Tests | 47 suites, all pass (`sprite_test` skips until `tools/unpack_bundle.py` has run). `check_savefile.mjs` passes |
| Firmware | compiles with esp32 core 3.3.0, GFX Library 1.6.9, **SensorLib 0.4.1** (0.5.0 boot-loops it -- `sensorlib_guard.h`), XPowersLib 0.3.4 |

**What happened in between.** On 2026-09-23 fifteen commits were made in
GitHub's web editor: a battle rework, a weight rework, and the version set to
"1.01". They were never compiled -- the old `battle.cpp` was kept beside the
new one as `z_battle.cpp`, every battle function was defined twice, and the
firmware did not link while the emulator suite stayed green. The 2026-10-11
cleanup kept both reworks and repaired around them; CLAUDE.md § TODO "Done: a
clean starting point" lists each fix and what is still the owner's call.

**Answered since § 1a / 1b:**

- The checkpoint write failure is **closed**: a stale `Pet::prefs` handle, cured
  by close/reopen/retry (§ 1b). Nothing has reported it since; watch for
  `save: ... retry after reopen` in a long session's log to be sure.
- `factoryReset()` ignoring `prefs.clear()` -- **fixed**: `WIPE` now prints
  `WIPE FAILED` and keeps the game running instead of claiming `DONE`.
- `IMPORT` missing the `saveInhibited` guard -- **fixed**, and it was worse than
  § 1a guessed: a restore over a save that had kept playing was undone on the
  reboot, because the RAM creature's checkpoint generation was the newer one.
- `link.cpp`'s shared `peerName` -- fixed in v3.27 (`d63ebc7`).

**Still not confirmed on hardware:** § 1a's `lanLeave()` fix against the
original guest-quits-mid-battle repro, and § 1's power-key flush.

---

## 0. Branch `feat/retire-and-release` (v3.6) -- HISTORICAL, landed long ago

Four things, all verified in the emulator; **nothing here has run on a board.**

1. **An early retire no longer banks the creature.** It is gone for good, and
   the next egg is NEUTRAL (`lastEnd = CER_RELEASE`) rather than blessed. It
   used to bank *and* bless exactly as an earned farewell does, so retiring was
   the good ending with a small tax: retire, check the egg, retire again, and
   farm blessed rolls for free. The one-day evolution penalty stays. Retiring
   one that HAS earned its farewell is unchanged -- it is the farewell.
2. **RELEASE on the party/box detail sheet** lets a banked creature go for good.
   It always asks first, and the creature does not fall through into the box.
   The box gained the same sheet: a box tap used to yank the creature into the
   party on one tap, and now opens the sheet with TO PARTY on it.
3. **The 3 s hold is gated on `uiCurrentScreen() == SCR_MAIN`.** It was gated by
   a list of screens to exclude, so it was live on the party screen -- where the
   grid overlaps `inPetZone` and the dialog's YES box sits on party slot 4.
4. **A sprite pack sent over `PUT` now unlocks its region without a reboot.**
   `sdScanRegionArt()` only ever ran at mount, so a region downloaded from the
   web installer stayed greyed out reading NEEDS PACK -- which looks exactly
   like the download having failed. This was reported from the board.

Tests: **34 suites, 34 passing** (`release_test` is new). Every guard was
negative-checked, and two of those checks were themselves found to be vacuous
before they were fixed -- see CLAUDE.md section 3.

Not done: a board has not seen any of it, and the installer has not been rebuilt
(`tools/build_web.sh`), so the published site is still v3.5.

---

## 1. What is live right now

| | |
|---|---|
| Published firmware | **v3.22**, live at https://eperdeme.github.io/TamaPoke/web/ |
| Repo version | **v3.22** in `TamaPoke.ino`, merged to `main` and tagged |
| Dex | `DEX_COUNT` **1025**, `REGION_COUNT` 10, `GYM_REGIONS` 7 |
| Your board | last flashed **v3.20**; v3.21 and v3.22 have not been on hardware |
| Live creature | Venusaur L100, `iv=29/20/19/22 tr=0/16/9`, bond 84, 12 medals |
| Tests | 40 suites + `check_savefile.mjs`, all passing (`sprite_test` skips without sprites) |
| NVS headroom | measured on the board: `used=132 avail=372 total=630` entries |

**Landed since the August snapshot below:** the dex reached 1025; the save is
checkpointed and atomic (issues #3 and #4 closed); the web installer backs up
before flashing and verifies what it captured; and releases are cut from `main`
with `check_release.py` enforcing it.

**Not verified on hardware**, and the first thing to do with a board:

1. The power-key long-press flush — hold PWR and watch for
   `save: flushing (pwr long press)` *before* it dies. If it never appears, the
   AXP2101's long-press IRQ fires too close to its own cut-off and
   `XPOWERS_POWEROFF_6S` in `pwrSetup()` widens the window.
2. The installer's backup-then-flash handoff, the twice-asked port prompt, the
   IndexedDB history and restore-from-history. All of it is untested end to end.
3. `STATS`'s new `board=` line, which is the real `ESP.getEfuseMac()` rather than
   the emulator's fake one.

### Save backups (all in `backups/`, gitignored)

    save-2026-08-21-dragonair-L45.txt          <- the current one, use this
    save-2026-08-20-dratini-maxed.txt          <- Lv 14, an EARLIER state
    save-2026-08-20-pre-3.0.txt
    save-2026-08-19-charizard-MIGRATE.txt      <- the migration block
    save-2026-08-19-board2-marshtomp.txt       <- board 2's original game
    save-2026-08-18.txt

Restore = paste the whole block into the serial console at 115200, **including
the bare `IMPORT` line at the end**, which is the commit. Without it nothing is
written.

---

## 1a. First real two-board LAN session (2026-09-10) — three bugs, all fixed

Four boards, real hardware, first time `linknow.cpp` had ever executed. Full
detail is in `CLAUDE.md`'s TODO section ("Found on real two-board LAN testing,
real hardware, four boards"); this is the short version for picking work back
up.

**`FW_VERSION` is 3.23** (bumped for these three fixes; `README.md`'s badge
matches). The board on COM5 is flashed with it. **This is a local dev bump
only** — not merged, not tagged, not released, and `web/manifest.json` is
deliberately left at 3.22 rather than hand-edited, since the right way to move
it is `build_web.sh` (which also recomputes the JS cache-buster hashes), and
that is a bigger step than this session did.

**Found and fixed, flashed to hardware, not yet re-soaked:**

1. `focusSwap()` could put the same creature in both a party slot and the
   live pet's own checkpoint at once, if the checkpoint write failed partway
   through the swap. `Pet::save()`/`Pet::switchTo()` now return `bool`;
   `focusSwap()` only commits the party-side write if the checkpoint one
   landed. Regression test in `focus_test.cpp` using the emulator's
   `nvsFailWritesAfter()` fault injection.
2. A failing checkpoint write used to retry on every loop iteration while the
   screen was dimmed — ~250 `save: pet checkpoint failed` lines captured in
   under 200ms on one board. Now throttled to one retry every 5s
   (`Pet::retryDue()`), and the failure message says which stage failed
   (write vs. read-back verify) plus live NVS headroom, instead of one
   undifferentiated line.
3. Dismissing a mid-battle "rival left" message (guest quit) crashed the host
   with a TASK WATCHDOG timeout on the battle screen. Cause: `lanLeave()` —
   which shuts the ESP-NOW radio off and resets `lan.state` — was called from
   `btlRun()`'s exit but not from either of `battleTap()`'s two dismiss paths,
   so the radio was left running, uninitialised-but-not-torn-down, into a LAN
   screen that expected `LINK_OFF`. Both paths now call it. **Not yet
   re-confirmed against the original repro** (guest quits mid-fight, host
   dismisses) — do that before trusting it closed.

**Still genuinely open:**

- *(Answered in § 1b.)* *Why* a checkpoint write fails at all. Captured with healthy NVS headroom
  (`used=202 avail=302 total=630`), so it is not a full partition. Worth
  re-watching now that #3 is fixed — a radio left running all session could
  plausibly have been destabilising something adjacent.
- One board's save got scrambled during live debugging BEFORE these fixes
  (a `WIPE` that did not fully clear NVS — `factoryReset()` never checks
  `prefs.clear()`'s return — followed by an `IMPORT` restore that also never
  got fully verified). It currently shows a Charmander/Abra/Squirtle party
  that matches neither the original save nor a fresh one. Emergency `EXPORT`
  captures from before any of that are in `backups/`, dated 2026-09-10, if
  it's worth trying to reconstruct. Two more real firmware bugs were found
  along the way and are fixed as of § 1d: `factoryReset()` ignored whether
  `prefs.clear()` actually succeeded, and `IMPORT`'s serial handler was
  missing the `saveInhibited = true` guard that `WIPE`'s has before its own
  `ESP.restart()`.

**New tool:** `tools/debugger/` — a tkinter serial console + save editor,
built mid-session because the ad-hoc reconnect-per-command approach used
before it was itself causing `USB_UART_CHIP_RESET`. Decodes a live `EXPORT`
into an editable view (live pet, party, box, bag, rivals, both checkpoint
pairs), flags duplicate creatures and unconsumed checkpoint handovers
automatically, and has a one-click bug-report bundle. Everything above was
found using it.

---

## 1b. Why the checkpoint failed, and two false alarms (2026-09-10) — FLASHED, `FW_VERSION` 3.24

Answers § 1a's open question: **a stale `Pet::prefs` handle**, not NVS space
(headroom was healthy every time it was captured) and not corruption. After a
long session's worth of WIPE/IMPORT/save cycles the handle degraded; closing
and reopening it and retrying the identical write succeeded immediately, and
the fix — close/reopen/retry once inside `saveCoreSnapshot()` and
`savePlayerSnapshot()` on a write failure — has not recurred since. This is
what was silently blocking `focusSwap()`'s party swap ("RAISE THIS ONE" doing
nothing after a wild catch): the checkpoint write failed, `switchTo()` failed
with it, and nothing on screen said why.

**Two false alarms found chasing it, both in the newly-added per-key save
logging itself, not in the save path:**

1. `nick`/`tnam` (`Pet::save()`'s legacy-key block) logged a failure on
   *every* save, forever, even on a handle just proven healthy by the
   checkpoint fix above. Root cause: `Preferences::putString()` returns
   `strlen(value)` on success — **0** for an empty string, indistinguishable
   from its own 0-on-failure return. Both keys were legitimately empty
   (`nick` by design — empty means "show the species name" — and `tnam`
   because this save had never had a trainer name set), so every "failure"
   was really a correct empty-string write being misread. Fixed:
   `SAVE_STR_KEY` only logs when the write returned 0 **and** the value being
   written was non-empty.
2. A one-time default closes the `tnam` gap for good rather than just quieting
   the log: `Pet::chooseStarter()` now sets `trainerName` to `"TRAINER"` if it
   is still empty when a new game's starter is chosen, and `Pet::begin()`
   backfills the same default on load for any existing save that is past
   starter selection and still has an empty name (this device included).
   `renameTrainer()` still overrides it whenever the player actually sets one.

**Found then, fixed in v3.27 (`d63ebc7`):**
`link.cpp`'s `HELLO` exchange reuses a single `peerName` field for both your
own outgoing name and the peer's received name. Tracing it shows a genuine
corruption path (a side that receives a `HELLO` while still `LISTENING`, or
any `LINK_SQUADS`-state resend, echoes the *peer's* name back to them instead
of sending its own), but `TamaPoke.ino`'s `lanOffer()` has both host and guest
call `start()` immediately, so in practice — confirmed by repeated real
testing — squad exchange finishes before the resend timer ever fires and the
corruption path is never hit. It would only show up under packet loss during
pairing (what `lossy_test` exists to simulate). Worth fixing before it is,
not because it has been.

---

## 1c. The Poke Mart, and the QMI8658's pedometer engine does not work (2026-09-11)

**Shipped:** a Poke Mart tile (between EXPLORE and GYM on the axis) that turns
real steps into Pokedollars at 1:1, wallet capped at $999,999 -- the real
games' own money ceiling. `FW_VERSION` moved to 3.25 for it. See `pet.h`
(`Pet::wallet`/`stepsTotal`/`addSteps()`/`spendWallet()`), `items.h` (real
Poke Mart prices, researched not invented), and `pedometer.cpp`.

**The QMI8658's own onboard pedometer engine
(`configPedometer()`/`getPedometerCounter()`) does not work, and was
abandoned for a software step detector instead.** Worth the full story
because the failure mode looked nothing like a bug at every level checked:

- No `configPedometer()` call at all (assuming usable power-on defaults):
  zero steps from 12s of active shaking on a real board.
- Added `configPedometer()` with values scaled for a low-power ODR
  (`ACC_ODR_LOWPOWER_21Hz`, `configPedometer(17, 200, 100, 67, 7)` -- the
  library's own example's numbers, rescaled by the ODR ratio since the
  library's doc says those params are literal sample counts, e.g. "80 means
  1.6s @ ODR=50Hz"): still zero.
- Reverted to the library's own proven example values UNSCALED
  (`ACC_ODR_62_5Hz`, `configPedometer(50, 200, 100, 200)`, matching
  SensorLib's `qmi8658_pedometer_deprecated.ino`): still zero, confirmed live
  over serial both shaking it and genuinely walking with it.
- Rebuilt with ESP32 core `DebugLevel=verbose` and forced a clean reset to
  capture the full boot sequence: **no errors anywhere.** No "QMI8658 not
  detected", no CTRL9 handshake timeout (the specific failure `writeCommand()`
  logs on a real timeout -- and `configPedometer()` ignores that return value
  entirely, so it always reports success regardless), no I2C NACKs. The chip
  acks, the commands complete, the counter just never moves.

Widened the search from "does this work on our board" to "does this work at
all": nobody found, on any board, has this engine working through SensorLib.
The closest data point is an open, **unanswered** GitHub issue asking whether
anyone has gotten the chip's *other* onboard engine (AttitudeEngine, sensor
fusion) working either:
[lewisxhe/SensorLib#3](https://github.com/lewisxhe/SensorLib/issues/3) — no
responses, no confirmation either way, opened 2023, still open. Whatever is
wrong (library bug, chip firmware revision, an undocumented requirement) is
not specific to this board or this session.

Meanwhile two independent real projects on this same chip both bypass the
hardware engine entirely: `VolosR/stepCounter` (a Waveshare AMOLED community
project) and `Melaja/ESP32-S3-Smartwatch`, both doing straightforward
magnitude-threshold detection on raw accelerometer samples. `pedometer.cpp`
now does the same -- `getAccelerometer()` + `sqrt(x^2+y^2+z^2)`, a rising
threshold crossing at 1.8g with a 100ms debounce (VolosR's own proven
values), polled every 30ms from `loop()` rather than every 2s, because a
software detector has to actually see the ~50-150ms footstep impact peak
rather than read an on-chip accumulator occasionally.

**First flash of the software detector ALSO read completely flat** -- zero
steps from both shaking and real walking, exactly like every hardware-engine
attempt before it. This time the bug was ours, not the chip's: `pedoPollSteps()`
gated every read behind `qmi.getDataReady()`, a `STATUS0` ready-bit check that
apparently never sets the way the library expects in this ODR/power-mode
combination -- silently returning 0 on literally every poll, indistinguishable
from "no steps" and just as thoroughly hidden as the CTRL9 dead end had been.
Dropping the gate entirely (a threshold detector does not need a
guaranteed-fresh sample; at 128Hz internal sampling against a ~33ms poll there
is always a recent value sitting in the register) and reading the
accelerometer unconditionally was the actual fix.

Added a diagnostic `ACCEL` serial command (`pedoLastMagnitude()`) specifically
because the earlier debugging had repeatedly been fooled by binary
pass/fail (wallet moved or it didn't) with no visibility into *why*. It
immediately paid for itself: a rapid poll (every ~150ms for 8s) during active
shaking showed real, sane, varying magnitude (0.23g-1.70g) for the first
time -- confirming the sensor and I2C path were fine all along, and the
earlier borrowed threshold (VolosR's 1.8g) simply never got reached by this
board/grip's actual motion. Lowered to 1.5g against that real measurement,
which **confirmed the whole pipeline end to end**: wallet and `stepsTotal`
both moved by exactly 10 during a real shake test read live over serial.
Lowered further to 1.35g from there for sensitivity, still against measured
hardware behaviour rather than a borrowed number -- see `pedometer.cpp` for
the constants and their reasoning.

Widened the threshold research afterward to other implementations on this
same chip for a sanity check, not because 1.35g looked wrong: VolosR uses
1.8g (simple crossing), Melaja's smartwatch uses a materially different
peak-valley state machine (~1.0g of deviation from the gravity baseline, so
effectively closer to a 2.0g raw peak, plus a 5-consecutive-step warm-up and
420-800ms inter-step timing gate), and a commercial hip-worn pedometer
(DigiwalkerSW200) uses 1.21g. The spread between just these three -- on
threshold *and* algorithm shape -- says grip/mounting/motion style dominates
over any single "correct" chip-wide number; 1.35g sits inside that whole
range and is the only one of the four actually measured against this exact
board.

**Also landed in the same session:** the Mart's rim scrollbar now uses the
real `UI_SCROLL_DAY`/`UI_SCROLL_NIGHT` palette pair (already defined and
contrast-tested in `palette_test.cpp`, just never wired up) instead of the
day-only colors every other paged screen still uses -- scoped to `SCR_MART`
only, deliberately not a global fix. The page-number text and a new
"tap: back" label are night-aware too, and tapping anywhere on the Mart that
isn't a button or the confirm dialog now actually goes back (previously a
stray tap silently did nothing, which the new label would have made a lie).
Separately, and unrelated to the Mart: the clock/settings screen's cancel
hint said "swipe up: cancel" in all six languages when the actual gesture is
swipe down (`onSwipeV`'s `back = dir > 0`) -- fixed everywhere.
`FW_VERSION` moved 3.25 -> 3.26 across this work.

---

## 2. The dex expansion — exactly where it stopped

**Goal:** `DEX_COUNT` 386 → 493 (Sinnoh). Chosen because it is the only
generation where every piece exists: 100% sprite coverage, a `pret` disassembly
for verifiable gym rosters, and badge art already reachable.

### Done and working

- **Phase 0, committed.** `--check` over the whole table, `tools/check_sprites.py`,
  and `dexdata_test`. See `CLAUDE.md` § "Adding a generation".
- **Six Gen 1 evolutions linked** (committed): GOLBAT→CROBAT, ONIX→STEELIX,
  CHANSEY→BLISSEY, SEADRA→KINGDRA, SCYTHER→SCIZOR, PORYGON→PORYGON2.
- **The generators are now idempotent** — three consecutive `--emit` runs produce
  byte-identical files. They were append-only before and would have duplicated
  everything on a second run.
- **Data regenerated to 493 — COMMITTED** in `486bacb`, not sitting in the
  working tree as an earlier draft of this file said: `dex_data.py`,
  `dex_types.py`, `dex_stats.py`, `dex_learnsets.py`, `dex.h`, `moves.h`.
  `--check` reports **0 unexpected differences over 1..493**.
- **`--link` picked up Sinnoh's cross-generation evolutions by itself**:
  LICKITUNG→LICKILICKY, RHYDON→RHYPERIOR, TANGELA→TANGROWTH,
  ELECTABUZZ→ELECTIVIRE, MAGMAR→MAGMORTAR and one more. That is the rule working
  as intended — it needs re-running after every expansion.

### The pipeline, in the order it must run

    python3 tools/gen_dex_data.py --emit 493   # dex_data.py + dex_types.py
    python3 tools/fetch_pokeapi.py             # dex_stats.py + dex_learnsets.py
    python3 tools/gen_dex.py                   # dex.h        (needs the stats)
    python3 tools/gen_moves.py                 # moves.h      (needs learnsets)
    python3 tools/gen_dex_data.py --link       # link new cross-gen evolutions
    python3 tools/gen_dex.py                   # again, so the links land
    python3 tools/gen_dex_data.py --check      # must say 0 unexpected

`REGIONS` in `tools/dex_data.py` needs its row added by hand — Sinnoh's is
already in (`('SINNOH', 387, 493, [387, 390, 393])`).

---

## 3. Not started

- ~~**Phase 2 — region gating.**~~ **DONE** (`05a28cb`) — see §7.
- ~~**Sinnoh sprites**~~ **DONE** (`6987c83`) — all 214 files packed, every
  region 100%, `thumbs.bin` regenerated at 493, installer shipping v3.3.
- ~~**Sinnoh gyms and badges**~~ **DONE.** `GYM_REGIONS` is 4, `BADGE_REGIONS`
  is 4, and `verify_rosters.py` checks all three added regions against their own
  disassemblies: **0 of 39 trainers differ.**

  Sinnoh is **Platinum**: Fantina is the THIRD gym, not Diamond/Pearl's fifth.
  The level ramp only runs 14/22/26/32/37/41/44/50 that way, and `roster_test`
  fails a leader 8+ levels below the previous, so the wrong order fails a test.

  `GYM_REGIONS` 3 -> 4 is purely additive for saves: `badgesX[GYM_REGIONS - 1]`
  grows and `getBytes` leaves the shorter stored blob in the front of the bigger
  array, so Johto/Hoenn badges keep their meaning and Sinnoh starts empty.

  **The gym chooser now needs TWO pages** (4 ladders, 3 rows a page), so Sinnoh
  sits on page 2 -- `swipe_test` drives that mode specifically, because the
  dexpick case runs in a different mode and does not speak for it.
- **Trading.** Discussed, not started. `linkMonFrom`/`linkMonTo` already
  serialise a creature both ways, so the exchange is cheap; the hard part is
  atomicity (both sides commit or neither). **Do the radio bring-up first** —
  `linknow.cpp` has never executed on hardware, and building trading on an
  unproven transport means debugging two unknowns at once.

---

## 4. The three failing tests — ALL FIXED (`739a41c`)

Suite is **33/33**. Kept here because two of them are worth knowing about.

**`dexdata_test` — DARKRAI.** Fixed by adding **DARK PULSE** to `dex_moves.py`.
BITE and CRUNCH were the only Dark moves and both are PHYSICAL, so it was never
just Darkrai — every special-attacking Dark type had no special STAB. Darkrai is
SpA 135 / Atk 90 and learns DARK PULSE at level 27. Not added to `NO_ATTACK`,
which stays reserved for Ditto/Unown/cocoons.

**A move's index is its position in `MOVES`, and saves store it RAW.** DARK
PULSE is appended *after* STRUGGLE, not filed under DARK, because inserting
mid-table shifts every later move by one and silently rewrites the moveset of
every saved creature. `dex_moves.py` now carries an `APPEND-ONLY BELOW HERE`
marker. **Any future move goes at the end.**

**`roster_test`.** Asserted `GYM_REGIONS == REGION_COUNT - 1`, which the dex
outgrows the moment a region has data but no roster — the normal state mid
expansion. Now asserts what matters: no ladder for ALL, the table is exactly
`GYM_REGIONS` long, every ladder has a roster and a name.

**`hit_test` — the one worth remembering.** Its "near miss" taps on the egg
region pill were 8 px from the graphic, *inside* the 16 px hit area. They were
direct hits cycling the region twelve times, and it passed only because
`12 % REGION_COUNT(4) == 0` came full circle. Sinnoh made it 5 and it broke. The
dead guard band CLAUDE.md says this test protects **had never been exercised** —
trap 3, a test proving arithmetic rather than firmware. It now taps in the real
band, derives the offset from the rects it already queries instead of copying
`EGGREG_PAD`/`EGGREG_GUARD` into the test, and checks after every tap.

---

## 4a. The overnight runaway (`91aa43c`) — fixed and FLASHED

A player woke to a Dragonair that had run away **after a night of correct
auto-sleep**. It went to bed with all four bars at zero, which armed
`neglectTicks` at 60 *before* the screen went off. The sleeping branch of
`tick()` returns before the neglect block, so the counter was neither counted
nor **cleared** for eight hours, and `canRunawayNow()` read it alone. On waking,
a creature at 100 energy was one tap from gone — and `FAR_BTN` (y176-234) sits
inside `inPetZone` (y95-310) and is checked before the caress, unconfirmed. The
next tick cleared it 60 s later.

`tick()` and `canRunawayNow()` now both ask `Pet::inTotalNeglect()`, so the
counter cannot outlive the state that earned it. `sleep_test` has the
morning-after repro plus a guard that a genuinely empty creature on waking is
still ready to leave. FW_VERSION and the README badge moved to **3.2**.

Flashed and confirmed on the board: `TamaPoke fw v3.3`, pet intact through the
upgrade (NVS is never touched by a USB flash).

    arduino-cli upload -p /dev/cu.usbmodem1101 \
      --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" .

Not done: `FAR_BTN` still overlaps `inPetZone`. The fix removes the cause, not
the delivery mechanism — it can now only fire on a creature genuinely empty
*right now*, which was judged the right line, but the geometry is still the
shape CLAUDE.md §4 warns about.

---

## 4b. Region gating and the installer (`05a28cb`, `6987c83`, `dee84f9`)

**The sprite pack is a real gate.** A region without its `.pak` on the card is
greyed in the chooser reading NEEDS PACK, denies on tap, is skipped by the egg
pill, refused by `setRegion()`, and excluded from the egg pool — `REGION_ALL`
filtering per species so a missing Sinnoh pack cannot put a Sinnoh creature in a
mixed egg. **Locked, never hidden.** Verified live on the board:

    art: KANTO  si
    art: JOHTO  si
    art: HOENN  si
    art: SINNOH NO (falta el pack)

`gRegionArt` defaults to ALL SET and is only narrowed by `sdScanRegionArt()`,
which probes THREE files per region (a half-copied 100 MB pack would pass a
single probe). That default is doing two jobs: no card keeps today's behaviour,
and `pet.cpp` stays free of SD symbols — it links into all 33 test binaries and
none of them build `sdmon.cpp`.

**The chooser is paged**, wraps rather than closing, and `rpickSwipe()` runs
FIRST in `onSwipe()` or the starter screen swallows the gesture. It is in
`swipe_test`, as every paged screen must be.

### The installer, and the trap in it

**The `.pak` files ARE committed and must be.** Release assets send **no CORS
headers**, so a browser `fetch()` of one is blocked — verified with an `Origin`
header: `200`, no `access-control-allow-origin`. `web/README.md` said the exact
opposite ("gitignored", "not committed", release "serves
Access-Control-Allow-Origin"); acting on it untracks them and breaks every
download button for everyone. That happened, in `06db7b9`, and was reverted in
`dee84f9`. The README is rewritten; **`.gitignore`'s comment was the true one.**

Second-order damage worth knowing: removing that bad ignore rule by truncating
the file also deleted the seven lines after it (`tamapoke.nvs`, `tools/_*.svg`,
`backups/`), and a `git add -A` promptly committed the save backups — whose own
comment reads "They were committed by accident once". Edit `.gitignore` by
replacing a block, never by slicing at an index.

`check_installer.py` still passes on every build, guarding both rules that each
destroyed a real save: four parts at their own offsets, and
`new_install_prompt_erase: true`.

## 4c. Unova (`feat/unova`) — DEX_COUNT 649

Two things happened here that had not happened in any earlier expansion.

**1. A region that is not 100% art.** 13 of Unova's 156 have no sprite upstream:

    514 516 520 522 523 538 558 564 565 591 592 616 626

They **keep their dex numbers**. Removing one renumbers every species after it,
and dex numbers are positional in saved data -- `dexReg` bits, `speciesId`,
every party record -- so a deletion corrupts existing saves. What is removed is
their ability to HATCH: `tools/check_sprites.py --emit` writes `noart.h`, and
`pickEggSpecies()`/`rollInRegion()` skip them. `region_test` rolls 2400 eggs and
fails if one appears. **Re-run `--emit` after any expansion and whenever
upstream adds art** -- several of these are base forms, so the list is doing
real work, not covering a theoretical case.

**2. A ladder with nothing to verify it against.** pret's DS disassemblies stop
at Platinum. Unova is therefore written from knowledge, which is precisely how
Johto and Hoenn were first written -- and `verify_rosters.py` later found TEN
errors in those, including two trainers carrying the wrong game's team. So:

- `trainers.h` carries a `*** NOT VERIFIED AGAINST A DISASSEMBLY ***` block
- `verify_rosters.py` prints a NOT VERIFIED section, so "0 trainers differ"
  cannot be read as covering Unova
- `roster_test`'s structural checks still apply, and its monotonic-ramp rule is
  what caught a wrong gym ORDER in Sinnoh earlier

It follows **B2W2**: Black/White's Striaton trio depends on your starter and
Drayden-or-Iris on your version, and one fixed ladder cannot express a choice.
Zebstrika, Throh and Carracosta are on it and have no art -- kept faithful, and
they draw as dex numbers.

**If a Gen 5 decomp ever appears, wire it into `verify_rosters.py` first.**

`check_sprites.py` also had to be fixed: it paged the GitHub API and `break`'d
on any failure, returning a partial set. Rate-limited, it reported all 156 Unova
species as art-less. That was survivable while it only printed a table; it is
not now that the output decides what can hatch. It fails loudly and prefers
authenticated `gh`.

---

## 5. Pitfalls — the ones that have actually cost time

### Build and test

- **A green emulator build does NOT mean the firmware compiles.** `build.sh`
  generates a `proto.h` with every prototype at the top; `arduino-cli` relies on
  auto-prototyping and rejects a function used above its declaration. This shipped
  once with 32 green suites. `tests/run.sh` now runs the real `arduino-cli
  compile` first — do not remove that.
- **Declare new sketch functions above their first use.** Same cause.
- **`const` at namespace scope is internal linkage in C++.** A table the tests
  need must be `extern const`, or the link fails with an undefined symbol.
- **Tests share one NVS store within a process.** A `Pet` built after another one
  slept will LOAD that sleep. Start each case from a known state (`sleep_test`
  has a `fresh()` helper for exactly this).
- **Negative-check every guard.** Break it on purpose and watch the test fail.
  This has caught a bad test roughly as often as a good one — `sprite_test`'s
  first version passed with the bug restored.

### Data and the dex

- **A move's index is its POSITION in `MOVES` (`dex_moves.py`), and saves store
  that index raw.** `gen_moves.py` does `idx = i + 1`; `Pet.moves[]` and
  `PartyMon.moves[]` write it straight to NVS. Inserting a move into its type
  section shifts every move after it by one and silently rewrites the moveset of
  every creature already saved — the live pet and every banked member, on every
  player's device. **New moves go at the end**, after STRUGGLE, under the
  `APPEND-ONLY BELOW HERE` marker. Same family as the box getting its own NVS key
  and badges being stored additively: never reinterpret bytes that already exist.
- **Anything holding a dex number is `int16_t`.** Never `uint8_t`, never the
  literal 151. This trap has fired five times: `evolvesTo`, `TrainerMon::dex`,
  `gen_moves.py`'s own `DEX_COUNT`, the Pokédex page cap, and `PmdMon::load` —
  which drew Ivysaur for Marshtomp and shipped in v2.8.
- **What hides it:** neighbouring code is usually already right, so the screen
  looks half-correct. `SdThumbs::get()` takes an `int16_t`, which is why the
  gallery was perfect while the creature on the main screen was somebody else.
- **A rule enforced in one path but not its twin** is the single most repeated
  mistake here. The TM gate (twice), the swipe paging bug (four times), the
  installer erase, the evolution threshold the card recomputed. Make every caller
  ask ONE function, and **test the caller, not just the rule**.

### The installer (both of these destroyed real saves)

- **The manifest must ship four parts at their own offsets**, never one merged
  image at `0x0` — `merge-bin` pads the gaps and writes 0xFF straight over NVS.
- **`new_install_prompt_erase` must be TRUE.** The name is the exact opposite of
  what it does: `false` means "do not ask, just erase the whole chip".
- `tools/check_installer.py` fails the build on either. `build_web.sh` runs it.

### Hardware

- **A stock board does not appear as a serial port at all.** Hold BOOT, tap
  RESET, release BOOT.
- **The board's RTC is months out** unless set (SETTINGS, or `RTCSET`). The sleep
  window depends on it; an unset clock never auto-sleeps, which fails safe.
- **Chrome holds the serial port** until the page that opened it closes. If
  `send_sd.py` says "Resource busy", close the installer tab.
- **Flashing over USB never touches NVS**; the web installer only leaves it alone
  because of the two fixes above.
- `EXPORT` before anything irreversible. It has been the safety net twice.

### The emulator

- `--save <path>` and `--wipe` keep experiments off your real save.
- `PANIC` / `WDT` at its console fake a crash so the boot report can be seen.
- It is launched here as a background process, so **it has no stdin you can type
  into** — drive it through a FIFO if commands are needed.
- It cannot see: timing, DMA tearing, PSRAM pressure, audio, battery, the radio,
  **touch accuracy** (synthetic taps are exact coordinates) or a missing
  `gfx->flush()`.

---

## 6. Older work still outstanding

- **LAN on two boards.** `linknow.cpp` has never executed. Treat the first
  session as debugging; `linkNowStats()` exists for it.
- **Battle background provenance** in `CREDITS.md` — the last licensing loose end
  before this goes public.
- **Audio by ear.** Nobody has judged the battle loop, the six cues, or whether
  `500 * vol` sounds linear.
- **A 24–48 h soak** on the current build. The last one passed at ~12 h.
