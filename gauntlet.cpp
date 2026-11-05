// The gauntlet's rules (v4.04). See gauntlet.h.
#include "gauntlet.h"
#include "dex.h"
#include "pet.h"
#include <string.h>

bool speciesEvolvesFurther(int16_t d) {
  if (d < 1 || d > DEX_COUNT) return false;
  if (DEX_TBL[d].evolvesTo) return true;
  if (d == EEVEE_EVOS[0] - 1) return true;   // EEVEE (133): its branches are a table of their own
  for (int i = 0; i < ITEM_EVO_COUNT; i++) if (ITEM_EVOS[i].from == d) return true;
  for (int i = 0; i < TRADE_EVO_COUNT; i++) if (TRADE_EVOS[i].from == d) return true;
  for (int i = 0; i < BRANCH_EVO_COUNT; i++) if (BRANCH_EVOS[i].from == d) return true;
  for (int i = 0; i < GENDER_EVO_COUNT; i++) if (GENDER_EVOS[i].from == d) return true;
  return false;
}

uint8_t speciesStage(int16_t d) {
  if (!speciesEvolvesFurther(d)) return 2;
  return DEX_TBL[d].rarity == R_EVO ? 1 : 0;
}

bool gauntletSpeciesFits(const GauntletRules &r, int16_t d) {
  if (d < 1 || d > DEX_COUNT) return false;
  if (r.type != GAUNTLET_TYPE_ANY && DEX_TBL[d].type1 != r.type && DEX_TBL[d].type2 != r.type) return false;
  switch (r.tier) {
    case GT_BASIC:    return speciesStage(d) == 0;
    case GT_MIDDLE:   return speciesStage(d) == 1;
    case GT_FINAL:    return speciesStage(d) == 2;
    case GT_NOLEGEND: return DEX_TBL[d].rarity != R_LEGENDARIO;
    default:          return true;
  }
}

// A small LCG of its own, so a challenger depends on (rules, round, seed) only
// and never on how many times anything else called random().
static uint32_t gRand(uint32_t &s) { s = s * 1103515245u + 12345u; return s >> 8; }

GauntletRules gauntletDaily(int32_t day) {
  uint32_t s = (uint32_t)(day < 0 ? 0 : day) * 2654435761u + 0x9E3779B9u;
  gRand(s);
  GauntletRules r;
  r.type = (gRand(s) % 4 == 0) ? GAUNTLET_TYPE_ANY : (uint8_t)(gRand(s) % TYPE_COUNT);
  r.levelCap = GAUNTLET_CAPS[gRand(s) % GAUNTLET_CAP_COUNT];
  r.tier = (uint8_t)(gRand(s) % GT_COUNT);
  r.size = (uint8_t)(3 + gRand(s) % 4);   // 3-6
  // A day whose rules leave too few species for a team loosens its tier.
  uint16_t n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (gauntletSpeciesFits(r, d) && speciesShowable(d) && DEX_TBL[d].rarity != R_LEGENDARIO) n++;
  if (n < 2 * r.size) r.tier = GT_ANY;
  return r;
}

bool gauntletLevelFits(const GauntletRules &r, uint8_t level) {
  return !r.levelCap || level <= r.levelCap;
}

// The first challenger is GAUNTLET_LEVEL_RAMP levels under the top, one level
// closer each round. IVs start at 10 (of 31), training at 0 and rises from the
// third round, the smart AI from the sixth.
uint8_t gauntletLevel(uint16_t round, uint8_t top) {
  int under = round >= GAUNTLET_LEVEL_RAMP ? 0 : GAUNTLET_LEVEL_RAMP - (int)round;
  int l = (int)top - under;
  return (uint8_t)(l < 1 ? 1 : l > 100 ? 100 : l);
}
uint8_t gauntletIv(uint16_t round) { return round >= 21 ? 31 : (uint8_t)(10 + round); }
uint8_t gauntletTr(uint16_t round) { return round < 2 ? 0 : round >= 27 ? 100 : (uint8_t)((round - 2) * 4); }
bool gauntletSmart(uint16_t round) { return round >= 5; }
// Switching last, after the smart AI (plan § 3's order), from the eleventh.
bool gauntletSwitches(uint16_t round) { return round >= 10; }
uint8_t gauntletBp(uint16_t round, bool doubled) {
  uint8_t bp = (uint8_t)(ZONE_BP_WIN[ZONE_GAUNTLET] + round / 7);
  return doubled ? (uint8_t)(bp * 2) : bp;
}

bool gauntletChallenger(const GauntletRules &r, uint16_t round, uint32_t seed,
                        uint8_t teamLevel, uint8_t avatarCount, ZoneFoe &out) {
  memset(&out, 0, sizeof(out));
  // The pool: species that fit, can be drawn, and are not legendary (a
  // challenger never fields one, whatever the tier).
  uint16_t n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (gauntletSpeciesFits(r, d) && speciesShowable(d) && DEX_TBL[d].rarity != R_LEGENDARIO) n++;
  uint8_t size = r.size < 1 ? 1 : r.size > 6 ? 6 : r.size;
  if (n < size) return false;
  uint32_t s = seed ^ ((uint32_t)round * 0x45D9F3Bu);
  gRand(s); gRand(s);
  for (uint8_t k = 0; k < size; k++) {
    for (int tries = 0; tries < 64; tries++) {
      uint16_t pick = (uint16_t)(gRand(s) % n), seen = 0;
      int16_t chosen = 0;
      for (int16_t d = 1; d <= DEX_COUNT && !chosen; d++)
        if (gauntletSpeciesFits(r, d) && speciesShowable(d) && DEX_TBL[d].rarity != R_LEGENDARIO && seen++ == pick)
          chosen = d;
      bool dup = false;
      for (uint8_t j = 0; j < k; j++) if (out.dex[j] == chosen) dup = true;
      if (dup && tries < 63) continue;
      out.dex[k] = chosen;
      break;
    }
    out.level[k] = gauntletLevel(round, r.levelCap ? r.levelCap : teamLevel);
  }
  out.count = size;
  out.iv = gauntletIv(round);
  out.tr = gauntletTr(round);
  out.smart = gauntletSmart(round) ? 1 : 0;
  out.switching = gauntletSwitches(round) ? 1 : 0;
  // A trainer class (avatars 3 onward: not RED, BLUE or GIRL, the player's).
  // The sketch names it from the avatar, the one includer of avatars.h.
  out.avatar = avatarCount > 3 ? (uint8_t)(3 + gRand(s) % (avatarCount - 3)) : 0;
  return true;
}
