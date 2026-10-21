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
METHOD_WEIGHT = {'land': 1.0, 'water': 0.5, 'fishing': 0.3, 'rock_smash': 0.2,
                 'headbutt': 0.2}   # Crystal's trees: Heracross, Aipom and Pineco live nowhere else

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
    # The level is READ from the game's own script ('script' is its map), never
    # typed. 'gate' defaults to the home area's; 'roams' marks a legendary that,
    # once met, can turn up in any open area of its region.
    'legends': [
        {'dex': 144, 'area': 'SEAFOAM ISLANDS', 'script': 'SeafoamIslands_B4F'},  # Articuno
        {'dex': 145, 'area': 'POWER PLANT',     'script': 'PowerPlant'},          # Zapdos
        # Moltres's summit has no wild table of its own, so Mt. Ember is the
        # area it borrows (docs/explore-plan.md § 7.2).
        {'dex': 146, 'area': 'MT. EMBER',       'script': 'MtEmber_Summit'},      # Moltres
        {'dex': 150, 'area': 'CERULEAN CAVE',   'script': 'CeruleanCave_B1F'},    # Mewtwo
    ],
}

# ---------------------------------------------------------------------------
# JOHTO -- Crystal alone (owner's call, Q-A): it is what pret's pokecrystal
# disassembly holds, so it stays verifiable, and it already carries nearly every
# species Gold and Silver had. Crystal has times of day, and they come through:
# a Hoothoot on Route 29 is a night creature here too. Gates follow Crystal's
# progression and its HMs: Cut after Bugsy (Ilex Forest), the SquirtBottle after
# Whitney (Route 36), Surf with the Fog Badge, Whirlpool with the Glacier Badge
# (Whirl Islands). Routes 26-28, Tohjo Falls and Victory Road live in Crystal's
# KANTO files but are the road to Johto's own Elite Four, so they are Johto's.
JOHTO = {
    'region': 1,
    'source': 'crystal',
    'areas': [
        ('ROUTE 29',         0, ['ROUTE_29', 'NEW_BARK_TOWN']),
        ('ROUTE 30',         0, ['ROUTE_30', 'CHERRYGROVE_CITY']),
        ('ROUTE 31',         0, ['ROUTE_31', 'VIOLET_CITY']),
        ('ROUTE 46',         0, ['ROUTE_46']),
        ('SPROUT TOWER',     0, ['SPROUT_TOWER_2F', 'SPROUT_TOWER_3F']),
        ('DARK CAVE',        0, ['DARK_CAVE_VIOLET_ENTRANCE']),
        # Falkner
        ('ROUTE 32',         1, ['ROUTE_32']),
        ('RUINS OF ALPH',    1, ['RUINS_OF_ALPH_OUTSIDE', 'RUINS_OF_ALPH_INNER_CHAMBER']),
        ('UNION CAVE',       1, ['UNION_CAVE_1F', 'UNION_CAVE_B1F']),
        ('ROUTE 33',         1, ['ROUTE_33', 'AZALEA_TOWN']),
        ('SLOWPOKE WELL',    1, ['SLOWPOKE_WELL_B1F', 'SLOWPOKE_WELL_B2F']),
        # Bugsy -- Cut opens Ilex Forest
        ('ILEX FOREST',      2, ['ILEX_FOREST']),
        ('ROUTE 34',         2, ['ROUTE_34']),
        ('ROUTE 35',         2, ['ROUTE_35']),
        ('NATIONAL PARK',    2, ['NATIONAL_PARK']),
        # Whitney -- the SquirtBottle moves the Sudowoodo off Route 36
        ('ROUTES 36-37',     3, ['ROUTE_36', 'ROUTE_37']),
        ('BURNED TOWER',     3, ['BURNED_TOWER_1F', 'BURNED_TOWER_B1F', 'ECRUTEAK_CITY']),
        ('ROUTES 38-39',     3, ['ROUTE_38', 'ROUTE_39']),
        # Morty -- the Fog Badge lets Surf be used
        ('ROUTES 40-41',     4, ['ROUTE_40', 'ROUTE_41', 'OLIVINE_CITY',
                                 'OLIVINE_PORT', 'CIANWOOD_CITY']),
        ('ROUTE 42',         4, ['ROUTE_42']),
        ('MT. MORTAR',       4, ['MOUNT_MORTAR_1F_OUTSIDE', 'MOUNT_MORTAR_1F_INSIDE',
                                 'MOUNT_MORTAR_2F_INSIDE', 'MOUNT_MORTAR_B1F']),
        ('ROUTE 43',         4, ['ROUTE_43']),
        ('LAKE OF RAGE',     4, ['LAKE_OF_RAGE']),
        ('UNION CAVE B2F',   4, ['UNION_CAVE_B2F']),
        # Pryce -- Mahogany opens Route 44 and the Ice Path; the Glacier Badge
        # lets Whirlpool be used
        ('ROUTE 44',         7, ['ROUTE_44']),
        ('ICE PATH',         7, ['ICE_PATH_1F', 'ICE_PATH_B1F', 'ICE_PATH_B2F_MAHOGANY_SIDE',
                                 'ICE_PATH_B2F_BLACKTHORN_SIDE', 'ICE_PATH_B3F']),
        ('ROUTE 45',         7, ['ROUTE_45', 'BLACKTHORN_CITY']),
        ('DARK CAVE NORTH',  7, ['DARK_CAVE_BLACKTHORN_ENTRANCE']),
        ('WHIRL ISLANDS',    7, ['WHIRL_ISLAND_NW', 'WHIRL_ISLAND_NE', 'WHIRL_ISLAND_SW',
                                 'WHIRL_ISLAND_CAVE', 'WHIRL_ISLAND_SE', 'WHIRL_ISLAND_B1F',
                                 'WHIRL_ISLAND_B2F', 'WHIRL_ISLAND_LUGIA_CHAMBER']),
        # Clair
        ('DRAGONS DEN',      8, ['DRAGONS_DEN_B1F']),
        ('ROUTES 26-27',     8, ['ROUTE_26', 'ROUTE_27']),
        ('TOHJO FALLS',      8, ['TOHJO_FALLS']),
        ('VICTORY ROAD',     8, ['VICTORY_ROAD']),
        # After the champion -- Tin Tower's upper floors and Mt. Silver
        ('TIN TOWER',        GATE_CHAMPION, ['TIN_TOWER_%dF' % i for i in range(2, 10)]),
        ('MT. SILVER',       GATE_CHAMPION, ['ROUTE_28', 'SILVER_CAVE_OUTSIDE',
                                             'SILVER_CAVE_ROOM_1', 'SILVER_CAVE_ROOM_2',
                                             'SILVER_CAVE_ROOM_3', 'SILVER_CAVE_ITEM_ROOMS']),
    ],
    # Johto maps the source has encounters on that are deliberately not here.
    'skip': {},
    'legends': [
        # Raikou and Entei flee the Burned Tower and roam Johto from then on --
        # met there once, then anywhere in Johto now and then until caught.
        {'dex': 243, 'area': 'BURNED TOWER', 'roams': True},              # Raikou
        {'dex': 244, 'area': 'BURNED TOWER', 'roams': True},              # Entei
        # Suicune waits in Tin Tower's first floor once the Clear Bell is had,
        # after the Radio Tower (seven badges). That floor has no wild table, so
        # it borrows Ecruteak's other tower (§ 7.2).
        {'dex': 245, 'area': 'BURNED TOWER', 'script': 'TinTower1F', 'gate': 7},  # Suicune
        # Lugia needs the Silver Wing and Ho-Oh the Rainbow Wing, both after the
        # champion in Crystal.
        {'dex': 249, 'area': 'WHIRL ISLANDS', 'script': 'WhirlIslandLugiaChamber',
         'gate': GATE_CHAMPION},                                          # Lugia
        {'dex': 250, 'area': 'TIN TOWER', 'script': 'TinTowerRoof'},      # Ho-Oh
    ],
}

REGIONS = [KANTO, JOHTO]

# WILD_LEGENDS[] is stored by INDEX in the save (the met/caught bitmasks), so
# the order legendaries are emitted in is APPEND-ONLY -- the same rule as move
# indices and avatar picks. gen_wild.py emits them region by region in the
# order above; a new region's legends must go AFTER every existing one, which
# gen_wild.py enforces against LEGEND_ORDER.
LEGEND_ORDER = [144, 145, 146, 150, 243, 244, 245, 249, 250]
