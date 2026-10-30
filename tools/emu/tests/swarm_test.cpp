// Swarms (v3.78, docs/explore-specialty-build-plan.md § 5): Crystal's phone
// swarms, Emerald's TV outbreaks, Platinum's daily swarms. One a region a day
// by the day hash, none without a clock, gated where the game gates them, a
// share of the area's encounters that lands on the game's swarm-day odds, and
// SWARM on the area's status line -- the caller, startWildBattle() and
// areaStatusText(), is driven as a player drives it.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "i18n.h"
#include "wild.h"
#include <cstdio>
#include <cstring>
#include <set>

uint32_t g_seed = 0x5A12;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
bool startWildBattle(uint8_t region, bool hard);
void areaStatusText(uint16_t a, char *buf, size_t n);
extern Pet pet;
extern bool exploreOpen, battleOpen;
extern uint8_t exploreRegion;
extern uint16_t exploreAreaPick[];
extern int16_t wildDex;
extern int8_t wildLegend, wildFixed;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static int areaIn(uint8_t region, const char *name) {
  for (uint8_t k = 0; k < wildAreaCount(region); k++) {
    uint16_t a = wildAreaFirst(region) + k;
    if (!strcmp(wildAreaName(a), name)) return a;
  }
  return -1;
}
static int swarmOf(int16_t dex, int area) {
  for (uint8_t i = 0; i < wildSwarmCount(); i++)
    if (wildSwarmDex(i) == dex && wildSwarmArea(i) == (uint16_t)area) return i;
  return -1;
}
static uint32_t dayEpoch(uint32_t d, int hour = 12) { return d * 86400 + hour * 3600; }
// The first day from `from` on which region's swarm is `want` (or is not, if !is).
static uint32_t dayWith(Pet &p, uint8_t region, int want, bool is = true, uint32_t from = 20000) {
  for (uint32_t d = from; d < from + 4000; d++) {
    p.lastSeenEpoch = dayEpoch(d);
    if ((wildSwarmToday(region, p) == want) == is) return d;
  }
  return 0;
}
static bool explore(uint8_t region, uint16_t area) {
  battleOpen = false; exploreOpen = true; exploreRegion = region;
  exploreAreaPick[region] = (uint16_t)(area + 1);
  return startWildBattle(region, false);
}
static bool endsWith(const char *s, const char *t) {
  size_t a = strlen(s), b = strlen(t);
  return a >= b && !strcmp(s + a - b, t);
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(6, false);
  pet.ageMinutes = 59UL * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  gRegionArt = 0xFFFF;

  int dark = areaIn(1, "DARK CAVE"), r35 = areaIn(1, "ROUTE 35"), r32 = areaIn(1, "ROUTE 32");
  int r116 = areaIn(2, "ROUTE 116"), forest = areaIn(3, "ETERNA FOREST");
  int dun = swarmOf(206, dark), yan = swarmOf(193, r35), qwil = swarmOf(211, r32);
  int sky = swarmOf(300, r116), slak = swarmOf(287, forest);

  // ---- the data ------------------------------------------------------------------
  ck(dun >= 0 && yan >= 0 && qwil >= 0, "Crystal's swarms: Dunsparce, Yanma, and Qwilfish on the rod");
  int n[9] = { 0 };
  for (uint8_t i = 0; i < wildSwarmCount(); i++) n[wildAreaRegion(wildSwarmArea(i))]++;
  ck(n[0] == 0 && n[1] == 3 && n[2] == 5 && n[3] == 22 && n[4] == 0,
     "FireRed none, Crystal 3, Emerald's 5 outbreaks, Platinum's 22, Unova none");
  ck(sky >= 0 && wildSwarmShare(sky) == 500,
     "Route 116's Skitty takes Emerald's own 50% of the encounters (the route is all grass)");
  ck(slak >= 0, "Platinum's last swarm map, Eterna Forest, is there too");

  // ---- one a day, none without a clock ---------------------------------------------
  {
    Pet p;
    for (uint8_t b = 0; b < 8; b++) p.winBadge(1, b, false);
    p.lastSeenEpoch = 0;
    ck(wildSwarmToday(1, p) < 0, "no clock, no swarm");
    p.lastSeenEpoch = dayEpoch(20123, 0);
    int a = wildSwarmToday(1, p);
    p.lastSeenEpoch = dayEpoch(20123, 23);
    ck(a >= 0 && wildSwarmToday(1, p) == a, "a day keeps its swarm from midnight to midnight");
    std::set<int> seen; int days[40] = { 0 };
    for (uint32_t d = 20000; d < 20365; d++) {
      p.lastSeenEpoch = dayEpoch(d);
      int s = wildSwarmToday(1, p);
      seen.insert(s); if (s >= 0) days[s]++;
    }
    ck(seen.count(dun) && seen.count(yan) && !seen.count(qwil) && !seen.count(-1),
       "over a year Johto's open swarms both come round -- the rod's waits");
    ck(days[dun] > 120 && days[yan] > 120, "roughly evenly");
    p.winBadge(1, 12, false);   // the champion: Johto's best rod
    seen.clear();
    for (uint32_t d = 20000; d < 20365; d++) { p.lastSeenEpoch = dayEpoch(d); seen.insert(wildSwarmToday(1, p)); }
    ck(seen.count(qwil), "and after the champion, Qwilfish on Route 32 too");
    Pet s;
    for (uint8_t b = 0; b < 12; b++) s.winBadge(3, b, false);
    bool any = false;
    for (uint32_t d = 20000; d < 20100; d++) { s.lastSeenEpoch = dayEpoch(d); if (wildSwarmToday(3, s) >= 0) any = true; }
    ck(!any, "Sinnoh has no swarm before the National Dex (the champion)");
    s.winBadge(3, 12, false);
    seen.clear();
    for (uint32_t d = 20000; d < 21000; d++) { s.lastSeenEpoch = dayEpoch(d); seen.insert(wildSwarmToday(3, s)); }
    ck(seen.size() == 22, "after it, every one of Platinum's 22 comes round");
    // The re-roll cheat's salt draws again.
    s.lastSeenEpoch = dayEpoch(20200);
    int before = wildSwarmToday(3, s), changed = 0;
    for (uint8_t salt = 1; salt < 20; salt++) { s.daySalt = salt; if (wildSwarmToday(3, s) != before) changed++; }
    s.daySalt = 0;
    ck(changed > 10, "the re-roll's salt draws another swarm");
  }

  // ---- the share, through the EXPLORE button ----------------------------------------
  for (uint8_t b = 0; b < 2; b++) pet.winBadge(2, b, false);
  {
    uint32_t on = dayWith(pet, 2, sky), off = dayWith(pet, 2, sky, false);
    ck(on && off, "(found a Skitty day and another)");
    pet.lastSeenEpoch = dayEpoch(on);
    ck(wildSwarmAt(r116, pet) == sky, "on its day Route 116 holds the swarm");
    int skitty = 0, runs = 2000;
    for (int i = 0; i < runs; i++) if (explore(2, r116) && wildDex == 300) skitty++;
    ck(skitty > runs * 45 / 100 && skitty < runs * 55 / 100, "Skitty is half of Route 116's encounters that day");
    char sub[48];
    areaStatusText(r116, sub, sizeof(sub));
    ck(endsWith(sub, T(S_SWARM)), "the row says SWARM");
    pet.lastSeenEpoch = dayEpoch(off);
    skitty = 0;
    for (int i = 0; i < runs; i++) if (explore(2, r116) && wildDex == 300) skitty++;
    areaStatusText(r116, sub, sizeof(sub));
    ck(skitty < runs * 5 / 100 && !strstr(sub, T(S_SWARM)),
       "another day: Skitty only at its ordinary rare rate, and no SWARM");
    pet.lastSeenEpoch = 0;
    areaStatusText(r116, sub, sizeof(sub));
    ck(!strstr(sub, T(S_SWARM)) && wildSwarmAt(r116, pet) < 0, "and no clock, no SWARM");
  }
  // Crystal's grass swarm: Dark Cave on its day.
  {
    for (uint8_t b = 0; b < 8; b++) pet.winBadge(1, b, false);
    uint32_t on = dayWith(pet, 1, dun);
    pet.lastSeenEpoch = dayEpoch(on);
    int d = 0, runs = 2000;
    for (int i = 0; i < runs; i++) if (explore(1, dark) && wildDex == 206) d++;
    pet.lastSeenEpoch = dayEpoch(dayWith(pet, 1, dun, false));
    int o = 0;
    for (int i = 0; i < runs; i++) if (explore(1, dark) && wildDex == 206) o++;
    printf("  Dunsparce in Dark Cave: %d / %d on its day, %d on another\n", d, runs, o);
    ck(d > runs * 15 / 100 && d > 4 * o, "Dunsparce crowds Dark Cave on its day (its grass, not its water)");
  }

  // ---- the status line fits, in every language -----------------------------------------
  {
    bool fits = true;
    Pet &p = pet;
    for (uint8_t r = 1; r <= 3; r++) for (uint8_t b = 0; b <= 12; b++) p.winBadge(r, b, false);
    for (int L = 0; L < LANG_COUNT; L++) {
      setLang((Lang)L);
      for (uint8_t i = 0; i < wildSwarmCount(); i++) {
        uint8_t region = wildAreaRegion(wildSwarmArea(i));
        uint32_t d = dayWith(p, region, i);
        p.lastSeenEpoch = dayEpoch(d);
        char sub[48];
        areaStatusText(wildSwarmArea(i), sub, sizeof(sub));
        if (strlen(sub) > 20 || !endsWith(sub, T(S_SWARM))) {
          printf("  %s: '%s'\n", wildAreaName(wildSwarmArea(i)), sub); fits = false;
        }
      }
    }
    setLang(LANG_EN);
    ck(fits, "every swarm area's line says SWARM in 20 characters or fewer, in all six languages");
  }

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
