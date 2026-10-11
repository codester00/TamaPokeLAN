// Asserts against the real Pet, per CLAUDE.md: restating the formulas here
// would only prove the transcription. Covers the level cap (which is what stops
// the uint8_t wrap) and the species-level special split.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "dex.h"
#include <cstdio>

uint32_t g_seed = 0xC0FFEE;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
volatile bool g_dummyTouch = false;
bool wasPressed = false;
uint32_t millis() { return 0; }
void FakeESP::restart() { exit(0); }
void sfxPlay(uint8_t) {}   // lives in the sketch; audio is not under test
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

int main() {
  Pet p;
  p.dbgHatchAs(65, false);      // Alakazam: bAtk 50, bSpA 135
  p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = 31;
  p.trAtk = p.trDef = p.trSpe = 0;

  // --- level cap. 3 days = 73 (first farewell offer), 4d3h = 100, and the
  // RTC's two-week catch-up used to compute 337 and wrap to 81.
  p.ageMinutes = 3UL * 24 * 60;
  ck(p.level() == 73, "3 days -> level 73");
  p.ageMinutes = 5940;                       // 4d 3h
  ck(p.level() == 100, "4d3h -> level 100");
  p.ageMinutes = 14UL * 24 * 60;             // max RTC offline catch-up
  printf("     two-week catch-up: level=%u (unclamped would be 337 -> wraps to 81)\n", p.level());
  ck(p.level() == 100, "two-week catch-up clamps instead of wrapping");
  p.ageMinutes = 60UL * 24 * 60;             // absurd, far past any wrap point
  ck(p.level() == 100, "60 days still clamps");

  // --- special split: Alakazam must hit far harder specially than physically.
  p.ageMinutes = 5940;
  uint16_t atk = p.atkStat(), spa = p.spaStat();
  printf("     Alakazam L%u: atk=%u spa=%u (base 50 vs 135)\n", p.level(), atk, spa);
  ck(spa > atk, "Alakazam spaStat beats atkStat");

  Pet m;
  m.dbgHatchAs(68, false);      // Machamp: bAtk 130, bSpA 65
  m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 31;
  m.trAtk = m.trDef = m.trSpe = 0;
  m.ageMinutes = 5940;
  printf("     Machamp  L%u: atk=%u spa=%u (base 130 vs 65)\n",
         m.level(), m.atkStat(), m.spaStat());
  ck(m.atkStat() > m.spaStat(), "Machamp atkStat beats spaStat");
  ck(m.atkStat() > atk && spa > m.spaStat(), "the two species are not interchangeable");

  // --- fitness and the weight on the card
  // Pinned to the PUBLISHED Pokedex weights, not to the generated table read
  // back: a misaligned table (every species one row off) would still agree
  // with itself.
  ck(DEX_WEIGHT_HG[25] == 60 && DEX_WEIGHT_HG[143] == 4600 &&
     DEX_WEIGHT_HG[92] == 1 && DEX_WEIGHT_HG[797] == 9999 && DEX_WEIGHT_HG[1025] == 3,
     "the table holds the real Pokedex weights (Pikachu 6.0, Snorlax 460, Gastly 0.1, Celesteela 999.9, Pecharunt 0.3 kg)");
  {
    Pet egg; egg.begin(); egg.newEgg();
    ck(egg.weight == WEIGHT_START && egg.fitness() == 75, "a new egg starts at fitness 75");
    ck(egg.weightHg() == 0, "and an egg has no weight to show");

    Pet s; s.dbgHatchAs(143, false);
    s.weight = WEIGHT_START;
    ck(s.weightHg() == 4600, "fitness 75 is exactly the Pokedex weight (Snorlax 460.0 kg)");
    s.weight = WEIGHT_MIN;
    ck(s.fitness() == 99 && s.weightHg() == 3496, "fitness 99 is 24% lighter (349.6 kg)");
    s.weight = 100;
    ck(s.fitness() == 0 && s.weightHg() == 8050, "fitness 0 is 75% heavier (805.0 kg)");

    Pet g; g.dbgHatchAs(92, false);
    g.weight = WEIGHT_MIN;
    ck(g.weightHg() >= 1, "the lightest creature at its lightest still weighs something");
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
