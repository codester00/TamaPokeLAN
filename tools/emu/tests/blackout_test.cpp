// The blackout (v3.38): losing a gym or wild fight no longer ends on one more
// line of battle text. When its last message is dismissed the screen goes
// dark, Mom is waiting, and the next tap or swipe wakes you up at home.
//
// The fights are played through the sketch's own tap handler and aimed with
// btlCellIndexAt(), the function the firmware itself answers taps with.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0xB1AC;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y); void onSwipeV(int dir); void onSwipe(int dir);
uint8_t uiCurrentScreen(); extern const char *const SCREEN_NAME[];
void startBattle(int16_t dex, uint8_t lvl);
int btlCellIndexAt(int16_t x, int16_t y);
extern Pet pet;
extern bool battleOpen, btlOver, btlWon, btlLost, blackoutOpen;
extern uint32_t blackoutAt;
extern uint8_t btlMsgCount;
extern Combatant btlYou, btlFoe;
extern Arduino_Canvas *gfx;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static bool on(const char *name) { return !strcmp(SCREEN_NAME[uiCurrentScreen()], name); }

// Where to tap for move cell i: measured off the firmware's own hit test.
static bool cellPoint(int i, int16_t &x, int16_t &y) {
  for (int16_t py = 240; py < 466; py += 4)
    for (int16_t px = 0; px < 466; px += 4)
      if (btlCellIndexAt(px, py) == i) { x = px + 8; y = py + 8; return true; }
  return false;
}

// Play the fight out with the first move, dismissing every message, until the
// battle screen closes.
static void playOut() {
  int16_t mx = 0, my = 0;
  int slot = 0;
  for (int i = 0; i < MOVE_SLOTS; i++) if (btlYou.moves[i]) { slot = i; break; }
  cellPoint(slot, mx, my);
  for (int t = 0; t < 600 && battleOpen; t++) {
    if (btlMsgCount) onTap(233, 233);
    else onTap(mx, my);
  }
}

static void freshCreature(uint8_t level) {
  pet.ageMinutes = (uint32_t)(level - 1) * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(4, false);

  // ---- a lost fight blacks out
  freshCreature(2);
  startBattle(150, 100);                       // MEWTWO at 100 against a newborn
  ck(battleOpen && !btlLost, "a new fight starts with nothing lost");
  playOut();
  ck(btlOver && !btlWon, "(the newborn lost)");
  ck(!battleOpen && blackoutOpen && on("blackout"), "losing ends on the blackout screen");
  gfx->frameReady = false; render();
  ck(gfx->frameReady, "the blackout screen flushes");

  // ---- the quick second tap that dismissed the last message must not skip her
  blackoutAt = millis();
  onTap(233, 233);
  ck(blackoutOpen, "a tap straight after it appears does not skip Mom");
  blackoutAt = millis() - 5000;
  onTap(233, 233);
  ck(!blackoutOpen && on("main"), "a tap after that wakes you up at home");

  // ---- a swipe dismisses it too
  freshCreature(2);
  startBattle(150, 100);
  playOut();
  ck(blackoutOpen, "(blacked out again)");
  blackoutAt = millis() - 5000;
  onSwipeV(1);
  ck(!blackoutOpen && on("main"), "swiping down also wakes you at home");
  freshCreature(2);
  startBattle(150, 100);
  playOut();
  blackoutAt = millis() - 5000;
  onSwipe(1);
  ck(!blackoutOpen && on("main"), "and so does a sideways swipe");

  // ---- winning never blacks out
  freshCreature(100);
  startBattle(10, 2);                          // CATERPIE at 2 against a level 100
  playOut();
  ck(btlOver && btlWon, "(the level 100 won)");
  ck(!blackoutOpen && !btlLost, "a win does not black out");

  // ---- a loss whose flag was left behind cannot leak into the next fight
  btlLost = true;
  freshCreature(100);
  startBattle(10, 2);
  ck(!btlLost, "starting a fight clears a stale loss");
  playOut();
  ck(!blackoutOpen, "so a win after a loss does not black out either");

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
