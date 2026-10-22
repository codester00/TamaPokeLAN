# Next phase: moves, TMs, PP, the AI, the Game Corners, Surfing Pikachu

Agreed with the owner, 2026-10-22. **Built the same night as v3.50-v3.54**
(section 0-1 v3.50, 2 v3.51, 3-4 v3.52, 5 v3.53, 6 v3.54); where the build
departs from this plan, CLAUDE.md § "Done: the next build phase" says how and
why. Kept as the record of what was agreed.

## 0. Small fix that rides along: the gym list remembers its difficulty

The gym list opens on EASY after every restart, because `gymHard` lives only in
RAM. A player whose badges are all on HARD read that as "my badges are gone"
after the v3.49 update. Save the last choice as a setting, like the language
and the volume, with its own `SAVE_FIELDS` entry.

## 1. A bigger move list

The source of truth is `docs/move-expansion-report.md`, produced by
`tools/move_report.py` from PokeAPI's CSV dump. The owner's rules:

- **Pick order:** first the moves that give a creature its first attack (missing,
  or later than in the real games), then the moves learned by the most species.
- **Discard** any move whose effect the battle engine cannot reproduce. **Except**
  the one engine feature added below: stat changes on damaging moves.
- **Names:** discard any name too long in any language (English 12 characters,
  the others 23 after accents are dropped). The games' pre-Gen-6 spellings are
  ALLOWED where the modern name is too long: DRAGONBREATH, FEATHERDANCE,
  DYNAMICPUNCH, EXTREMESPEED.
- **No accuracy or evasion stages** (Sand Attack, Double Team, Minimize,
  Mud-Slap). They encourage stalling.
- **Keep about 15 of the 165 free move slots spare** unless filling them buys
  something major. A move index is one byte in every saved creature, so 255 is
  a hard ceiling short of a save-format change (CLAUDE.md trap 2).
- New moves go at the END of the table. A move inserted in the middle would
  rewrite every saved moveset.
- Every new move needs a Portuguese name (PokeAPI has none) of at most 23
  characters.
- **A test fails if any species has no attacking level-up move**, except the
  creatures that have none in the real games either: Metapod, Kakuna, Abra,
  Ditto, Smeargle, Spewpa, Cosmog and Cosmoem. Unown, Wobbuffet, Wynaut,
  Pyukumuku and Applin attack in the games only with one-off effects (Hidden
  Power, Counter, Mirror Coat, Astonish). They stay listed as known gaps, not
  new cases.

### 1a. Engine feature: stat changes on damaging moves

Today a stat change exists only as a STATUS move (Swords Dance, Growl). A
damaging move gains four fields: a chance, a stat mask, one stage delta, and
whether it hits the user or the target. This covers Bubble Beam, Metal Claw,
Icy Wind, Close Combat, Overheat, Draco Meteor, Ancient Power, Power-Up Punch
and Struggle Bug. That is 42 more moves, and it takes the attackless count from
19 down to 13.

- **Apply it to the existing moves that ignore theirs today** (owner's call):
  Psychic, Shadow Ball, Crunch, Acid, Aurora Beam, Play Rough, Moonblast,
  Bug Buzz, Flash Cannon, Ancient Power and the rest. This is a balance change,
  so update the README manual and bump `FW_VERSION`.
- The stat changes live on the move table, which is in flash. Nothing is saved.
- The hard AI must value the stat changes. Battle narration says "X's Speed
  fell!" as it already does for status moves.

## 2. TMs become items

- **Today**, a TM move on a species' list unlocks by itself at `TM_LEVEL` 40.
- **New rule, for the player's creatures only:** a TM move is learnable only once
  the player owns that TM. Gym leaders and wild creatures keep today's rule;
  NPC trainers had TM moves in the games too.
  - `moveUnlockLevel()` is the one place the move picker, the banked-creature
    editor and `relearnFromLevel()` ask. The gate goes there, with the opponent
    path kept separate on purpose.
- **Moves a creature already knows are KEPT.** Never strip a saved moveset.
- **TMs are reusable** (the Gen 5+ rule): own it once, teach it to any creature.
  Stored as owned bits in the player record, append-only, plus a legacy key in
  `SAVE_FIELDS`.
- **Ways to get TMs:**
  - occasional Explore drops;
  - Game Corner prizes (section 5);
  - **the Mart sells 5 a day.** The five rotate daily on a day hash, the same
    idea as `wildAreaWeather()`, and the Mart never offers a TM the player
    already owns.
- **Pikachu and Surf:** owning the SURF TM lets Pikachu learn Surf at any level,
  so the player no longer has to decline evolution from 30 to 39. That matters
  for section 6.

## 3. PP

This reverses "No PP by design" (owner's call, 2026-10-22). Reasons: strategy,
and a gym AI healed for ~25 turns.

- **Every move gets its real PP from PokeAPI**: Recover 5, Thunderbolt 15,
  Tackle 35.
- **PP persists between battles and refills when the live pet's energy reaches
  full while it sleeps.** That is one place in the code: the sleeping minute in
  `Pet::drainMinute()`, which runs whether the board is on or off. A nap from
  empty takes about 17 minutes (6 energy a minute). Battles already cost the
  live pet energy, so a few battles, then a rest.
- **When the live pet refills, the whole team refills:** live pet, party and box.
- **Stored per creature ID** (`CreatureUid`), in one small save key, so PP
  follows a creature through the party, the box and trades. Parking a tired
  creature in the box therefore does not refill it. A creature with no entry has
  full PP. That covers everything existing on first boot, and every received
  trade. About 400 bytes at most, roughly 14 of the 504 NVS entries.
- **A newly learned move starts with full PP.**
- **Each creature keeps its own PP for a whole gym fight.**
- **When every move is at 0 PP, the creature uses Struggle**, as in the games.
  Struggle already exists as the fallback move.
- **Opponents (gyms, wild) always start full.**
- **LAN battles start everyone full and never touch saved PP**, so a friendly
  battle is always even. The PP state rides in the link messages, so both boards
  need the new firmware. The build fingerprint already refuses a mismatch.
- **The UI:** move buttons show remaining PP (for example "8/15"); the party
  sheet shows each creature's PP.
- The AI sees its PP and never picks an empty move.

## 4. The AI stops healing forever

The cause: the AI scores healing by the share of HP it is MISSING, and scores
attacks by the HP they would REMOVE. The units don't match, so a creature whose
attacks barely hurt you heals every turn while you knock it back to half.

The fix:
- Heal only when the heal outruns the damage the foe deals per turn.
- Value healing less with each consecutive heal.
- PP also caps it (Recover has 5).
- A test plays exactly the owner's scenario and fails if the loop comes back.

## 5. The Game Corners

- **Two halls, each an Explore area** in its region's list, behind the usual
  badge gate:
  - **CELADON GAME CORNER** (Kanto, after Lt. Surge): slots with Red's reels.
  - **GOLDENROD GAME CORNER** (Johto, after Bugsy): slots and card flip, in
    Crystal's art.
- **Tapping a hall opens it, never a battle.** Area indices, `exploreAreaOf()`
  and `wildAreaState()` all assume a WILD area today, so the hall rows must be
  kept out of the wild paths.
- **Coins** are bought with step money at the games' price ($1,000 = 50) and won
  at the tables. They are a saved player-record field. Coins can be bought only
  with in-game money.
- **Prize counters, as in the games:**
  - Celadon: Abra, Clefairy, Dratini, Scyther or Pinsir, Porygon, plus TMs.
    **The SURF TM is one of them.**
  - Goldenrod: Abra, Ekans or Sandshrew, Dratini.
  - A prize creature goes to the party or box like a catch, and gets a fresh
    `CreatureUid`.
- **Art and sounds** come from pret's disassemblies (`gfx/slots/`,
  `gfx/card_flip/`, plus the slot sounds). This is unlicensed Nintendo art, the
  same arrangement as the avatars: a generator script fetches it, and
  `CREDITS.md` records it.

## 6. Surfing Pikachu: a secret

- **It appears on SEA ROUTES 19-21** (Kanto, gate 5; Yellow's Summer Beach
  House is on Route 19) only when ALL of these hold:
  - the live pet is PIKACHU;
  - it KNOWS SURF;
  - the area is unlocked;
  - every species the area offers is registered in the Pokedex, caught anywhere
    and by any means. Owner: "all pokemon are the same regardless of where or how
    they're acquired". So no new save data for the condition itself.
- **SECRET, like the rod.** Nothing hints at it until all four hold.
- **Play:** tilt to ride and jump (the tilt sensor that Tilt Maze already uses,
  via `tiltToScreen()`).
- **Pays:** a saved high score, coins, and bond.
- **Art and sounds** from `pret/pokeyellow` (`gfx/surfing_pikachu/`, the surfing
  sound effects, and `engine/minigame/surfing_pikachu.asm` for how it played).

## Later, not in this phase

- The real Game Corner and Surfing Pikachu MUSIC needs the chiptune synth wired
  to the speaker. It is built and tested (`gbsynth`), but the firmware still
  plays the old tone table.
- Weather battle backgrounds (`wildAreaWeather()` is the hook).
- The clock drift (CLAUDE.md B), on hold by the owner's call.
- Tagging the release, once the Actions minutes reset.
