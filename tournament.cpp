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

// A species' final form along its table route (v4.10): Geodude -> Golem. Stops
// at the last form the installed packs can draw.
int16_t tourFinalForm(int16_t d) {
  for (int i = 0; i < 4 && d >= 1 && d <= DEX_COUNT && DEX_TBL[d].evolvesTo; i++) {
    int16_t n = (int16_t)DEX_TBL[d].evolvesTo;
    if (!speciesShowable(n)) break;
    d = n;
  }
  return d;
}

// The seat's story strength: its roster's best level in the story. The
// seeding, and (v4.10) how hard the seat fights now that every seat is level 100.
uint8_t tourBestLevel(uint16_t id, uint32_t seed) {
  (void)seed;
  if (id & TOUR_GEN) return TOUR_GEN_LEVEL;
  uint8_t region = (uint8_t)(id / TRAINER_COUNT), seat = (uint8_t)(id % TRAINER_COUNT);
  if (region >= GYM_REGIONS) return 1;
  const Trainer &t = TRAINER_SETS[region].list[seat];
  uint8_t best = 1;
  for (uint8_t k = 0; k < t.count; k++) if (t.team[k].level > best) best = t.team[k].level;
  return best;
}

// Story strength 0..1000: TOUR_STR_LO (an early gym) .. TOUR_STR_HI (a champion).
static uint16_t tourStrength(uint16_t id) {
  int l = tourBestLevel(id, 0);
  if (l <= TOUR_STR_LO) return 0;
  if (l >= TOUR_STR_HI) return 1000;
  return (uint16_t)((l - TOUR_STR_LO) * 1000 / (TOUR_STR_HI - TOUR_STR_LO));
}
uint8_t tourIv(uint16_t id) { return (uint8_t)(TOUR_IV_LO + (TOUR_IV_HI - TOUR_IV_LO) * tourStrength(id) / 1000); }
uint8_t tourTr(uint16_t id) { return (uint8_t)(TOUR_TR_LO + (TOUR_TR_HI - TOUR_TR_LO) * tourStrength(id) / 1000); }
bool tourSmart(uint16_t id) { return tourStrength(id) >= TOUR_SMART_FROM; }

bool tourFoe(uint16_t id, uint8_t round, uint32_t seed, uint8_t avatarCount, ZoneFoe &out) {
  memset(&out, 0, sizeof(out));
  if (id & TOUR_GEN) {
    GauntletRules r;
    r.size = TRAINER_TEAM_MAX;
    uint32_t s = seed ^ ((uint32_t)((id & 0xFF) + 1) * 0x9E3779B1u);
    if (!gauntletChallenger(r, TOUR_GEN_ROUND, s, TOUR_LEVEL, avatarCount, out)) return false;
    for (uint8_t k = 0; k < out.count; k++) out.level[k] = TOUR_LEVEL;
  } else {
    // v4.10 (owner, 2026-11-05): the post-game, not the story. Every seat brings
    // six at TOUR_LEVEL: its own roster, each at its final form (two of one
    // species if the roster had a line twice), then the open slots filled from
    // the final forms of its specialty type -- a stable pick per seat, so Brock's
    // tournament team is always the same team.
    uint8_t region = (uint8_t)(id / TRAINER_COUNT), seat = (uint8_t)(id % TRAINER_COUNT);
    if (region >= GYM_REGIONS) return false;
    const Trainer &t = TRAINER_SETS[region].list[seat];
    strncpy(out.name, t.name, sizeof(out.name) - 1);
    out.count = 0;
    for (uint8_t k = 0; k < t.count && out.count < TRAINER_TEAM_MAX; k++)
      out.dex[out.count++] = tourFinalForm((int16_t)t.team[k].dex);
    // any of the type's final forms (TOUR_FILL_TOP > 0: its N strongest)
    int floorBst = 0;
    if (TOUR_FILL_TOP > 0) {
      int top[TOUR_FILL_TOP > 0 ? TOUR_FILL_TOP : 1] = { 0 };
      for (int16_t d = 1; d <= DEX_COUNT; d++) {
        if (!tourFillFits(t.type, d)) continue;
        int b = gauntletBst(d);
        for (int j = 0; j < TOUR_FILL_TOP; j++)
          if (b > top[j]) { for (int m = TOUR_FILL_TOP - 1; m > j; m--) top[m] = top[m - 1]; top[j] = b; break; }
      }
      floorBst = top[TOUR_FILL_TOP - 1];
    }
    uint16_t cands = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) if (tourFillFits(t.type, d) && gauntletBst(d) >= floorBst) cands++;
    uint32_t s = (uint32_t)id * 2654435761u + 0x7A11u;
    for (int tries = 0; out.count < TRAINER_TEAM_MAX && cands && tries < 64; tries++) {
      uint16_t pick = (uint16_t)(tRand(s) % cands), seen = 0;
      int16_t c = 0;
      for (int16_t d = 1; d <= DEX_COUNT && !c; d++)
        if (tourFillFits(t.type, d) && gauntletBst(d) >= floorBst && seen++ == pick) c = d;
      bool dup = false;
      for (uint8_t k = 0; k < out.count; k++) if (out.dex[k] == c) dup = true;
      if (!dup) out.dex[out.count++] = c;
    }
    for (uint8_t k = 0; k < out.count; k++) out.level[k] = TOUR_LEVEL;
    out.avatar = avatarCount > 3 ? (uint8_t)(3 + id % (avatarCount - 3)) : 0;
  }
  // the levers (v4.10): IVs, training and the AI by the seat's story strength
  out.iv = tourIv(id);
  out.tr = tourTr(id);
  out.smart = tourSmart(id) ? 1 : 0;
  out.switching = tourSwitches(round) ? 1 : 0;
  return true;
}

// A filler for a seat of this type: a drawable, non-legendary final form of it.
bool tourFillFits(uint8_t type, int16_t d) {
  if (d < 1 || d > DEX_COUNT) return false;
  const DexEntry &e = DEX_TBL[d];
  return (e.type1 == type || e.type2 == type) && !speciesEvolvesFurther(d) &&
         e.rarity != R_LEGENDARIO && speciesShowable(d);
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
