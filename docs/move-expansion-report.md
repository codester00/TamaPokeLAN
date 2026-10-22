# Move expansion report

Generated 2026-10-22 from PokeAPI's CSV dump, against the move table on the branch. **Nothing in the game has changed yet**; this is the list to approve before the build.

## The rules applied

- **Budget: 165 new moves.** Every saved creature stores a move as one byte, so the table tops out at 255; 90 are in use. Going past that is a save-format change for every creature on every board.
- **First, moves that fix attackless creatures.** A species needs a fix when it has a real attacking level-up move, but our table gives it none, or gives it its first one later than the real game does. Moves are chosen to fix the most such species first.
- **Then, the moves learned by the most species** by level-up, until the budget is spent.
- **Discarded outright: any effect the battle engine cannot reproduce** (listed below with what would bring each group back), and **any name too long** for the firmware: English over 12 characters, or Spanish, French, German or Italian over 23 after accents are dropped.
- Levels come from each species' own generation's last main game (FireRed, Crystal, Emerald, Platinum, Black 2/White 2, Omega Ruby, Ultra Sun, Sword, Scarlet), falling back to the newest game that has it.

## The result

| | before | after |
|---|---|---|
| species with no attacking level-up move | 19 | 15 |
| species whose first attack comes after level 10 | 96 | 50 |
| moves in the table | 90 | 216 |

Fixing every creature whose first attack was missing or late takes **24 moves**; the other **102** are the most widely learned of what is left. **0** acceptable moves did not fit the budget.

Still without an attacking level-up move after this (their real attacks were all discarded, or they have none in the games): METAPOD, KAKUNA, ABRA, DITTO, UNOWN, WOBBUFFET, SMEARGLE, WYNAUT, SPEWPA, WIMPOD, PYUKUMUKU, COSMOG, COSMOEM, BLIPBUG, APPLIN.

Fixed outright: KOMALA, SNOM, DIPPLIN, HYDRAPPLE.

**Decision for you.** Of the 15 left, 8 have no attack in the real games either (METAPOD, KAKUNA, ABRA, DITTO, SMEARGLE, SPEWPA, COSMOG, COSMOEM) -- the same kind of case as Magikarp, so your rule is met. The other **7 do attack in the games** (UNOWN, WOBBUFFET, WYNAUT, WIMPOD, PYUKUMUKU, BLIPBUG, APPLIN), but only with moves this rule discards. None of them is a NEW case -- all 15 are without an attack in the game today too -- but they are cases the games did not have. Two engine features would close most of the gap: **a stat change on a damaging move** (Struggle Bug, for Wimpod and Blipbug, plus 60 other moves) and **flinch** (Astonish, for Applin, plus 20 others). Counter, Mirror Coat, Bide and Hidden Power are one-offs.

### Why each of these is still without an attack

| Creature | Its real attacking level-up moves | Why none can be added |
|---|---|---|
| METAPOD | none -- it has no attack in the games either | -- |
| KAKUNA | none -- it has no attack in the games either | -- |
| ABRA | none -- it has no attack in the games either | -- |
| DITTO | none -- it has no attack in the games either | -- |
| UNOWN | Hidden Power | Hidden Power (L1): unique effect: Power and type depend upon user’s IVs.  Power can range from |
| WOBBUFFET | Counter, Mirror Coat | Counter (L1): unique effect: Inflicts twice the damage the user received from the last ph; Mirror Coat (L1): unique effect: Inflicts twice the damage the user received from the last sp |
| SMEARGLE | none -- it has no attack in the games either | -- |
| WYNAUT | Counter, Mirror Coat | Counter (L15): unique effect: Inflicts twice the damage the user received from the last ph; Mirror Coat (L15): unique effect: Inflicts twice the damage the user received from the last sp |
| SPEWPA | none -- it has no attack in the games either | -- |
| WIMPOD | Struggle Bug | Struggle Bug (L1): stat change on a damaging move |
| PYUKUMUKU | Bide, Counter | Bide (L1): unique effect: User waits for two turns, then hits back for twice the damag; Counter (L17): unique effect: Inflicts twice the damage the user received from the last ph |
| COSMOG | none -- it has no attack in the games either | -- |
| COSMOEM | none -- it has no attack in the games either | -- |
| BLIPBUG | Struggle Bug | Struggle Bug (L1): stat change on a damaging move |
| APPLIN | Astonish | Astonish (L1): flinch chance |

## Part 1: moves that fix attackless or late creatures (24)

In the order chosen. "Fixes" counts the species whose first attack this move supplies.

| Move | Type | Kind | Power | Effect | Level-up learners | TM learners | Fixes |
|---|---|---|---|---|---|---|---|
| POWDER SNOW | ice | spec | 40 | 10% freeze | 30 | 0 | 11 |
| GUST | flying | spec | 40 | - | 36 | 0 | 9 |
| VISE GRIP | normal | phys | 55 | - | 16 | 0 | 8 |
| FAIRY WIND | fairy | spec | 40 | - | 14 | 0 | 5 |
| LEAFAGE | grass | phys | 40 | - | 13 | 0 | 4 |
| BULLET SEED | grass | phys | 25 | 2-5 hits | 18 | 133 | 3 |
| ICICLE SPEAR | ice | phys | 25 | 2-5 hits | 4 | 48 | 3 |
| FEINT ATTACK | dark | phys | 60 | never miss | 64 | 0 | 2 |
| DOUBLE SLAP | normal | phys | 15 | 2-5 hits | 34 | 0 | 2 |
| AQUA JET | water | phys | 40 | priority +1 | 30 | 0 | 2 |
| SMOG | poison | spec | 30 | 40% poison | 23 | 0 | 2 |
| ROCK BLAST | rock | phys | 25 | 2-5 hits | 23 | 112 | 2 |
| HORN ATTACK | normal | phys | 65 | - | 16 | 0 | 2 |
| BRUTAL SWING | dark | phys | 60 | - | 14 | 132 | 2 |
| SLAM | normal | phys | 80 | - | 55 | 0 | 1 |
| WATER PULSE | water | spec | 60 | 20% confuse | 41 | 251 | 1 |
| DRAGON PULSE | dragon | spec | 85 | - | 37 | 123 | 1 |
| HURRICANE | flying | spec | 110 | 30% confuse | 13 | 72 | 1 |
| SHOCK WAVE | electric | spec | 60 | never miss | 12 | 166 | 1 |
| SHADOW SNEAK | ghost | phys | 40 | priority +1 | 12 | 0 | 1 |
| SONIC BOOM | normal | spec | - | 20 damage | 10 | 0 | 1 |
| ICE SHARD | ice | phys | 40 | priority +1 | 10 | 0 | 1 |
| CUT | normal | phys | 50 | - | 1 | 258 | 1 |
| SACRED FIRE | fire | phys | 100 | 50% burn | 1 | 0 | 1 |

## Part 2: the most widely learned (102)

| Move | Type | Kind | Power | Effect | Level-up learners | TM learners |
|---|---|---|---|---|---|---|
| TAKE DOWN | normal | phys | 90 | recoil 1/4 | 128 | 661 |
| SCARY FACE | normal | status | - | foe SPE-2 | 108 | 332 |
| TAIL WHIP | normal | status | - | foe DEF-1 | 95 | 0 |
| HARDEN | normal | status | - | self DEF+1 | 71 | 0 |
| IRON DEFENSE | steel | status | - | self DEF+2 | 67 | 228 |
| FURY SWIPES | normal | phys | 18 | 2-5 hits | 66 | 0 |
| GROWTH | normal | status | - | self ATK+SPA+1 | 59 | 0 |
| CHARM | fairy | status | - | foe ATK-2 | 54 | 215 |
| DEFENSE CURL | normal | status | - | self DEF+1 | 51 | 87 |
| DISCHARGE | electric | spec | 80 | 30% para | 43 | 0 |
| GIGA DRAIN | grass | spec | 75 | drain 50% | 39 | 217 |
| SYNTHESIS | grass | status | - | heal 50% | 37 | 0 |
| METAL SOUND | steel | status | - | foe SPD-2 | 36 | 54 |
| HYPER VOICE | normal | spec | 90 | - | 35 | 186 |
| WITHDRAW | water | status | - | self DEF+1 | 33 | 0 |
| WORK UP | normal | status | - | self ATK+SPA+1 | 29 | 280 |
| CALM MIND | psychic | status | - | self SPA+SPD+1 | 27 | 226 |
| BOUNCE | flying | phys | 85 | charge (hidden), 30% para | 25 | 87 |
| GIGA IMPACT | normal | phys | 150 | recharge | 23 | 577 |
| ZAP CANNON | electric | spec | 120 | 100% para | 23 | 75 |
| AERIAL ACE | flying | phys | 60 | never miss | 22 | 336 |
| MAGICAL LEAF | grass | spec | 60 | never miss | 22 | 132 |
| FEINT | normal | phys | 30 | priority +2 | 21 | 0 |
| SEED BOMB | grass | phys | 80 | - | 20 | 183 |
| BRAVE BIRD | flying | phys | 120 | recoil 1/3 | 20 | 59 |
| POISON JAB | poison | phys | 80 | 30% poison | 18 | 189 |
| FLARE BLITZ | fire | phys | 120 | recoil 1/3, 10% burn | 18 | 81 |
| HOWL | normal | status | - | self ATK+1 | 17 | 0 |
| FAKE TEARS | dark | status | - | foe SPD-2 | 16 | 128 |
| POWER WHIP | grass | phys | 120 | - | 16 | 27 |
| ACID ARMOR | poison | status | - | self DEF+2 | 16 | 0 |
| TICKLE | normal | status | - | foe ATK+DEF-1 | 16 | 0 |
| AQUA TAIL | water | phys | 90 | - | 16 | 0 |
| ROCK POLISH | rock | status | - | self SPE+2 | 15 | 97 |
| COSMIC POWER | psychic | status | - | self DEF+SPD+1 | 15 | 26 |
| WOOD HAMMER | grass | phys | 120 | recoil 1/3 | 15 | 0 |
| WILD CHARGE | electric | phys | 90 | recoil 1/4 | 14 | 137 |
| COTTON SPORE | grass | status | - | foe SPE-2 | 14 | 0 |
| MOONLIGHT | fairy | status | - | heal 50% | 14 | 0 |
| POWER GEM | rock | spec | 80 | - | 13 | 70 |
| HEAD SMASH | rock | phys | 150 | recoil 1/2 | 13 | 0 |
| FLAME WHEEL | fire | phys | 60 | 10% burn | 11 | 0 |
| DIVE | water | phys | 80 | charge (hidden) | 9 | 137 |
| ROCK CLIMB | normal | phys | 90 | 20% confuse | 9 | 92 |
| AURA SPHERE | fighting | spec | 80 | never miss | 9 | 45 |
| MEDITATE | psychic | status | - | self ATK+1 | 9 | 0 |
| ARM THRUST | fighting | phys | 15 | 2-5 hits | 9 | 0 |
| NUZZLE | electric | phys | 20 | 100% para | 9 | 0 |
| DIZZY PUNCH | normal | phys | 70 | 20% confuse | 8 | 0 |
| SHADOW PUNCH | ghost | phys | 60 | never miss | 8 | 0 |
| SKY UPPERCUT | fighting | phys | 85 | - | 8 | 0 |
| COTTON GUARD | grass | status | - | self DEF+3 | 8 | 0 |
| MEGA PUNCH | normal | phys | 80 | - | 7 | 174 |
| COMET PUNCH | normal | phys | 18 | 2-5 hits | 7 | 0 |
| MORNING SUN | normal | status | - | heal 50% | 7 | 0 |
| SIGNAL BEAM | bug | spec | 75 | 10% confuse | 7 | 0 |
| FORCE PALM | fighting | phys | 60 | 30% para | 7 | 0 |
| LAVA PLUME | fire | spec | 80 | 30% burn | 7 | 0 |
| QUIVER DANCE | bug | status | - | self SPA+SPD+SPE+1 | 7 | 0 |
| HORN LEECH | grass | phys | 75 | drain 50% | 7 | 0 |
| MEGA KICK | normal | phys | 120 | - | 6 | 180 |
| GUNK SHOT | poison | phys | 120 | 30% poison | 6 | 110 |
| SHARPEN | normal | status | - | self ATK+1 | 6 | 0 |
| SLACK OFF | normal | status | - | heal 50% | 6 | 0 |
| INFERNO | fire | spec | 100 | 100% burn | 6 | 0 |
| HEAT WAVE | fire | spec | 95 | 10% burn | 5 | 141 |
| DRAIN PUNCH | fighting | phys | 75 | drain 50% | 5 | 137 |
| SLUDGE WAVE | poison | spec | 95 | 10% poison | 5 | 88 |
| TAIL SLAP | normal | phys | 25 | 2-5 hits | 5 | 17 |
| SPIKE CANNON | normal | phys | 20 | 2-5 hits | 5 | 0 |
| BOOMBURST | normal | spec | 140 | - | 5 | 0 |
| MACH PUNCH | fighting | phys | 40 | priority +1 | 4 | 0 |
| BONE RUSH | ground | phys | 25 | 2-5 hits | 4 | 0 |
| SCALD | water | spec | 80 | 30% burn | 3 | 148 |
| EGG BOMB | normal | phys | 100 | - | 3 | 4 |
| MILK DRINK | normal | status | - | heal 50% | 3 | 0 |
| TAIL GLOW | bug | status | - | self SPA+3 | 3 | 0 |
| ROCK WRECKER | rock | phys | 150 | recharge | 3 | 0 |
| MAGNET BOMB | steel | phys | 60 | never miss | 3 | 0 |
| BRANCH POKE | grass | phys | 40 | - | 3 | 0 |
| STRENGTH | normal | phys | 80 | - | 2 | 341 |
| SOLAR BLADE | grass | phys | 125 | charge | 2 | 46 |
| BARRAGE | normal | phys | 15 | 2-5 hits | 2 | 0 |
| SHORE UP | ground | status | - | heal 50% | 2 | 0 |
| SMART STRIKE | steel | phys | 70 | never miss | 1 | 53 |
| VACUUM WAVE | fighting | spec | 40 | priority +1 | 1 | 47 |
| DEFEND ORDER | bug | status | - | self DEF+SPD+1 | 1 | 0 |
| HEAL ORDER | bug | status | - | heal 50% | 1 | 0 |
| ROAR OF TIME | dragon | spec | 150 | recharge | 1 | 0 |
| SHADOW FORCE | ghost | phys | 120 | charge (hidden) | 1 | 0 |
| HEAD CHARGE | normal | phys | 120 | recoil 1/4 | 1 | 0 |
| SEARING SHOT | fire | spec | 100 | 30% burn | 1 | 0 |
| BOLT STRIKE | electric | phys | 130 | 20% para | 1 | 0 |
| BLUE FLARE | fire | spec | 130 | 20% burn | 1 | 0 |
| LAND'S WRATH | ground | phys | 90 | - | 1 | 0 |
| ACCELEROCK | rock | phys | 40 | priority +1 | 1 | 0 |
| PYRO BALL | fire | phys | 120 | 10% burn | 1 | 0 |
| OVERDRIVE | electric | spec | 80 | - | 1 | 0 |
| ETERNABEAM | dragon | spec | 160 | recharge | 1 | 0 |
| BLAST BURN | fire | spec | 150 | recharge | 0 | 10 |
| HYDRO CANNON | water | spec | 150 | recharge | 0 | 10 |
| FRENZY PLANT | grass | spec | 150 | recharge | 0 | 10 |

## Kept, with an effect that does not exist in this game

Their extra effect involves something our battles never have (weather, Protect, a target mid-Fly), so they play exactly as they would in the games here. The table already treats EARTHQUAKE, SURF and THUNDER this way. Say if you want these dropped too.

- GUST: hits a target mid-Fly
- HURRICANE: hits a target mid-Fly / weather accuracy
- SACRED FIRE: user thaws itself
- SYNTHESIS: heal varies with weather
- FEINT: hits through Protect
- MOONLIGHT: heal varies with weather
- FLAME WHEEL: user thaws itself
- SKY UPPERCUT: hits a target mid-Fly
- MORNING SUN: heal varies with weather
- SHORE UP: heal varies with weather
- SHADOW FORCE: hits through Protect

## Acceptable but over budget (0)



## Discarded: effect the engine cannot reproduce

Grouped by what is missing. The count is how many moves each engine feature would bring back, and in brackets the most widely learned of them.

| Missing feature | Moves | Most learned |
|---|---|---|
| unique status effect | 133 | Focus Energy, Taunt, Light Screen, Safeguard, Rest, Curse |
| status move that only inflicts a status | 35 | Swagger, Confuse Ray, Leech Seed, Supersonic, Thunder Wave, Hypnosis |
| stat change on a damaging move | 61 | Mud-Slap, Metal Claw, Bubble Beam, Rapid Spin, Close Combat, Icy Wind |
| flinch chance | 21 | Astonish, Headbutt, Stomp, Air Slash, Fake Out, Dragon Rush |
| priority on a status move | 24 | Protect, Helping Hand, Roar, Endure, Detect, Whirlwind |
| high critical-hit ratio | 22 | Slash, Night Slash, Stone Edge, Leaf Blade, Razor Wind, Air Cutter |
| accuracy/evasion stages | 9 | Sand Attack, Sweet Scent, Double Team, Hone Claws, Smokescreen, Minimize |
| unsupported status (bad poison, trap, ...) | 13 | Wrap, Fire Spin, Bind, Smack Down, Sand Tomb, Whirlpool |
| fixed number of hits | 10 | Double Kick, Double Hit, Dual Wingbeat, Dual Chop, Gear Grind, Twineedle |
| priority plus another effect | 2 | Vital Throw, Water Shuriken |
| a one-off effect of its own | 142 | Flail, Thrash, Pursuit, Rollout, Bide, Future Sight, Sucker Punch, Uproar, Fury Cutter, Explosion |

## Not judged: PokeAPI has no battle data for these (69)

Not an engine limit: PokeAPI has not yet filled in effect data for most Scarlet/Violet moves, so the rules above could not be applied. Several are plainly replicable (WAVE CRASH is recoil, AQUA CUTTER plain damage) and could be judged by hand from the games if you want them. Level-up learners in brackets.

Wave Crash (4), Ruination (4), Aqua Cutter (4), Snowscape (3), Headlong Rush (2), Spin Out (2), Population Bomb (2), Ice Spinner (2), Revival Blessing (2), Salt Cure (2), Shed Tail (2), Syrup Bomb (2), Dire Claw (1), Psyshield Bash (1), Stone Axe (1), Springtide Storm (1), Raging Fury (1), Barb Barrage (1), Silk Trap (1), Axe Kick (1), Last Respects (1), Lumina Crash (1), Order Up (1), Jet Punch (1), Spicy Extract (1), Glaive Rush (1), Triple Dive (1), Mortal Spin (1), Doodle (1), Fillet Away (1), Kowtow Cleave (1), Flower Trick (1), Torch Song (1), Aqua Step (1), Make It Rain (1), Psyblade (1), Hydro Steam (1), Collision Course (1), Electro Drift (1), Tidy Up (1), Hyper Drill (1), Twin Beam (1), Rage Fist (1), Armor Cannon (1), Bitter Blade (1), Double Shock (1), Gigaton Hammer (1), Comeuppance (1), Matcha Gotcha (1), Ivy Cudgel (1), Electro Shot (1), Tera Starstorm (1), Fickle Beam (1), Burning Bulwark (1), Thunderclap (1), Mighty Cleave (1), Tachyon Cutter (1), Malignant Chain (1), Tera Blast (0), Pounce (0), Trailblaze (0), Chilling Water (0), Hard Press (0), Dragon Cheer (0), Alluring Voice (0), Temper Flare (0), Supercell Slam (0), Psychic Noise (0), Upper Hand (0)

## Discarded: name too long (18)

Many of these are old moves whose modern English name gained a space: the games themselves spelled them THUNDERSHOCK, DYNAMICPUNCH, ANCIENTPOWER until Gen 6, inside the same 12 characters, which is how our table already names THUNDERSHOCK and ANCIENTPOWER. Allowing those older official spellings would bring back the ones marked *.

| Move | Too long in | Level-up learners |
|---|---|---|
| Dragon Breath * | EN 13 | 39 |
| Feather Dance * | EN 13 | 21 |
| Disarming Voice | EN 15 | 18 |
| Draining Kiss | EN 13 | 17 |
| Phantom Force | EN 13 | 16 |
| Dynamic Punch * | EN 13 | 15 |
| Extreme Speed * | EN 13 | 7 |
| Petal Blizzard | EN 14 | 6 |
| High Horsepower | EN 15 | 6 |
| Eerie Impulse | EN 13 | 3 |
| False Surrender | EN 15 | 2 |
| Steam Eruption | EN 14 | 1 |
| Oblivion Wing | EN 13 | 1 |
| Dragon Hammer | EN 13 | 1 |
| Prismatic Laser | EN 15 | 1 |
| Photon Geyser | EN 13 | 1 |
| Meteor Assault | EN 14 | 1 |
| Scorching Sands | EN 15 | 0 |

## Still to do in the build

- **Portuguese names.** PokeAPI has no Portuguese; the table today uses Pokemon GO's Brazilian names where they exist and reviewed translations otherwise. Every new move needs one, at most 23 characters.
- Effects and powers are the modern games' (as the table already is); a move's Gen 1 value is not used.
- New moves go at the END of the table, so no saved moveset changes.
