// Surfing Pikachu's rules (v3.54): skill pays, the ride ends at the Goal, a
// jump happens only fast over a crest, a bad landing is a wipeout that banks
// nothing, and tricks are worth more than plain air. The secret that opens it
// and the screen are surfui_test's.
#include "surf.h"
#include <cstdio>
#include <cmath>

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// A PERSON, not a robot (v3.67): they see the slope about 0.3 s late and have
// three inputs -- hold the front side (lean forward), nothing, or hold the
// back side (lean back). v3.54's test followed the slope exactly every frame,
// so it proved a robot could play while no person could: the sea flipped its
// slope several times a second and the air was too short for a trick.
enum Style { PERSON, LEVEL, WRONG };
struct Player {
  Style style; bool tricks; float seen[4] = { 0, 0, 0, 0 };   // 4 frames of 85 ms
  float input(const SurfGame &g) {
    float px = g.worldX + SURF_PIKA_X;
    float now = surfSlopeAngle(px);
    float late = seen[0];
    for (int i = 0; i < 3; i++) seen[i] = seen[i + 1];
    seen[3] = now;
    if (style == LEVEL) return 0;
    float lean = late < -0.08f ? -SURF_LEAN : late > 0.08f ? SURF_LEAN : 0;
    if (style == WRONG) return -lean;
    if (g.air) {
      float surface = SURF_SEA_Y - surfHeight(px);
      if (g.vy > 0 && surface - g.y < 50) return now < -0.08f ? -SURF_LEAN : now > 0.08f ? SURF_LEAN : 0;
      if (tricks) return (g.airMs / 255) % 2 ? SURF_LEAN : -SURF_LEAN;   // rock it side to side
      return 0;
    }
    return lean;
  }
};

static SurfGame run(Style st, bool tricks, uint32_t *bestBank = nullptr, uint8_t *bestTricks = nullptr) {
  SurfGame g; g.start();
  Player p{ st, tricks };
  uint32_t landSeen = 0;
  while (!g.over()) {
    g.step(85, p.input(g));
    if (g.lastLandAt != landSeen) {
      landSeen = g.lastLandAt;
      if (bestBank && g.lastBank > *bestBank) *bestBank = g.lastBank;
      if (bestTricks && g.lastBank && g.lastTricks > *bestTricks) *bestTricks = g.lastTricks;
    }
  }
  return g;
}

int main() {
  uint32_t bank = 0; uint8_t tr = 0;
  SurfGame good = run(PERSON, true, &bank, &tr), plain = run(PERSON, false),
           level = run(LEVEL, false), wrong = run(WRONG, false);
  printf("     person %u (jumps %u clean %u wipe %u, best landing %u with %u tricks), plain %u, level %u (jumps %u), wrong %u\n",
         good.score, good.jumps, good.cleanLandings, good.wipeouts, bank, tr, plain.score,
         level.score, level.jumps, wrong.score);
  ck(good.over() && good.elapsed >= SURF_RUN_MS, "a run ends at the Goal");
  ck(good.jumps >= 5 && good.cleanLandings >= 4, "a person leaning with the waves, late, gets airborne again and again");
  ck(tr >= 2, "and rocking the board in the air lands tricks");
  ck(good.score > plain.score, "tricks are worth more than plain air");
  ck(level.jumps == 0, "doing nothing never builds the speed to jump");
  ck(good.score > 3 * level.score, "skill pays: riding well scores far more than not");
  ck(wrong.jumps == 0 && wrong.score * 20 < good.score, "leaning against the waves never jumps and scores next to nothing");

  // a careless landing (the board left nose up) is a wipeout
  {
    SurfGame g; g.start();
    Player p{ PERSON, false };
    uint32_t guard = 0;
    while (!g.air && !g.over() && guard++ < 2000) g.step(85, p.input(g));
    ck(g.air && g.speed >= SURF_JUMP_SPEED, "(a jump, at take-off speed)");
    uint16_t w0 = g.wipeouts;
    while (g.air && !g.over()) g.step(85, 1.0f);
    ck(g.wipeouts == w0 + 1 && g.lastBank == 0, "landing nose-up all the way is a wipeout that banks nothing");
  }

  // a stalled frame cannot skip the Goal or teleport through the sea
  {
    SurfGame g; g.start();
    g.step(60000, 0);
    ck(g.elapsed == 200 && !g.over(), "one long frame is clamped");
    g.step(0, 0);
    ck(g.elapsed == 200, "a zero frame is nothing");
  }

  // what a run pays
  ck(surfCoins(0) == 0 && surfCoins(757) == 75 && surfCoins(100000) == 500, "coins: a tenth of the score, at most 500");
  ck(surfJoyScore(10000) == 40 && surfJoyScore(250) == 10, "joy and bond by the score, capped");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
