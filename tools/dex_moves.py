# -*- coding: utf-8 -*-
"""The move list, hand-authored. Source of truth for moves.h via gen_moves.py.

86 moves, trimmed from Gen 1's 165 and then topped up with the cheap early
attacks the early game needs: 63 attacking (three or four per type, a
cheap one and a strong one), 11 stat-stage moves and 2 heals. STRUGGLE at the
end makes 77 -- it is a fallback, not a learnable move.

Not a pure Gen 1 list, and it can't be. dex_types.py gives the 151 their
CURRENT typings, so the dex contains Fairy (Clefable, Mr. Mime), Steel
(Magneton) and Dark types -- and Fairy moves are Gen 6, Steel and Dark moves
are Gen 2. Restricting to Red/Blue's move list would leave exactly those
species with no same-type move to their name. Entries marked LATER below are
in for that reason: they buy STAB for a typing the game already committed to.
Ghost is the same story -- Gen 1 Ghost had no usable attacking move at all,
hence SHADOW BALL.

Display names are capped at 12 characters: four move buttons have to fit
across a 466 px round panel at text size 2. ANCIENTPOWER is the games' own
Gen 3 spelling; DAZZLE GLEAM is ours.

No PP by design. Status conditions (burn/para/sleep/poison) are deliberately
absent for now -- the effect enum has room and they land in a later phase.
"""

# Move categories. There is no separate special-attack IV: special moves run
# off ivAtk/trAtk against the species' bSpA, and special defence off
# ivDef/trDef against bSpD. See gen_moves.py and the note in dex_stats.py.
MC_PHYS, MC_SPEC, MC_STATUS = 0, 1, 2

# Status ailments. Battle-only by design: they live in the battle state and are
# cleared when it ends, never written to the pet or replayed by the RTC's
# offline catch-up, so a burn can never grind against the care sim.
#
# There is no dedicated status move for these (no THUNDER WAVE, no SLEEP
# POWDER), so they ride along as a SECONDARY chance on a damaging move -- which
# is why they need their own two fields rather than reusing `effect`, whose one
# slot is already spent on things like EF_RECOIL.
AIL_NONE = 0
AIL_PARA = 1        # speed cut, and some turns are lost outright
AIL_BURN = 2        # chip damage each turn, physical attack cut
AIL_POISON = 3      # chip damage each turn
AIL_SLEEP = 4       # loses its turn for a few turns (no move inflicts it yet)
AIL_FREEZE = 5      # loses its turn until it thaws
AIL_CONFUSE = 6     # may hit itself instead

# Effects. Anything not listed here is a plain damaging move.
EF_NONE = 0
EF_STAGE = 1        # raise/lower stat stages (statMask + stages + target)
EF_RECOIL = 2       # param = denominator: 3 -> user takes 1/3 of damage dealt
EF_DRAIN = 3        # param = percent of damage dealt healed back
EF_FIXED_LVL = 4    # damage = user's level, ignores every stat
EF_FIXED = 5        # param = flat damage
EF_PRIORITY = 6     # param = priority bracket, moves before slower brackets
EF_NEVER_MISS = 7   # skips the accuracy roll entirely
EF_MULTI = 8        # hits 2-5 times, power is per hit
EF_HEAL = 9         # param = percent of max VIT restored
EF_RECHARGE = 10    # user loses the following turn, exposed
EF_CHARGE = 11      # turn 1 charges; param 1 = invulnerable while charging

# Stat bitmask for EF_STAGE. One delta applies to every bit set, which is how
# DRAGON DANCE (ATK+SPE) and BULK UP (ATK+DEF) get to be one table row.
ST_ATK, ST_DEF, ST_SPA, ST_SPD, ST_SPE = 1, 2, 4, 8, 16

TG_SELF, TG_FOE = 0, 1

# name (<=12 chars), pokeapi slug, type, category, power, accuracy,
# effect, param, statMask, stages, target
#
# accuracy 0 means the move cannot miss (only ever paired with EF_NEVER_MISS
# or a status move, which never rolls).
MOVES = [
    # --- NORMAL -----------------------------------------------------------
    # The cheap early attacks. Without these a young creature had NOTHING of its
    # own: about 15% of species reached level 15 with no attacking move at all,
    # because their real early moves were missing here and they were quietly
    # leaning on TMs to fill the gap. That only became visible once TMs were
    # gated -- a level 1 Squirtle had been opening with SURF instead.
    ("TACKLE",       "tackle",        'normal',   MC_PHYS,  40, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("SCRATCH",      "scratch",       'normal',   MC_PHYS,  40, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("POUND",        "pound",         'normal',   MC_PHYS,  40, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("FURY ATTACK",  "fury-attack",   'normal',   MC_PHYS,  15,  85, EF_MULTI,     0, 0, 0, TG_FOE),
    ("QUICK ATTACK", "quick-attack",  'normal',   MC_PHYS,  40, 100, EF_PRIORITY,  1, 0, 0, TG_FOE),
    ("SWIFT",        "swift",         'normal',   MC_SPEC,  60,   0, EF_NEVER_MISS, 0, 0, 0, TG_FOE),
    ("BODY SLAM",    "body-slam",     'normal',   MC_PHYS,  85, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("DOUBLE-EDGE",  "double-edge",   'normal',   MC_PHYS, 120, 100, EF_RECOIL,    3, 0, 0, TG_FOE),
    ("HYPER BEAM",   "hyper-beam",    'normal',   MC_SPEC, 150,  90, EF_RECHARGE,  0, 0, 0, TG_FOE),
    # --- FLYING (the early ones; WING ATTACK and up are further down) ------
    ("PECK",         "peck",          'flying',   MC_PHYS,  35, 100, EF_NONE,      0, 0, 0, TG_FOE),
    # --- FIRE -------------------------------------------------------------
    ("EMBER",        "ember",         'fire',     MC_SPEC,  40, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_BURN, 10),
    ("FIRE PUNCH",   "fire-punch",    'fire',     MC_PHYS,  75, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_BURN, 10),
    ("FLAMETHROWER", "flamethrower",  'fire',     MC_SPEC,  90, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_BURN, 10),
    ("FIRE BLAST",   "fire-blast",    'fire',     MC_SPEC, 110,  85, EF_NONE,      0, 0, 0, TG_FOE, AIL_BURN, 10),
    # --- WATER ------------------------------------------------------------
    ("BUBBLE",       "bubble",        'water',    MC_SPEC,  30, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("WATER GUN",    "water-gun",     'water',    MC_SPEC,  40, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("WATERFALL",    "waterfall",     'water',    MC_PHYS,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("SURF",         "surf",          'water',    MC_SPEC,  90, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("HYDRO PUMP",   "hydro-pump",    'water',    MC_SPEC, 110,  80, EF_NONE,      0, 0, 0, TG_FOE),
    # --- ELECTRIC ---------------------------------------------------------
    ("SPARK",        "spark",         'electric', MC_PHYS,  35, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_PARA, 20),
    ("THUNDERSHOCK", "thunder-shock", 'electric', MC_SPEC,  40, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_PARA, 10),
    ("THUNDERPUNCH", "thunder-punch", 'electric', MC_PHYS,  75, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_PARA, 10),
    ("THUNDERBOLT",  "thunderbolt",   'electric', MC_SPEC,  90, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_PARA, 10),
    ("THUNDER",      "thunder",       'electric', MC_SPEC, 110,  70, EF_NONE,      0, 0, 0, TG_FOE, AIL_PARA, 30),
    # --- GRASS ------------------------------------------------------------
    ("ABSORB",       "absorb",        'grass',    MC_SPEC,  20, 100, EF_DRAIN,    50, 0, 0, TG_FOE),
    ("VINE WHIP",    "vine-whip",     'grass',    MC_PHYS,  45, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("RAZOR LEAF",   "razor-leaf",    'grass',    MC_PHYS,  55,  95, EF_NONE,      0, 0, 0, TG_FOE),
    ("MEGA DRAIN",   "mega-drain",    'grass',    MC_SPEC,  40, 100, EF_DRAIN,    50, 0, 0, TG_FOE),
    # charges in the open: the one 2-turn move that really does cost you a hit
    ("SOLAR BEAM",   "solar-beam",    'grass',    MC_SPEC, 120, 100, EF_CHARGE,    0, 0, 0, TG_FOE),
    # --- ICE --------------------------------------------------------------
    ("AURORA BEAM",  "aurora-beam",   'ice',      MC_SPEC,  65, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("ICE PUNCH",    "ice-punch",     'ice',      MC_PHYS,  75, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_FREEZE, 10),
    ("ICE BEAM",     "ice-beam",      'ice',      MC_SPEC,  90, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_FREEZE, 10),
    ("BLIZZARD",     "blizzard",      'ice',      MC_SPEC, 110,  70, EF_NONE,      0, 0, 0, TG_FOE, AIL_FREEZE, 10),
    # --- FIGHTING ---------------------------------------------------------
    ("KARATE CHOP",  "karate-chop",   'fighting', MC_PHYS,  50, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("SEISMIC TOSS", "seismic-toss",  'fighting', MC_PHYS,   0, 100, EF_FIXED_LVL, 0, 0, 0, TG_FOE),
    ("SUBMISSION",   "submission",    'fighting', MC_PHYS,  80,  80, EF_RECOIL,    4, 0, 0, TG_FOE),
    # 130 in the modern games; 100 here so one move can't decide a gym
    ("HI JUMP KICK", "high-jump-kick",'fighting', MC_PHYS, 100,  90, EF_NONE,      0, 0, 0, TG_FOE),
    # --- POISON -----------------------------------------------------------
    ("POISON STING", "poison-sting",  'poison',   MC_PHYS,  15, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_POISON, 30),
    ("ACID",         "acid",          'poison',   MC_SPEC,  40, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_POISON, 10),
    ("SLUDGE",       "sludge",        'poison',   MC_SPEC,  65, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_POISON, 30),
    ("SLUDGE BOMB",  "sludge-bomb",   'poison',   MC_SPEC,  90, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_POISON, 30),  # LATER (Gen 2)
    # --- GROUND -----------------------------------------------------------
    ("BONE CLUB",    "bone-club",     'ground',   MC_PHYS,  65,  85, EF_NONE,      0, 0, 0, TG_FOE),
    ("DIG",          "dig",           'ground',   MC_PHYS,  80, 100, EF_CHARGE,    1, 0, 0, TG_FOE),
    ("EARTHQUAKE",   "earthquake",    'ground',   MC_PHYS, 100, 100, EF_NONE,      0, 0, 0, TG_FOE),
    # --- FLYING -----------------------------------------------------------
    ("WING ATTACK",  "wing-attack",   'flying',   MC_PHYS,  60, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("DRILL PECK",   "drill-peck",    'flying',   MC_PHYS,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("FLY",          "fly",           'flying',   MC_PHYS,  90,  95, EF_CHARGE,    1, 0, 0, TG_FOE),
    # --- PSYCHIC ----------------------------------------------------------
    ("PSYWAVE",      "psywave",       'psychic',  MC_SPEC,  30, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("CONFUSION",    "confusion",     'psychic',  MC_SPEC,  50, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_CONFUSE, 10),
    ("PSYBEAM",      "psybeam",       'psychic',  MC_SPEC,  65, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_CONFUSE, 10),
    ("PSYCHIC",      "psychic",       'psychic',  MC_SPEC,  90, 100, EF_NONE,      0, 0, 0, TG_FOE),
    # --- BUG --------------------------------------------------------------
    ("BUG BITE",     "bug-bite",      'bug',      MC_PHYS,  30, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("PIN MISSILE",  "pin-missile",   'bug',      MC_PHYS,  25,  95, EF_MULTI,     0, 0, 0, TG_FOE),
    # 20 power in Gen 1, 80 since Gen 7 -- the modern value, like the stats
    ("LEECH LIFE",   "leech-life",    'bug',      MC_PHYS,  80, 100, EF_DRAIN,    50, 0, 0, TG_FOE),
    ("MEGAHORN",     "megahorn",      'bug',      MC_PHYS, 120,  85, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 2)
    # The three above reach almost no Gen 1 bug: BUTTERFREE and PINSIR learn
    # none of them, and would have gone to a gym with no same-type move at
    # all. These two are the standard modern Bug attacks and fix exactly that.
    ("BUG BUZZ",     "bug-buzz",      'bug',      MC_SPEC,  90, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 4)
    ("X-SCISSOR",    "x-scissor",     'bug',      MC_PHYS,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 4)
    # --- ROCK -------------------------------------------------------------
    ("ROCK SMASH",   "rock-smash",    'fighting', MC_PHYS,  40, 100, EF_NONE,      0, 0, 0, TG_FOE),
    ("ROCK THROW",   "rock-throw",    'rock',     MC_PHYS,  50,  90, EF_NONE,      0, 0, 0, TG_FOE),
    ("ROCK SLIDE",   "rock-slide",    'rock',     MC_PHYS,  75,  90, EF_NONE,      0, 0, 0, TG_FOE),
    ("ANCIENTPOWER", "ancient-power", 'rock',     MC_SPEC,  60, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 2)
    # --- GHOST ------------------------------------------------------------
    ("LICK",         "lick",          'ghost',    MC_PHYS,  30, 100, EF_NONE,      0, 0, 0, TG_FOE, AIL_PARA, 30),
    ("NIGHT SHADE",  "night-shade",   'ghost',    MC_SPEC,   0, 100, EF_FIXED_LVL, 0, 0, 0, TG_FOE),
    ("SHADOW BALL",  "shadow-ball",   'ghost',    MC_SPEC,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 2)
    # --- DRAGON -----------------------------------------------------------
    ("DRAGON RAGE",  "dragon-rage",   'dragon',   MC_SPEC,   0, 100, EF_FIXED,    40, 0, 0, TG_FOE),
    ("DRAGON CLAW",  "dragon-claw",   'dragon',   MC_PHYS,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 3)
    ("OUTRAGE",      "outrage",       'dragon',   MC_PHYS, 120, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 2)
    # --- DARK -------------------------------------------------------------
    # BITE existed in Gen 1 but as a Normal move; Dark from Gen 2 on
    ("BITE",         "bite",          'dark',     MC_PHYS,  60, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER
    ("CRUNCH",       "crunch",        'dark',     MC_PHYS,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 2)
    # --- STEEL ------------------------------------------------------------
    ("IRON HEAD",    "iron-head",     'steel',    MC_PHYS,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 4)
    ("FLASH CANNON", "flash-cannon",  'steel',    MC_SPEC,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 4)
    # --- FAIRY ------------------------------------------------------------
    ("DAZZLE GLEAM", "dazzling-gleam",'fairy',    MC_SPEC,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 6)
    ("PLAY ROUGH",   "play-rough",    'fairy',    MC_PHYS,  90,  90, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 6)
    ("MOONBLAST",    "moonblast",     'fairy',    MC_SPEC,  95, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 6)

    # --- stat stages, self ------------------------------------------------
    # The player's answer to a gym leader with perfect IVs: you are behind on
    # raw stats by design, so you win by setting up or by type advantage.
    # Stages reset on switch out, which is what makes switching a decision.
    ("SWORDS DANCE", "swords-dance",  'normal',   MC_STATUS, 0,   0, EF_STAGE,     0, ST_ATK,          2, TG_SELF),
    ("AGILITY",      "agility",       'psychic',  MC_STATUS, 0,   0, EF_STAGE,     0, ST_SPE,          2, TG_SELF),
    ("BARRIER",      "barrier",       'psychic',  MC_STATUS, 0,   0, EF_STAGE,     0, ST_DEF,          2, TG_SELF),
    ("AMNESIA",      "amnesia",       'psychic',  MC_STATUS, 0,   0, EF_STAGE,     0, ST_SPD,          2, TG_SELF),
    ("NASTY PLOT",   "nasty-plot",    'dark',     MC_STATUS, 0,   0, EF_STAGE,     0, ST_SPA,          2, TG_SELF),  # LATER (Gen 4)
    ("DRAGON DANCE", "dragon-dance",  'dragon',   MC_STATUS, 0,   0, EF_STAGE,     0, ST_ATK | ST_SPE, 1, TG_SELF),  # LATER (Gen 2)
    ("BULK UP",      "bulk-up",       'fighting', MC_STATUS, 0,   0, EF_STAGE,     0, ST_ATK | ST_DEF, 1, TG_SELF),  # LATER (Gen 3)
    # --- stat stages, foe -------------------------------------------------
    ("GROWL",        "growl",         'normal',   MC_STATUS, 0,   0, EF_STAGE,     0, ST_ATK,         -1, TG_FOE),
    ("LEER",         "leer",          'normal',   MC_STATUS, 0,   0, EF_STAGE,     0, ST_DEF,         -1, TG_FOE),
    ("SCREECH",      "screech",       'normal',   MC_STATUS, 0,   0, EF_STAGE,     0, ST_DEF,         -2, TG_FOE),
    ("STRING SHOT",  "string-shot",   'bug',      MC_STATUS, 0,   0, EF_STAGE,     0, ST_SPE,         -1, TG_FOE),
    # --- healing ----------------------------------------------------------
    ("RECOVER",      "recover",       'normal',   MC_STATUS, 0,   0, EF_HEAL,     50, 0, 0, TG_SELF),
    ("SOFT-BOILED",  "soft-boiled",   'normal',   MC_STATUS, 0,   0, EF_HEAL,     50, 0, 0, TG_SELF),

    # --- fallback ---------------------------------------------------------
    # Not in any learnset (slug None keeps the fetcher off it): the engine
    # substitutes this when a species has nothing else. METAPOD, KAKUNA and
    # DITTO learn none of the 70 above and would otherwise be unable to act.
    # For the first two that is a few hours before they evolve; DITTO is a
    # final form and stays this way, since its entire movepool is TRANSFORM.
    ("STRUGGLE",     None,            'normal',   MC_PHYS,  50,   0, EF_RECOIL,    4, 0, 0, TG_FOE),

    # --- APPEND-ONLY BELOW HERE -------------------------------------------
    # A move's index IS its position in this list (gen_moves.py: idx = i + 1),
    # and Pet.moves[]/PartyMon.moves[] store that index RAW in NVS. Inserting a
    # move into its type section above would shift every move after it by one
    # and silently rewrite the moveset of every creature already saved -- the
    # live pet and every banked party member. So a new move goes HERE, at the
    # end, however untidy that looks beside the type grouping.
    #
    # DARK PULSE, for DARKRAI (491), which dexdata_test caught with no same-type
    # attack once Sinnoh landed. BITE and CRUNCH are the only other Dark moves
    # and both are PHYSICAL, so every special-attacking Dark type had no special
    # STAB; Darkrai is SpA 135 / Atk 90, so neither was usable to it. It learns
    # DARK PULSE by level-up at 27, so this is its real move, not a contrivance.
    ("DARK PULSE",   "dark-pulse",    'dark',     MC_SPEC,  80, 100, EF_NONE,      0, 0, 0, TG_FOE),  # LATER (Gen 4)

    # --- v3.50: the move expansion (docs/move-expansion-report.md) -------
    # 150 moves picked by tools/move_report.py: first those that give a
    # creature its real early attack, then the most widely learned. A stat
    # change on a damaging move lives in HIT_STATS below, not in the row.
    ("POWDER SNOW", "powder-snow", 'ice', MC_SPEC, 40, 100, EF_NONE, 0, 0, 0, TG_FOE, AIL_FREEZE, 10),
    ("GUST", "gust", 'flying', MC_SPEC, 40, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("VISE GRIP", "vice-grip", 'normal', MC_PHYS, 55, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("DRAGONBREATH", "dragon-breath", 'dragon', MC_SPEC, 60, 100, EF_NONE, 0, 0, 0, TG_FOE, AIL_PARA, 30),
    ("CONSTRICT", "constrict", 'normal', MC_PHYS, 10, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("FAIRY WIND", "fairy-wind", 'fairy', MC_SPEC, 40, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("METAL CLAW", "metal-claw", 'steel', MC_PHYS, 50, 95, EF_NONE, 0, 0, 0, TG_FOE),
    ("LEAFAGE", "leafage", 'grass', MC_PHYS, 40, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("STRUGGLE BUG", "struggle-bug", 'bug', MC_SPEC, 50, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("ICICLE SPEAR", "icicle-spear", 'ice', MC_PHYS, 25, 100, EF_MULTI, 0, 0, 0, TG_FOE),
    ("FEINT ATTACK", "feint-attack", 'dark', MC_PHYS, 60, 0, EF_NEVER_MISS, 0, 0, 0, TG_FOE),
    ("DOUBLE SLAP", "double-slap", 'normal', MC_PHYS, 15, 85, EF_MULTI, 0, 0, 0, TG_FOE),
    ("AQUA JET", "aqua-jet", 'water', MC_PHYS, 40, 100, EF_PRIORITY, 1, 0, 0, TG_FOE),
    ("SMOG", "smog", 'poison', MC_SPEC, 30, 70, EF_NONE, 0, 0, 0, TG_FOE, AIL_POISON, 40),
    ("ROCK BLAST", "rock-blast", 'rock', MC_PHYS, 25, 90, EF_MULTI, 0, 0, 0, TG_FOE),
    ("HORN ATTACK", "horn-attack", 'normal', MC_PHYS, 65, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("BRUTAL SWING", "brutal-swing", 'dark', MC_PHYS, 60, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("SLAM", "slam", 'normal', MC_PHYS, 80, 75, EF_NONE, 0, 0, 0, TG_FOE),
    ("WATER PULSE", "water-pulse", 'water', MC_SPEC, 60, 100, EF_NONE, 0, 0, 0, TG_FOE, AIL_CONFUSE, 20),
    ("DRAGON PULSE", "dragon-pulse", 'dragon', MC_SPEC, 85, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("ICY WIND", "icy-wind", 'ice', MC_SPEC, 55, 95, EF_NONE, 0, 0, 0, TG_FOE),
    ("MUD SHOT", "mud-shot", 'ground', MC_SPEC, 55, 95, EF_NONE, 0, 0, 0, TG_FOE),
    ("BULLDOZE", "bulldoze", 'ground', MC_PHYS, 60, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("BULLET SEED", "bullet-seed", 'grass', MC_PHYS, 25, 100, EF_MULTI, 0, 0, 0, TG_FOE),
    ("OMINOUS WIND", "ominous-wind", 'ghost', MC_SPEC, 60, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("HURRICANE", "hurricane", 'flying', MC_SPEC, 110, 70, EF_NONE, 0, 0, 0, TG_FOE, AIL_CONFUSE, 30),
    ("SHOCK WAVE", "shock-wave", 'electric', MC_SPEC, 60, 0, EF_NEVER_MISS, 0, 0, 0, TG_FOE),
    ("SHADOW SNEAK", "shadow-sneak", 'ghost', MC_PHYS, 40, 100, EF_PRIORITY, 1, 0, 0, TG_FOE),
    ("SONIC BOOM", "sonic-boom", 'normal', MC_SPEC, 0, 90, EF_FIXED, 20, 0, 0, TG_FOE),
    ("ICE SHARD", "ice-shard", 'ice', MC_PHYS, 40, 100, EF_PRIORITY, 1, 0, 0, TG_FOE),
    ("CUT", "cut", 'normal', MC_PHYS, 50, 95, EF_NONE, 0, 0, 0, TG_FOE),
    ("SACRED FIRE", "sacred-fire", 'fire', MC_PHYS, 100, 95, EF_NONE, 0, 0, 0, TG_FOE, AIL_BURN, 50),
    ("TAKE DOWN", "take-down", 'normal', MC_PHYS, 90, 85, EF_RECOIL, 4, 0, 0, TG_FOE),
    ("SCARY FACE", "scary-face", 'normal', MC_STATUS, 0, 0, EF_STAGE, 0, ST_SPE, -2, TG_FOE),
    ("TAIL WHIP", "tail-whip", 'normal', MC_STATUS, 0, 0, EF_STAGE, 0, ST_DEF, -1, TG_FOE),
    ("HARDEN", "harden", 'normal', MC_STATUS, 0, 0, EF_STAGE, 0, ST_DEF, 1, TG_SELF),
    ("IRON DEFENSE", "iron-defense", 'steel', MC_STATUS, 0, 0, EF_STAGE, 0, ST_DEF, 2, TG_SELF),
    ("FURY SWIPES", "fury-swipes", 'normal', MC_PHYS, 18, 80, EF_MULTI, 0, 0, 0, TG_FOE),
    ("GROWTH", "growth", 'normal', MC_STATUS, 0, 0, EF_STAGE, 0, ST_ATK | ST_SPA, 1, TG_SELF),
    ("CHARM", "charm", 'fairy', MC_STATUS, 0, 0, EF_STAGE, 0, ST_ATK, -2, TG_FOE),
    ("DEFENSE CURL", "defense-curl", 'normal', MC_STATUS, 0, 0, EF_STAGE, 0, ST_DEF, 1, TG_SELF),
    ("DISCHARGE", "discharge", 'electric', MC_SPEC, 80, 100, EF_NONE, 0, 0, 0, TG_FOE, AIL_PARA, 30),
    ("GIGA DRAIN", "giga-drain", 'grass', MC_SPEC, 75, 100, EF_DRAIN, 50, 0, 0, TG_FOE),
    ("BUBBLE BEAM", "bubble-beam", 'water', MC_SPEC, 65, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("SYNTHESIS", "synthesis", 'grass', MC_STATUS, 0, 0, EF_HEAL, 50, 0, 0, TG_SELF),
    ("METAL SOUND", "metal-sound", 'steel', MC_STATUS, 0, 85, EF_STAGE, 0, ST_SPD, -2, TG_FOE),
    ("HYPER VOICE", "hyper-voice", 'normal', MC_SPEC, 90, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("CLOSE COMBAT", "close-combat", 'fighting', MC_PHYS, 120, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("WITHDRAW", "withdraw", 'water', MC_STATUS, 0, 0, EF_STAGE, 0, ST_DEF, 1, TG_SELF),
    ("SUPERPOWER", "superpower", 'fighting', MC_PHYS, 120, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("WORK UP", "work-up", 'normal', MC_STATUS, 0, 0, EF_STAGE, 0, ST_ATK | ST_SPA, 1, TG_SELF),
    ("CALM MIND", "calm-mind", 'psychic', MC_STATUS, 0, 0, EF_STAGE, 0, ST_SPA | ST_SPD, 1, TG_SELF),
    ("FLAME CHARGE", "flame-charge", 'fire', MC_PHYS, 50, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("BOUNCE", "bounce", 'flying', MC_PHYS, 85, 85, EF_CHARGE, 1, 0, 0, TG_FOE, AIL_PARA, 30),
    ("GIGA IMPACT", "giga-impact", 'normal', MC_PHYS, 150, 90, EF_RECHARGE, 0, 0, 0, TG_FOE),
    ("ZAP CANNON", "zap-cannon", 'electric', MC_SPEC, 120, 50, EF_NONE, 0, 0, 0, TG_FOE, AIL_PARA, 100),
    ("HAMMER ARM", "hammer-arm", 'fighting', MC_PHYS, 100, 90, EF_NONE, 0, 0, 0, TG_FOE),
    ("AERIAL ACE", "aerial-ace", 'flying', MC_PHYS, 60, 0, EF_NEVER_MISS, 0, 0, 0, TG_FOE),
    ("MAGICAL LEAF", "magical-leaf", 'grass', MC_SPEC, 60, 0, EF_NEVER_MISS, 0, 0, 0, TG_FOE),
    ("FEATHERDANCE", "feather-dance", 'flying', MC_STATUS, 0, 0, EF_STAGE, 0, ST_ATK, -2, TG_FOE),
    ("FEINT", "feint", 'normal', MC_PHYS, 30, 100, EF_PRIORITY, 2, 0, 0, TG_FOE),
    ("SEED BOMB", "seed-bomb", 'grass', MC_PHYS, 80, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("BRAVE BIRD", "brave-bird", 'flying', MC_PHYS, 120, 100, EF_RECOIL, 3, 0, 0, TG_FOE),
    ("ENERGY BALL", "energy-ball", 'grass', MC_SPEC, 90, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("LEAF STORM", "leaf-storm", 'grass', MC_SPEC, 130, 90, EF_NONE, 0, 0, 0, TG_FOE),
    ("POISON JAB", "poison-jab", 'poison', MC_PHYS, 80, 100, EF_NONE, 0, 0, 0, TG_FOE, AIL_POISON, 30),
    ("LIQUIDATION", "liquidation", 'water', MC_PHYS, 85, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("FLARE BLITZ", "flare-blitz", 'fire', MC_PHYS, 120, 100, EF_RECOIL, 3, 0, 0, TG_FOE, AIL_BURN, 10),
    ("HOWL", "howl", 'normal', MC_STATUS, 0, 0, EF_STAGE, 0, ST_ATK, 1, TG_SELF),
    ("FAKE TEARS", "fake-tears", 'dark', MC_STATUS, 0, 0, EF_STAGE, 0, ST_SPD, -2, TG_FOE),
    ("POWER WHIP", "power-whip", 'grass', MC_PHYS, 120, 85, EF_NONE, 0, 0, 0, TG_FOE),
    ("ACID ARMOR", "acid-armor", 'poison', MC_STATUS, 0, 0, EF_STAGE, 0, ST_DEF, 2, TG_SELF),
    ("TICKLE", "tickle", 'normal', MC_STATUS, 0, 0, EF_STAGE, 0, ST_ATK | ST_DEF, -1, TG_FOE),
    ("AQUA TAIL", "aqua-tail", 'water', MC_PHYS, 90, 90, EF_NONE, 0, 0, 0, TG_FOE),
    ("ROCK POLISH", "rock-polish", 'rock', MC_STATUS, 0, 0, EF_STAGE, 0, ST_SPE, 2, TG_SELF),
    ("DYNAMICPUNCH", "dynamic-punch", 'fighting', MC_PHYS, 100, 50, EF_NONE, 0, 0, 0, TG_FOE, AIL_CONFUSE, 100),
    ("COSMIC POWER", "cosmic-power", 'psychic', MC_STATUS, 0, 0, EF_STAGE, 0, ST_DEF | ST_SPD, 1, TG_SELF),
    ("WOOD HAMMER", "wood-hammer", 'grass', MC_PHYS, 120, 100, EF_RECOIL, 3, 0, 0, TG_FOE),
    ("ROCK TOMB", "rock-tomb", 'rock', MC_PHYS, 60, 95, EF_NONE, 0, 0, 0, TG_FOE),
    ("EARTH POWER", "earth-power", 'ground', MC_SPEC, 90, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("WILD CHARGE", "wild-charge", 'electric', MC_PHYS, 90, 100, EF_RECOIL, 4, 0, 0, TG_FOE),
    ("OVERHEAT", "overheat", 'fire', MC_SPEC, 130, 90, EF_NONE, 0, 0, 0, TG_FOE),
    ("COTTON SPORE", "cotton-spore", 'grass', MC_STATUS, 0, 0, EF_STAGE, 0, ST_SPE, -2, TG_FOE),
    ("MOONLIGHT", "moonlight", 'fairy', MC_STATUS, 0, 0, EF_HEAL, 50, 0, 0, TG_SELF),
    ("POWER GEM", "power-gem", 'rock', MC_SPEC, 80, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("HEAD SMASH", "head-smash", 'rock', MC_PHYS, 150, 80, EF_RECOIL, 2, 0, 0, TG_FOE),
    ("ACID SPRAY", "acid-spray", 'poison', MC_SPEC, 40, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("SILVER WIND", "silver-wind", 'bug', MC_SPEC, 60, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("SNARL", "snarl", 'dark', MC_SPEC, 55, 95, EF_NONE, 0, 0, 0, TG_FOE),
    ("LUNGE", "lunge", 'bug', MC_PHYS, 80, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("FLAME WHEEL", "flame-wheel", 'fire', MC_PHYS, 60, 100, EF_NONE, 0, 0, 0, TG_FOE, AIL_BURN, 10),
    ("DIVE", "dive", 'water', MC_PHYS, 80, 100, EF_CHARGE, 1, 0, 0, TG_FOE),
    ("ROCK CLIMB", "rock-climb", 'normal', MC_PHYS, 90, 85, EF_NONE, 0, 0, 0, TG_FOE, AIL_CONFUSE, 20),
    ("AURA SPHERE", "aura-sphere", 'fighting', MC_SPEC, 80, 0, EF_NEVER_MISS, 0, 0, 0, TG_FOE),
    ("MEDITATE", "meditate", 'psychic', MC_STATUS, 0, 0, EF_STAGE, 0, ST_ATK, 1, TG_SELF),
    ("ARM THRUST", "arm-thrust", 'fighting', MC_PHYS, 15, 100, EF_MULTI, 0, 0, 0, TG_FOE),
    ("NUZZLE", "nuzzle", 'electric', MC_PHYS, 20, 100, EF_NONE, 0, 0, 0, TG_FOE, AIL_PARA, 100),
    ("CHARGE BEAM", "charge-beam", 'electric', MC_SPEC, 50, 90, EF_NONE, 0, 0, 0, TG_FOE),
    ("DIZZY PUNCH", "dizzy-punch", 'normal', MC_PHYS, 70, 100, EF_NONE, 0, 0, 0, TG_FOE, AIL_CONFUSE, 20),
    ("SHADOW PUNCH", "shadow-punch", 'ghost', MC_PHYS, 60, 0, EF_NEVER_MISS, 0, 0, 0, TG_FOE),
    ("SKY UPPERCUT", "sky-uppercut", 'fighting', MC_PHYS, 85, 90, EF_NONE, 0, 0, 0, TG_FOE),
    ("COTTON GUARD", "cotton-guard", 'grass', MC_STATUS, 0, 0, EF_STAGE, 0, ST_DEF, 3, TG_SELF),
    ("MEGA PUNCH", "mega-punch", 'normal', MC_PHYS, 80, 85, EF_NONE, 0, 0, 0, TG_FOE),
    ("RAZOR SHELL", "razor-shell", 'water', MC_PHYS, 75, 95, EF_NONE, 0, 0, 0, TG_FOE),
    ("COMET PUNCH", "comet-punch", 'normal', MC_PHYS, 18, 85, EF_MULTI, 0, 0, 0, TG_FOE),
    ("MORNING SUN", "morning-sun", 'normal', MC_STATUS, 0, 0, EF_HEAL, 50, 0, 0, TG_SELF),
    ("EXTREMESPEED", "extreme-speed", 'normal', MC_PHYS, 80, 100, EF_PRIORITY, 2, 0, 0, TG_FOE),
    ("CRUSH CLAW", "crush-claw", 'normal', MC_PHYS, 75, 95, EF_NONE, 0, 0, 0, TG_FOE),
    ("SIGNAL BEAM", "signal-beam", 'bug', MC_SPEC, 75, 100, EF_NONE, 0, 0, 0, TG_FOE, AIL_CONFUSE, 10),
    ("FORCE PALM", "force-palm", 'fighting', MC_PHYS, 60, 100, EF_NONE, 0, 0, 0, TG_FOE, AIL_PARA, 30),
    ("LAVA PLUME", "lava-plume", 'fire', MC_SPEC, 80, 100, EF_NONE, 0, 0, 0, TG_FOE, AIL_BURN, 30),
    ("QUIVER DANCE", "quiver-dance", 'bug', MC_STATUS, 0, 0, EF_STAGE, 0, ST_SPA | ST_SPD | ST_SPE, 1, TG_SELF),
    ("HORN LEECH", "horn-leech", 'grass', MC_PHYS, 75, 100, EF_DRAIN, 50, 0, 0, TG_FOE),
    ("IRON TAIL", "iron-tail", 'steel', MC_PHYS, 100, 75, EF_NONE, 0, 0, 0, TG_FOE),
    ("MEGA KICK", "mega-kick", 'normal', MC_PHYS, 120, 75, EF_NONE, 0, 0, 0, TG_FOE),
    ("LOW SWEEP", "low-sweep", 'fighting', MC_PHYS, 65, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("GUNK SHOT", "gunk-shot", 'poison', MC_PHYS, 120, 80, EF_NONE, 0, 0, 0, TG_FOE, AIL_POISON, 30),
    ("SHARPEN", "sharpen", 'normal', MC_STATUS, 0, 0, EF_STAGE, 0, ST_ATK, 1, TG_SELF),
    ("SLACK OFF", "slack-off", 'normal', MC_STATUS, 0, 0, EF_HEAL, 50, 0, 0, TG_SELF),
    ("INFERNO", "inferno", 'fire', MC_SPEC, 100, 50, EF_NONE, 0, 0, 0, TG_FOE, AIL_BURN, 100),
    ("HEAT WAVE", "heat-wave", 'fire', MC_SPEC, 95, 90, EF_NONE, 0, 0, 0, TG_FOE, AIL_BURN, 10),
    ("DRAIN PUNCH", "drain-punch", 'fighting', MC_PHYS, 75, 100, EF_DRAIN, 50, 0, 0, TG_FOE),
    ("STEEL WING", "steel-wing", 'steel', MC_PHYS, 70, 90, EF_NONE, 0, 0, 0, TG_FOE),
    ("SLUDGE WAVE", "sludge-wave", 'poison', MC_SPEC, 95, 100, EF_NONE, 0, 0, 0, TG_FOE, AIL_POISON, 10),
    ("TAIL SLAP", "tail-slap", 'normal', MC_PHYS, 25, 85, EF_MULTI, 0, 0, 0, TG_FOE),
    ("SPIKE CANNON", "spike-cannon", 'normal', MC_PHYS, 20, 100, EF_MULTI, 0, 0, 0, TG_FOE),
    ("BOOMBURST", "boomburst", 'normal', MC_SPEC, 140, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("MACH PUNCH", "mach-punch", 'fighting', MC_PHYS, 40, 100, EF_PRIORITY, 1, 0, 0, TG_FOE),
    ("BONE RUSH", "bone-rush", 'ground', MC_PHYS, 25, 90, EF_MULTI, 0, 0, 0, TG_FOE),
    ("SCALD", "scald", 'water', MC_SPEC, 80, 100, EF_NONE, 0, 0, 0, TG_FOE, AIL_BURN, 30),
    ("ELECTROWEB", "electroweb", 'electric', MC_SPEC, 55, 95, EF_NONE, 0, 0, 0, TG_FOE),
    ("EGG BOMB", "egg-bomb", 'normal', MC_PHYS, 100, 75, EF_NONE, 0, 0, 0, TG_FOE),
    ("MILK DRINK", "milk-drink", 'normal', MC_STATUS, 0, 0, EF_HEAL, 50, 0, 0, TG_SELF),
    ("TAIL GLOW", "tail-glow", 'bug', MC_STATUS, 0, 0, EF_STAGE, 0, ST_SPA, 3, TG_SELF),
    ("METEOR MASH", "meteor-mash", 'steel', MC_PHYS, 90, 90, EF_NONE, 0, 0, 0, TG_FOE),
    ("ROCK WRECKER", "rock-wrecker", 'rock', MC_PHYS, 150, 90, EF_RECHARGE, 0, 0, 0, TG_FOE),
    ("MAGNET BOMB", "magnet-bomb", 'steel', MC_PHYS, 60, 0, EF_NEVER_MISS, 0, 0, 0, TG_FOE),
    ("BRANCH POKE", "branch-poke", 'grass', MC_PHYS, 40, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("STRENGTH", "strength", 'normal', MC_PHYS, 80, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("SOLAR BLADE", "solar-blade", 'grass', MC_PHYS, 125, 100, EF_CHARGE, 0, 0, 0, TG_FOE),
    ("BARRAGE", "barrage", 'normal', MC_PHYS, 15, 85, EF_MULTI, 0, 0, 0, TG_FOE),
    ("FIERY DANCE", "fiery-dance", 'fire', MC_SPEC, 80, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("SHORE UP", "shore-up", 'ground', MC_STATUS, 0, 0, EF_HEAL, 50, 0, 0, TG_SELF),
    ("FIRE LASH", "fire-lash", 'fire', MC_PHYS, 80, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("SPIRIT BREAK", "spirit-break", 'fairy', MC_PHYS, 75, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("FOCUS BLAST", "focus-blast", 'fighting', MC_SPEC, 120, 70, EF_NONE, 0, 0, 0, TG_FOE),
    ("SMART STRIKE", "smart-strike", 'steel', MC_PHYS, 70, 0, EF_NEVER_MISS, 0, 0, 0, TG_FOE),
    ("VACUUM WAVE", "vacuum-wave", 'fighting', MC_SPEC, 40, 100, EF_PRIORITY, 1, 0, 0, TG_FOE),
    ("LUSTER PURGE", "luster-purge", 'psychic', MC_SPEC, 95, 100, EF_NONE, 0, 0, 0, TG_FOE),
    ("MIST BALL", "mist-ball", 'psychic', MC_SPEC, 95, 100, EF_NONE, 0, 0, 0, TG_FOE),
]

# Accuracy drops (SAND ATTACK, SMOKESCREEN, FLASH) are deliberately left out:
# accuracy stages are a whole second stage system for one marginal effect.

SLUG_TO_NAME = {slug: name for name, slug, *_ in MOVES if slug}


# A damaging move's stat change, applied after it hits: slug ->
# (stat mask, stages, TG_SELF or TG_FOE, chance in percent). Closes over
# the existing moves that used to ignore theirs too (PSYCHIC, CRUNCH, ...).
# From PokeAPI's move_meta_stat_changes; accuracy and evasion are left out
# on purpose (owner's call, 2026-10-22), as are moves that change two stats
# by different amounts.
HIT_STATS = {
    "bubble": (ST_SPE, -1, TG_FOE, 10),
    "aurora-beam": (ST_ATK, -1, TG_FOE, 10),
    "acid": (ST_SPD, -1, TG_FOE, 10),
    "psychic": (ST_SPD, -1, TG_FOE, 10),
    "bug-buzz": (ST_SPD, -1, TG_FOE, 10),
    "rock-smash": (ST_DEF, -1, TG_FOE, 50),
    "ancient-power": (ST_ATK | ST_DEF | ST_SPA | ST_SPD | ST_SPE, 1, TG_SELF, 10),
    "shadow-ball": (ST_SPD, -1, TG_FOE, 20),
    "crunch": (ST_DEF, -1, TG_FOE, 20),
    "flash-cannon": (ST_SPD, -1, TG_FOE, 10),
    "play-rough": (ST_ATK, -1, TG_FOE, 10),
    "moonblast": (ST_SPA, -1, TG_FOE, 30),
    "constrict": (ST_SPE, -1, TG_FOE, 10),
    "metal-claw": (ST_ATK, 1, TG_SELF, 10),
    "struggle-bug": (ST_SPA, -1, TG_FOE, 100),
    "icy-wind": (ST_SPE, -1, TG_FOE, 100),
    "mud-shot": (ST_SPE, -1, TG_FOE, 100),
    "bulldoze": (ST_SPE, -1, TG_FOE, 100),
    "ominous-wind": (ST_ATK | ST_DEF | ST_SPA | ST_SPD | ST_SPE, 1, TG_SELF, 10),
    "bubble-beam": (ST_SPE, -1, TG_FOE, 10),
    "close-combat": (ST_DEF | ST_SPD, -1, TG_SELF, 100),
    "superpower": (ST_ATK | ST_DEF, -1, TG_SELF, 100),
    "flame-charge": (ST_SPE, 1, TG_SELF, 100),
    "hammer-arm": (ST_SPE, -1, TG_SELF, 100),
    "energy-ball": (ST_SPD, -1, TG_FOE, 10),
    "leaf-storm": (ST_SPA, -2, TG_SELF, 100),
    "liquidation": (ST_DEF, -1, TG_FOE, 20),
    "rock-tomb": (ST_SPE, -1, TG_FOE, 100),
    "earth-power": (ST_SPD, -1, TG_FOE, 10),
    "overheat": (ST_SPA, -2, TG_SELF, 100),
    "acid-spray": (ST_SPD, -2, TG_FOE, 100),
    "silver-wind": (ST_ATK | ST_DEF | ST_SPA | ST_SPD | ST_SPE, 1, TG_SELF, 10),
    "snarl": (ST_SPA, -1, TG_FOE, 100),
    "lunge": (ST_ATK, -1, TG_FOE, 100),
    "charge-beam": (ST_SPA, 1, TG_SELF, 70),
    "razor-shell": (ST_DEF, -1, TG_FOE, 50),
    "crush-claw": (ST_DEF, -1, TG_FOE, 50),
    "iron-tail": (ST_DEF, -1, TG_FOE, 30),
    "low-sweep": (ST_SPE, -1, TG_FOE, 100),
    "steel-wing": (ST_DEF, 1, TG_SELF, 10),
    "electroweb": (ST_SPE, -1, TG_FOE, 100),
    "meteor-mash": (ST_ATK, 1, TG_SELF, 20),
    "fiery-dance": (ST_SPA, 1, TG_SELF, 50),
    "fire-lash": (ST_DEF, -1, TG_FOE, 100),
    "spirit-break": (ST_SPA, -1, TG_FOE, 100),
    "focus-blast": (ST_SPD, -1, TG_FOE, 10),
    "luster-purge": (ST_SPD, -1, TG_FOE, 50),
    "mist-ball": (ST_SPA, -1, TG_FOE, 50),
}


# Each move's real PP, from PokeAPI's moves.csv (the modern value, like the
# powers). STRUGGLE has none: it is what a creature uses when every move is
# out, and it is never spent. Read by gen_moves.py into MoveEntry::pp.
MOVE_PP = {
    "tackle": 35,
    "scratch": 35,
    "pound": 35,
    "fury-attack": 20,
    "quick-attack": 30,
    "swift": 20,
    "body-slam": 15,
    "double-edge": 15,
    "hyper-beam": 5,
    "peck": 35,
    "ember": 25,
    "fire-punch": 15,
    "flamethrower": 15,
    "fire-blast": 5,
    "bubble": 30,
    "water-gun": 25,
    "waterfall": 15,
    "surf": 15,
    "hydro-pump": 5,
    "spark": 20,
    "thunder-shock": 30,
    "thunder-punch": 15,
    "thunderbolt": 15,
    "thunder": 10,
    "absorb": 25,
    "vine-whip": 25,
    "razor-leaf": 25,
    "mega-drain": 15,
    "solar-beam": 10,
    "aurora-beam": 20,
    "ice-punch": 15,
    "ice-beam": 10,
    "blizzard": 5,
    "karate-chop": 25,
    "seismic-toss": 20,
    "submission": 20,
    "high-jump-kick": 10,
    "poison-sting": 35,
    "acid": 30,
    "sludge": 20,
    "sludge-bomb": 10,
    "bone-club": 20,
    "dig": 10,
    "earthquake": 10,
    "wing-attack": 35,
    "drill-peck": 20,
    "fly": 15,
    "psywave": 15,
    "confusion": 25,
    "psybeam": 20,
    "psychic": 10,
    "bug-bite": 20,
    "pin-missile": 20,
    "leech-life": 10,
    "megahorn": 10,
    "bug-buzz": 10,
    "x-scissor": 15,
    "rock-smash": 15,
    "rock-throw": 15,
    "rock-slide": 10,
    "ancient-power": 5,
    "lick": 30,
    "night-shade": 15,
    "shadow-ball": 15,
    "dragon-rage": 10,
    "dragon-claw": 15,
    "outrage": 10,
    "bite": 25,
    "crunch": 15,
    "iron-head": 15,
    "flash-cannon": 10,
    "dazzling-gleam": 10,
    "play-rough": 10,
    "moonblast": 15,
    "swords-dance": 20,
    "agility": 30,
    "barrier": 20,
    "amnesia": 20,
    "nasty-plot": 20,
    "dragon-dance": 20,
    "bulk-up": 20,
    "growl": 40,
    "leer": 30,
    "screech": 40,
    "string-shot": 40,
    "recover": 5,
    "soft-boiled": 5,
    "dark-pulse": 15,
    "powder-snow": 25,
    "gust": 35,
    "vice-grip": 30,
    "dragon-breath": 20,
    "constrict": 35,
    "fairy-wind": 30,
    "metal-claw": 35,
    "leafage": 40,
    "struggle-bug": 20,
    "icicle-spear": 30,
    "feint-attack": 20,
    "double-slap": 10,
    "aqua-jet": 20,
    "smog": 20,
    "rock-blast": 10,
    "horn-attack": 25,
    "brutal-swing": 20,
    "slam": 20,
    "water-pulse": 20,
    "dragon-pulse": 10,
    "icy-wind": 15,
    "mud-shot": 15,
    "bulldoze": 20,
    "bullet-seed": 30,
    "ominous-wind": 5,
    "hurricane": 10,
    "shock-wave": 20,
    "shadow-sneak": 30,
    "sonic-boom": 20,
    "ice-shard": 30,
    "cut": 30,
    "sacred-fire": 5,
    "take-down": 20,
    "scary-face": 10,
    "tail-whip": 30,
    "harden": 30,
    "iron-defense": 15,
    "fury-swipes": 15,
    "growth": 20,
    "charm": 20,
    "defense-curl": 40,
    "discharge": 15,
    "giga-drain": 10,
    "bubble-beam": 20,
    "synthesis": 5,
    "metal-sound": 40,
    "hyper-voice": 10,
    "close-combat": 5,
    "withdraw": 40,
    "superpower": 5,
    "work-up": 30,
    "calm-mind": 20,
    "flame-charge": 20,
    "bounce": 5,
    "giga-impact": 5,
    "zap-cannon": 5,
    "hammer-arm": 10,
    "aerial-ace": 20,
    "magical-leaf": 20,
    "feather-dance": 15,
    "feint": 10,
    "seed-bomb": 15,
    "brave-bird": 15,
    "energy-ball": 10,
    "leaf-storm": 5,
    "poison-jab": 20,
    "liquidation": 10,
    "flare-blitz": 15,
    "howl": 40,
    "fake-tears": 20,
    "power-whip": 10,
    "acid-armor": 20,
    "tickle": 20,
    "aqua-tail": 10,
    "rock-polish": 20,
    "dynamic-punch": 5,
    "cosmic-power": 20,
    "wood-hammer": 15,
    "rock-tomb": 15,
    "earth-power": 10,
    "wild-charge": 15,
    "overheat": 5,
    "cotton-spore": 40,
    "moonlight": 5,
    "power-gem": 20,
    "head-smash": 5,
    "acid-spray": 20,
    "silver-wind": 5,
    "snarl": 15,
    "lunge": 15,
    "flame-wheel": 25,
    "dive": 10,
    "rock-climb": 20,
    "aura-sphere": 20,
    "meditate": 40,
    "arm-thrust": 20,
    "nuzzle": 20,
    "charge-beam": 10,
    "dizzy-punch": 10,
    "shadow-punch": 20,
    "sky-uppercut": 15,
    "cotton-guard": 10,
    "mega-punch": 20,
    "razor-shell": 10,
    "comet-punch": 15,
    "morning-sun": 5,
    "extreme-speed": 5,
    "crush-claw": 10,
    "signal-beam": 15,
    "force-palm": 10,
    "lava-plume": 15,
    "quiver-dance": 20,
    "horn-leech": 10,
    "iron-tail": 15,
    "mega-kick": 5,
    "low-sweep": 20,
    "gunk-shot": 5,
    "sharpen": 30,
    "slack-off": 5,
    "inferno": 5,
    "heat-wave": 10,
    "drain-punch": 10,
    "steel-wing": 25,
    "sludge-wave": 10,
    "tail-slap": 10,
    "spike-cannon": 15,
    "boomburst": 10,
    "mach-punch": 30,
    "bone-rush": 10,
    "scald": 15,
    "electroweb": 15,
    "egg-bomb": 10,
    "milk-drink": 5,
    "tail-glow": 20,
    "meteor-mash": 10,
    "rock-wrecker": 5,
    "magnet-bomb": 20,
    "branch-poke": 40,
    "strength": 15,
    "solar-blade": 10,
    "barrage": 20,
    "fiery-dance": 10,
    "shore-up": 5,
    "fire-lash": 15,
    "spirit-break": 15,
    "focus-blast": 5,
    "smart-strike": 10,
    "vacuum-wave": 30,
    "luster-purge": 5,
    "mist-ball": 5,
}
