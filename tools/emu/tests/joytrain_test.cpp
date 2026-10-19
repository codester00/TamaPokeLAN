// Petting that wears off, the joy/bond game that refreshes it, and training
// split across stats -- the owner's design (v3.34), asserted against the real
// Pet. Step values come from PET_JOY_STEPS, not retyped here.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cstdio>
uint32_t g_seed = 41; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static void fresh(Pet &p) {
  p.begin();
  p.dbgHatchAs(25, false);
  p.sleeping = false;
  p.joy = 20;
  p.bond = 10;
  p.dbgResetBondToday();
  p.petStep = 0;
  p.trAtk = p.trDef = p.trSpe = 0;
  p.ivAtk = p.ivDef = p.ivSpe = 31;
}

int main() {
  const uint8_t STEPS[PET_JOY_STEP_COUNT] = PET_JOY_STEPS;

  // --- 1. petting wears off, step by step, to nothing
  {
    Pet p; fresh(p);
    bool steps = true, bondFades = true;
    for (int i = 0; i < PET_JOY_STEP_COUNT + 2; i++) {
      uint8_t j0 = p.joy, b0 = p.bond;
      p.caress();
      uint8_t want = STEPS[i < PET_JOY_STEP_COUNT ? i : PET_JOY_STEP_COUNT - 1];
      if (p.joy - j0 != want) { steps = false; printf("      tap %d gave %d, want %d\n", i, p.joy - j0, want); }
      if ((p.bond - b0) != (want ? 1 : 0)) bondFades = false;
    }
    ck(steps, "each tap gives 10, 8, 6, 4, 2, then 0 -- and stays at 0");
    ck(bondFades, "bond fades with it: +1 per tap while it still gives joy");
  }

  // --- 2. the joy game makes petting fresh again, and pays joy AND bond
  {
    Pet p; fresh(p);
    for (int i = 0; i < PET_JOY_STEP_COUNT; i++) p.caress();
    uint8_t j0 = p.joy, b0 = p.bond;
    p.caress();
    ck(p.joy == j0, "(worn out: a tap does nothing)");
    j0 = p.joy;
    p.playJoyGame(20);
    ck(p.joy - j0 == JOYGAME_JOY_MAX, "a good round of the joy game gives the full joy");
    ck(p.bond - b0 == JOYGAME_BOND_MAX, "and the full bond");
    j0 = p.joy;
    p.caress();
    ck(p.joy - j0 == STEPS[0], "and petting is back to its best");
    Pet q; fresh(q);
    uint8_t jq = q.joy;
    q.playJoyGame(0);
    ck(q.joy > jq && q.joy - jq < JOYGAME_JOY_MAX, "a poor round still pays something, but less");
  }

  // --- 3. petting is NOT care for the 8-hour window -- or tapping would hold
  //        off neglect forever without feeding it
  {
    Pet p; fresh(p);
    p.unattended = 300;
    p.caress();
    ck(p.unattended == 300, "petting does not restart the grace window");
    p.playJoyGame(5);
    ck(p.unattended == 0, "playing does");
  }

  // --- 4. the petting step survives a reboot
  {
    Pet p; fresh(p);
    p.caress(); p.caress(); p.caress();
    uint8_t s = p.petStep;
    p.saveNow();
    Pet r; r.begin();
    ck(s == 3 && r.petStep == s, "how worn out petting is survives a reload");
  }

  // --- 5. training split across two stats: the same total, halved
  {
    Pet p; fresh(p);
    uint8_t g = p.trainSplit(TR_STR | TR_SPE, TRAIN_SESSION_MAX);
    ck(p.trAtk == TRAIN_SESSION_MAX / 2 && p.trSpe == TRAIN_SESSION_MAX / 2 && p.trDef == 0,
       "a full hybrid session: half to each of its two stats, none to the third");
    ck(g == TRAIN_SESSION_MAX, "and the same total a single-stat session pays");
    Pet q; fresh(q);
    q.trainSplit(TR_DEF | TR_SPE, 17);
    ck(q.trDef + q.trSpe == 17 && (q.trDef == 9 || q.trSpe == 9), "an odd total still pays every point");
    Pet r; fresh(r);
    r.trainSplit(TR_STR, 200);
    ck(r.trAtk == TRAIN_SESSION_MAX, "a session never pays more than its cap");
    Pet c; fresh(c);
    c.ivAtk = 0;
    c.trAtk = c.trMaxAtk();
    uint8_t gc = c.trainSplit(TR_STR | TR_SPE, TRAIN_SESSION_MAX);
    ck(c.trAtk == c.trMaxAtk() && gc == TRAIN_SESSION_MAX / 2,
       "a stat already at its ceiling takes nothing; the other still gets its half");
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
