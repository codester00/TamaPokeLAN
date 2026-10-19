// The owner's rule for how needs fall (v3.34), tested as the scenario he gave:
//
//   "A kid should be able to walk away from a fully topped-up pet for an
//    8 hour period, without putting the pet to sleep, and have it be hungry
//    and tired and needing attention, but not get penalised. Beyond 8 hours
//    there should be consequences."
//
// Every number here is asked of pet.h (GRACE_MIN, GRACE_FLOOR) or measured off
// the real Pet; nothing restates a drain rate.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cstdio>
uint32_t g_seed = 31; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// 13:00, so nothing here is near the night window
static uint32_t at(uint32_t minutes) { return 13UL * 3600 + 90 + minutes * 60; }

static void fresh(Pet &p) {
  p.begin();
  p.dbgHatchAs(147, false);
  p.sleeping = false;
  p.sleepAuto = SLEEP_NONE;
  p.fullness = p.joy = p.energy = p.hygiene = 100;
  p.poops = 0;
  p.careMistakes = 0;
  p.feedCandy();                    // topped up = just cared for (petting is not care)
  p.fullness = p.joy = p.energy = p.hygiene = 100;
  p.lastSeenEpoch = at(0);
}
static uint32_t clockMin = 0;
static void live(Pet &p, uint32_t n) {
  for (uint32_t i = 0; i < n; i++) { p.lastSeenEpoch = at(clockMin++); p.dbgTick(); }
}
static uint8_t lowest(const Pet &p) {
  uint8_t m = p.fullness;
  if (p.joy < m) m = p.joy;
  if (p.energy < m) m = p.energy;
  if (p.hygiene < m) m = p.hygiene;
  return m;
}

int main() {
  // --- 1. the 8 hours: hungry and tired, NOT penalised
  {
    Pet p; fresh(p); clockMin = 0;
    uint8_t lowestSeen = 100;
    for (uint32_t m = 0; m < GRACE_MIN - 1; m++) {
      live(p, 1);
      if (lowest(p) < lowestSeen) lowestSeen = lowest(p);
    }
    printf("      after %lu min untended: food=%u ene=%u joy=%u hyg=%u poops=%u\n",
           (unsigned long)GRACE_MIN - 1, p.fullness, p.energy, p.joy, p.hygiene, p.poops);
    ck(p.careMistakes == 0, "8 hours untended, awake: no care mistake");
    ck(!p.canRunawayNow(), "and nowhere near running away");
    ck(lowestSeen >= GRACE_FLOOR, "no need ever went below the grace floor");
    ck(p.fullness <= 30 && p.energy <= 30, "but it IS hungry and tired");
    ck(p.joy <= 40, "and wants attention");
    ck(!p.sleeping, "(and it was awake the whole time)");
  }

  // --- 2. it does not hit the floor in the first hours: the drain is gentle,
  //        so a short walk away is not the same as the whole 8 hours
  {
    Pet p; fresh(p); clockMin = 0;
    live(p, 2 * 60);
    ck(p.fullness >= 70 && p.energy >= 70, "two hours in it is still mostly fine");
    ck(p.fullness < 100, "(but it is getting hungry)");
  }

  // --- 3. beyond 8 hours: consequences
  {
    Pet p; fresh(p); clockMin = 0;
    live(p, GRACE_MIN + 90);
    ck(p.careMistakes >= 1, "an hour and a half past the window, mistakes count");
    live(p, 4 * 60);
    ck(p.canRunawayNow(), "and left long enough it is ready to run away");
    printf("      runaway-ready by %lu min untended\n", (unsigned long)p.unattended);
  }

  // --- 4. a single action does NOT restart the window (the owner's rule):
  //        only full care -- every need at CARE_FULL_PCT -- does
  {
    Pet p; fresh(p); clockMin = 0;
    live(p, 7 * 60);
    p.feedCandy();                   // a snack, nothing else
    p.clean();                       // and a bath
    ck(p.unattended >= 7 * 60, "a snack and a bath at 7 h do not restart the window");
    live(p, 3 * 60);
    ck(p.careMistakes >= 1, "so 7 h + a snack + 3 h is past the window, and counts");
  }
  {
    Pet p; fresh(p); clockMin = 0;
    live(p, 7 * 60);
    // Top up all four with real actions: bath, food, a joy game, then a rest.
    p.clean();
    while (p.fullness < 100) p.feed();
    p.playJoyGame(30);
    p.playJoyGame(30);
    p.toggleLight();                 // to bed
    live(p, 20);
    p.toggleLight();                 // and up, rested
    printf("      topped up: food=%u ene=%u joy=%u hyg=%u\n", p.fullness, p.energy, p.joy, p.hygiene);
    ck(p.fullyCared(), "(every need is at the full-care line)");
    ck(p.unattended == 0, "full care restarts the window");
    live(p, 7 * 60);
    ck(p.careMistakes == 0, "so 7 h + full care + 7 h is never penalised");
  }
  {
    // Nearly full is not full: one need short and nothing restarts.
    Pet p; fresh(p); clockMin = 0;
    live(p, 3 * 60);
    p.fullness = p.joy = p.hygiene = 100;
    p.energy = CARE_FULL_PCT - 1;
    p.clean();
    ck(p.unattended >= 3 * 60, "three needs full and one just short does not restart it");
    p.energy = CARE_FULL_PCT;
    p.clean();
    ck(p.unattended == 0, "and at the line exactly, it does");
  }
  {
    // The clock starts at the top-up, not when the needs later slip under 90.
    Pet p; fresh(p); clockMin = 0;
    live(p, 3 * 60);
    ck(p.unattended >= 3 * 60 - 1, "left alone after a top-up, the clock runs from the top-up");
  }
  {
    // A creature kept topped up all day never runs out of window.
    Pet p; fresh(p); clockMin = 0;
    for (int h = 0; h < 14; h++) {
      live(p, 50);
      p.fullness = p.energy = p.joy = 100;   // stand-ins for feeding and a rest
      p.clean();                             // a care action while fully cared for
    }
    ck(p.unattended < 60 && p.careMistakes == 0, "kept full for 14 h, the window keeps restarting");
  }

  // --- 5. the window forgives even a creature already at zero
  {
    Pet p; fresh(p); clockMin = 0;
    p.fullness = p.joy = p.energy = p.hygiene = 0;
    live(p, 3 * 60);
    ck(p.careMistakes == 0 && !p.canRunawayNow(),
       "found at zero, it still has its 8 hours before anything counts");
  }

  // --- 6. sleep is gentler than awake, never harsher
  {
    Pet a; fresh(a); clockMin = 0;
    live(a, 4 * 60);
    uint8_t awakeFood = a.fullness;
    Pet s; fresh(s); clockMin = 0;
    s.toggleLight();                 // put to bed by hand
    live(s, 4 * 60);
    ck(s.sleeping && s.fullness > awakeFood, "four hours asleep cost less food than four awake");
    ck(s.unattended == 0, "and sleeping time does not count as untended");
  }

  // --- 7. the window survives a reboot (it lives in the checkpoint)
  {
    Pet p; fresh(p); clockMin = 0;
    live(p, 5 * 60);
    uint16_t u = p.unattended;
    p.saveNow();
    Pet r; r.begin();
    ck(u >= 5 * 60 - 1 && r.unattended == u, "untended minutes survive a reload");
  }

  // --- 8. time with the board OFF is the gentle path it always was
  {
    Pet p; fresh(p); clockMin = 0;
    p.saveNow();
    p.syncClock(at(20 * 60));        // twenty hours switched off
    ck(lowest(p) >= GRACE_FLOOR, "twenty hours off: never below the floor");
    ck(p.careMistakes == 0 && p.unattended == 0, "and it neither penalises nor counts as untended");
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
