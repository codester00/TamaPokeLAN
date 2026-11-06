// Running is a loss (v4.12, owner). RUN from a trainer, the zone, the Hall or a
// LAN rival asks first -- "Leave? It counts as a loss" -- and NO goes back to
// the fight. YES ends it as a loss: a gym's goes on to Mom's blackout (the
// owner's call), a League fight ends the League run, and nothing is won. A
// wild creature keeps its escape roll and is never asked. Driven through
// battleTap(); the LAN half (both boards record it) is link_test's.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "trainers.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed = 0x4A2; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render(); void battleTap(int16_t, int16_t);
void onTap(int16_t x, int16_t y);
bool startWildBattle(uint8_t region, bool hard);
void startTrainerBattle(uint8_t idx, bool hard);
void btlCellCenter(int i, int *x, int *y);
void uiConfirmRects(int *b1Top, int *b1Bot, int *b2Top, int *b2Bot);
int btlCellIndexAt(int16_t x, int16_t y);
void uiConfirmX(int *x0, int *x1);
int leagueNext();
void emuAdvanceMs(uint64_t ms);
extern Pet pet;
extern bool battleOpen, btlOver, btlWon, btlRunAsk, blackoutOpen, btlWild;
extern uint8_t btlMsgCount, btlMenu, gymRegion;
extern uint16_t squadMask;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static void tapCell(int i) { int x, y; btlCellCenter(i, &x, &y); render(); battleTap(x, y); }
static void tapYes() { int a, b, c, d; uiConfirmRects(&a, &b, &c, &d); render(); battleTap(233, (a + b) / 2); }
static void tapNo() { int a, b, c, d; uiConfirmRects(&a, &b, &c, &d); render(); battleTap(233, (c + d) / 2); }
static void clearMsgs() { for (int i = 0; i < 40 && btlMsgCount; i++) { render(); battleTap(233, 300); } }
static void readyPet() {
  if (pet.isEgg()) pet.dbgHatchAs(6, false);
  pet.ageMinutes = 60UL * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
  squadMask = 1;
}

int main() {
  setup();
  for (int i = 0; i < 3; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  readyPet();

  // --- 1. a gym: asked, NO keeps the fight, YES is a loss and Mom's blackout ---
  gymRegion = 0;
  startTrainerBattle(0, false);                 // Brock
  ck(battleOpen && !btlWild, "(a gym fight)");
  tapCell(3);                                   // RUN
  ck(btlRunAsk && battleOpen && !btlOver, "RUN from a gym asks first");
  // a tap on FIGHT beside the question's buttons (inside them would be an answer)
  int ex = 0, ey = 0, CONFIRM_X0, CONFIRM_X1;
  uiConfirmX(&CONFIRM_X0, &CONFIRM_X1);
  for (int y = 260; y < 400 && !ex; y += 4) for (int x = 60; x < 120 && !ex; x += 2)
    if (btlCellIndexAt(x, y) == 0 && (x < CONFIRM_X0 || x > CONFIRM_X1)) { ex = x; ey = y; }
  ck(ex > 0, "(a point on FIGHT outside the question's buttons)");
  render(); battleTap(ex, ey);
  ck(btlRunAsk && btlMenu == 0, "nothing under the question answers");
  tapNo();
  ck(!btlRunAsk && battleOpen && !btlOver, "NO goes back to the fight");
  tapCell(3);
  tapYes();
  ck(btlOver && !btlWon && btlMsgCount > 0, "YES ends it as a loss, said as a line");
  ck(!pet.hasBadge(0, 0, false), "and wins nothing");
  clearMsgs();
  ck(!battleOpen && blackoutOpen, "dismissed, it blacks out like any gym loss");
  for (int g = 0; g < 20 && blackoutOpen; g++) { emuAdvanceMs(3000); onTap(233, 233); }

  // --- 2. a wild creature: the escape roll, never the question -----------------
  readyPet();
  ck(startWildBattle(0, false) && battleOpen && btlWild, "(a wild fight)");
  tapCell(3);
  ck(!btlRunAsk, "RUN from a wild creature does not ask");
  battleOpen = false;

  // --- 3. a League fight: running ends the run --------------------------------
  readyPet();
  const int LF = TRAINER_COUNT - TRAINER_ELITE4 - 1;
  for (int i = 0; i < LF; i++) pet.winBadge(0, (uint8_t)i, false);
  startTrainerBattle((uint8_t)LF, false);
  ck(battleOpen && leagueNext() == LF, "(the first Elite Four seat starts a League run)");
  tapCell(3);
  tapYes();
  ck(leagueNext() == -1, "running from a League fight ends the League run");
  clearMsgs();
  ck(blackoutOpen, "and blacks out");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
