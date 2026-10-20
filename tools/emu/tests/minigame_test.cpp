// The hybrid training games (v3.34): Rock Smash (strength + speed), Block!
// (defence + speed) and Tilt Maze (speed + defence). Each pays through
// Pet::trainSplit(), so a game training two stats gives half to each.
//
// Driven through the sketch's own mgPress()/mgStep() with a hand-held clock,
// so the rules -- timing windows, lanes, the arena -- are the firmware's.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cmath>
#include <cstdio>
uint32_t g_seed = 53; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void setup();
extern Pet pet;
extern uint8_t miniGame;
extern uint16_t mgScore, mgPoints;
extern uint32_t mgStart, mgUntil, mgOverUntil, blockNext, blockLockUntil;
extern uint32_t rockBeatAt, rockPeriod;
extern uint8_t rockBeat;
extern bool mgNoSensor;
struct BlockAttack { uint8_t side; uint32_t at, dur; bool live; };
extern BlockAttack blockAtk[];
extern float tiltBx, tiltBy, tiltVx, tiltVy, tiltGx, tiltGy, tiltHx[], tiltHy[];
extern bool trainOpen; extern uint8_t trainPage;
void startRock(); void startBlock(); void startTilt(); void leaveMini();
void startPush(); void startSimon(); void startSeek();
float pushNeedle(uint32_t now); uint8_t pushZone(float v);
extern uint16_t pushSteps; extern uint32_t pushLockUntil;
extern uint8_t simonSeq[], simonLen, simonPos; extern bool simonShowing;
void simonBerryPos(uint8_t i, int *x, int *y); int simonBerryAt(int16_t x, int16_t y);
extern uint8_t seekSlot[], seekPet, seekRound, seekStage;
void seekBushPos(uint8_t i, uint32_t now, int *x, int *y); int seekBushAt(int16_t x, int16_t y);
void mgPress(int16_t x, int16_t y, uint32_t now);
void mgStep(uint32_t now);
uint8_t blockSideAt(int16_t x, int16_t y);
bool blockAttackPos(int i, uint32_t now, int *x, int *y);
int blockAttackR();
int mgLeaveStripY();
void trainTap(int16_t x, int16_t y);
void trainRowCenter(int i, int *x, int *y);
uint8_t trainGameCount();
uint8_t trainPages();
void emuSetTilt(float, float);
static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static void ready() {
  pet.sleeping = false;
  pet.energy = pet.fullness = pet.joy = 100;
  pet.trAtk = pet.trDef = pet.trSpe = 0;
  pet.ivAtk = pet.ivDef = pet.ivSpe = 31;
  miniGame = 0;
}
#define CXT 233
#define CYT 233

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(25, false);

  // Which value each start function sets: asked, not retyped.
  ready(); startRock();  const uint8_t ROCK = miniGame;  leaveMini();
  ready(); startBlock(); const uint8_t BLOCK = miniGame; leaveMini();
  ck(ROCK && BLOCK && ROCK != BLOCK, "(the games open)");

  // ---------- Rock Smash ----------
  {
    ready(); startRock();
    bool fasterEach = true;
    uint32_t prev = rockPeriod;
    int beats = 0;
    while (miniGame == ROCK && !mgOverUntil && beats < 100) {
      uint32_t at = rockBeatAt;
      mgStep(at - 5);
      mgPress(CXT, CYT, at);              // dead on the beat
      mgStep(at + 1);
      if (rockPeriod > prev) fasterEach = false;
      prev = rockPeriod;
      beats++;
    }
    for (uint32_t t = rockBeatAt; !mgOverUntil && t < rockBeatAt + 5000; t += 85) mgStep(t);
    ck(mgOverUntil != 0, "a round of Rock Smash ends by itself after its beats");
    ck(fasterEach, "the beat never slows down");
    ck(mgPoints == TRAIN_SESSION_MAX, "every beat perfect: a full training session");
    ck(pet.trAtk == TRAIN_SESSION_MAX / 2 && pet.trSpe == TRAIN_SESSION_MAX / 2 && pet.trDef == 0,
       "paid half to strength and half to speed, nothing to defence");
  }
  {
    ready(); startRock();
    uint32_t at = rockBeatAt;
    mgPress(CXT, CYT, at - 1000);         // a nervous tap long before the ring arrives
    ck(rockBeat == 0 && mgScore == 0, "a tap far ahead of the beat is ignored, not a miss");
    mgPress(CXT, CYT, at);
    ck(rockBeat == 1 && mgScore == 1, "and the beat can still be hit after it");
    uint32_t at2 = rockBeatAt;
    mgPress(CXT, CYT, at2 - 250);         // early, but aimed at it
    ck(rockBeat == 2 && mgScore == 1, "an early tap aimed at the beat spends it as a miss");
    uint16_t p0 = mgPoints;
    mgPress(CXT, CYT, rockBeatAt + 150);  // late but within reach
    ck(mgScore == 2 && mgPoints >= p0, "a slightly late tap still counts");
    leaveMini();
  }
  {
    ready(); startRock();
    uint8_t e0 = pet.energy;
    for (uint32_t t = mgStart; !mgOverUntil && t < mgStart + 60000; t += 85) mgStep(t);
    ck(mgOverUntil && mgScore == 0, "never tapping: every beat passes as a miss and the round ends");
    ck(pet.energy == e0 && pet.trAtk == 0, "nothing earned, nothing charged");
  }

  // ---------- Block! ----------
  {
    ck(blockSideAt(CXT, CYT - 100) == 0 && blockSideAt(CXT + 100, CYT) == 1 &&
       blockSideAt(CXT, CYT + 100) == 2 && blockSideAt(CXT - 100, CYT) == 3,
       "a tap is read as the side of the creature it is on");
    ready(); startBlock();
    uint32_t now = mgStart + 1000;
    blockNext = now + 1000000;            // only what the test launches
    for (int i = 0; i < 4; i++) blockAtk[i].live = false;
    blockAtk[0] = { 1, now, 1200, true };     // from the right
    mgPress(CXT - 100, CYT, now + 100);       // the left: wrong side
    ck(blockAtk[0].live && mgScore == 0, "tapping a side with nothing on it blocks nothing");
    mgPress(CXT + 100, CYT, now + 150);       // right side, but the shield is wobbling
    ck(blockAtk[0].live && mgScore == 0, "and locks the shield briefly, so spamming every side fails");
    mgPress(CXT + 100, CYT, blockLockUntil + 1);
    ck(!blockAtk[0].live && mgScore == 1, "the right side, once the shield is back, blocks it");
    blockAtk[1] = { 2, now, 1200, true };
    mgStep(now + 1300);
    ck(!blockAtk[1].live && mgScore == 1, "an attack that lands is gone, and scores nothing");
    leaveMini();
    ck(pet.trDef + pet.trSpe > 0 || mgPoints == 0, "(leaving pays what was earned)");
  }
  {
    ready(); startBlock();
    bool oneASide = true, onGlass = true;
    int launched = 0;
    for (uint32_t t = mgStart; !mgOverUntil; t += 85) {
      mgStep(t);
      uint8_t seen = 0;
      for (int i = 0; i < 4; i++) {
        int x, y;
        if (!blockAttackPos(i, t, &x, &y)) continue;
        if (seen & (1 << blockAtk[i].side)) oneASide = false;
        seen |= 1 << blockAtk[i].side;
        float r = sqrtf((float)(x - CXT) * (x - CXT) + (float)(y - CYT) * (y - CYT));
        if (r + blockAttackR() > 233) onGlass = false;
        if (blockAtk[i].at == t) launched++;
        // the perfect defender: block everything the moment it is half way in
        if (t - blockAtk[i].at >= blockAtk[i].dur / 2) mgPress((x - CXT) / 2 + CXT, (y - CYT) / 2 + CYT, t);
      }
    }
    ck(oneASide, "never two attacks in one lane at once");
    ck(onGlass, "every attack is drawn on the round panel");
    printf("      %d attacks in a round, %u blocked\n", launched, mgScore);
    ck(mgPoints >= TRAIN_SESSION_MAX, "blocking everything is a full session");
    ck(pet.trDef == TRAIN_SESSION_MAX / 2 && pet.trSpe == TRAIN_SESSION_MAX / 2 && pet.trAtk == 0,
       "paid half to defence and half to speed, nothing to strength");
  }
  {
    // Hands off: with nothing blocked, attacks pile up, which is when two
    // could share a lane. (The defender above clears them too fast to tell.)
    ready(); startBlock();
    bool oneASide = true;
    int most = 0;
    for (uint32_t t = mgStart; !mgOverUntil; t += 85) {
      mgStep(t);
      uint8_t seen = 0;
      int n = 0;
      for (int i = 0; i < 4; i++) {
        if (!blockAtk[i].live) continue;
        if (seen & (1 << blockAtk[i].side)) oneASide = false;
        seen |= 1 << blockAtk[i].side;
        n++;
      }
      if (n > most) most = n;
    }
    ck(most >= 2, "(untouched, several attacks are in the air at once)");
    ck(oneASide, "and still never two in one lane");
    ck(mgScore == 0 && pet.trDef == 0, "blocking nothing earns nothing");
  }
  {
    // The way out must not sit on the top lane: a tap at an attack just
    // launched from the top is a block, not a goodbye.
    ready(); startBlock();
    blockNext = mgStart + 1000000;
    for (int i = 0; i < 4; i++) blockAtk[i].live = false;
    blockAtk[0] = { 0, mgStart, 1200, true };
    int x, y;
    blockAttackPos(0, mgStart, &x, &y);
    ck(mgLeaveStripY() <= y - blockAttackR(), "Block!'s leave strip is above the top lane");
    leaveMini();
    ready(); startRock();
    ck(mgLeaveStripY() > 40, "(other games keep the ordinary top strip)");
    leaveMini();
  }

  // ---------- Tilt Maze ----------
  {
    ready(); startTilt();
    uint8_t TILT = miniGame;
    ck(TILT && mgNoSensor && mgOverUntil, "no motion sensor: Tilt Maze says so and ends");
    mgStep(mgOverUntil + 1);
    ck(miniGame == 0 && pet.trSpe == 0 && pet.trDef == 0, "and pays nothing");

    emuSetTilt(0, 0);
    ready(); startTilt();
    ck(!mgNoSensor && !mgOverUntil, "with a sensor it plays");
    for (uint32_t t = mgStart + 85; t < mgStart + 2000; t += 85) mgStep(t);
    ck(fabsf(tiltBx - CXT) < 1 && fabsf(tiltBy - CYT) < 1, "held as it was at the start, the ball stays put");

    // tilting moves it, and the other way moves it the other way
    auto run = [&](float tx, float ty) {
      ready(); emuSetTilt(0, 0); startTilt();
      tiltHx[0] = tiltHx[1] = tiltHy[0] = tiltHy[1] = -1000;   // no holes
      tiltGx = tiltGy = -1000;                                // no goal
      emuSetTilt(tx, ty);
      for (uint32_t t = mgStart + 85; t < mgStart + 600; t += 85) mgStep(t);
    };
    // The mapping the owner measured on a real board: sensor +x is the board
    // tilted DOWN, sensor +y is tilted LEFT. Pinned so a refactor cannot
    // quietly swap the axes back -- the emulator has no sensor to notice.
    run(0.3f, 0);
    float dX = tiltBx - CXT, dY = tiltBy - CYT;
    ck(dY > 5 && fabsf(dX) < 1, "tilting the board down rolls the ball down, and only down");
    run(0, 0.3f);
    dX = tiltBx - CXT; dY = tiltBy - CYT;
    ck(dX < -5 && fabsf(dY) < 1, "tilting it left rolls the ball left, and only left");
    run(-0.3f, 0);
    ck(tiltBy - CYT < -5, "tilting it up rolls the ball up");
    run(0, -0.3f);
    ck(tiltBx - CXT > 5, "tilting it right rolls the ball right");

    run(1.0f, 1.0f);
    float maxR = 0;
    for (uint32_t t = mgStart + 600; t < mgStart + 8000; t += 85) {
      mgStep(t);
      float r = sqrtf((tiltBx - CXT) * (tiltBx - CXT) + (tiltBy - CYT) * (tiltBy - CYT));
      if (r > maxR) maxR = r;
    }
    ck(maxR > 120 && maxR < 233 - 10, "tipped hard into a wall, the ball stays inside the arena");
    leaveMini();

    // the goal scores and moves; a hole sends the ball home
    ready(); emuSetTilt(0, 0); startTilt();
    tiltBx = tiltGx; tiltBy = tiltGy;
    mgStep(mgStart + 85);
    ck(mgScore == 1, "rolling into the goal scores");
    float gd = sqrtf((tiltGx - tiltBx) * (tiltGx - tiltBx) + (tiltGy - tiltBy) * (tiltGy - tiltBy));
    ck(gd > 60, "and a new goal appears away from the ball");
    tiltBx = tiltHx[0]; tiltBy = tiltHy[0]; tiltVx = tiltVy = 0;
    mgStep(mgStart + 170);
    ck(fabsf(tiltBx - CXT) < 1 && fabsf(tiltBy - CYT) < 1 && mgScore == 1,
       "falling in a hole sends the ball back to the middle");
    bool apart = true;
    for (int n = 0; n < 200; n++) {
      tiltBx = tiltGx; tiltBy = tiltGy;
      mgStep(mgStart + 255);
      for (int h = 0; h < 2; h++) {
        float d = sqrtf((tiltGx - tiltHx[h]) * (tiltGx - tiltHx[h]) + (tiltGy - tiltHy[h]) * (tiltGy - tiltHy[h]));
        if (d < 40) apart = false;
      }
    }
    ck(apart, "a goal never sits in a hole, over 200 layouts");
    mgStep(mgUntil);
    ck(mgOverUntil != 0, "the clock ends the round");
    ck(pet.trSpe > 0 && pet.trDef > 0 && pet.trAtk == 0, "and pays speed and defence, not strength");
  }

  // ---------- Boulder Push ----------
  {
    ready(); startPush();
    ck(miniGame && miniGame != ROCK && miniGame != BLOCK, "(Boulder Push opens)");
    // a moment with the needle in the green, and one with it far out
    uint32_t green = 0, out = 0;
    for (uint32_t t = mgStart + 50; t < mgStart + 3000 && (!green || !out); t += 5) {
      uint8_t z = pushZone(pushNeedle(t));
      if (z == 2 && !green) green = t;
      if (z == 0 && !out) out = t;
    }
    mgPress(CXT, CYT, green);
    ck(pushSteps == 2, "a tap in the green shoves the boulder two steps");
    mgPress(CXT, CYT, out > green ? out : out + 10000);
    ck(pushSteps == 1 && pushLockUntil > mgStart, "a tap outside the bar's middle lets it slip back");
    uint32_t g2 = 0;
    for (uint32_t t = pushLockUntil - 300; t < pushLockUntil; t++) if (pushZone(pushNeedle(t)) == 2) { g2 = t; break; }
    if (g2) { mgPress(CXT, CYT, g2); ck(pushSteps == 1, "and for a moment the boulder will not move at all"); }
    else ck(true, "(no green moment inside the slip; skipped)");
    // the needle swings faster as the round goes on
    auto crossings = [&](uint32_t a, uint32_t b) {
      int n = 0; float prev = pushNeedle(a);
      for (uint32_t t = a + 5; t < b; t += 5) { float v = pushNeedle(t); if ((v > 0) != (prev > 0)) n++; prev = v; }
      return n;
    };
    ck(crossings(mgStart, mgStart + 5000) < crossings(mgUntil - 5000, mgUntil), "the needle quickens");
    leaveMini();
  }
  {
    // A steady hand: one press each time the needle crosses the green.
    ready(); startPush();
    bool in = false;
    for (uint32_t t = mgStart; !mgOverUntil; t += 10) {
      bool g = pushZone(pushNeedle(t)) == 2;
      if (g && !in) mgPress(CXT, CYT, t);
      in = g;
      mgStep(t);
    }
    printf("      a steady hand pushed %u steps\n", pushSteps);
    ck(mgPoints >= TRAIN_SESSION_MAX, "pressing every green is a full session");
    ck(pet.trAtk == TRAIN_SESSION_MAX / 2 && pet.trDef == TRAIN_SESSION_MAX / 2 && pet.trSpe == 0,
       "paid half to strength and half to defence, nothing to speed");
    // A masher: a tap every frame regardless.
    ready(); startPush();
    for (uint32_t t = mgStart; !mgOverUntil; t += 85) { mgPress(CXT, CYT, t); mgStep(t); }
    printf("      a masher pushed %u steps\n", pushSteps);
    ck(pushSteps < 60 / 4, "mashing gets nowhere");
  }

  // ---------- Memory ----------
  {
    ready(); startSimon();
    const uint8_t SIMON = miniGame;
    ck(simonBerryAt(CXT, CYT) == -1, "the middle is not a berry");
    int x, y;
    simonBerryPos(simonSeq[0], &x, &y);
    mgPress(x, y, mgStart + 10);
    ck(simonPos == 0 && simonLen == 1 && mgScore == 0 && !mgOverUntil, "taps while it shows the sequence are ignored");
    // A perfect memory: repeat every run until the longest.
    uint32_t t = mgStart;
    int guard = 0;
    while (!mgOverUntil && guard++ < 5000) {
      t += 20; mgStep(t);
      if (!simonShowing && miniGame == SIMON) {
        simonBerryPos(simonSeq[simonPos], &x, &y);
        mgPress(x, y, t);
      }
    }
    printf("      perfect memory reached %u\n", mgScore);
    ck(mgOverUntil && mgScore >= 12, "a perfect memory runs to the longest sequence and ends");
    ck(pet.petStep == 0, "a joy game: petting is fresh again");
    ck(pet.trAtk == 0 && pet.trDef == 0 && pet.trSpe == 0, "and it trains nothing");

    ready();
    for (int i = 0; i < 6; i++) pet.caress();
    pet.joy = 20;
    startSimon();
    t = mgStart;
    while (simonShowing) { t += 20; mgStep(t); }
    simonBerryPos(simonSeq[0], &x, &y); mgPress(x, y, t);      // run 1 right
    while (simonShowing) { t += 20; mgStep(t); }
    uint8_t wrong = (simonSeq[0] + 1) % 4;
    simonBerryPos(wrong, &x, &y); mgPress(x, y, t);           // run 2 wrong at once
    ck(mgOverUntil && mgScore == 1, "a wrong berry ends it; the score is the longest run repeated");
    ck(pet.joy > 20 && pet.petStep == 0, "and it still pays joy and refreshes petting");

    ready(); startSimon();
    int tx, ty; simonBerryPos(0, &tx, &ty);
    ck(mgLeaveStripY() < ty - 44 - 10, "the leave strip sits above the top berry");
    leaveMini();
  }

  // ---------- Hide and Seek ----------
  {
    ready(); startSeek();
    const uint8_t SEEK = miniGame;
    int x, y;
    seekBushPos(seekPet, mgStart, &x, &y);
    mgPress(x, y, mgStart + 10);
    ck(mgScore == 0 && seekRound == 0, "pointing while it is still showing does nothing");
    bool perm = true, apart = true, moved = false;
    float closest = 1e9;
    uint32_t t = mgStart;
    while (!mgOverUntil && miniGame == SEEK) {
      uint8_t shownSlot = seekSlot[seekPet];
      while (!mgOverUntil && seekStage != 2) {           // until it is time to point
        t += 20; mgStep(t);
        uint8_t m = (1 << seekSlot[0]) | (1 << seekSlot[1]) | (1 << seekSlot[2]);
        if (m != 7) perm = false;
        for (int a = 0; a < 3; a++) for (int b = a + 1; b < 3; b++) {
          int ax, ay, bx, by;
          seekBushPos(a, t, &ax, &ay); seekBushPos(b, t, &bx, &by);
          float d = sqrtf((float)(ax - bx) * (ax - bx) + (float)(ay - by) * (ay - by));
          if (d < closest) closest = d;
        }
      }
      if (mgOverUntil) break;
      if (seekSlot[seekPet] != shownSlot) moved = true;
      seekBushPos(seekPet, t, &x, &y);
      ck(seekBushAt(x, y) == seekPet, "(pointing at a bush picks that bush)");
      mgPress(x, y, t);                                    // a sharp eye
      while (!mgOverUntil && seekStage == 3) { t += 20; mgStep(t); }
    }
    printf("      found %u, bushes never closer than %.0f px\n", mgScore, closest);
    ck(perm, "the bushes always stand in three different places");
    ck(closest > 70, "two bushes swapping never pass through each other");
    ck(moved, "the shuffle really does move the creature's bush");
    ck(mgScore == 5 && mgOverUntil, "a sharp eye finds it every round, and the game ends");
    ck(pet.petStep == 0, "a joy game: petting is fresh again");

    ready();
    pet.joy = 20;
    startSeek();
    t = mgStart;
    while (!mgOverUntil) {
      t += 20; mgStep(t);
      if (seekStage == 2) { seekBushPos((seekPet + 1) % 3, t, &x, &y); mgPress(x, y, t); }
    }
    ck(mgScore == 0, "pointing at the wrong bush never finds it");
    ck(pet.joy > 20, "but playing it through still pays a little joy");
  }

  // ---------- the menu reaches them ----------
  {
    ck(trainGameCount() == 10, "ten games on the training menu");
    // Every row of every page, asked of the firmware's own layout: a row past
    // the end of a page lands on nothing or on BACK, which opens no game.
    uint8_t seen = 0;
    for (int pg = 0; pg < trainPages(); pg++)
      for (int i = 0; i < 8; i++) {
        ready();
        emuSetTilt(0, 0);
        trainOpen = true;
        trainPage = pg;
        int x, y;
        trainRowCenter(i, &x, &y);
        if (y > 466) break;
        trainTap(x, y);
        if (miniGame) seen |= 1 << miniGame;
        leaveMini();
      }
    ck(__builtin_popcount(seen) == 6 && (seen & (1 << ROCK)) && (seen & (1 << BLOCK)),
       "six of its rows open the six games in this dispatcher");
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
