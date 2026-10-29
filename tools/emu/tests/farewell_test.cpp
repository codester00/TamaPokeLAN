// The farewell waits for an evolution only when one can still come (v3.72).
//
// It used to ask the species table whether there was a next form, so a
// KADABRA -- whose ALAKAZAM is reached only by trade -- was never offered a
// farewell, at level 100 or ever: reported from a board after days at 100.
// Pinned both ways, since the fix must not let a creature leave with an
// evolution still ahead of it.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cstdio>
uint32_t g_seed = 72; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 1; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// a cared-for creature at level 100, well past the farewell age
static void old(Pet &p, int16_t dex) {
  p.dbgHatchAs(dex, false);
  p.ageMinutes = (uint32_t)(MAX_LEVEL - 1) * MINUTES_PER_LEVEL + 5 * 24 * 60;
  if (p.ageMinutes < FAREWELL_AGE_MIN) p.ageMinutes = FAREWELL_AGE_MIN;
  p.fullness = p.joy = p.energy = p.hygiene = 100;
  p.sleeping = false;
  p.tradeEvo = false;
}

int main() {
  Pet p; p.begin();

  old(p, 64);                                   // KADABRA
  ck(p.level() == MAX_LEVEL, "(KADABRA is at level 100)");
  ck(p.evoInfo().status == Pet::EVO_NEEDS_TRADE, "(its evolution needs a trade)");
  ck(p.canFarewellNow(), "an unmarked KADABRA at 100 is offered its farewell");

  p.tradeEvo = true;                            // traded, or the Linking Cord
  ck(p.evoInfo().status == Pet::EVO_READY, "(marked, it can evolve now)");
  ck(!p.canFarewellNow(), "a marked KADABRA waits for its ALAKAZAM");
  p.evolve();
  ck(p.speciesId == 65, "(it evolves)");
  ck(p.canFarewellNow(), "and the ALAKAZAM is offered its farewell");

  old(p, 93);                                   // HAUNTER, another pure trade
  ck(p.canFarewellNow(), "an unmarked HAUNTER at 100 is offered its farewell");

  old(p, 5);                                    // CHARMELEON: levels evolve it
  ck(!p.canFarewellNow(), "a CHARMELEON with its evolution due is not");
  p.joy = 10;                                   // due but blocked by care
  ck(!p.canFarewellNow(), "nor while care holds its evolution back");

  old(p, 6);                                    // CHARIZARD, final form
  ck(p.canFarewellNow(), "a CHARIZARD at 100 is offered its farewell");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
