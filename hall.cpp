// The Battle Hall's rules (v4.07). See hall.h.
#include "hall.h"
#include "pet.h"
#include "dex.h"
#include <string.h>

static uint32_t hRand(uint32_t &s) { s = s * 1103515245u + 12345u; return s >> 8; }

uint8_t bhallTypeAt(uint32_t seed, uint8_t rung) {
  uint8_t order[TYPE_COUNT];
  for (uint8_t i = 0; i < TYPE_COUNT; i++) order[i] = i;
  uint32_t s = seed ^ 0x7A11A11u;
  hRand(s);
  for (uint8_t i = TYPE_COUNT - 1; i > 0; i--) {   // Fisher-Yates
    uint8_t j = (uint8_t)(hRand(s) % (i + 1));
    uint8_t t = order[i]; order[i] = order[j]; order[j] = t;
  }
  return order[rung % TYPE_COUNT];
}

static int bst(int16_t d) {
  const DexEntry &e = DEX_TBL[d];
  return e.bHp + e.bAtk + e.bDef + e.bSpA + e.bSpD + e.bSpe;
}

bool bhallFoe(uint32_t seed, uint8_t rung, int16_t petDex, uint8_t petLevel, HallFoe &out) {
  out = HallFoe();
  if (rung >= HALL_RUNGS || petDex < 1 || petDex > DEX_COUNT) return false;
  uint8_t type = bhallTypeAt(seed, rung);
  int target = bst(petDex) * HALL_BST_PCT / 100;   // v4.10: a little weaker species than the pet
  // the HALL_POOL species of this type nearest the pet's base-stat total
  int16_t pool[HALL_POOL];
  int gap[HALL_POOL];
  uint8_t n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    const DexEntry &e = DEX_TBL[d];
    if (e.type1 != type && e.type2 != type) continue;
    if (e.rarity == R_LEGENDARIO || !speciesShowable(d)) continue;
    int g = bst(d) - target; if (g < 0) g = -g;
    if (n < HALL_POOL) { pool[n] = d; gap[n] = g; n++; continue; }
    uint8_t worst = 0;
    for (uint8_t k = 1; k < HALL_POOL; k++) if (gap[k] > gap[worst]) worst = k;
    if (g < gap[worst]) { pool[worst] = d; gap[worst] = g; }
  }
  if (!n) return false;
  uint32_t s = seed ^ ((uint32_t)(rung + 1) * 0x2545F491u);
  hRand(s);
  out.dex = pool[hRand(s) % n];
  // HALL_UNDER levels below the pet at the first rung, closing to level with
  // it at rung HALL_LEVEL_AT -- never above it (owner, 2026-11-05).
  int under = rung >= HALL_LEVEL_AT ? 0 : HALL_UNDER - rung * HALL_UNDER / HALL_LEVEL_AT;
  int lvl = (int)petLevel - under;
  out.level = (uint8_t)(lvl < 1 ? 1 : lvl > 100 ? 100 : lvl);
  // v4.10 (owner, 2026-11-05): IVs stay moderate (a perfect-IV pet has the
  // edge, a poor one is behind), training climbs from a little to ~90%.
  out.iv = (uint8_t)(HALL_IV_LO + rung * (HALL_IV_HI - HALL_IV_LO) / (HALL_RUNGS - 1));
  out.tr = (uint8_t)(HALL_TR_LO + rung * (HALL_TR_HI - HALL_TR_LO) / (HALL_RUNGS - 1));
  out.smart = rung >= HALL_SMART_FROM;
  return true;
}

uint32_t bhallPrize(uint8_t rung, uint8_t foeLevel) {
  return (uint32_t)HALL_PAY * foeLevel * (HALL_CLASS + rung / 3u);
}
