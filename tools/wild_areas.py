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
                 'headbutt': 0.2,   # Crystal's trees: Heracross, Aipom and Pineco live nowhere else
                 'phenomenon': 0.2}  # Gen 5's shaking grass, dust clouds, ripples: Audino, Drilbur

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
    # The best rod's catch is held back until the badge the game hands the rod
    # out at, the same gate rule an area or a legendary has -- and, by the
    # owner's call, nothing on the panel says so. FireRed's Super Rod is the
    # fishing guru's brother on Route 12, past the Poke Flute: Erika's gate.
    # gen_wild.py checks the script really gives the rod.
    'super_rod': {'gate': 4, 'script': 'Route12_FishingHouse'},
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
    # Crystal's Super Rod is handed out on Kanto's Route 12, which a Johto
    # player only reaches after the champion.
    'super_rod': {'gate': GATE_CHAMPION, 'script': 'Route12SuperRodHouse'},
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

# ---------------------------------------------------------------------------
# HOENN -- Emerald alone (owner's call, Q-A), the same game as the Hoenn gym
# ladder. Emerald has no times of day and no Headbutt. Gates follow Emerald's
# progression and its HMs: Mr. Briney's boat to Dewford after Roxanne; Mauville
# after Brawly; Wattson's Basement Key for New Mauville, and the way north to
# Fallarbor and Mt. Chimney; the Go-Goggles after Flannery for the desert; Surf
# after Norman; Fortree's Winona opens Lilycove and the eastern sea; Dive after
# Tate and Liza; Waterfall after Juan. The postgame's Safari Zone extension,
# Desert Underpass, Artisan Cave, Altering Cave and Steven's cave open after the
# champion.
HOENN = {
    'region': 2,
    'source': 'emerald',
    # Emerald's Super Rod is in a Mossdeep house, reached after Winona.
    'super_rod': {'gate': 6, 'script': 'MossdeepCity_House3'},
    'areas': [
        ('ROUTE 101',        0, ['ROUTE101']),
        ('ROUTE 102',        0, ['ROUTE102']),
        ('ROUTE 103',        0, ['ROUTE103']),
        ('ROUTE 104',        0, ['ROUTE104', 'PETALBURG_CITY']),
        ('PETALBURG WOODS',  0, ['PETALBURG_WOODS']),
        ('ROUTE 115',        0, ['ROUTE115']),
        ('ROUTE 116',        0, ['ROUTE116']),
        ('RUSTURF TUNNEL',   0, ['RUSTURF_TUNNEL']),
        # Roxanne -- Mr. Briney sails to Dewford
        ('GRANITE CAVE',     1, ['GRANITE_CAVE_1F', 'GRANITE_CAVE_B1F', 'GRANITE_CAVE_B2F',
                                 'GRANITE_CAVE_STEVENS_ROOM', 'DEWFORD_TOWN']),
        # Brawly -- on to Slateport and Mauville
        ('ROUTE 110',        2, ['ROUTE110', 'SLATEPORT_CITY']),
        ('ROUTE 117',        2, ['ROUTE117']),
        ('ROUTE 118',        2, ['ROUTE118']),
        # Wattson -- the Basement Key, and the road north
        ('NEW MAUVILLE',     3, ['NEW_MAUVILLE_ENTRANCE', 'NEW_MAUVILLE_INSIDE']),
        ('ROUTE 112',        3, ['ROUTE112']),
        ('FIERY PATH',       3, ['FIERY_PATH']),
        ('ROUTES 113-114',   3, ['ROUTE113', 'ROUTE114']),
        ('METEOR FALLS',     3, ['METEOR_FALLS_1F_1R']),
        ('JAGGED PASS',      3, ['JAGGED_PASS']),
        # Flannery -- the Go-Goggles open the desert
        ('ROUTE 111 DESERT', 4, ['ROUTE111', 'MIRAGE_TOWER_1F', 'MIRAGE_TOWER_2F',
                                 'MIRAGE_TOWER_3F', 'MIRAGE_TOWER_4F']),
        # Norman -- Surf
        ('ROUTES 105-109',   5, ['ROUTE105', 'ROUTE106', 'ROUTE107', 'ROUTE108', 'ROUTE109']),
        ('ABANDONED SHIP',   5, ['ABANDONED_SHIP_ROOMS_B1F',
                                 'ABANDONED_SHIP_HIDDEN_FLOOR_CORRIDORS']),
        ('ROUTE 119',        5, ['ROUTE119']),
        ('ROUTE 120',        5, ['ROUTE120']),
        # Winona -- Lilycove, Mt. Pyre and the eastern sea
        ('ROUTE 121',        6, ['ROUTE121']),
        ('SAFARI ZONE',      6, ['SAFARI_ZONE_SOUTH', 'SAFARI_ZONE_SOUTHWEST',
                                 'SAFARI_ZONE_NORTH', 'SAFARI_ZONE_NORTHWEST']),
        ('MT. PYRE',         6, ['ROUTE122', 'MT_PYRE_1F', 'MT_PYRE_2F', 'MT_PYRE_3F',
                                 'MT_PYRE_4F', 'MT_PYRE_5F', 'MT_PYRE_6F',
                                 'MT_PYRE_EXTERIOR', 'MT_PYRE_SUMMIT']),
        ('ROUTE 123',        6, ['ROUTE123']),
        ('MAGMA HIDEOUT',    6, ['MAGMA_HIDEOUT_1F', 'MAGMA_HIDEOUT_2F_1R', 'MAGMA_HIDEOUT_2F_2R',
                                 'MAGMA_HIDEOUT_2F_3R', 'MAGMA_HIDEOUT_3F_1R',
                                 'MAGMA_HIDEOUT_3F_2R', 'MAGMA_HIDEOUT_3F_3R',
                                 'MAGMA_HIDEOUT_4F']),
        ('ROUTES 124-125',   6, ['ROUTE124', 'ROUTE125', 'LILYCOVE_CITY', 'MOSSDEEP_CITY']),
        ('SHOAL CAVE',       6, ['SHOAL_CAVE_LOW_TIDE_ENTRANCE_ROOM',
                                 'SHOAL_CAVE_LOW_TIDE_INNER_ROOM',
                                 'SHOAL_CAVE_LOW_TIDE_STAIRS_ROOM',
                                 'SHOAL_CAVE_LOW_TIDE_LOWER_ROOM',
                                 'SHOAL_CAVE_LOW_TIDE_ICE_ROOM']),
        # Tate and Liza -- Dive
        ('UNDERWATER',       7, ['UNDERWATER_ROUTE124', 'UNDERWATER_ROUTE126']),
        ('ROUTES 126-129',   7, ['ROUTE126', 'ROUTE127', 'ROUTE128', 'ROUTE129']),
        ('SEAFLOOR CAVERN',  7, ['SEAFLOOR_CAVERN_ENTRANCE'] +
                                ['SEAFLOOR_CAVERN_ROOM%d' % i for i in range(1, 9)]),
        ('CAVE OF ORIGIN',   7, ['SOOTOPOLIS_CITY', 'CAVE_OF_ORIGIN_ENTRANCE',
                                 'CAVE_OF_ORIGIN_1F']),
        ('ROUTES 130-134',   7, ['ROUTE130', 'ROUTE131', 'ROUTE132', 'ROUTE133',
                                 'ROUTE134', 'PACIFIDLOG_TOWN']),
        ('SKY PILLAR',       7, ['SKY_PILLAR_1F', 'SKY_PILLAR_3F', 'SKY_PILLAR_5F']),
        # Juan -- Waterfall
        ('METEOR FALLS B1F', 8, ['METEOR_FALLS_1F_2R', 'METEOR_FALLS_B1F_1R',
                                 'METEOR_FALLS_B1F_2R']),
        ('VICTORY ROAD',     8, ['VICTORY_ROAD_1F', 'VICTORY_ROAD_B1F', 'VICTORY_ROAD_B2F',
                                 'EVER_GRANDE_CITY']),
        # After the champion
        ('SAFARI EXTENSION', GATE_CHAMPION, ['SAFARI_ZONE_NORTHEAST', 'SAFARI_ZONE_SOUTHEAST']),
        ('DESERT UNDERPASS', GATE_CHAMPION, ['DESERT_UNDERPASS']),
        ('ARTISAN CAVE',     GATE_CHAMPION, ['ARTISAN_CAVE_1F', 'ARTISAN_CAVE_B1F']),
        ('ALTERING CAVE',    GATE_CHAMPION, ['ALTERING_CAVE']),
        ('STEVENS CAVE',     GATE_CHAMPION, ['METEOR_FALLS_STEVENS_CAVE']),
    ],
    'skip': {
        'CAVE_OF_ORIGIN_UNUSED_RUBY_SAPPHIRE_MAP1': 'left over from Ruby/Sapphire, unreachable in Emerald',
        'CAVE_OF_ORIGIN_UNUSED_RUBY_SAPPHIRE_MAP2': 'left over from Ruby/Sapphire, unreachable in Emerald',
        'CAVE_OF_ORIGIN_UNUSED_RUBY_SAPPHIRE_MAP3': 'left over from Ruby/Sapphire, unreachable in Emerald',
    },
    'legends': [
        # The Regis wait behind the Sealed Chamber, which needs Dive (Mind
        # Badge). Their chambers have no wild tables, so each borrows the area
        # its door is in (docs/explore-plan.md § 7.2).
        {'dex': 377, 'area': 'ROUTE 111 DESERT', 'script': 'DesertRuins', 'gate': 7},   # Regirock
        {'dex': 378, 'area': 'ROUTES 105-109',   'script': 'IslandCave',  'gate': 7},   # Regice
        {'dex': 379, 'area': 'ROUTE 120',        'script': 'AncientTomb', 'gate': 7},   # Registeel
        # Latias and Latios roam Hoenn after the champion; Emerald roams one
        # and offers the other on Southern Island (an event ticket), so both
        # roam here, first met off Southern Island's sea.
        {'dex': 380, 'area': 'ROUTES 130-134', 'roams': True, 'gate': GATE_CHAMPION},   # Latias
        {'dex': 381, 'area': 'ROUTES 130-134', 'roams': True, 'gate': GATE_CHAMPION},   # Latios
        # Emerald's Marine and Terra Caves surface after the champion on a
        # handful of routes; each borrows one of them.
        {'dex': 382, 'area': 'ROUTES 126-129', 'script': 'MarineCave_End',
         'gate': GATE_CHAMPION},                                                         # Kyogre
        {'dex': 383, 'area': 'ROUTES 113-114', 'script': 'TerraCave_End',
         'gate': GATE_CHAMPION},                                                         # Groudon
        {'dex': 384, 'area': 'SKY PILLAR', 'script': 'SkyPillar_Top',
         'gate': GATE_CHAMPION},                                                         # Rayquaza
    ],
}

# ---------------------------------------------------------------------------
# SINNOH -- Platinum alone (owner's call, Q-A), the same game as the Sinnoh gym
# ladder. pokeplatinum keeps one table per map in res/field/encounters/, named
# here without the "encounters_" prefix. Platinum has morning, day and night in
# its grass. Gates follow Platinum's progression and its HMs: Rock Smash after
# Roark opens Ravaged Path and the road north; Cut after Gardenia; Fantina's
# Hearthome opens the south and east; after Maylene, Route 214 and the Great
# Marsh; Surf from Celestic after Wake, with the SecretPotion clearing Route
# 210; Canalave's Byron opens Iron Island and the snowy north; after Candice,
# the lakes' caverns, Mt. Coronet's peak and the Distortion World; after
# Volkner, Victory Road. The Battle Zone and its routes, Stark Mountain and
# Snowpoint Temple are after the champion.
SINNOH = {
    'region': 3,
    'source': 'platinum',
    # Platinum's Super Rod is the Fight Area's fisherman, in the Battle Zone.
    'super_rod': {'gate': GATE_CHAMPION, 'script': 'fight_area'},
    'areas': [
        ('ROUTE 201',        0, ['ROUTE_201']),
        ('LAKE VERITY',      0, ['LAKE_VERITY', 'LAKE_VERITY_LOW_WATER']),
        ('ROUTE 202',        0, ['ROUTE_202']),
        ('ROUTE 203',        0, ['ROUTE_203']),
        ('OREBURGH GATE',    0, ['OREBURGH_GATE_1F', 'OREBURGH_GATE_B1F']),
        ('ROUTE 204',        0, ['ROUTE_204_SOUTH']),
        ('OREBURGH MINE',    0, ['OREBURGH_MINE_B1F', 'OREBURGH_MINE_B2F']),
        ('ROUTE 207',        0, ['ROUTE_207']),
        # Roark -- Rock Smash: Ravaged Path and the road to Eterna
        ('RAVAGED PATH',     1, ['RAVAGED_PATH']),
        ('ROUTE 204 NORTH',  1, ['ROUTE_204_NORTH']),
        ('VALLEY WINDWORKS', 1, ['VALLEY_WINDWORKS_OUTSIDE']),
        ('ROUTE 205',        1, ['ROUTE_205_SOUTH', 'ROUTE_205_NORTH']),
        ('ETERNA FOREST',    1, ['ETERNA_FOREST']),
        # Gardenia -- Cut, and the way south through Mt. Coronet
        ('OLD CHATEAU',      2, ['OLD_CHATEAU', 'OLD_CHATEAU_DINING_AREA',
                                 'OLD_CHATEAU_SIDE_ROOMS', 'OLD_CHATEAU_CORRIDOR',
                                 'OLD_CHATEAU_BACK_WEST_ROOM',
                                 'OLD_CHATEAU_BACK_MIDDLE_WEST_ROOM',
                                 'OLD_CHATEAU_BACK_MIDDLE_ROOM',
                                 'OLD_CHATEAU_BACK_MIDDLE_EAST_ROOM',
                                 'OLD_CHATEAU_BACK_EAST_ROOM']),
        ('ROUTE 211 WEST',   2, ['ROUTE_211_WEST']),
        ('MT. CORONET',      2, ['MT_CORONET_1F_SOUTH', 'MT_CORONET_1F_NORTH_ROOM_1']),
        ('ROUTE 206',        2, ['ROUTE_206']),
        ('WAYWARD CAVE',     2, ['WAYWARD_CAVE_1F', 'WAYWARD_CAVE_B1F']),
        ('ROUTE 208',        2, ['ROUTE_208']),
        # Fantina -- Hearthome's roads south and east
        ('ROUTE 209',        3, ['ROUTE_209']),
        ('LOST TOWER',       3, ['ROUTE_209_LOST_TOWER_%dF' % i for i in range(1, 6)]),
        ('SOLACEON RUINS',   3, ['SOLACEON_RUINS_ROOM_1_NORTHWEST_DEAD_END',
                                 'SOLACEON_RUINS_ROOM_1_SOUTHEAST_DEAD_END',
                                 'SOLACEON_RUINS_ROOM_2',
                                 'SOLACEON_RUINS_ROOM_2_NORTHEAST_DEAD_END',
                                 'SOLACEON_RUINS_ROOM_2_SOUTHEAST_DEAD_END',
                                 'SOLACEON_RUINS_ROOM_3',
                                 'SOLACEON_RUINS_ROOM_3_NORTHWEST_DEAD_END',
                                 'SOLACEON_RUINS_ROOM_3_SOUTHWEST_DEAD_END',
                                 'SOLACEON_RUINS_ROOM_4',
                                 'SOLACEON_RUINS_ROOM_4_SOUTHEAST_DEAD_END',
                                 'SOLACEON_RUINS_ROOM_5',
                                 'SOLACEON_RUINS_ROOM_5_SOUTHEAST_DEADEND',
                                 'SOLACEON_RUINS_ROOM_5_SOUTHWEST_DEAD_END',
                                 'SOLACEON_RUINS_ROOM_6',
                                 'SOLACEON_RUINS_ROOM_6_NORTHWEST_DEAD_END',
                                 'SOLACEON_RUINS_ROOM_6_SOUTHEAST_DEAD_END',
                                 'SOLACEON_RUINS_ROOM_7']),
        ('ROUTE 210 SOUTH',  3, ['ROUTE_210_SOUTH']),
        ('ROUTE 215',        3, ['ROUTE_215']),
        ('ROUTE 212',        3, ['ROUTE_212_NORTH', 'ROUTE_212_SOUTH']),
        ('TROPHY GARDEN',    3, ['TROPHY_GARDEN']),
        # Maylene -- Route 214 and the coast to Pastoria
        ('ROUTE 214',        4, ['ROUTE_214']),
        ('RUIN MANIAC CAVE', 4, ['RUIN_MANIAC_CAVE_SHORT', 'RUIN_MANIAC_CAVE_LONG',
                                 'MANIAC_TUNNEL', 'SOLACEON_RUINS_MANIAC_TUNNEL_ROOM']),
        ('VALOR LAKEFRONT',  4, ['VALOR_LAKEFRONT']),
        ('ROUTE 213',        4, ['ROUTE_213']),
        ('GREAT MARSH',      4, ['GREAT_MARSH_%d' % i for i in range(1, 7)] + ['PASTORIA_CITY']),
        # Wake -- the SecretPotion clears Route 210, and Celestic gives Surf
        ('ROUTE 210 NORTH',  5, ['ROUTE_210_NORTH', 'CELESTIC_TOWN']),
        ('ROUTE 211 EAST',   5, ['ROUTE_211_EAST']),
        ('ROUTE 218',        5, ['ROUTE_218', 'CANALAVE_CITY']),
        ('FUEGO IRONWORKS',  5, ['FUEGO_IRONWORKS_OUTSIDE']),
        ('ETERNA CITY',      5, ['ETERNA_CITY']),
        ('ROUTES 219-221',   5, ['ROUTE_219', 'ROUTE_220', 'ROUTE_221', 'TWINLEAF_TOWN']),
        # Byron -- Iron Island, Lake Valor's crater, and the snowy north
        ('IRON ISLAND',      6, ['IRON_ISLAND', 'IRON_ISLAND_1F', 'IRON_ISLAND_B1F_LEFT_ROOM',
                                 'IRON_ISLAND_B1F_RIGHT_ROOM', 'IRON_ISLAND_B2F_LEFT_ROOM',
                                 'IRON_ISLAND_B2F_RIGHT_ROOM', 'IRON_ISLAND_B3F']),
        ('LAKE VALOR',       6, ['LAKE_VALOR']),
        ('ROUTES 216-217',   6, ['ROUTE_216', 'ROUTE_217', 'ACUITY_LAKEFRONT']),
        # Candice -- Lake Acuity, Mt. Coronet's peak, Sendoff Spring
        ('LAKE ACUITY',      7, ['LAKE_ACUITY']),
        ('MT. CORONET PEAK', 7, ['MT_CORONET_1F_NORTH_ROOM_2', 'MT_CORONET_1F_TUNNEL_ROOM',
                                 'MT_CORONET_B1F', 'MT_CORONET_2F', 'MT_CORONET_3F',
                                 'MT_CORONET_4F_ROOMS_1_AND_2', 'MT_CORONET_4F_ROOM_3',
                                 'MT_CORONET_5F', 'MT_CORONET_6F',
                                 'MT_CORONET_OUTSIDE_NORTH', 'MT_CORONET_OUTSIDE_SOUTH']),
        ('SENDOFF SPRING',   7, ['SENDOFF_SPRING']),
        # Turnback Cave's rooms. UNKNOWN_533-557 are unnamed in the decomp, but
        # two sources put them here: the decomp's map headers UNKNOWN_533-557
        # carry Turnback Cave's place name, and PokeAPI's Platinum data has the
        # same table (L45-48 Golbat, Haunter, Dusclops, Chimecho, Bronzong) as
        # Turnback Cave "after pillar 3". (The decomp's headers link no table
        # to those rooms or to the entrance; PokeAPI says they are met.)
        ('TURNBACK CAVE',    7, ['TURNBACK_CAVE_ENTRANCE', 'TURNBACK_CAVE_PILLAR_ROOM',
                                 'TURNBACK_CAVE_GIRATINA_ROOM'] +
                                ['TURNBACK_CAVE_PILLAR_%d_ROOM_%d' % (p, r)
                                 for p in (1, 2, 3) for r in range(1, 7)] +
                                ['UNKNOWN_%d' % i for i in range(533, 558)]),
        ('ROUTE 222',        7, ['ROUTE_222', 'SUNYSHORE_CITY']),
        # Volkner -- Victory Road and the League
        ('VICTORY ROAD',     8, ['VICTORY_ROAD_1F', 'VICTORY_ROAD_2F', 'VICTORY_ROAD_B1F']),
        ('ROUTE 223',        8, ['ROUTE_223', 'POKEMON_LEAGUE']),
        # After the champion -- Victory Road's back rooms, Route 224, the
        # Battle Zone, Stark Mountain and Snowpoint Temple
        ('VICTORY RD BACK',  GATE_CHAMPION, ['VICTORY_ROAD_1F_ROOM_1', 'VICTORY_ROAD_1F_ROOM_2',
                                             'VICTORY_ROAD_1F_ROOM_3']),
        ('ROUTE 224',        GATE_CHAMPION, ['ROUTE_224']),
        ('ROUTES 225-226',   GATE_CHAMPION, ['ROUTE_225', 'ROUTE_226']),
        ('ROUTES 227-228',   GATE_CHAMPION, ['ROUTE_227', 'ROUTE_228']),
        ('STARK MOUNTAIN',   GATE_CHAMPION, ['STARK_MOUNTAIN_OUTSIDE', 'STARK_MOUNTAIN_ROOM_1',
                                             'STARK_MOUNTAIN_ROOM_2']),
        ('ROUTES 229-230',   GATE_CHAMPION, ['ROUTE_229', 'ROUTE_230', 'RESORT_AREA']),
        ('SNOWPOINT TEMPLE', GATE_CHAMPION, ['SNOWPOINT_TEMPLE_1F', 'SNOWPOINT_TEMPLE_B1F',
                                             'SNOWPOINT_TEMPLE_B2F', 'SNOWPOINT_TEMPLE_B3F',
                                             'SNOWPOINT_TEMPLE_B4F', 'SNOWPOINT_TEMPLE_B5F']),
    ],
    'skip': {},
    'legends': [
        # The lake guardians wait in caverns with no wild tables, so each
        # borrows its lake (docs/explore-plan.md § 7.2). Platinum opens them
        # after the Distortion World, which follows Candice's badge; Mesprit
        # flees and roams Sinnoh once met.
        {'dex': 480, 'area': 'LAKE ACUITY', 'script': 'acuity_cavern', 'gate': 7},       # Uxie
        {'dex': 481, 'area': 'LAKE VERITY', 'roams': True, 'gate': 7},                  # Mesprit
        {'dex': 482, 'area': 'LAKE VALOR',  'script': 'valor_cavern',  'gate': 7},       # Azelf
        # Dialga and Palkia return to Spear Pillar after the champion, which
        # has no wild table: it borrows the peak it stands on.
        {'dex': 483, 'area': 'MT. CORONET PEAK', 'script': 'spear_pillar_dialga',
         'gate': GATE_CHAMPION},                                                         # Dialga
        {'dex': 484, 'area': 'MT. CORONET PEAK', 'script': 'spear_pillar_palkia',
         'gate': GATE_CHAMPION},                                                         # Palkia
        {'dex': 485, 'area': 'STARK MOUNTAIN', 'script': 'stark_mountain_room_3'},      # Heatran
        # Platinum's script fights Regigigas at level 1 (Diamond/Pearl's is 70).
        # 'level' overrides the script by the owner's call (2026-10-21): 70,
        # Diamond/Pearl's level. gen_wild.py still reads the script, so the
        # game's own value stays on record in verify_wild.py's report.
        {'dex': 486, 'area': 'SNOWPOINT TEMPLE', 'script': 'snowpoint_temple_b5f',
         'level': 70},                                                                   # Regigigas
        # Giratina is met in the Distortion World after Candice; it waits in
        # Turnback Cave if not caught there, so that is its home.
        {'dex': 487, 'area': 'TURNBACK CAVE', 'script': 'turnback_cave_giratina_room'},  # Giratina
        # Cresselia flees Fullmoon Island, which has no wild table, and roams;
        # it borrows Canalave's coast, where the boat leaves from.
        {'dex': 488, 'area': 'ROUTE 218', 'roams': True, 'gate': GATE_CHAMPION},        # Cresselia
    ],
}

# ---------------------------------------------------------------------------
# UNOVA -- Black 2 + White 2, averaged (the same games as the Unova gym ladder).
# The first region with NO disassembly: species, rates and levels come from
# PokeAPI's location-area encounters (gen_wild.load_pokeapi), and the whole
# region is NOT VERIFIED -- the gym ladder's own Unova rosters are the same.
# A "map" is PokeAPI's location/area name. Gen 5 has seasons, not times of
# day; they are averaged. Gates follow B2W2's progression: Cheren after the
# Floccesy start; Roxie's Virbank; Burgh's Castelia, then Route 4 and the
# desert; Elesa's Nimbasa and Route 5; Clay's Driftveil opens Route 6, the
# Relic Passage and Chargestone; Skyla's Mistralton opens Route 7, Lentimas,
# Reversal Mountain and Undella; Drayden's Opelucid opens Route 9 and
# Humilau; Marlon's badge opens Route 22, the Giant Chasm, Seaside Cave and
# Victory Road. B2W2's post-game is Black/White's old Unova: Routes 1-3, the
# Dreamyard, Pinwheel Forest, Icirrus, Twist Mountain, Dragonspiral Tower.
UNOVA = {
    'region': 4,
    'source': 'pokeapi-b2w2',
    'versions': ['black-2', 'white-2'],
    'pokeapi_region': 5,
    # B2W2's only rod: Cedric Juniper gives it in Nuvema Town's lab after the
    # Hall of Fame (dittobase.com and the GameFAQs/PokeBase answers agree;
    # Black/White's Looker gift does not apply to the sequels).
    'super_rod': {'gate': GATE_CHAMPION,
                  'note': 'Cedric Juniper, Nuvema Town, after the Hall of Fame'},
    'areas': [
        ('ROUTE 19',         0, ['unova-route-19', 'aspertia-city']),
        ('ROUTE 20',         0, ['unova-route-20']),
        ('FLOCCESY RANCH',   0, ['floccesy-ranch/inner', 'floccesy-ranch/outer']),
        # Cheren -- Virbank
        ('VIRBANK COMPLEX',  1, ['virbank-complex/inner', 'virbank-complex/outer',
                                 'virbank-city']),
        # Roxie -- Castelia
        ('CASTELIA CITY',    2, ['castelia-city']),
        ('CASTELIA SEWERS',  2, ['castelia-sewers', 'castelia-sewers/unknown-area-38',
                                 'relic-passage/castelia-sewers-entrance']),
        # Burgh -- Route 4, the desert, Nimbasa's side roads
        ('ROUTE 4',          3, ['unova-route-4']),
        ('DESERT RESORT',    3, ['desert-resort/entrance', 'desert-resort']),
        ('RELIC CASTLE',     3, ['relic-castle/a', 'relic-castle/c', 'relic-castle/d']),
        ('ROUTE 16',         3, ['unova-route-16']),
        ('LOSTLORN FOREST',  3, ['lostlorn-forest']),
        # Elesa -- Route 5 to Driftveil
        ('ROUTE 5',          4, ['unova-route-5', 'driftveil-drawbridge']),
        # Clay -- Route 6, the Relic Passage, Chargestone Cave
        ('ROUTE 6',          5, ['unova-route-6']),
        ('RELIC PASSAGE',    5, ['relic-passage/pwt-entrance', 'relic-passage/relic-castle-entrance']),
        ('CHARGESTONE CAVE', 5, ['chargestone-cave/1f', 'chargestone-cave/b1f',
                                 'chargestone-cave/b2f']),
        # Skyla -- Route 7, Lentimas, Reversal Mountain, Undella and the east
        ('ROUTE 7',          6, ['unova-route-7']),
        ('CELESTIAL TOWER',  6, ['celestial-tower/2f', 'celestial-tower/3f',
                                 'celestial-tower/4f', 'celestial-tower/5f']),
        ('MISTRALTON CAVE',  6, ['mistralton-cave', 'guidance-chamber']),
        ('REVERSAL MTN',     6, ['reversal-mountain/unknown-area-%d' % i for i in range(48, 61)]),
        ('STRANGE HOUSE',    6, ['strange-house/1f', 'strange-house/b1f']),
        ('UNDELLA BAY',      6, ['undella-town', 'undella-bay']),
        ('ROUTE 13',         6, ['unova-route-13']),
        ('ROUTE 14',         6, ['unova-route-14', 'abundant-shrine']),
        ('ROUTE 12',         6, ['unova-route-12']),
        ('ROUTE 11',         6, ['unova-route-11', 'village-bridge']),
        # Drayden -- Route 9 and Humilau
        ('ROUTE 9',          7, ['unova-route-9']),
        ('ROUTE 21',         7, ['unova-route-21', 'humilau-city']),
        # Marlon -- Route 22, the Giant Chasm, Seaside Cave, Victory Road
        ('ROUTE 22',         8, ['unova-route-22']),
        ('GIANT CHASM',      8, ['giant-chasm', 'giant-chasm/outside', 'giant-chasm/forest',
                                 'giant-chasm/forest-cave']),
        ('SEASIDE CAVE',     8, ['seaside-cave/1f', 'seaside-cave/b1f']),
        ('ROUTE 23',         8, ['unova-route-23']),
        ('VICTORY ROAD',     8, ['unova-victory-road/7f'] +
                                ['unova-victory-road-2/unknown-area-%d' % i for i in range(71, 81)]),
        # After the champion -- Black and White's Unova
        ('ROUTES 1-3',       GATE_CHAMPION, ['unova-route-1', 'unova-route-2', 'unova-route-3']),
        ('DREAMYARD',        GATE_CHAMPION, ['dreamyard', 'dreamyard/b1f', 'striaton-city']),
        ('PINWHEEL FOREST',  GATE_CHAMPION, ['pinwheel-forest/inside', 'pinwheel-forest/outside']),
        ('NATURE SANCTUARY', GATE_CHAMPION, ['nature-sanctuary']),
        ('ROUTES 17-18',     GATE_CHAMPION, ['unova-route-17', 'unova-route-18', 'p2-laboratory']),
        ('ROUTE 15',         GATE_CHAMPION, ['unova-route-15', 'marvelous-bridge']),
        ('ICIRRUS CITY',     GATE_CHAMPION, ['icirrus-city', 'moor-of-icirrus', 'unova-route-8']),
        ('TWIST MOUNTAIN',   GATE_CHAMPION, ['twist-mountain/b1f-3f']),
        ('DRAGONSPIRAL',     GATE_CHAMPION, ['dragonspiral-tower/outside', 'dragonspiral-tower/entrance',
                                             'dragonspiral-tower/1f', 'dragonspiral-tower/2f']),
        ('CLAY TUNNEL',      GATE_CHAMPION, ['clay-tunnel', 'underground-ruins', 'rocky-mountain-room',
                                             'glacier-room', 'iron-room']),
        ('WELLSPRING CAVE',  GATE_CHAMPION, ['wellspring-cave']),
    ],
    'skip': {},
    # 'static' names the PokeAPI location area of the legendary's static
    # encounter; its level is read from there. Tornadus, Thundurus and
    # Landorus have no encounter in B2W2 (the 3DS Dream Radar only), so they
    # stay rare egg spawns, like a mythical (plan § 8).
    'legends': [
        {'dex': 638, 'area': 'ROUTE 13', 'static': 'unova-route-13'},                    # Cobalion
        {'dex': 639, 'area': 'ROUTE 22', 'static': 'unova-route-22'},                    # Terrakion
        {'dex': 640, 'area': 'ROUTE 11', 'static': 'unova-route-11'},                    # Virizion
        # One per version in B2W2; both live here, after the champion.
        {'dex': 643, 'area': 'DRAGONSPIRAL', 'static': 'dragonspiral-tower/7f',
         'gate': GATE_CHAMPION},                                                          # Reshiram
        {'dex': 644, 'area': 'DRAGONSPIRAL', 'static': 'dragonspiral-tower/7f',
         'gate': GATE_CHAMPION},                                                          # Zekrom
        {'dex': 646, 'area': 'GIANT CHASM', 'static': 'giant-chasm/forest-cave',
         'gate': GATE_CHAMPION},                                                          # Kyurem
    ],
}

REGIONS = [KANTO, JOHTO, HOENN, SINNOH, UNOVA]

# WILD_LEGENDS[] is stored by INDEX in the save (the met/caught bitmasks), so
# the order legendaries are emitted in is APPEND-ONLY -- the same rule as move
# indices and avatar picks. gen_wild.py emits them region by region in the
# order above; a new region's legends must go AFTER every existing one, which
# gen_wild.py enforces against LEGEND_ORDER.
LEGEND_ORDER = [144, 145, 146, 150, 243, 244, 245, 249, 250,
                377, 378, 379, 380, 381, 382, 383, 384,
                480, 481, 482, 483, 484, 485, 486, 487, 488,
                638, 639, 640, 643, 644, 646]
