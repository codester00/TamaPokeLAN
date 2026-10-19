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
    run(0.3f, 0); float right = tiltBx - CXT;
    run(-0.3f, 0); float left = tiltBx - CXT;
    ck(fabsf(right) > 5 && right * left < 0, "tilting rolls the ball, and the other way rolls it back");
    run(0, 0.3f); float down = tiltBy - CYT;
    run(0, -0.3f); float up = tiltBy - CYT;
    ck(fabsf(down) > 5 && down * up < 0, "on both axes");

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

  // ---------- the menu reaches them ----------
  {
    ck(trainGameCount() == 7, "seven games on the training menu");
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
    ck(__builtin_popcount(seen) == 3 && (seen & (1 << ROCK)) && (seen & (1 << BLOCK)),
       "three of its rows open the three hybrid games");
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
