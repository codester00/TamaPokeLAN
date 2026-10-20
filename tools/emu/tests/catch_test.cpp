// Berry Catch, the first joy/bond game (v3.34): berries score, rocks cost a
// point, a miss costs nothing, and finishing -- or leaving early -- pays joy and
// bond through Pet::playJoyGame(), which makes petting fresh again.
//
// Driven through the sketch's own catchStep() with a hand-held clock, so the
// rules are the firmware's and not restated here.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cstdio>
uint32_t g_seed = 47; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void setup();
extern Pet pet;
extern bool catchOpen;
extern uint16_t catchScore;
extern uint32_t catchUntil, catchOverUntil, catchNextSpawn;
extern float catchBasketX, catchTarget;
struct CatchThing { float x, y, vy; bool rock, live; };
extern CatchThing catchThings[];
void startCatch();
void leaveCatch();
void catchStep(uint32_t now);
void joyGainText(char *out, size_t n, uint8_t joyGain, uint8_t bondGain);
extern uint32_t catchOuchUntil;
#include <cstring>
static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// Drop one thing straight onto (or beside) the basket and run frames until it lands.
static void drop(bool rock, float dx, uint32_t &now) {
  for (int i = 0; i < 8; i++) catchThings[i].live = false;
  catchThings[0] = { catchBasketX + dx, 300, 6, rock, true };
  catchNextSpawn = now + 1000000;     // nothing else falls meanwhile
  for (int f = 0; f < 40 && catchThings[0].live; f++) { now += 85; catchStep(now); }
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(25, false);
  pet.sleeping = false;

  startCatch();
  ck(catchOpen && catchScore == 0, "BERRY CATCH opens with nothing caught");
  uint32_t now = catchUntil - 20000;
  catchTarget = catchBasketX;
  drop(false, 0, now);
  ck(catchScore == 1, "a berry into the basket scores");
  drop(false, 0, now);
  drop(true, 0, now);
  ck(catchScore == 1, "a rock into the basket costs a point");
  drop(false, 200, now);
  ck(catchScore == 1, "a berry that misses costs nothing");

  // the basket follows the finger, eased rather than teleported
  catchTarget = catchBasketX + 100;
  float x0 = catchBasketX;
  now += 85; catchStep(now);
  ck(catchBasketX > x0 && catchBasketX < x0 + 100, "the basket eases after the finger");

  // finishing pays joy and bond, and refreshes petting
  for (int i = 0; i < 6; i++) pet.caress();
  pet.joy = 20;
  pet.dbgResetBondToday();
  uint8_t j0 = pet.joy, b0 = pet.bond;
  catchStep(catchUntil);
  ck(catchOverUntil != 0, "the clock running out ends the round");
  ck(pet.joy > j0 && pet.bond > b0, "and pays joy and bond");
  ck(pet.petStep == 0, "and petting is fresh again");

  // leaving early keeps what was caught
  startCatch();
  now = catchUntil - 20000;
  catchTarget = catchBasketX;
  for (int i = 0; i < 6; i++) pet.caress();
  drop(false, 0, now);
  j0 = pet.joy;
  leaveCatch();
  ck(!catchOpen && pet.joy > j0 && pet.petStep == 0, "leaving early still pays, and refreshes petting");

  // A rock in the basket says so: the "-1" flash over the basket.
  startCatch();
  now = catchUntil - 20000;
  catchTarget = catchBasketX;
  drop(true, 0, now);
  ck(catchOuchUntil > now, "a caught rock flashes -1");
  drop(false, 0, now);
  catchOuchUntil = 0;
  drop(false, 0, now);
  ck(catchOuchUntil == 0, "and a berry does not");
  leaveCatch();

  // A result that paid nothing says WHY, rather than "+0 joy +0 bond" -- which
  // a player read as the game being broken (v3.37). Each case is played for
  // real, so the text is judged against what playJoyGame() actually paid.
  char g[64];
  auto play = [&](uint8_t joy, bool spentToday) {
    pet.joy = joy; pet.bond = 40; pet.dbgResetBondToday();
    if (spentToday) while (!pet.bondCappedToday()) pet.playJoyGame(30);
    pet.joy = joy;
    uint8_t b0 = pet.bond;
    uint8_t jg = pet.playJoyGame(12);
    joyGainText(g, sizeof(g), jg, pet.bond - b0);
    printf("      joy=%u spent=%d -> \"%s\"\n", joy, spentToday, g);
  };
  play(20, false);
  ck(strstr(g, "+30 JOY") && strstr(g, " BOND") && !strstr(g, "+0") && !strstr(g, "MAX"), "room to grow: plain gains");
  play(100, false);
  ck(strstr(g, "JOY MAX") && strstr(g, "+") && !strstr(g, "TODAY"), "joy already full: JOY MAX");
  play(20, true);
  ck(strstr(g, "+30 JOY") && strstr(g, "BOND MAX TODAY"), "bond allowance spent: BOND MAX TODAY");
  play(100, true);
  ck(strstr(g, "JOY MAX") && strstr(g, "BOND MAX TODAY") && !strstr(g, "+"),
     "both: neither reads as +0");

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
