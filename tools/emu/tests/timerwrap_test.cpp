// Timers across millis()'s 49.7-day wrap (v3.88). Every deadline used to be
// compared directly (`millis() > until`), which is wrong when a timer spans the
// wrap: started 5 s before it, the deadline lands just past zero and reads as
// long over; checked last just before the wrap and next just after, a deadline
// near 2^32 reads as 49 days away -- a farewell ceremony that never ends.
// timers.h's helpers compare the difference; these check the ceremony (pet.cpp),
// a screen's timer (the sketch's release dialog), and that 0 still means "no
// timer" once millis() is past 2^31.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include <cstdio>

uint32_t g_seed = 0x3A9;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void loop();
extern Pet pet;
extern uint32_t confirmUntil;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// Moves the clock to `ms` before the 32-bit wrap (millis() reads 2^32 - ms).
static void toBeforeWrap(uint32_t ms) {
  uint32_t now = millis();
  emuAdvanceMs((uint64_t)(0xFFFFFFFFu - now) + 1 - ms);
}
static void step(uint32_t ms) { emuAdvanceMs(ms); loop(); }

int main() {
  emuVirtualClock(true);                     // time moves only when the test says
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(4, false);

  // 1. A ceremony that spans the wrap runs its full length, then ends.
  pet.ageMinutes = 74UL * MINUTES_PER_LEVEL;
  toBeforeWrap(3000);
  printf("      millis=%u, %lu ms before the wrap\n", (unsigned)millis(), 3000UL);
  pet.startFarewell();
  ck(pet.ceremony != CER_NONE, "(the farewell ceremony started)");
  pet.update(millis());
  ck(pet.ceremony != CER_NONE, "a ceremony spanning the wrap does not end at once");
  emuAdvanceMs(CEREMONY_MS / 2);             // now past the wrap, half way
  pet.update(millis());
  ck(pet.ceremony != CER_NONE, "half way through, past the wrap, it is still running");
  emuAdvanceMs(CEREMONY_MS / 2 + 1000);
  pet.update(millis());
  ck(pet.ceremony == CER_NONE, "it ends once its time is up");
  pet.clearEnded();
  if (pet.isEgg()) pet.dbgHatchAs(4, false);  // the farewell left an egg
  pet.ageMinutes = 74UL * MINUTES_PER_LEVEL;

  // 2. A deadline just before the wrap, next looked at just after it.
  toBeforeWrap(CEREMONY_MS + 2000);
  pet.startFarewell();                       // ends 2 s before the wrap
  ck(pet.ceremony != CER_NONE, "(the second ceremony started)");
  emuAdvanceMs(CEREMONY_MS - 1000);          // checked once, 3 s before the wrap
  pet.update(millis());
  ck(pet.ceremony != CER_NONE, "(still running just before its deadline)");
  emuAdvanceMs(5000);                        // next check 2 s after the wrap
  pet.update(millis());
  ck(pet.ceremony == CER_NONE, "a deadline passed over the wrap is seen as passed, not 49 days off");
  pet.clearEnded();
  if (pet.isEgg()) pet.dbgHatchAs(4, false);

  // 3. A screen's timer (the release dialog) across the wrap, through loop().
  toBeforeWrap(1000);
  confirmUntil = millis() + 3000;            // lands 2 s past the wrap
  step(500);
  ck(confirmUntil != 0, "the dialog stays up across the wrap");
  step(1000);
  ck(confirmUntil != 0, "...and just after it");
  step(2000);
  ck(confirmUntil == 0, "it closes when its time is up");

  // 4. 0 is "no timer", even with millis() past 2^31.
  emuAdvanceMs(0x90000000u);                 // millis() now ~2.4e9
  printf("      millis=%u\n", (unsigned)millis());
  Pet fresh;                                 // every timer unset (0)
  ck(!fresh.eating() && !fresh.showHeart() && !fresh.evolving(), "an unset timer does not run past 2^31");
  ck(timeLeft(millis(), 0) == 0 && !timerRunning(millis(), 0), "timeLeft/timerRunning of 0 are 0/false");

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
