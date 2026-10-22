#pragma once
// TMs as items (v3.51) -- the rules every source of a TM shares: the Mart's
// daily five, the Explore drop and (v3.53) the Game Corner's prize counters.
// One header so they cannot drift apart (CLAUDE.md trap 1).
//
// A TM is a MOVE INDEX from TM_MOVES (moves.h); owning it is a bit in
// Pet::tmOwned. They are reusable (the Gen 5 rule), so a TM the player already
// owns is never offered or dropped again.
#include <stdint.h>
#include "moves.h"
#include "pet.h"

#define MART_TM_PER_DAY 5

// The real games sold TMs for $1,000-$5,500. Here a TM costs by what it does:
// a status TM $2,000, an attack 40 x its power, between $1,500 and $5,000. In
// steps, a strong TM is a few days' walking.
inline uint16_t tmPrice(uint8_t mv) {
  if (!mv || mv >= MOVE_COUNT) return 0;
  const MoveEntry &m = MOVE_TBL[mv];
  if (m.cat == MC_STATUS) return 2000;
  uint32_t p = 40UL * (m.power ? m.power : 60);   // fixed-damage moves price as 60
  if (p < 1500) p = 1500;
  if (p > 5000) p = 5000;
  return (uint16_t)(p / 100 * 100);
}

// A small, well-mixed hash: the Mart's pick has to look random from one day to
// the next yet be the same all day, like wildAreaWeather().
inline uint32_t tmMix(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}

// Today's TMs at the Mart: up to MART_TM_PER_DAY the player does not own,
// chosen by the day. Buying one takes it off the list for the rest of the day
// (it is owned now) rather than putting another in its place -- the day's five
// are the day's five. Returns how many.
inline uint8_t martTmToday(const Pet &p, uint32_t day, uint8_t out[MART_TM_PER_DAY]) {
  uint8_t n = 0;
  // Five DISTINCT positions in TM_MOVES for this day, owned or not: then the
  // owned ones drop out. Picking among the unowned instead would slide a new
  // TM into a bought one's place.
  uint16_t picked[MART_TM_PER_DAY];
  uint8_t k = 0;
  for (uint32_t i = 0; k < MART_TM_PER_DAY && i < 64; i++) {
    uint16_t pos = (uint16_t)(tmMix(day * 2654435761U + i * 40503U + 0x7E5D) % TM_COUNT);
    bool dup = false;
    for (uint8_t j = 0; j < k; j++) if (picked[j] == pos) dup = true;
    if (!dup) picked[k++] = pos;
  }
  for (uint8_t j = 0; j < k; j++) {
    uint8_t mv = TM_MOVES[picked[j]];
    if (!p.hasTm(mv)) out[n++] = mv;
  }
  return n;
}

// A TM the player does not own yet, chosen by `roll`; 0 when they own them all.
inline uint8_t tmRandomUnowned(const Pet &p, uint32_t roll) {
  uint16_t left = 0;
  for (uint16_t i = 0; i < TM_COUNT; i++) if (!p.hasTm(TM_MOVES[i])) left++;
  if (!left) return 0;
  uint16_t pick = (uint16_t)(roll % left);
  for (uint16_t i = 0; i < TM_COUNT; i++) {
    if (p.hasTm(TM_MOVES[i])) continue;
    if (!pick) return TM_MOVES[i];
    pick--;
  }
  return 0;
}

// The chance a won wild battle also drops a TM, in percent. Hard pays more,
// as it does for items.
#define TM_DROP_PCT_EASY 5
#define TM_DROP_PCT_HARD 8
