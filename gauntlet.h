// The Battle Zone's gauntlet (v4.04, postgame-plan § 3): challengers one after
// another until the player loses, under a set of rules both sides obey. Pure
// rules, no screen and no battle: the sketch's zone code drives them. The
// pacing numbers (gauntletIv/Tr/Smart) are PLACEHOLDERS until league_sim
// tunes them (plan § 3, "Pacing").
#pragma once
#include <stdint.h>
#include "zone.h"
#include "party.h"

enum : uint8_t { GT_ANY = 0, GT_BASIC, GT_MIDDLE, GT_FINAL, GT_NOLEGEND, GT_COUNT };
#define GAUNTLET_TYPE_ANY 0xFF
#define GAUNTLET_CAP_COUNT 4
static const uint8_t GAUNTLET_CAPS[GAUNTLET_CAP_COUNT] = { 0, 30, 50, 70 };   // 0 = no cap

struct GauntletRules {
  uint8_t type = GAUNTLET_TYPE_ANY;   // a creature must have it, or any
  uint8_t levelCap = 0;               // a creature at or below it (0: none)
  uint8_t tier = GT_ANY;
  uint8_t size = 3;                   // 1-6, both sides
};

// A species' place in its line: 0 a base form that evolves, 1 an evolution
// that evolves further, 2 a final form (or a creature that never evolves).
uint8_t speciesStage(int16_t dex);
bool speciesEvolvesFurther(int16_t dex);
// Does this species fit the rules' type and tier? (Level is separate: it is
// the creature's, not the species'.)
bool gauntletSpeciesFits(const GauntletRules &r, int16_t dex);
// The level cap LOCKS OUT a creature above it (owner, v4.04): no scaling down,
// so a capped run is built from creatures other than the player's top six.
bool gauntletLevelFits(const GauntletRules &r, uint8_t level);
// The day's challenge: rules from a hash of the day, so every board shows the
// same ones on the same day. -1 (no clock) gives a fixed set.
GauntletRules gauntletDaily(int32_t day);
// The round-th challenger (0 = the first). The same rules, round and seed
// always give the same team, so a saved run meets the challenger it would have.
// `teamLevel` is the level it fights at when the rules have no cap; its
// trainer class is out.avatar, one of avatars 3..avatarCount-1 (the sketch
// names it). Returns false if the rules leave too few species for a team.
bool gauntletChallenger(const GauntletRules &r, uint16_t round, uint32_t seed,
                        uint8_t teamLevel, uint8_t avatarCount, ZoneFoe &out);
// How a challenger grows with the round (placeholders). The first are easy on
// purpose (owner): below the level, mid IVs, no training, the plain AI, so a
// team that is not the player's best can still get going.
#define GAUNTLET_LEVEL_RAMP 10   // rounds until a challenger reaches the level
uint8_t gauntletLevel(uint16_t round, uint8_t top);   // top: the cap, or the team's best
uint8_t gauntletIv(uint16_t round);
uint8_t gauntletTr(uint16_t round);
bool gauntletSmart(uint16_t round);
bool gauntletSwitches(uint16_t round);   // AI switching (v4.05), the upper rounds only
// v4.09: every GAUNTLET_BOSS_EVERY-th challenger is a BOSS -- IV 31, full
// training, the team's level, the smart AI and switching, drawn from the
// GAUNTLET_BOSS_TOP strongest species the rules allow -- and pays
// GAUNTLET_BOSS_BP more. Beating the first two earns the silver and gold
// symbols (zone.h zoneSymbols(): a best streak of 20 / 40).
#define GAUNTLET_BOSS_EVERY 20
#define GAUNTLET_BOSS_TOP 100
#define GAUNTLET_BOSS_BP 10
bool gauntletIsBoss(uint16_t round);
int gauntletBst(int16_t dex);   // base-stat total
// BP for beating the round-th challenger (doubled for the day's first daily win).
uint8_t gauntletBp(uint16_t round, bool doubled);

// The saved gauntlet (NVS key "gnt", v4.04): one run at a time, kept between
// battles indefinitely, and the day's 2x and best. `fighting` is set while a
// battle is on: a reboot with it set is the loss (plan § 3).
#define GAUNT_SAVE_MAGIC 0x47   // 'G'
struct GauntletSave {
  uint8_t magic = GAUNT_SAVE_MAGIC;
  uint8_t version = 1;
  uint8_t active = 0;
  uint8_t fighting = 0;
  GauntletRules rules;
  uint8_t daily = 0, doubled = 0;
  uint16_t wins = 0;
  int32_t startDay = -1;
  uint32_t seed = 0;
  uint8_t n = 0;
  CreatureUid uid[6];
  uint8_t pp[6][MOVE_SLOTS];
  int32_t claimDay = -2;    // the last day whose daily 2x was claimed
  int32_t bestDay = -2;     // the day bestToday belongs to
  uint16_t bestToday = 0;
};
