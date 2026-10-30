// A move offer must not outlive the creature it was for (v3.87). The offer queue
// was cleared by hatch() but not by newEgg(), so a creature that ended -- a
// runaway needs no tap at all -- with "learn a new move?" pending left the
// offer on the EGG: the learn screen drew a creature of species -1 (a read
// before the dex table) and a tap would have taught an egg a move. Found by the
// simulated playthrough (tools/emu/playsim.cpp) under AddressSanitizer.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cstdio>
uint32_t g_seed = 0xE0FF; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
static uint32_t g_ms = 0; uint32_t millis() { return g_ms; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

int main() {
  Pet p; p.begin();
  if (p.awaitingStarter()) p.chooseStarter(4);
  const char *names[] = { "farewell", "runaway", "release" };
  for (int e = 0; e < 3; e++) {
    p.dbgHatchAs(4, false);                    // a Charmander...
    p.ageMinutes = 59UL * MINUTES_PER_LEVEL;   // ...at 60, with every slot full
    for (int i = 0; i < MOVE_SLOTS; i++) p.moves[i] = (uint8_t)(1 + i);
    p.learnRange(0, 60, true);
    char m[80];
    snprintf(m, sizeof(m), "(%s: a move offer is waiting)", names[e]);
    ck(p.hasLearnOffer(), m);
    if (e == 0) p.startFarewell(); else if (e == 1) p.startRunaway(); else p.release();
    g_ms += 60000;                             // the ceremony runs out
    p.update(g_ms);
    snprintf(m, sizeof(m), "after a %s the egg carries no move offer", names[e]);
    ck(p.isEgg() && !p.hasLearnOffer(), m);
  }
  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
