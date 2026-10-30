// How likely is each species? A report, not a test (v3.85 audit).
//
//   g++ -std=c++17 -O2 -w -Itools/emu -I. -DSPRITE_DIR='""' tools/emu/odds_report.cpp \
//       <the CORE sources of tools/emu/tests/run.sh> -o /tmp/odds && /tmp/odds > odds.tsv
//
// For every drawable species, the best chance of meeting it, asked of the
// firmware's own pick (wildAreaPick() over every roll, every area, every time
// band, and every daily weather where an area has them), with every gate open
// -- so it is the odds once the player has reached that area, not how long the
// way there takes. Also: a swarm's share on its day, a one-off's or a
// legendary's guaranteed first meeting, a Game Corner prize, and an egg's
// chance in its home region (tier share / species in that tier: the worst case,
// once every one of them is registered -- unregistered species come first).
// One line per species, tab-separated, then a summary.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "wild.h"
#include "wild_areas.h"
#include "gamecorner.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include <algorithm>
uint32_t g_seed = 0x0DD5; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

struct Best { double p = 0; uint16_t area = WILD_NO_AREA; uint8_t when = 0; };

int main() {
  gRegionArt = 0xFFFF;
  Pet pet;
  pet.dbgHatchAs(25, false);
  for (uint8_t r = 0; r < GYM_REGIONS; r++)
    for (uint8_t b = 0; b < 13; b++) pet.winBadge(r, b, false);
  const uint32_t ROLLS = 20000;
  std::map<int16_t, Best> wild;
  const uint8_t BAND[3] = { WILD_WHEN_MORN, WILD_WHEN_DAY, WILD_WHEN_NITE };
  const char *BANDN[8] = { "", "morning", "day", "", "night", "", "", "" };
  for (uint16_t a = 0; a < WILD_AREA_COUNT; a++) {
    int days = WILD_AREAS[a].wxMask ? 40 : 1;     // a weather zone: try its days
    for (int dday = 0; dday < days; dday++) {
      pet.lastSeenEpoch = days > 1 ? 1767225600UL + (uint32_t)dday * 86400 + 12 * 3600 : 0;
      for (uint8_t b = 0; b < 3; b++) {
        std::map<int16_t, uint32_t> n;
        uint32_t hits = 0;
        for (uint32_t r = 0; r < ROLLS; r++) {
          int16_t d = wildAreaPick(a, BAND[b], pet, r * 2654435761u);
          if (d > 0) { n[d]++; hits++; }
        }
        if (!hits) continue;
        for (auto &kv : n) {
          double p = (double)kv.second / hits;
          Best &w = wild[kv.first];
          if (p > w.p) { w.p = p; w.area = a; w.when = BAND[b]; }
        }
      }
    }
  }
  std::map<int16_t, double> swarm;
  for (uint8_t i = 0; i < WILD_SWARM_COUNT; i++) {
    double p = WILD_SWARMS[i].share / 1000.0;
    if (p > swarm[WILD_SWARMS[i].dex]) swarm[WILD_SWARMS[i].dex] = p;
  }
  std::map<int16_t, const char *> special;
  for (uint8_t i = 0; i < wildLegendCount(); i++) special[wildLegendDex(i)] = "legendary";
  for (uint8_t i = 0; i < WILD_FIXED_COUNT; i++) special[WILD_FIXED[i].dex] = "one-off";
  for (uint8_t h = 0; h < 2; h++)
    for (uint8_t i = 0; i < hallPrizeCount(h); i++)
      if (hallPrize(h, i).kind == PRIZE_MON && !special.count((int16_t)hallPrize(h, i).what))
        special[(int16_t)hallPrize(h, i).what] = "prize";
  // eggs: the share of each tier a species gets in its home region's roll
  std::map<int16_t, double> egg;
  {
    Pet e;
    const double TIER[4] = { 0, 0.45, 0.45, 0.10 };   // a farewell egg: common, rare, legendary
    for (uint8_t r = 0; r < REGION_COUNT - 1; r++)
      for (uint8_t t = R_COMUN; t <= R_LEGENDARIO; t++) {
        std::map<int16_t, uint32_t> n;
        const int K = 20000;
        for (int i = 0; i < K; i++) { int16_t x = e.rollInRegion(r, t); if (x > 0) n[x]++; }
        for (auto &kv : n) {
          double p = TIER[t] * kv.second / K;
          if (p > egg[kv.first]) egg[kv.first] = p;
        }
      }
  }
  printf("dex\tname\tbest_wild\tarea\twhen\tswarm_day\tspecial\tegg\troute\n");
  int wildOnly = 0, rare1 = 0, rare05 = 0, eggOnly = 0, evoOnly = 0, none = 0;
  std::vector<std::pair<double, int16_t>> low;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    if (!speciesShowable(d)) continue;
    Best w = wild.count(d) ? wild[d] : Best();
    double sw = swarm.count(d) ? swarm[d] : 0, eg = egg.count(d) ? egg[d] : 0;
    const char *sp = special.count(d) ? special[d] : "";
    const char *route = w.p > 0 ? (eg > 0 ? "wild+egg" : "wild") : eg > 0 ? "egg" : *sp ? sp :
                        sw > 0 ? "swarm" : DEX_TBL[d].rarity == R_EVO ? "evolution" : "NONE";
    if (!strcmp(route, "wild")) { wildOnly++; if (w.p < 0.01 && !*sp && sw < 0.01) { rare1++; low.push_back({w.p, d}); } if (w.p < 0.005 && !*sp && sw < 0.005) rare05++; }
    if (!strcmp(route, "egg")) eggOnly++;
    if (!strcmp(route, "evolution")) evoOnly++;
    if (!strcmp(route, "NONE")) none++;
    printf("%d\t%s\t%.4f\t%s\t%s\t%.3f\t%s\t%.4f\t%s\n", d, DEX_TBL[d].name, w.p,
           w.area == WILD_NO_AREA ? "-" : wildAreaName(w.area), BANDN[w.when], sw, sp, eg, route);
  }
  fprintf(stderr, "wild only: %d (under 1%%: %d, under 0.5%%: %d) | egg only: %d | evolution only: %d | none: %d\n",
          wildOnly, rare1, rare05, eggOnly, evoOnly, none);
  std::sort(low.begin(), low.end());
  for (auto &x : low)
    fprintf(stderr, "  %-12s %.2f%%  %s (%s)\n", DEX_TBL[x.second].name, 100 * x.first,
            wildAreaName(wild[x.second].area), BANDN[wild[x.second].when]);
  return 0;
}
