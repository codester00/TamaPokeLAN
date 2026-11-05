// The post-game Battle Zone (v4.03): its fees, pay and prize counter. The
// screens and the fight are in the sketch (renderZone/zoneTap, zoneStartFight);
// the plan is docs/postgame-plan.md. EVERY NUMBER HERE IS A PLACEHOLDER until
// the playthrough bot measures the money flow (plan § 1: the zone must drain
// money, not create it).
#pragma once
#include <stdint.h>
#include "items.h"
#include "battle.h"

// A zone opponent: a name, a team of species and levels, and the format's IVs
// and training for every creature in it.
struct ZoneFoe {
  char name[12];
  uint8_t count;
  int16_t dex[6];
  uint8_t level[6];
  uint8_t iv, tr;
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

// The prize counter (plan § 5): items that already exist, for BP. The Master
// Ball is here because the Mart never sells it. TMs, PP Max, the Bottle Cap and
// the signature moves come in step 7.
struct ZonePrize { ItemKey item; uint16_t bp; };
static const ZonePrize ZONE_PRIZES[] = {
  { IT_MASTERBALL, 100 },
  { IT_PPUP, 16 }, { IT_ETHER, 2 },
  { IT_PROTEIN, 4 }, { IT_IRON, 4 }, { IT_CARBOS, 4 },
  { IT_LINKCORD, 16 },
  { IT_FIRESTONE, 6 }, { IT_WATERSTONE, 6 }, { IT_THUNDERSTONE, 6 }, { IT_LEAFSTONE, 6 },
  { IT_MOONSTONE, 6 }, { IT_SUNSTONE, 6 }, { IT_SHINYSTONE, 6 }, { IT_DUSKSTONE, 6 },
  { IT_DAWNSTONE, 6 }, { IT_ICESTONE, 6 }, { IT_BLACKAUGURITE, 8 },
};
static const uint8_t ZONE_PRIZE_COUNT = sizeof(ZONE_PRIZES) / sizeof(ZONE_PRIZES[0]);
