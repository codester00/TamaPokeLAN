// The hidden SETTINGS (v3.59): rule changes that grant nothing, opened by a
// secret code in the Memory game (codes.h). Each is a bit in gameSettings,
// checked INSIDE the one function that already owns its rule -- never by a
// caller. Saved on the player record (v8 tail) and legacy key "stng".
//
// The bit index is saved: APPEND-ONLY. A new setting goes at the end and gets
// its strings appended in i18n in the same order (S_SET_FIRST + bit).
#pragma once
#include <stdint.h>

enum : uint16_t {
  SET_NO_LOCKOUT      = 1u << 0,   // Pet::legendLockedFor()
  SET_SAFETY5         = 1u << 1,   // Pet::legendSafetyEggs()
  SET_LEGEND_EGGS     = 1u << 2,   // eggCanHold() in pet.cpp
  SET_EXPLORE_ALL     = 1u << 3,   // exploreRegionAllowed()
  SET_NO_LEVEL_CAP    = 1u << 4,   // gymLevelCap()
  SET_NO_TEAM_CAP     = 1u << 5,   // squadCap()
  SET_COMPANION_GROWS = 1u << 6,   // Pet::grows()
  SET_NO_BOND_CAP     = 1u << 7,   // Pet::bondCappedToday()
  SET_NO_RUNAWAY      = 1u << 8,   // Pet::canRunawayNow()
  SET_FAIR_SLOTS      = 1u << 9,   // slotDrawOdds()
  SET_NECROZMA_MET    = 1u << 10,  // wildLegendLive()
  SET_CLASSIC_STATS   = 1u << 11,  // battleStat() in stats.h
};
#define SET_COUNT 12

extern uint16_t gameSettings;
// True while a LAN battle is being offered or fought: the settings never apply
// there (both boards must play by the same rules), so every setting reads off.
extern bool settingsSuspended;

inline bool settingOn(uint16_t bit) { return !settingsSuspended && (gameSettings & bit); }
