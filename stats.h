// The ONE stat formula, for the live pet (pet.cpp) and a banked creature
// (party.cpp) alike. They each kept a copy until v3.58 (CLAUDE.md trap 1).
#pragma once
#include <stdint.h>
#include "settings.h"

// The IV's share (IV x level / 100) is the games' own from Gen 3 on: a perfect
// IV is worth +31 at level 100. The rest is TamaPoke's (a flat base + level),
// not the games': the canonical x level/100 on the base would show a newborn
// single-digit stats, which on a pet screen reads as a bug.
//
// Training is the same shape as the IV (v3.58): x level / 100, so full
// training is +100 at level 100 and +14 at level 14. It stays 0-100 on the
// training screen; this is only what the creature fights with.
// The hidden setting "classic stats" puts training back flat (v3.57's rule).
inline uint16_t battleStat(uint8_t base, uint8_t iv, uint16_t lvl, uint8_t tr) {
  if (settingOn(SET_CLASSIC_STATS))
    return (uint16_t)(base + lvl + (uint32_t)iv * lvl / 100 + tr);
  return (uint16_t)(base + lvl + (uint32_t)iv * lvl / 100 + (uint32_t)tr * lvl / 100);
}

// Vitality is never trained; it carries a flat +10 instead, like the games'
// +level+10 on HP. Not scaled: it is not training.
inline uint16_t battleHp(uint8_t base, uint8_t iv, uint16_t lvl) {
  return (uint16_t)(base + lvl + (uint32_t)iv * lvl / 100 + 10);
}
