// The tournament's rules (v4.06). See tournament.h.
#include "tournament.h"
#include "gauntlet.h"
#include "pet.h"
#include <string.h>

static uint32_t tRand(uint32_t &s) { s = s * 1103515245u + 12345u; return s >> 8; }

uint16_t tourSeatId(uint8_t region, uint8_t seat) { return (uint16_t)(region * TRAINER_COUNT + seat); }

bool tourSeatDrawable(uint16_t id) {
  if (id & TOUR_GEN) return true;
  uint8_t region = (uint8_t)(id / TRAINER_COUNT), seat = (uint8_t)(id % TRAINER_COUNT);
  if (region >= GYM_REGIONS) return false;
  const Trainer &t = TRAINER_SETS[region].list[seat];
  if (!t.count) return false;
  for (uint8_t k = 0; k < t.count; k++) if (!speciesShowable((int16_t)t.team[k].dex)) return false;
  return true;
}

bool tourSwitches(uint8_t round) { return round >= 2; }

bool tourFoe(uint16_t id, uint8_t round, uint32_t seed, uint8_t avatarCount, ZoneFoe &out) {
  memset(&out, 0, sizeof(out));
  if (id & TOUR_GEN) {
    GauntletRules r;
    r.size = TRAINER_TEAM_MAX;
    uint32_t s = seed ^ ((uint32_t)((id & 0xFF) + 1) * 0x9E3779B1u);
    if (!gauntletChallenger(r, TOUR_GEN_ROUND, s, TOUR_GEN_LEVEL, avatarCount, out)) return false;
  } else {
    uint8_t region = (uint8_t)(id / TRAINER_COUNT), seat = (uint8_t)(id % TRAINER_COUNT);
    if (region >= GYM_REGIONS) return false;
    const Trainer &t = TRAINER_SETS[region].list[seat];
    strncpy(out.name, t.name, sizeof(out.name) - 1);
    out.count = t.count;
    for (uint8_t k = 0; k < t.count && k < TRAINER_TEAM_MAX; k++) {
      out.dex[k] = (int16_t)t.team[k].dex;
      out.level[k] = t.team[k].level;
    }
    out.iv = HARD_IV;
    out.tr = hardTraining(region, seat);
    out.avatar = avatarCount > 3 ? (uint8_t)(3 + id % (avatarCount - 3)) : 0;
  }
  out.smart = 1;
  out.switching = tourSwitches(round) ? 1 : 0;
  return true;
}

uint8_t tourBestLevel(uint16_t id, uint32_t seed) {
  ZoneFoe f;
  if (!tourFoe(id, 0, seed, 0, f)) return 1;
  uint8_t best = 1;
  for (uint8_t k = 0; k < f.count; k++) if (f.level[k] > best) best = f.level[k];
  return best;
}

// Bracket position -> seed (1 = the player), the standard 16 draw: 1 meets
// 16, then 8/9, then 4/5/12/13, and 2 only in the final.
static const uint8_t SEED_AT[TOUR_SIZE] = { 1, 16, 8, 9, 4, 13, 5, 12, 2, 15, 7, 10, 3, 14, 6, 11 };

void tourDraw(uint32_t seed, uint16_t out[TOUR_SIZE]) {
  static uint16_t pool[GYM_REGIONS * TRAINER_COUNT];
  uint16_t n = 0;
  for (uint8_t r = 0; r < GYM_REGIONS; r++)
    for (uint8_t s = 0; s < TRAINER_COUNT; s++)
      if (tourSeatDrawable(tourSeatId(r, s))) pool[n++] = tourSeatId(r, s);
  uint32_t st = seed ^ 0x51ED270Bu;
  tRand(st);
  uint16_t pick[TOUR_SIZE - 1];
  uint8_t got = 0;
  for (; got < TOUR_SIZE - 1 && n > 0; got++) {   // a partial shuffle: distinct, at random
    uint16_t j = (uint16_t)(tRand(st) % n);
    pick[got] = pool[j];
    pool[j] = pool[--n];
  }
  for (uint8_t k = 0; got < TOUR_SIZE - 1; k++) pick[got++] = (uint16_t)(TOUR_GEN | k);
  // strongest first (a stable insertion sort; the id breaks a tie)
  uint8_t lvl[TOUR_SIZE - 1];
  for (uint8_t i = 0; i < TOUR_SIZE - 1; i++) lvl[i] = tourBestLevel(pick[i], seed);
  for (uint8_t i = 1; i < TOUR_SIZE - 1; i++) {
    uint16_t p = pick[i]; uint8_t l = lvl[i]; int j = i - 1;
    while (j >= 0 && (lvl[j] < l || (lvl[j] == l && pick[j] > p))) { pick[j + 1] = pick[j]; lvl[j + 1] = lvl[j]; j--; }
    pick[j + 1] = p; lvl[j + 1] = l;
  }
  for (uint8_t pos = 0; pos < TOUR_SIZE; pos++) {
    uint8_t sd = SEED_AT[pos];
    out[pos] = sd == 1 ? TOUR_PLAYER : pick[sd - 2];
  }
}

int8_t tourPlayerOpponent(const TourSave &t) {
  if (!(t.alive & 1)) return -1;
  for (uint8_t i = 1; i < TOUR_SIZE; i++) if (t.alive & (1u << i)) return (int8_t)i;
  return -1;
}
