// The sleep cycle (v4.13, the owner's model), and the board's battery changing
// nothing about it:
//
//   "simulate a pet's normal sleep cycle: awake during the day, asleep at
//    night, a transition at dawn where it keeps sleeping unless disturbed but
//    wakes for the day eventually, a transition at night where it stays awake
//    unless completely drained of energy, and a time buffer to simulate it
//    passing out on the floor. Slip-ups and runaways at reasonable times, and
//    the 8-hour buffer kept for work and school."
//
// Found from a board: a pet put to bed, with the board switched off overnight
// and all the next day, was still asleep at 19:30 at ~50% on everything,
// because the catch-up at power-on played the whole gap as one sleep with the
// clock standing still. A board left off is the normal case, so the catch-up
// now replays every minute through the same rules as a running board.
//
// Every number is asked of pet.h (NAP_START, SLEEP_IN_END, NAP_END,
// PASSOUT_MIN, GRACE_MIN, GRACE_FLOOR, RUNAWAY_TICKS); nothing restates a rate.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=23; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

// day d (from 1), hour h, minute m, as an RTC epoch (day 0 is never used, so a
// "no clock" 0 is never confused with a real time)
static uint32_t T(int d, int h, int m = 0){ return (uint32_t)d * 86400 + h * 3600 + m * 60; }

// Every case starts awake, just fully cared for (the 8-hour window fresh), with
// no slip-ups. The suite shares one NVS store, so a Pet built after another one
// would otherwise LOAD that one's state.
static void fresh(Pet &p, uint32_t when){
  p.begin();
  p.dbgHatchAs(147,false);
  p.sleeping = false;
  p.sleepAuto = SLEEP_NONE;
  p.lastSeenEpoch = when;
  p.feedCandy();                                   // care: the window starts now
  p.fullness = p.joy = p.energy = p.hygiene = 100;
  p.poops = 0;
  p.unattended = 0;
  p.careMistakes = 0;
  p.passOutMin = 0;
  p.weight = WEIGHT_START;
  p.bond = 50;
}

// a running board: one tick a minute, the clock moving with it, up to `until`
static void runTo(Pet &p, uint32_t until){
  while (p.lastSeenEpoch + 60 <= until) { p.lastSeenEpoch += 60; p.dbgTick(); }
}

static uint8_t lowest(const Pet &p){ return p.lowestStat(); }

// The state that has to come out the same whether the board was on or off.
static bool same(const Pet &a, const Pet &b, const char *what){
  bool ok = a.fullness == b.fullness && a.joy == b.joy && a.energy == b.energy &&
            a.hygiene == b.hygiene && a.poops == b.poops && a.weight == b.weight &&
            a.sleeping == b.sleeping && a.sleepAuto == b.sleepAuto &&
            a.careMistakes == b.careMistakes && a.unattended == b.unattended &&
            a.canRunawayNow() == b.canRunawayNow() && a.ageMinutes == b.ageMinutes &&
            a.bond == b.bond;
  if (!ok)
    printf("      on : food=%u joy=%u ene=%u hyg=%u slept=%d/%u mist=%u un=%u run=%d\n"
           "      off: food=%u joy=%u ene=%u hyg=%u slept=%d/%u mist=%u un=%u run=%d\n",
           a.fullness, a.joy, a.energy, a.hygiene, a.sleeping, a.sleepAuto, a.careMistakes, a.unattended, a.canRunawayNow(),
           b.fullness, b.joy, b.energy, b.hygiene, b.sleeping, b.sleepAuto, b.careMistakes, b.unattended, b.canRunawayNow());
  if (!ok) printf("      w %u/%u poops %u/%u age %lu/%lu bond %u/%u\n", a.weight, b.weight, a.poops, b.poops,
                  (unsigned long)a.ageMinutes, (unsigned long)b.ageMinutes, a.bond, b.bond);
  ck(ok, what);
  return ok;
}

// The same stretch two ways: a board left running, and a board switched off
// and caught up at power-on. Same seed, so the same poops.
static void onAndOff(Pet &on, Pet &off, uint32_t from, uint32_t to, void (*setup)(Pet &)){
  fresh(on, from); if (setup) setup(on);
  fresh(off, from); if (setup) setup(off);
  uint32_t seed = 777;
  g_seed = seed; runTo(on, to);
  g_seed = seed; off.syncClock(to);
}

static void toBed(Pet &p){ p.toggleLight(); }

int main(){
  // --- 1. THE REPORT: put to bed at 21:00, the board switched off, back at
  //     19:30 the next evening. It was still asleep at ~50%.
  {
    Pet on, off;
    onAndOff(on, off, T(1, 21), T(2, 19, 30), toBed);
    ck(!off.sleeping, "back at 19:30: it got up for the day, it did not sleep it away");
    ck(lowest(off) <= 10, "and it is hungry, tired, dirty or sad by the evening");
    ck(off.careMistakes > 0, "and the day without care cost slip-ups");
    same(on, off, "and the board being off changed nothing: same as a board left on");
  }

  // --- 2. the dawn: a night's bed sleeps in until SLEEP_IN_END, then gets up
  {
    Pet p; fresh(p, T(1, 22)); toBed(p);
    runTo(p, T(2, SLEEP_IN_END) - 120);
    ck(p.sleeping, "sleeping in at dawn, before SLEEP_IN_END");
    ck(p.dawnWakeable(), "and a care action could wake it now");
    runTo(p, T(2, SLEEP_IN_END, 5));
    ck(!p.sleeping, "up by itself just after SLEEP_IN_END, rested");
  }

  // --- 3. disturbed at dawn: a care action wakes it; at night it does not
  {
    Pet p; fresh(p, T(1, 22)); toBed(p);
    runTo(p, T(2, 3));
    ck(!p.dawnWakeable(), "at 03:00 it is night, not dawn");
    p.caress();
    ck(p.sleeping, "petting it at 03:00 does not wake it");
    runTo(p, T(2, NAP_START, 30));
    ck(p.dawnWakeable(), "at dawn it can be disturbed");
    p.caress();
    ck(!p.sleeping, "petting it at dawn wakes it");
    Pet q; fresh(q, T(1, 22)); toBed(q);
    runTo(q, T(2, NAP_START, 30));
    ck(q.wakeAtDawn() && !q.sleeping, "and so does any care action (the care buttons call the same)");
  }

  // --- 4. PASSING OUT. Cared for that afternoon, energy run down: inside the
  //     8-hour window its energy stops at GRACE_FLOOR, and from NAP_END it has
  //     PASSOUT_MIN minutes before it drops where it stands.
  {
    Pet p; fresh(p, T(1, 16));
    p.energy = GRACE_FLOOR + 1;              // tired already; the window is still on
    runTo(p, T(1, NAP_END) + PASSOUT_MIN * 60 - 60);
    ck(!p.sleeping, "drained since the afternoon, but still up a minute before the earliest time");
    uint16_t mistakes = p.careMistakes;
    runTo(p, T(1, NAP_END) + PASSOUT_MIN * 60);
    ck(p.sleeping && p.sleepAuto == SLEEP_PASSED, "passed out exactly PASSOUT_MIN after NAP_END: 20:00 at the earliest");
    ck(p.careMistakes == mistakes + 1, "and passing out on the floor is a slip-up");
    runTo(p, T(2, NAP_START) - 120);
    ck(p.sleeping, "it sleeps through the night");
    runTo(p, T(2, NAP_START, 2));
    ck(!p.sleeping, "and is up at NAP_START: no sleeping in after a night like that");
  }

  // --- 5. a creature with energy left stays up, even past dawn, tired
  {
    Pet p; fresh(p, T(1, 22));               // topped up late, never put to bed
    runTo(p, T(2, NAP_START) - 60);
    ck(!p.sleeping, "with energy left it stays up all night");
    runTo(p, T(2, 11));
    ck(!p.sleeping, "and once the day comes it stays up, grumpy and tired");
    ck(p.energy <= GRACE_FLOOR + 5, "running on empty by late morning");
  }

  // --- 6. a forgotten pet: passing out wins over a runaway at night, and the
  //     runaway waits for the next day. It wakes at 06:00 already neglected,
  //     with no sleep floors under it, so the slip-ups start at once.
  {
    Pet p; fresh(p, T(1, 17));
    p.fullness = p.joy = p.energy = p.hygiene = 0;
    p.unattended = GRACE_MIN;                // the 8 hours long gone
    runTo(p, T(1, 19, 30));
    ck(p.canRunawayNow(), "all four at zero for an hour: a runaway is armed by 19:30");
    runTo(p, T(1, NAP_END) + PASSOUT_MIN * 60 + 60);
    ck(p.sleeping && p.sleepAuto == SLEEP_PASSED, "but at night it passes out anyway");
    ck(!p.canRunawayNow(), "and asleep it cannot run away");
    runTo(p, T(2, NAP_START) - 60);
    uint16_t mistakes = p.careMistakes;
    runTo(p, T(2, NAP_START, 3));
    ck(!p.sleeping && p.careMistakes > mistakes, "up at 06:00, and the slip-ups start at once");
    ck(!p.canRunawayNow(), "with its energy back it cannot run away in the morning");
    runTo(p, T(2, 20) - 60);
    ck(p.canRunawayNow(), "but left alone all day, the runaway comes before the next night");
  }

  // --- 6b. a poor night: after passing out, food, joy and hygiene fall
  //      through the sleep floors (30 / 35 / 45). Set above them once it is
  //      down, so a floor would have to hold them there.
  {
    Pet p; fresh(p, T(1, 17));
    p.energy = 0; p.unattended = GRACE_MIN;
    runTo(p, T(1, NAP_END) + PASSOUT_MIN * 60);
    ck(p.sleepAuto == SLEEP_PASSED, "(passed out)");
    p.fullness = p.joy = p.hygiene = 60;
    runTo(p, T(2, NAP_START) - 60);
    ck(p.fullness < 30 && p.joy < 35 && p.hygiene < 45, "a poor night: no sleep floors under food, joy or hygiene");
  }

  // --- 7. THE 8 HOURS, board off: school or work costs nothing
  {
    Pet on, off;
    onAndOff(on, off, T(1, 7, 30), T(1, 7, 30) + GRACE_MIN * 60 - 120, nullptr);
    ck(off.careMistakes == 0 && lowest(off) >= GRACE_FLOOR, "eight hours with the board off: hungry and tired, no slip-ups");
    same(on, off, "the same as eight hours with the board on");
  }

  // --- 8. days off: the cycle keeps turning, the same either way
  {
    Pet on, off;
    onAndOff(on, off, T(1, 21), T(4, 12), toBed);
    same(on, off, "two and a half days off: the same as a board left on");
    ck(on.careMistakes > 2, "and a pet left that long is a neglected one");
  }

  // --- 9. a nap in the day ends at full energy, board on or off
  {
    Pet on, off;
    onAndOff(on, off, T(1, 13), T(1, 15), [](Pet &p){ p.energy = 10; p.toggleLight(); });
    ck(!off.sleeping, "a daytime nap ends by itself with the board off too");
    same(on, off, "and the same as with the board on");
  }

  // --- 10. the screen-off sleep is retired: putting the device down, day or
  //     night, does not put it to bed
  {
    Pet p; fresh(p, T(1, 13));
    p.setScreenOff(true);
    runTo(p, T(1, 14));
    ck(!p.sleeping, "screen off at noon: still awake");
    Pet q; fresh(q, T(1, 23));
    q.setScreenOff(true);
    runTo(q, T(2, 2));
    ck(!q.sleeping, "screen off at night: still awake (it passes out only when drained)");
    // an old save may still hold a screen-off sleep; the screen still wakes it
    Pet r; fresh(r, T(1, 2));
    r.sleeping = true; r.sleepAuto = SLEEP_AUTO;
    r.setScreenOff(false);
    ck(!r.sleeping, "an old screen-off sleep still ends when the screen comes on");
  }

  // --- 11. the light button still beats everything
  {
    Pet p; fresh(p, T(1, 22)); toBed(p);
    runTo(p, T(2, 1));
    p.toggleLight();
    ck(!p.sleeping, "woken by hand at 01:00");
    runTo(p, T(2, 2));
    ck(!p.sleeping, "and nothing puts it straight back");
    p.toggleLight();
    ck(p.sleeping, "put back to bed by hand");
  }

  // --- 12. with no clock there is no night: it never passes out
  {
    Pet p; fresh(p, T(1, 13));
    p.lastSeenEpoch = 0;
    p.energy = 0; p.unattended = GRACE_MIN;
    for (int m = 0; m < PASSOUT_MIN * 3; m++) p.dbgTick();
    ck(!p.sleeping, "no clock: drained, but it never passes out");
  }

  // --- 13. an egg is not something that sleeps
  {
    Pet p; p.begin(); p.newEgg();
    p.lastSeenEpoch = T(1, 23);
    p.setScreenOff(true);
    ck(!p.sleeping, "an egg does not sleep");
  }

  // --- 14. weight: an absolute minimum of 1, board on or off
  {
    Pet on, off;
    onAndOff(on, off, T(1, 9), T(1, 15), [](Pet &p){ p.weight = WEIGHT_MIN + 3; });
    ck(on.weight == WEIGHT_MIN, "six hours awake: down to the minimum, not 0");
    ck(off.weight == WEIGHT_MIN, "and the same with the board off: fitness is not capped by the battery");

    Pet e; fresh(e, T(1, 13));
    e.weight = 3;
    e.playResult(40);              // a long rally: burns the 4-a-session maximum
    ck(e.weight == WEIGHT_MIN, "exercise cannot take it below the minimum either");
    e.weight = 2;
    e.trainStrength(60);
    ck(e.weight == WEIGHT_MIN, "nor can training");

    // a v3.28 save stored weight 0, which no creature may have any more
    Pet o; fresh(o, T(1, 13));
    o.weight = 0;
    o.saveNow();
    Pet o2; o2.begin();
    ck(o2.weight == WEIGHT_MIN, "a save from before the rework loads at the minimum, not 0");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
