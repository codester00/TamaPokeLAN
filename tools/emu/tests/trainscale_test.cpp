// v3.58: who fights with training. Easy leaders have none (the games' story
// leaders have no EVs), hard leaders have full training, wild creatures none --
// read off the combatants the real battle starts build, not a copy of the rule.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "battle.h"
#include "trainers.h"
#include "stats.h"
#include "dex.h"
#include <cstdio>
uint32_t g_seed = 58; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void startTrainerBattle(uint8_t idx, bool hard);
// v3.94: a League seat is fought inside a run; this puts one in place.
void leagueDbgAt(uint8_t region, bool hard, uint8_t seat); void leagueEnd(); extern uint8_t gymRegion;
bool startWildBattle(uint8_t region, bool hard);
extern Pet pet;
extern Combatant btlFoe;
extern bool battleOpen;
extern uint8_t wildIv[4], wildLvl;
extern uint16_t squadMask;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// The training a combatant was built with, recovered from its attack.
static int trainingOf(const Combatant &c, uint8_t iv) {
  const DexEntry &e = DEX_TBL[c.dex];
  for (int tr = 0; tr <= 100; tr++)
    if (battleStat(e.bAtk, iv, c.level, tr) == c.base[SI_ATK] &&
        battleStat(e.bDef, iv, c.level, tr) == c.base[SI_DEF] &&
        battleStat(e.bSpe, iv, c.level, tr) == c.base[SI_SPE]) return tr;
  return -1;
}
// Whether c's stats are exactly those of training tr (rounding can make two
// neighbouring values build the same stats, so trainingOf() may name the lower).
static bool builtWith(const Combatant &c, uint8_t iv, uint8_t tr) {
  const DexEntry &e = DEX_TBL[c.dex];
  return battleStat(e.bAtk, iv, c.level, tr) == c.base[SI_ATK] &&
         battleStat(e.bDef, iv, c.level, tr) == c.base[SI_DEF] &&
         battleStat(e.bSpe, iv, c.level, tr) == c.base[SI_SPE];
}

int main() {
  setup();
  pet.dbgHatchAs(6, false);
  pet.ageMinutes = 49UL * MINUTES_PER_LEVEL;
  squadMask = 1;

  const Trainer &lance = TRAINER_SETS[0].list[11];
  leagueDbgAt(gymRegion, false, 11);
  startTrainerBattle(11, false);
  ck(battleOpen && btlFoe.dex == lance.team[0].dex, "an easy Lance fight opened");
  int t = trainingOf(btlFoe, EASY_IV);
  ck(t == 0, "an easy leader fights untrained");
  battleOpen = false;

  leagueDbgAt(gymRegion, true, 11);
  startTrainerBattle(11, true);
  t = trainingOf(btlFoe, HARD_IV);
  char w[96]; snprintf(w, sizeof w, "a hard leader fights at its ladder's tuned training (got %d, want %u)", t, HARD_TRAINING[0]);
  ck(t == HARD_TRAINING[0], w);
  ck(btlFoe.base[SI_ATK] > battleStat(DEX_TBL[btlFoe.dex].bAtk, HARD_IV, btlFoe.level, 0),
     "and it shows in what the creature hits with");
  battleOpen = false;

  // each ladder has its own (v3.95): a Sinnoh hard leader reads Sinnoh's
  {
    uint8_t keep = gymRegion;
    gymRegion = 3;
    leagueDbgAt(gymRegion, true, 11);
    startTrainerBattle(11, true);
    snprintf(w, sizeof w, "a Sinnoh hard leader reads Sinnoh's training (%u), not Kanto's (%u)", HARD_TRAINING[3], HARD_TRAINING[0]);
    ck(battleOpen && builtWith(btlFoe, HARD_IV, HARD_TRAINING[3]) && !builtWith(btlFoe, HARD_IV, HARD_TRAINING[0]), w);
    battleOpen = false;
    gymRegion = keep;
  }

  int wild = 0, untrained = 0;
  for (int i = 0; i < 20; i++) {
    if (!startWildBattle(0, false)) continue;
    wild++;
    // the wild foe's own IVs, per stat
    const DexEntry &e = DEX_TBL[btlFoe.dex];
    if (btlFoe.base[SI_ATK] == battleStat(e.bAtk, wildIv[0], btlFoe.level, 0) &&
        btlFoe.base[SI_DEF] == battleStat(e.bDef, wildIv[1], btlFoe.level, 0) &&
        btlFoe.base[SI_SPE] == battleStat(e.bSpe, wildIv[2], btlFoe.level, 0)) untrained++;
    battleOpen = false;
  }
  ck(wild > 0 && untrained == wild, "wild creatures fight untrained");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
