// Melmetal by walking (v3.83, plan § E7): Meltan evolves after 10,000 steps
// walked while IT is the live pet -- counted in hundreds, kept by the creature
// through banking, the checkpoint and a trade -- and its farewell does not wait.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "trade.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed = 0x3E17;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
bool wasPressed = false;
uint32_t millis() { return 0; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static void raise(Pet &p, int16_t dex) {
  p.dbgHatchAs(dex, false);
  p.uid = p.mintUid();
  p.ageMinutes = 19UL * MINUTES_PER_LEVEL;
  p.energy = p.fullness = p.joy = p.hygiene = 100;
}

int main() {
  gRegionArt = 0xFFFF;
  ck(DEX_TBL[809].rarity == R_EVO, "Melmetal no longer hatches: it is Meltan's");
  Pet p; p.begin();
  raise(p, 808);
  Pet::EvoInfo e = p.evoInfo();
  ck(e.status == Pet::EVO_STEPS && e.target == 809 && e.stepsLeft == 10000,
     "a new Meltan is 10,000 steps from Melmetal");
  ck(!p.evolutionAhead(), "and its farewell does not wait for the walk");
  p.addSteps(9999);
  ck(p.evoInfo().status == Pet::EVO_STEPS && p.evoInfo().stepsLeft == 1, "9,999 steps: one to go");
  p.addSteps(1);
  ck(p.evoInfo().status == Pet::EVO_READY && p.canEvolveNow(), "10,000: it can evolve");
  p.evolve();
  ck(p.speciesId == 809 && p.evoSteps == 0, "into Melmetal, and the count is spent");

  // only while it is the live pet
  raise(p, 25);
  p.addSteps(20000);
  ck(p.evoSteps == 0, "steps walked as another creature count for nothing");

  // it travels with the creature
  raise(p, 808);
  p.addSteps(4321);
  ck(p.evoSteps == 43, "4,321 steps are 43 hundreds");
  PartyMon m = p.toPartyMon();
  ck(m.reserved[0] == 43, "banked, the record keeps them");
  Pet q; q.begin();
  q.switchTo(m);
  ck(q.evoSteps == 43, "and raised again, so does the creature");
  ck(tradeMonValid(m), "a Meltan with a walk under way can be traded");
  PartyMon bad1 = m; bad1.reserved[1] = 1;
  ck(!tradeMonValid(bad1), "the other reserved bytes stay reserved");
  ck(TRADE_PROTO >= 3, "and the trade protocol moved, so an older board refuses rather than drops them");
  p.saveNow();
  Pet r; r.begin();
  ck(r.speciesId == 808 && r.evoSteps == 43, "the pet checkpoint keeps them across a reboot");

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
