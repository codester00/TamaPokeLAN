// v3.59: the secret codes and the hidden SETTINGS. Codes are tapped into the
// Memory game's watch phase through the real tap handler; each setting is
// checked through the function that owns its rule, and the rules mark must
// outlive turning the setting off and a reload (checkpoint and legacy key).
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "battle.h"
#include "trainers.h"
#include "gamecorner.h"
#include "codes.h"
#include "settings.h"
#include <cstdio>
uint32_t g_seed = 59; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
void startSimon();
void mgPress(int16_t x, int16_t y, uint32_t now);
void simonBerryPos(uint8_t i, int *x, int *y);
void codeApply(uint8_t id);
void codesTap(int16_t x, int16_t y);
int codesRowAt(int16_t x, int16_t y);
uint8_t codesPages();
void areaRowRect(int i, int *x, int *y, int *w, int *h);
uint8_t squadCap(uint8_t idx, bool hard);
bool exploreRegionAllowed(uint8_t r);
void startTrainerBattle(uint8_t idx, bool hard);
extern Pet pet;
extern uint8_t miniGame, hallOpen, codesPage;
extern bool simonShowing, codesOpen, exploreOpen, battleOpen, trainOpen;
extern uint16_t squadMask;
extern Combatant btlSquad[];

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static void tapBerries(const uint8_t *seq, int n) {
  for (int i = 0; i < n; i++) { int x, y; simonBerryPos(seq[i], &x, &y); mgPress(x, y, millis()); }
}
static void freshMemory() {
  codesOpen = false; hallOpen = 0; exploreOpen = false;
  startSimon();
  ck(miniGame != 0 && simonShowing, "the Memory game opens on its watch phase");
}

int main() {
  setup();
  pet.dbgHatchAs(25, false);
  pet.ageMinutes = 20UL * MINUTES_PER_LEVEL;
  pet.joy = 50;

  // --- 1. a code in the watch phase fires, and pays nothing
  freshMemory();
  uint8_t joy0 = pet.joy, bond0 = pet.bond;
  tapBerries(CODES[CODE_SETTINGS].seq, CODE_LEN);
  ck(codesOpen && !miniGame, "SETTINGS tapped in the watch phase opens the hidden screen");
  ck(pet.joy == joy0 && pet.bond == bond0, "and the round it ended paid nothing");

  // --- 2. seven of eight does nothing; the game goes on
  freshMemory();
  tapBerries(CODES[CODE_SETTINGS].seq, CODE_LEN - 1);
  ck(!codesOpen && miniGame && simonShowing, "seven of eight taps do nothing");

  // --- 3. the same taps outside the watch phase are just the game
  freshMemory();
  simonShowing = false;
  tapBerries(CODES[CODE_SETTINGS].seq, CODE_LEN);
  ck(!codesOpen, "the code tapped while repeating the sequence is not a code");
  miniGame = 0;

  // --- 4. a hall code opens the hall once, without opening its gate
  freshMemory();
  tapBerries(CODES[CODE_CELADON].seq, CODE_LEN);
  ck(exploreOpen && hallOpen == HALL_CELADON + 1, "CELADON opens Celadon Game Corner directly");
  ck(!hallUnlocked(HALL_CELADON, pet), "and its gate in the area list is still shut");
  hallOpen = 0; exploreOpen = false;
  codeApply(CODE_SURF);
  ck(exploreOpen && hallOpen == HALL_BEACH + 1, "SURF opens the beach house");
  ck(!surfSecretOpen(pet), "and the secret itself is still closed");
  hallOpen = 0; exploreOpen = false;

  // --- 5. each setting changes its rule through the function that owns it
  codeApply(CODE_SETTINGS);
  ck(codesOpen && pet.rulesUsed == 0, "a fresh save has no rules mark");
  pet.legendAttempts = 5;
  ck(pet.legendLockedFor(CER_RELEASE), "past the free attempt, a release egg is locked out");
  pet.setSetting(SET_NO_LOCKOUT, true);
  ck(!pet.legendLockedFor(CER_RELEASE), "with the lockout off it is not");
  ck(Pet::legendSafetyEggs() == LEGEND_SAFETY_EGGS, "the safety net waits its usual count");
  pet.setSetting(SET_SAFETY5, true);
  ck(Pet::legendSafetyEggs() == 5, "and 5 with the setting");
  ck(!exploreRegionAllowed(REGION_ALL), "Explore never rests on ALL");
  pet.setSetting(SET_EXPLORE_ALL, true);
  ck(exploreRegionAllowed(REGION_ALL), "unless the setting allows it");
  uint8_t cap = squadCap(0, true);
  pet.setSetting(SET_NO_TEAM_CAP, true);
  ck(cap < TRAINER_TEAM_MAX && squadCap(0, true) == TRAINER_TEAM_MAX, "hard's team cap lifts");
  pet.bond = 10;
  for (int i = 0; i < 30 && !pet.bondCappedToday(); i++) pet.playJoyGame(100);
  ck(pet.bondCappedToday(), "the daily bond cap holds");
  uint8_t b = pet.bond; pet.playJoyGame(100);
  ck(pet.bond == b, "and a game pays no more bond");
  pet.setSetting(SET_NO_BOND_CAP, true);
  b = pet.bond; pet.playJoyGame(100);
  ck(!pet.bondCappedToday() && pet.bond > b, "with the setting bond keeps coming");
  pet.frozen = true;
  ck(!pet.grows(), "a revived companion does not grow");
  pet.setSetting(SET_COMPANION_GROWS, true);
  ck(pet.grows(), "unless companions grow");
  pet.frozen = false;
  uint16_t atk = pet.atkStat();
  pet.trAtk = 100; uint16_t scaled = pet.atkStat();
  pet.setSetting(SET_CLASSIC_STATS, true);
  ck(pet.atkStat() - scaled == 100 - pet.level(), "classic stats count training flat");
  (void)atk;

  // the level cap, through a real gym start
  pet.ageMinutes = 79UL * MINUTES_PER_LEVEL; squadMask = 1;
  startTrainerBattle(0, false);
  uint8_t capped = btlSquad[0].level; battleOpen = false;
  pet.setSetting(SET_NO_LEVEL_CAP, true);
  startTrainerBattle(0, false);
  ck(capped < 80 && btlSquad[0].level == 80, "the gym level cap lifts");
  battleOpen = false;

  // --- 6. none of it applies to a LAN battle
  settingsSuspended = true;
  ck(!settingOn(SET_CLASSIC_STATS) && pet.atkStat() != 0, "a LAN battle reads every setting as off");
  settingsSuspended = false;

  // --- 7. the rules mark is permanent
  uint16_t used = pet.rulesUsed;
  for (int k = 0; k < SET_COUNT; k++) pet.setSetting((uint16_t)(1u << k), false);
  ck(gameSettings == 0 && pet.rulesUsed == used && used != 0, "turning them all off keeps the mark");
  pet.flushSave();
  { Pet r; r.begin(); ck(r.rulesUsed == used && gameSettings == 0, "the mark survives a reload"); }
  pet.setSetting(SET_NO_RUNAWAY, true); pet.flushSave();
  {
    Preferences pr; pr.begin("tamapoke", false);
    pr.remove("plyA"); pr.remove("plyB"); pr.end();
    Pet r; r.begin();
    ck(r.rulesUsed == (used | SET_NO_RUNAWAY) && gameSettings == SET_NO_RUNAWAY, "and through the legacy keys alone");
  }

  // --- 8. the screen toggles the bit under the tap
  gameSettings = 0;
  codesOpen = true; codesPage = 0;
  int rx, ry, rw, rh; areaRowRect(1, &rx, &ry, &rw, &rh);
  ck(codesRowAt(rx + rw / 2, ry + rh / 2) == 1, "the second row is the second setting");
  onTap(rx + rw / 2, ry + rh / 2);
  ck(gameSettings == (1u << 1), "tapping it turns it on");
  // every row, every page: each offered setting once, and never the retired
  // fair slots (v3.73) -- the row after it is the next bit, not a dead one
  gameSettings = 0;
  for (uint8_t pg = 0; pg < codesPages(); pg++) {
    codesPage = pg;
    for (int i = 0; i < 3; i++) {
      areaRowRect(i, &rx, &ry, &rw, &rh);
      if (codesRowAt(rx + rw / 2, ry + rh / 2) >= 0) onTap(rx + rw / 2, ry + rh / 2);
    }
  }
  uint16_t all = (uint16_t)((1u << SET_COUNT) - 1);
  ck(gameSettings == (all & ~SET_RETIRED) && !(gameSettings & SET_FAIR_SLOTS),
     "the screen offers every setting but the retired fair slots");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
