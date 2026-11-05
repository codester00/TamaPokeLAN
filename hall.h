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
#define HALL_POOL 6            // a rung's foe is drawn from the N species of its type nearest the pet in base stats

// The rung-th type of a run (a shuffle of all 18 by the run's seed, so every
// run meets them in its own order; the same seed, the same order).
uint8_t bhallTypeAt(uint32_t seed, uint8_t rung);

// A rung's foe, ready for the sketch to build: a drawable, non-legendary
// species of the rung's type, close to the pet's own species in base stats.
struct HallFoe {
  int16_t dex = 0;
  uint8_t level = 1;
  uint8_t iv = 0;
  uint8_t tr = 0;
  bool smart = false;
};
// The first rung starts BELOW the pet -- three levels under, IV 8, no
// training, the simple AI -- and each rung climbs: a level back every three
// rungs (level with the pet from rung 10), IVs to 31 and training to 100 by
// the last, the smart AI from rung 7. False if the type has no drawable
// species (the sketch then skips the rung). The same arguments, the same foe.
bool bhallFoe(uint32_t seed, uint8_t rung, int16_t petDex, uint8_t petLevel, HallFoe &out);

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
  uint8_t version = 1;
  uint8_t active = 0;
  uint8_t fighting = 0;
  uint8_t rung = 0;               // the next rung, 0..17
  uint8_t result = HALL_RES_NONE; // how the last run ended
  uint8_t resultRung = 0;
  uint8_t pad = 0;
  uint32_t seed = 0;
  CreatureUid uid;                // the creature the run belongs to
  uint16_t clears = 0;            // runs that cleared all 18
};
