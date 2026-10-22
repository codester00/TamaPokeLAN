// Surfing Pikachu's rules (v3.54): skill pays, the ride ends at the Goal, a
// jump happens only fast over a crest, a bad landing is a wipeout that banks
// nothing, and tricks are worth more than plain air. The secret that opens it
// and the screen are surfui_test's.
#include "surf.h"
#include <cstdio>
#include <cmath>

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// Policies the player might follow. `trick` swings the board while airborne;
// `land` lines it up with the sea just before touching down.
static float play(const SurfGame &g, bool ride, bool trick, bool land) {
  float px = g.worldX + SURF_PIKA_X;
  float sl = surfSlopeAngle(px);
  if (!ride) return 0;
  if (g.air) {
    float surface = SURF_SEA_Y - surfHeight(px);
    if (land && g.vy > 0 && surface - g.y < 60) return sl / 1.1f;
    if (trick) return (g.airMs / 250) % 2 ? 1.0f : -1.0f;
    return land ? sl / 1.1f : 1.0f;    // no landing care: nose up all the way
  }
  return sl / 1.1f;
}

static SurfGame run(bool ride, bool trick, bool land) {
  SurfGame g;
  g.start();
  while (!g.over()) g.step(85, play(g, ride, trick, land));
  return g;
}

int main() {
  SurfGame good = run(true, true, true), plain = run(true, false, true),
           careless = run(true, false, false), flat = run(false, false, false);
  printf("     good %u (jumps %u clean %u wipe %u), plain %u, careless %u (wipe %u), flat %u\n",
         good.score, good.jumps, good.cleanLandings, good.wipeouts, plain.score,
         careless.score, careless.wipeouts, flat.score);
  ck(good.over() && good.elapsed >= SURF_RUN_MS, "a run ends at the Goal");
  ck(flat.jumps == 0, "never leaving the flat (not riding the waves) never jumps");
  ck(good.jumps >= 5 && good.cleanLandings >= 5, "riding the faces builds the speed to jump");
  ck(good.score > 3 * flat.score, "skill pays: riding well scores far more than not");
  ck(good.score > plain.score, "tricks are worth more than plain air");
  ck(careless.wipeouts > careless.cleanLandings, "landing nose-up is mostly wipeouts");
  ck(plain.score > careless.score, "and costs the points");

  // one jump in detail: the board swung side to side in the air counts tricks
  {
    SurfGame g; g.start();
    uint32_t guard = 0;
    while (!g.air && !g.over() && guard++ < 2000) g.step(85, play(g, true, true, true));
    ck(g.air, "(a jump happens)");
    float atTakeoff = g.speed;
    ck(atTakeoff >= SURF_JUMP_SPEED, "only at take-off speed");
    // over the next jumps, one with a trick in it banks the trick
    uint32_t best = 0;
    for (int j = 0; j < 10 && !g.over(); j++) {
      while (g.air && !g.over()) g.step(85, play(g, true, true, true));
      if (g.lastBank > best) best = g.lastBank;
      while (!g.air && !g.over()) g.step(85, play(g, true, true, true));
    }
    ck(best >= 10 + 50, "a jump with a trick in it banks the trick");
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
