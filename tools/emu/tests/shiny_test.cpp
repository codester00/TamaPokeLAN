// Shiny eggs (v3.99, owner: "1 shiny per quarter with good care"). The odds are
// pinned to the owner's numbers, not to the function: 1 in 100 after a
// farewell, 1 in 200 after any other ending, up to four times likelier with the
// best care. Then eggs are really laid through newEgg(), so the roll that
// decides is the one measured -- a table of odds nobody rolls proves nothing.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cstdio>
uint32_t g_seed = 0x5A1E; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static void care(Pet &p, bool best) { p.streak = best ? 30 : 0; p.bond = best ? 100 : 0; }

// Shiny eggs among n laid after `ending`, with the given care.
static double rate(Pet &p, uint8_t ending, bool best, int n) {
  int s = 0;
  for (int i = 0; i < n; i++) { care(p, best); p.lastEnd = ending; p.newEgg(); if (p.eggShinyPeek()) s++; }
  return (double)s / n;
}

int main() {
  Pet p; p.begin();
  // --- 1. the owner's numbers
  care(p, false); p.lastEnd = CER_FAREWELL;
  ck(p.eggShinyOdds() == 100, "a farewell egg with no care bonus: 1 in 100");
  care(p, true);
  ck(p.careBonus() == 14, "(the best care is a bonus of 14)");
  ck(p.eggShinyOdds() == 25, "a farewell egg with the best care: 1 in 25 (about one a quarter)");
  p.lastEnd = CER_RELEASE; care(p, false);
  ck(p.eggShinyOdds() == 200, "a release egg with no care: 1 in 200");
  care(p, true);
  ck(p.eggShinyOdds() == 50, "a release egg with the best care: 1 in 50");
  p.lastEnd = CER_RUNAWAY; care(p, false);
  ck(p.eggShinyOdds() == 200, "a runaway egg pays like a release, not a farewell");
  // care in between moves the odds steadily, never past the best
  p.lastEnd = CER_FAREWELL;
  bool steady = true;
  for (int st = 0; st <= 30; st += 3) for (int bd = 0; bd <= 100; bd += 25) {
    p.streak = st; p.bond = bd;
    int o = p.eggShinyOdds();
    if (o < 25 || o > 100) steady = false;
  }
  p.streak = 15; p.bond = 50;      // bonus 7: halfway
  ck(steady && p.eggShinyOdds() == 40, "care in between lands in between (bonus 7: 1 in 40), never past 1 in 25");

  // --- 2. the roll newEgg() really makes
  const int N = 20000;
  double fb = rate(p, CER_FAREWELL, true, N), f0 = rate(p, CER_FAREWELL, false, N);
  double rb = rate(p, CER_RELEASE, true, N);
  printf("measured: farewell best %.4f, farewell none %.4f, release best %.4f\n", fb, f0, rb);
  ck(fb > 0.033 && fb < 0.047, "laid eggs: a well-cared farewell is shiny about 4% of the time");
  ck(f0 > 0.007 && f0 < 0.013, "laid eggs: an uncared farewell about 1%");
  ck(rb > 0.015 && rb < 0.025, "laid eggs: a well-cared release about 2%");
  ck(fb < 0.06, "no longer the 1 in 10 two boards were hatching");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
