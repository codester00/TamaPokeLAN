#include "wild.h"
#include "pet.h"     // regionAvailable(), regionOfDex()
#include "noart.h"   // speciesHasArt()
#include "wild_areas.h"

static_assert(WILD_REGIONS == REGION_COUNT - 1,
              "wild_areas.h was generated for a different region list -- re-run tools/gen_wild.py");
// Area indices are uint16_t end to end (CLAUDE.md trap 2): Kanto has 36 areas
// and nine regions pass 255. These fail the build if any link narrows again.
static_assert(sizeof(WILD_REGION_AREAS[0][0]) >= 2, "WILD_REGION_AREAS must hold an area index past 255");
static_assert(sizeof(((WildLegend *)0)->area) >= 2, "WildLegend::area must hold an area index past 255");
static_assert(sizeof(decltype(wildAreaFirst(0))) >= 2, "wildAreaFirst() must return an area index past 255");
static_assert(WILD_AREA_COUNT < WILD_NO_AREA, "WILD_NO_AREA is a real area");

static uint8_t shinyBonus(uint8_t bonus) {
  return bonus > WILD_SHINY_BONUS_MAX ? WILD_SHINY_BONUS_MAX : bonus;
}

uint32_t wildShinyThreshold(uint8_t bonus) {
  return WILD_SHINY_BASE + (uint32_t)shinyBonus(bonus) * WILD_SHINY_PER_POINT;
}

bool wildShinyForRoll(uint32_t roll, uint8_t bonus) {
  return (roll % WILD_SHINY_SCALE) < wildShinyThreshold(bonus);
}

void wildApplyShiny(bool shiny, uint8_t &ivAtk, uint8_t &ivDef,
                    uint8_t &ivSpe, uint8_t &ivHp) {
  if (!shiny) return;
  uint8_t *ivs[] = { &ivAtk, &ivDef, &ivSpe, &ivHp };
  for (uint8_t *iv : ivs)
    if (*iv < WILD_SHINY_IV_FLOOR) *iv = WILD_SHINY_IV_FLOOR;
}

uint8_t wildCaptureChance(uint8_t rarity, uint16_t hp, uint16_t maxHp,
                          bool hasStatus, int16_t ballPct) {
  if (!maxHp || !hp) return 0;
  if (ballPct == ITEM_CATCH_GUARANTEED) return 100;
  if (ballPct <= 0) return 0;
  if (hp > maxHp) hp = maxHp;
  uint8_t base;
  switch (rarity) {
    case R_LEGENDARIO: base = 5; break;
    case R_RARO:       base = 18; break;
    case R_EVO:        base = 24; break;
    default:           base = 30; break;
  }
  // A full-health target is 100; an almost-fainted one is 180. Weakening it
  // matters, but never as much as the ball you chose.
  uint16_t hpFactor = (uint16_t)(100U + (uint32_t)(maxHp - hp) * 80U / maxHp);
  uint16_t statusFactor = hasStatus ? 130 : 100;
  uint32_t chance = (uint32_t)base * hpFactor * statusFactor *
                    (uint32_t)(uint16_t)ballPct / 1000000U;
  if (!chance) chance = 1;
  return chance > 95 ? 95 : (uint8_t)chance;
}

uint8_t wildLevelMin(uint8_t playerLevel) {
  return playerLevel > WILD_LEVEL_SPREAD
             ? (uint8_t)(playerLevel - WILD_LEVEL_SPREAD) : 1;
}

uint8_t wildLevelMax(uint8_t playerLevel) {
  uint16_t top = (uint16_t)playerLevel + WILD_LEVEL_SPREAD;
  return top > MAX_LEVEL ? MAX_LEVEL : (uint8_t)top;
}

uint8_t wildDropCount(bool hard, uint8_t bonusRoll) {
  return (uint8_t)((hard ? 2 : 1) +
                   (bonusRoll < WILD_BONUS_DROP_CHANCE ? 1 : 0));
}

uint8_t wildEscapeChance(uint8_t playerLevel, uint8_t foeLevel) {
  if (!foeLevel || playerLevel >= foeLevel) return 90;
  uint8_t chance = (uint8_t)((uint16_t)90 * playerLevel / foeLevel);
  return chance < 10 ? 10 : chance;
}

uint8_t wildFoeEscapeChance(uint16_t hp, uint16_t maxHp) {
  if (!maxHp || !hp) return 0;
  if (hp > maxHp) hp = maxHp;
  uint32_t scaled = (uint32_t)hp * 100U;
  if (scaled <= (uint32_t)maxHp * 10U) return 10;
  if (scaled <= (uint32_t)maxHp * 20U) {
    uint32_t above = scaled - (uint32_t)maxHp * 10U;
    return (uint8_t)(10U + above * 10U / ((uint32_t)maxHp * 10U));
  }
  if (scaled <= (uint32_t)maxHp * 40U) {
    uint32_t below = (uint32_t)maxHp * 40U - scaled;
    return (uint8_t)(10U + below * 10U / ((uint32_t)maxHp * 20U));
  }
  return 0;
}

uint8_t wildTierForRoll(uint8_t roll) {
  if (roll < 1) return R_LEGENDARIO;   // 1%
  if (roll < 8) return R_RARO;         // 7%
  if (roll < 30) return R_EVO;         // 22%
  return R_COMUN;                      // the rest
}

int16_t wildPickSpecies(uint8_t region, uint8_t tier, uint32_t roll) {
  uint16_t lo = 1, hi = DEX_COUNT;
  if (region < REGION_COUNT && region != REGION_ALL) {
    lo = REGIONS[region].lo;
    hi = REGIONS[region].hi;
  }
  if (hi > DEX_COUNT) hi = DEX_COUNT;
  // Two passes rather than a candidate array: DEX_COUNT is 809 and an int16_t
  // buffer of that size is 1.6 KB of stack on a device whose framebuffer
  // already lives in PSRAM.
  uint16_t n = 0;
  for (uint16_t d = lo; d <= hi; d++) {
    if (DEX_TBL[d].rarity != tier) continue;
    if (!speciesHasArt((int16_t)d)) continue;
    if (!regionAvailable(regionOfDex((int16_t)d))) continue;
    n++;
  }
  if (!n) return 0;
  uint16_t want = (uint16_t)(roll % n);
  for (uint16_t d = lo; d <= hi; d++) {
    if (DEX_TBL[d].rarity != tier) continue;
    if (!speciesHasArt((int16_t)d)) continue;
    if (!regionAvailable(regionOfDex((int16_t)d))) continue;
    if (!want) return (int16_t)d;
    want--;
  }
  return 0;
}

// ---------------------------------------------------------------------------
// Explore's areas.

uint8_t wildAreaCount(uint8_t region) {
  return region < WILD_REGIONS ? (uint8_t)WILD_REGION_AREAS[region][1] : 0;
}

uint16_t wildAreaFirst(uint8_t region) {
  return region < WILD_REGIONS ? WILD_REGION_AREAS[region][0] : 0;
}

bool wildRegionHasAreas(uint8_t region) { return wildAreaCount(region) > 0; }

static const WildArea NO_AREA = { "", 0xFF, 0, 0, 0, 1, 1 };
static const WildArea &areaAt(uint16_t a) { return a < WILD_AREA_COUNT ? WILD_AREAS[a] : NO_AREA; }
const char *wildAreaName(uint16_t a) { return areaAt(a).name; }
uint8_t wildAreaRegion(uint16_t a) { return areaAt(a).region; }
uint8_t wildAreaGate(uint16_t a) { return areaAt(a).gate; }
bool wildAreaNeedsChampion(uint16_t a) { return areaAt(a).gate >= WILD_GATE_CHAMPION; }
uint8_t wildAreaLvMin(uint16_t a) { return areaAt(a).lvMin; }
uint8_t wildAreaLvMax(uint16_t a) { return areaAt(a).lvMax; }

bool wildAreaUnlocked(uint16_t area, const Pet &p) {
  if (area >= WILD_AREA_COUNT) return false;
  const WildArea &a = WILD_AREAS[area];
  if (a.gate == 0) return true;
  uint8_t bit = (uint8_t)(a.gate - 1);   // the ladder trainer that opens it
  return p.hasBadge(a.region, bit, false) || p.hasBadge(a.region, bit, true);
}

uint8_t wildTimeBand(int hour) {
  if (hour < 0) return WILD_WHEN_DAY;
  hour %= 24;
  if (hour >= 4 && hour < 10) return WILD_WHEN_MORN;
  if (hour >= 10 && hour < 18) return WILD_WHEN_DAY;
  return WILD_WHEN_NITE;
}

// One answer for "can this slot be met now", so the pick, the NEEDS PACK test
// and the weight total can never disagree about it.
static bool slotLive(const WildSlot &s, uint8_t when) {
  return (s.when & when) && speciesHasArt(s.dex) &&
         regionAvailable(regionOfDex(s.dex));
}

bool wildAreaHasLive(uint16_t area, uint8_t when) {
  if (area >= WILD_AREA_COUNT) return false;
  const WildArea &a = WILD_AREAS[area];
  for (uint8_t i = 0; i < a.count; i++)
    if (slotLive(WILD_SLOTS[a.first + i], when)) return true;
  return false;
}

int16_t wildAreaPick(uint16_t area, uint8_t when, uint32_t roll) {
  if (area >= WILD_AREA_COUNT) return 0;
  const WildArea &a = WILD_AREAS[area];
  uint32_t total = 0;
  for (uint8_t i = 0; i < a.count; i++)
    if (slotLive(WILD_SLOTS[a.first + i], when)) total += WILD_SLOTS[a.first + i].weight;
  if (!total) return 0;
  uint32_t want = roll % total;
  for (uint8_t i = 0; i < a.count; i++) {
    const WildSlot &s = WILD_SLOTS[a.first + i];
    if (!slotLive(s, when)) continue;
    if (want < s.weight) return s.dex;
    want -= s.weight;
  }
  return 0;
}

uint8_t wildAreaLevel(uint16_t area, int16_t dex, uint32_t roll) {
  if (area >= WILD_AREA_COUNT) return 1;
  const WildArea &a = WILD_AREAS[area];
  uint8_t lo = a.lvMin ? a.lvMin : 1, hi = a.lvMax < lo ? lo : a.lvMax;
  uint8_t lvl = (uint8_t)(lo + roll % (uint32_t)(hi - lo + 1));
  for (uint8_t i = 0; i < a.count; i++) {
    const WildSlot &s = WILD_SLOTS[a.first + i];
    if (s.dex == dex && lvl < s.floor) lvl = s.floor;
  }
  return lvl > MAX_LEVEL ? MAX_LEVEL : lvl;
}
