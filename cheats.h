// The hidden CHEATS (v3.60), opened by a secret code (codes.h). Grants and
// gates. Using ANY sets its bit in Pet::cheatsUsed, which nothing clears -- the
// player card's cheats mark, distinct from the rules mark (settings.h). A
// creature a cheat changed directly carries PMON_CHEATED, which travels with
// it through banking, swapping, evolving and trades.
//
// The bit index is saved: APPEND-ONLY, strings at S_CHEAT_FIRST + index.
#pragma once
#include <stdint.h>
#include "settings.h"   // settingsSuspended: no cheat applies to a LAN battle

enum CheatId : uint8_t {
  CHEAT_COINS,        // +1,000 coins
  CHEAT_MASTERBALL,   // +1 Master Ball
  CHEAT_MONEY,        // +$100,000
  CHEAT_ALL_TMS,      // every TM; a move taught from one marks the creature
  CHEAT_UNLIMITED_PP, // toggle: the player's side spends no PP (never LAN)
  CHEAT_ALL_BADGES,   // every badge, both difficulties
  CHEAT_ALL_AREAS,    // toggle: every Explore gate open (gateOpen() in wild.cpp)
  CHEAT_LEGEND,       // the next Explore encounter is a legendary of its region
  CHEAT_MAX_TRAINING, // the live pet's training to its ceiling -- marks it
  CHEAT_HATCH,        // hatch the egg now -- marks the creature
  CHEAT_SHINY_EGG,    // the next egg hatches shiny -- marks the creature
  CHEAT_REROLL_DAY,   // re-roll today's weather and the Mart's TMs
  CHEAT_COUNT
};

// The saved toggles and one-shot states (Pet::cheatToggles).
enum : uint8_t {
  CT_UNLIMITED_PP = 1u << 0,
  CT_ALL_AREAS    = 1u << 1,
  CT_EGG_MARKED   = 1u << 2,   // the egg waiting was made shiny by a cheat
};

extern uint8_t cheatToggles;
inline bool cheatOn(uint8_t ct) { return !settingsSuspended && (cheatToggles & ct); }
