"""The HAND-WRITTEN half of Explore's wild areas. The other half is fetched.

    python3 tools/gen_wild.py       # this file + the games' data -> wild_areas.h
    python3 tools/verify_wild.py    # proves wild_areas.h still matches both

What lives here is only what the games' data cannot tell us: which maps make
one area, what an area is called on the panel, which badge opens it, and where
each legendary waits. Species, encounter rates and levels are NEVER typed here
-- they come from the disassemblies, so they cannot be misremembered. See
docs/explore-plan.md for the design this implements.

GATES are ladder positions, the same numbers the gym ladder uses: an area with
gate g opens once ladder trainer g-1 has been beaten (0 = open from the start,
1 = the first badge ... 8 = all eight badges). GATE_CHAMPION (13) is the
champion. Because the ladder is sequential, "trainer g-1 beaten" is the same
as "g badges held", on either difficulty.

Every map the source has encounters on must be either in an area or in SKIP
with a reason -- verify_wild.py fails otherwise, so nothing is dropped by
accident.
"""

GATE_CHAMPION = 13

# How the encounter methods of one area share it. Within a method the game's
# own slot rates stand; between methods these weights apply, renormalised over
# the methods the area actually has (owner-approved in the plan, § 12).
METHOD_WEIGHT = {'land': 1.0, 'water': 0.5, 'fishing': 0.3, 'rock_smash': 0.2}

# The longest area name the area list will be asked to draw.
NAME_MAX = 16

# ---------------------------------------------------------------------------
# KANTO -- FireRed + LeafGreen, averaged (a version exclusive comes in at half
# weight). The same games as the Kanto gym ladder. Gates follow FireRed's
# progression; town ponds and harbours ride with the route beside them so the
# list stays short.
KANTO = {
    'region': 0,
    'source': 'frlg',
    'areas': [
        # name               gate  maps
        ('ROUTE 1',          0, ['ROUTE1']),
        ('ROUTE 2',          0, ['ROUTE2']),
        ('ROUTE 22',         0, ['ROUTE22', 'VIRIDIAN_CITY']),
        ('VIRIDIAN FOREST',  0, ['VIRIDIAN_FOREST']),
        # Brock
        ('ROUTE 3',          1, ['ROUTE3']),
        ('MT. MOON',         1, ['MT_MOON_1F', 'MT_MOON_B1F', 'MT_MOON_B2F']),
        ('ROUTE 4',          1, ['ROUTE4']),
        ('ROUTES 24-25',     1, ['ROUTE24', 'ROUTE25', 'CERULEAN_CITY']),
        # Misty
        ('ROUTES 5-6',       2, ['ROUTE5', 'ROUTE6', 'VERMILION_CITY',
                                 'SSANNE_EXTERIOR']),
        ('ROUTE 11',         2, ['ROUTE11']),
        ('DIGLETTS CAVE',    2, ['DIGLETTS_CAVE_B1F']),
        # Lt. Surge
        ('ROUTES 7-8',       3, ['ROUTE7', 'ROUTE8']),
        ('ROUTES 9-10',      3, ['ROUTE9', 'ROUTE10']),
        ('ROCK TUNNEL',      3, ['ROCK_TUNNEL_1F', 'ROCK_TUNNEL_B1F']),
        # Erika
        ('POKEMON TOWER',    4, ['POKEMON_TOWER_3F', 'POKEMON_TOWER_4F',
                                 'POKEMON_TOWER_5F', 'POKEMON_TOWER_6F',
                                 'POKEMON_TOWER_7F']),
        ('ROUTES 12-15',     4, ['ROUTE12', 'ROUTE13', 'ROUTE14', 'ROUTE15']),
        ('ROUTES 16-18',     4, ['ROUTE16', 'ROUTE17', 'ROUTE18',
                                 'CELADON_CITY']),
        ('SAFARI ZONE',      4, ['SAFARI_ZONE_CENTER', 'SAFARI_ZONE_EAST',
                                 'SAFARI_ZONE_NORTH', 'SAFARI_ZONE_WEST',
                                 'FUCHSIA_CITY']),
        # Koga -- the games' Surf opens the sea and the islands on it
        ('SEAFOAM ISLANDS',  5, ['SEAFOAM_ISLANDS_1F', 'SEAFOAM_ISLANDS_B1F',
                                 'SEAFOAM_ISLANDS_B2F', 'SEAFOAM_ISLANDS_B3F',
                                 'SEAFOAM_ISLANDS_B4F']),
        ('POWER PLANT',      5, ['POWER_PLANT']),
        ('POKEMON MANSION',  5, ['POKEMON_MANSION_1F', 'POKEMON_MANSION_2F',
                                 'POKEMON_MANSION_3F', 'POKEMON_MANSION_B1F',
                                 'CINNABAR_ISLAND']),
        ('SEA ROUTES 19-21', 5, ['ROUTE19', 'ROUTE20', 'ROUTE21_NORTH',
                                 'ROUTE21_SOUTH', 'PALLET_TOWN']),
        # Blaine -- Sevii Islands 1-3 open after Cinnabar in FireRed
        ('KINDLE ROAD',      7, ['ONE_ISLAND_KINDLE_ROAD',
                                 'ONE_ISLAND_TREASURE_BEACH', 'ONE_ISLAND']),
        ('MT. EMBER',        7, ['MT_EMBER_EXTERIOR', 'MT_EMBER_RUBY_PATH_1F',
                                 'MT_EMBER_RUBY_PATH_B1F',
                                 'MT_EMBER_RUBY_PATH_B1F_STAIRS',
                                 'MT_EMBER_RUBY_PATH_B2F',
                                 'MT_EMBER_RUBY_PATH_B2F_STAIRS',
                                 'MT_EMBER_RUBY_PATH_B3F',
                                 'MT_EMBER_SUMMIT_PATH_1F',
                                 'MT_EMBER_SUMMIT_PATH_2F',
                                 'MT_EMBER_SUMMIT_PATH_3F']),
        ('BERRY FOREST',     7, ['THREE_ISLAND_BERRY_FOREST',
                                 'THREE_ISLAND_BOND_BRIDGE',
                                 'THREE_ISLAND_PORT']),
        ('CAPE BRINK',       7, ['TWO_ISLAND_CAPE_BRINK']),
        # Giovanni
        ('ROUTE 23',         8, ['ROUTE23']),
        ('VICTORY ROAD',     8, ['VICTORY_ROAD_1F', 'VICTORY_ROAD_2F',
                                 'VICTORY_ROAD_3F']),
        # After the champion -- Cerulean Cave and Sevii Islands 4-7
        ('CERULEAN CAVE',    GATE_CHAMPION, ['CERULEAN_CAVE_1F',
                                             'CERULEAN_CAVE_2F',
                                             'CERULEAN_CAVE_B1F']),
        ('ICEFALL CAVE',     GATE_CHAMPION, ['FOUR_ISLAND_ICEFALL_CAVE_ENTRANCE',
                                             'FOUR_ISLAND_ICEFALL_CAVE_1F',
                                             'FOUR_ISLAND_ICEFALL_CAVE_B1F',
                                             'FOUR_ISLAND_ICEFALL_CAVE_BACK',
                                             'FOUR_ISLAND']),
        ('LOST CAVE',        GATE_CHAMPION, ['FIVE_ISLAND_LOST_CAVE_ROOM%d' % i
                                             for i in range(1, 15)]),
        ('FIVE ISLAND',      GATE_CHAMPION, ['FIVE_ISLAND_MEADOW',
                                             'FIVE_ISLAND_MEMORIAL_PILLAR',
                                             'FIVE_ISLAND_RESORT_GORGEOUS',
                                             'FIVE_ISLAND_WATER_LABYRINTH',
                                             'FIVE_ISLAND']),
        ('SIX ISLAND',       GATE_CHAMPION, ['SIX_ISLAND_GREEN_PATH',
                                             'SIX_ISLAND_OUTCAST_ISLAND',
                                             'SIX_ISLAND_PATTERN_BUSH',
                                             'SIX_ISLAND_RUIN_VALLEY',
                                             'SIX_ISLAND_WATER_PATH']),
        ('ALTERING CAVE',    GATE_CHAMPION, ['SIX_ISLAND_ALTERING_CAVE']),
        ('SEVAULT CANYON',   GATE_CHAMPION, ['SEVEN_ISLAND_SEVAULT_CANYON',
                                             'SEVEN_ISLAND_SEVAULT_CANYON_ENTRANCE',
                                             'SEVEN_ISLAND_TRAINER_TOWER']),
        ('TANOBY RUINS',     GATE_CHAMPION, ['SEVEN_ISLAND_TANOBY_RUINS'] +
                                            ['SEVEN_ISLAND_TANOBY_RUINS_%s_CHAMBER' % c
                                             for c in ('DILFORD', 'LIPTOO',
                                                       'MONEAN', 'RIXY',
                                                       'SCUFIB', 'VIAPOIS',
                                                       'WEEPTH')]),
    ],
    # Maps with encounters that are deliberately NOT an area. Empty for Kanto:
    # every FireRed/LeafGreen map with wild creatures belongs to one.
    'skip': {},
    # The legendaries that live in this region, ONE HOME PER SPECIES. FireRed's
    # roaming Raikou/Entei/Suicune are Johto's and get their home there.
    # (dex, area, script map in the decomp, the level is READ from that script)
    'legends': [
        (144, 'SEAFOAM ISLANDS', 'SeafoamIslands_B4F'),   # Articuno
        (145, 'POWER PLANT',     'PowerPlant'),           # Zapdos
        # Moltres's summit has no wild table of its own, so Mt. Ember is the
        # area it borrows (docs/explore-plan.md § 7.2).
        (146, 'MT. EMBER',       'MtEmber_Summit'),       # Moltres
        (150, 'CERULEAN CAVE',   'CeruleanCave_B1F'),     # Mewtwo
    ],
}

REGIONS = [KANTO]

# WILD_LEGENDS[] is stored by INDEX in the save (the met/caught bitmasks), so
# the order legendaries are emitted in is APPEND-ONLY -- the same rule as move
# indices and avatar picks. gen_wild.py emits them region by region in the
# order above; a new region's legends must go AFTER every existing one, which
# gen_wild.py enforces against LEGEND_ORDER.
LEGEND_ORDER = [144, 145, 146, 150]
