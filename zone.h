// The post-game Battle Zone (v4.03): its fees, pay and prize counter. The
// screens and the fight are in the sketch (renderZone/zoneTap, zoneStartFight);
// the plan is docs/postgame-plan.md. EVERY NUMBER HERE IS A PLACEHOLDER until
// the playthrough bot measures the money flow (plan § 1: the zone must drain
// money, not create it).
#pragma once
#include <stdint.h>
#include "items.h"
#include "battle.h"
#include "tms.h"

// A zone opponent: a name, a team of species and levels, and the format's IVs
// and training for every creature in it.
struct ZoneFoe {
  char name[12];
  uint8_t count;
  int16_t dex[6];
  uint8_t level[6];
  uint8_t iv, tr;
  uint8_t avatar;  // a trainer class to draw and name (v4.04)
  uint8_t smart;   // the smart AI (v4.04: the gauntlet's later rounds; 0 = the simple one)
  uint8_t switching;   // switches out of a bad matchup (v4.05: aiSwitchTarget(); 0 = never)
};
// One run (RAM; saving comes with the formats, plan step 4). The team is the
// run's own copy, fresh as it entered; pp[][] is what it has left.
struct ZoneRun {
  bool active = false;
  uint8_t kind = 0;
  uint8_t n = 0;
  Combatant team[6];
  uint8_t pp[6][MOVE_SLOTS];
  uint16_t wins = 0;
};

enum : uint8_t { ZONE_TOURNAMENT = 0, ZONE_GAUNTLET, ZONE_KINDS };

// Pokedollars to start a run.
static const uint32_t ZONE_FEE[ZONE_KINDS] = { 5000, 3000 };
// BP a won battle pays, by format (the formats will scale it; plan § 2-3).
static const uint8_t ZONE_BP_WIN[ZONE_KINDS] = { 3, 2 };

// The prize counter (plan § 5): items, for BP. The Master Ball is here because
// the Mart never sells it; PP MAX and the BOTTLE CAP (v4.08) are sold nowhere
// else. After the items the counter lists TMs (zonePrizeAt(), below).
struct ZonePrize { ItemKey item; uint16_t bp; };
static const ZonePrize ZONE_PRIZES[] = {
  { IT_MASTERBALL, 100 },
  { IT_PPUP, 16 }, { IT_PPMAX, 40 }, { IT_BOTTLECAP, 25 }, { IT_ETHER, 2 },
  { IT_PROTEIN, 4 }, { IT_IRON, 4 }, { IT_CARBOS, 4 },
  { IT_LINKCORD, 16 },
  { IT_FIRESTONE, 6 }, { IT_WATERSTONE, 6 }, { IT_THUNDERSTONE, 6 }, { IT_LEAFSTONE, 6 },
  { IT_MOONSTONE, 6 }, { IT_SUNSTONE, 6 }, { IT_SHINYSTONE, 6 }, { IT_DUSKSTONE, 6 },
  { IT_DAWNSTONE, 6 }, { IT_ICESTONE, 6 }, { IT_BLACKAUGURITE, 8 },
};
static const uint8_t ZONE_PRIZE_COUNT = sizeof(ZONE_PRIZES) / sizeof(ZONE_PRIZES[0]);

// ---- v4.08: TMs at the counter, and the zone's symbols ----
//
// After the items, the counter lists the SIGNATURE TMs the player does not own
// (moves.h SIG_TM_MOVES, sold nowhere else) and then ZONE_TM_PER_DAY ordinary
// TMs a day: ones the player does not own that the Mart is NOT offering today,
// so between the two a player sees ten a day. An owned TM leaves the list (TMs
// are reusable). zonePrizeAt() is THE list: the drawing, the tap, the paging
// and the purchase all walk it.
#define ZONE_TM_PER_DAY 5
#define ZONE_SIG_TM_BP 48
enum : uint8_t { ZPR_ITEM = 0, ZPR_TM };
struct ZonePrizeRow { uint8_t kind; uint8_t what; uint16_t bp; };

// An ordinary TM's BP: its Mart price / 250 ($1,500-$5,000 -> 6-20 BP).
inline uint16_t zoneTmBp(uint8_t mv) { return (uint16_t)(tmPrice(mv) / 250); }

// Today's counter TMs: up to ZONE_TM_PER_DAY unowned TMs not in today's Mart
// five, chosen by the day (a different salt from the Mart's).
inline uint8_t zoneTmToday(const Pet &p, uint32_t day, uint8_t out[ZONE_TM_PER_DAY]) {
  uint8_t mart[MART_TM_PER_DAY];
  uint8_t nm = martTmToday(p, day, mart);
  uint16_t picked[ZONE_TM_PER_DAY];
  uint8_t k = 0, n = 0;
  // the same rule as the Mart's: the day's positions first, owned ones drop out
  for (uint32_t i = 0; k < ZONE_TM_PER_DAY && i < 96; i++) {
    uint16_t pos = (uint16_t)(tmMix((day + p.daySalt * 100003U) * 2246822519U + i * 69069U + 0x2B0E) % TM_COUNT);
    bool skip = false;
    for (uint8_t j = 0; j < k; j++) if (picked[j] == pos) skip = true;
    for (uint8_t j = 0; j < nm; j++) if (mart[j] == TM_MOVES[pos]) skip = true;
    if (!skip) picked[k++] = pos;
  }
  for (uint8_t j = 0; j < k; j++)
    if (!p.hasTm(TM_MOVES[picked[j]])) out[n++] = TM_MOVES[picked[j]];
  return n;
}

inline uint8_t zonePrizeCount(const Pet &p, uint32_t day) {
  uint8_t n = ZONE_PRIZE_COUNT;
  for (uint8_t i = 0; i < SIG_TM_COUNT; i++) if (!p.hasTm(SIG_TM_MOVES[i])) n++;
  uint8_t t[ZONE_TM_PER_DAY];
  return (uint8_t)(n + zoneTmToday(p, day, t));
}
inline bool zonePrizeAt(const Pet &p, uint32_t day, uint8_t i, ZonePrizeRow *out) {
  if (i < ZONE_PRIZE_COUNT) {
    *out = { ZPR_ITEM, (uint8_t)ZONE_PRIZES[i].item, ZONE_PRIZES[i].bp };
    return true;
  }
  i -= ZONE_PRIZE_COUNT;
  for (uint8_t s = 0; s < SIG_TM_COUNT; s++) {
    if (p.hasTm(SIG_TM_MOVES[s])) continue;
    if (!i) { *out = { ZPR_TM, SIG_TM_MOVES[s], ZONE_SIG_TM_BP }; return true; }
    i--;
  }
  uint8_t t[ZONE_TM_PER_DAY];
  uint8_t nt = zoneTmToday(p, day, t);
  if (i >= nt) return false;
  *out = { ZPR_TM, t[i], zoneTmBp(t[i]) };
  return true;
}

// The zone's symbols on the player card (plan § 5, cosmetic), derived from what
// is already saved: the first tournament won, and the gauntlet's best streak.
// PLACEHOLDER thresholds, like every zone number.
#define ZONE_SYM_SILVER_STREAK 10
#define ZONE_SYM_GOLD_STREAK 25
enum : uint8_t { ZSYM_TOUR = 1, ZSYM_SILVER = 2, ZSYM_GOLD = 4 };
inline uint8_t zoneSymbols(uint16_t tourWins, uint16_t bestStreak) {
  uint8_t m = 0;
  if (tourWins >= 1) m |= ZSYM_TOUR;
  if (bestStreak >= ZONE_SYM_SILVER_STREAK) m |= ZSYM_SILVER;
  if (bestStreak >= ZONE_SYM_GOLD_STREAK) m |= ZSYM_GOLD;
  return m;
}
