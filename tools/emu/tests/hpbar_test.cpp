// The HP bars after a faint (v4.12). Every queued line remembers both sides'
// HP, and the bars drain to the line on screen. A line written before a
// creature came on held the HP of the one it replaced, so the newcomer's bar
// drained to 0 and refilled a line later: the trainer's next on a SHIFT switch
// ("Go, X!" said before it came on), and your replacement when both sides went
// down in one turn (the foe's "sends out" said first). A newcomer's bar must
// never dip below its real HP while nothing has hit it.
#include <cstring>
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "types.h"
#include "trainers.h"
#include <cstdio>
uint32_t g_seed = 0xFA17; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render(); void battleTap(int16_t, int16_t);
void startBattle(int16_t dex, uint8_t lvl);
void startTrainerBattle(uint8_t idx, bool hard);
void btlSwitchCell(uint8_t k, int *x, int *y);
void btlCellCenter(int i, int *x, int *y);
uint8_t btlMenuNow();
int8_t btlTrainerNext();
extern Pet pet;
extern bool battleOpen, btlOver, btlHard;
extern Combatant btlYou, btlFoe, btlSquad[];
extern uint8_t btlSquadN, btlSquadAt, btlFoeAt, btlMenu, btlMsgCount, btlFoeSent, btlRegion;
extern int8_t btlTrainer;
extern uint16_t btlHpShown[2];
extern uint8_t btlSwapDue;
extern uint16_t squadMask;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static void clearMsgs() { for (int i = 0; i < 40 && btlMsgCount; i++) battleTap(233, 300); }
static void tapCell(int i) { int x, y; btlCellCenter(i, &x, &y); battleTap(x, y); }
static void tapOpt(uint8_t k) { int x, y; btlSwitchCell(k, &x, &y); battleTap(x + 80, y + 27); }
static void team() {
  if (pet.isEgg()) pet.dbgHatchAs(150, false);
  pet.dbgHatchAs(150, false);   // MEWTWO
  pet.ageMinutes = 60UL * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
  const int16_t DEX[3] = { 6, 9, 3 };   // CHARIZARD, BLASTOISE, VENUSAUR
  for (int i = 0; i < 3; i++) {
    PartyMon m; m.dex = DEX[i]; m.level = 60;
    m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20;
    m.moves[0] = MV_TACKLE;
    m.uid = pet.mintUid();
    party.slots[i] = m;
  }
  squadMask = 0x0F;   // the pet and the three
}
// Our creature at 1 HP against a level 100 MEWTWO until it goes down.
static void faintOurs() {
  for (int t = 0; t < 30 && !btlYou.fainted(); t++) {
    btlYou.hp = 1;
    btlMenu = 1;
    tapCell(0);
    clearMsgs();
  }
}


// Read every line of the queue, rendering frames, and return the lowest each
// bar showed against what it should show.
static bool barsHold(const char *what) {
  bool ok = true;
  for (int l = 0; l < 12; l++) {
    for (int f = 0; f < 30; f++) {
      render();
      if (btlHpShown[0] < btlYou.hp || btlHpShown[1] < btlFoe.hp) ok = false;
    }
    if (!btlMsgCount) break;
    battleTap(233, 300);
  }
  ck(ok, what);
  return ok;
}

int main() {
  setup();
  for (int i = 0; i < 3; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  for (uint8_t i = 0; i < 13; i++) pet.winBadge(0, i, false);

  // --- 1. SHIFT, YES: the trainer's next must not drain while "Go, X!" shows ---
  team();
  startTrainerBattle(0, false);           // Brock: GEODUDE then ONIX
  btlFoe.hp = 1; btlYou.hp = btlYou.maxHp; btlMenu = 1;
  tapCell(0);
  clearMsgs();
  ck(btlMenuNow() == 5, "(the foe's faint asks whether to switch)");
  tapCell(2);                             // YES
  tapOpt(0);                              // CHARIZARD
  ck(btlFoeAt == 1 && btlSquadAt == 1, "(CHARIZARD and ONIX are out)");
  barsHold("a SHIFT switch: neither newcomer's bar dips while the lines are read");
  battleOpen = false;

  // --- 2. both down in one turn: your pick's bar must not drain on "sends out" ---
  team();
  startTrainerBattle(0, false);
  btlSquad[2].hp = 0;                      // only CHARIZARD and VENUSAUR besides
  btlFoe.hp = 1;                           // our hit drops GEODUDE...
  btlYou.hp = 1;                           // ...and its recoil drops MEWTWO
  btlYou.moves[0] = MV_DOUBLE_EDGE; btlYou.pp[0] = 15;
  btlMenu = 1;
  tapCell(0);
  clearMsgs();
  ck(btlYou.fainted() && btlMenuNow() == 4, "(both went down: you pick)");
  tapOpt(0);                               // CHARIZARD
  ck(btlSquadAt == 1 && btlFoeAt == 1, "(CHARIZARD and ONIX come on together)");
  barsHold("both down in one turn: neither newcomer's bar dips");
  battleOpen = false;

  // --- 3. the mechanism engaged: a hit still drains with its own line ---------------
  team();
  startTrainerBattle(0, false);
  btlYou.hp = btlYou.maxHp;
  uint16_t before = btlFoe.hp;
  btlMenu = 1;
  tapCell(0);                              // a hit on GEODUDE
  bool drained = false;
  for (int f = 0; f < 60; f++) { render(); if (btlHpShown[1] < before) drained = true; }
  ck(drained, "a hit still drains the bar while its line is on screen");
  battleOpen = false;

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
