// The Battle Hall (v4.07, postgame-plan § 4): normal play, not the zone. The
// LIVE pet alone, one battle against each of the 18 types, in a ladder that
// starts below it and climbs. Pure rules -- the type order, who a rung's foe
// is, what it pays -- no screen and no battle: the sketch drives them.
#pragma once
#include <stdint.h>
#include "party.h"

#define HALL_RUNGS 18          // one rung per type
#define HALL_ENERGY_COST 6     // v4.09: a clear is ~108 energy, a full bar and a nap
// v4.09: a rung pays HALL_PAY x the foe's level x (HALL_CLASS + rung / 3), so a
// level-50 clear is ~$11,700 -- about one first champion win, not several.
#define HALL_PAY 2
#define HALL_CLASS 4
// v4.10 (owner, 2026-11-05; tools/emu/zone_sim.cpp ZS_ONLY=hallx): the Hall is
// a parlay of 18, so the foe never outlevels the pet (HALL_UNDER below, closing
// to level only at the last rung), its IVs stay moderate (HALL_IV_HI) so a
// perfect-IV pet has the edge, its training climbs from none to ~90%, the smart
// AI comes late, and it is drawn from species near HALL_BST_PCT of the pet's
// base stats -- the type matchups do the rest. v4.11 orders the types worst
// matchup first, which made it easier (IV 20 16-22%), so HALL_BST_PCT went
// 80 -> 90. Measured for a fully trained pet of any final form (legendaries
// included): IV 20 clears 8-11%, IV 31 15-23%, IV 10 4-5%, at levels 50-100.
// #ifndef so the sim can sweep them.
#ifndef HALL_IV_LO
#define HALL_IV_LO 4
#endif
#ifndef HALL_IV_HI
#define HALL_IV_HI 16
#endif
#ifndef HALL_TR_LO
#define HALL_TR_LO 0
#endif
#ifndef HALL_TR_HI
#define HALL_TR_HI 90
#endif
#ifndef HALL_UNDER
#define HALL_UNDER 3
#endif
#ifndef HALL_LEVEL_AT
#define HALL_LEVEL_AT 17      // the rung (0-based) where it is level with the pet: the last
#endif
#ifndef HALL_SMART_FROM
#define HALL_SMART_FROM 10    // the smart AI from the 11th rung
#endif
#ifndef HALL_BST_PCT
#define HALL_BST_PCT 90
#endif
#define HALL_POOL 6            // a rung's foe is drawn from the N species of its type nearest HALL_BST_PCT of the pet's base stats

// How a type meets the pet: the pet's best type against it less that type
// against the pet (typeEffPct percentages). Negative = a counter to the pet.
int bhallMatchup(uint8_t type, int16_t petDex);

// The rung-th type of a run: all 18, the pet's worst matchups FIRST and its
// best LAST (v4.11, owner: lose to your counter early, while the foes are
// weak; the strong late foes are the types you beat). Types that meet the pet
// alike come in a shuffle by the run's seed. The same seed and pet, the same
// order (an invalid petDex, e.g. -1, is the plain shuffle). petDex is the creature the run STARTED with (HallSave::orderDex), so
// an evolution mid-run does not reshuffle what is left.
uint8_t bhallTypeAt(uint32_t seed, uint8_t rung, int16_t petDex);

// A rung's foe, ready for the sketch to build: a drawable, non-legendary
// species of the rung's type, close to the pet's own species in base stats.
struct HallFoe {
  int16_t dex = 0;
  uint8_t level = 1;
  uint8_t iv = 0;
  uint8_t tr = 0;
  bool smart = false;
};
// The first rung starts BELOW the pet -- HALL_UNDER levels under, IV
// HALL_IV_LO, no training, the simple AI -- and each rung climbs: level with
// the pet only at the last (never above it), IVs to HALL_IV_HI and training to
// HALL_TR_HI, the smart AI from rung HALL_SMART_FROM + 1. False if the type has
// no drawable species (the sketch then skips the rung). The same arguments,
// the same foe.
// orderDex: the species the run's type order is for (0 = petDex).
bool bhallFoe(uint32_t seed, uint8_t rung, int16_t petDex, uint8_t petLevel, HallFoe &out, int16_t orderDex = 0);

// A rung's money: trainerPrize()'s Gen 3 shape, HALL_PAY x the foe's level x a
// class value, the class rising a point every three rungs from HALL_CLASS.
uint32_t bhallPrize(uint8_t rung, uint8_t foeLevel);

// The saved run (NVS key "bhall"): kept between rungs for as long as you
// like, for ONE creature -- changing the live pet ends it. `fighting` is saved
// before the battle opens; a reboot with it set is the loss.
#define HALL_SAVE_MAGIC 0x48   // 'H'
enum : uint8_t { HALL_RES_NONE = 0, HALL_RES_CLEARED, HALL_RES_LOST, HALL_RES_CHANGED };
struct HallSave {
  uint8_t magic = HALL_SAVE_MAGIC;
  uint8_t version = 2;            // 2 = v4.11, orderDex
  uint8_t active = 0;
  uint8_t fighting = 0;
  uint8_t rung = 0;               // the next rung, 0..17
  uint8_t result = HALL_RES_NONE; // how the last run ended
  uint8_t resultRung = 0;
  uint8_t pad = 0;
  uint32_t seed = 0;
  CreatureUid uid;                // the creature the run belongs to
  uint16_t clears = 0;            // runs that cleared all 18
  int16_t orderDex = 0;           // v4.11: the species the type order is for (the run's start; -1 = a v4.10 run's shuffle), in what was padding
};
