// Explore's fixed one-offs (v3.77, docs/explore-specialty-build-plan.md § 4):
// Snorlax on the road, the Power Plant's two Electrode, Crystal's Sudowoodo,
// Friday Lapras and red Gyarados, Emerald's Kecleon. Each is met once for
// certain, then turns up now and then until caught -- like a legendary, but
// it stays in the egg pool. The '!' on its area's row means the certain
// meeting is due and nothing else.
//
// The rules are driven through wildFixedFor(); the callers through the
// sketch's own startWildBattle(), the battle bag and areaMarkedName(), the
// text the area list and Explore's area row both draw.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "inventory.h"
#include "wild.h"
#include <cstdio>
#include <cstring>
#include <vector>

uint32_t g_seed = 0xF1ED;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void onTap(int16_t x, int16_t y);
bool startWildBattle(uint8_t region, bool hard);
int btlCellIndexAt(int16_t x, int16_t y);
const char *areaMarkedName(uint16_t area, bool open_);
extern Pet pet;
extern Inventory bag;
extern bool exploreOpen, battleOpen, btlOver, wildCaught, wildShiny;
extern uint8_t exploreRegion, btlMsgCount, wildLvl;
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
// The n-th fixed one-off of a species (0 = the first), or -1.
static int fixedOf(int16_t dex, int n = 0) {
  for (uint8_t i = 0; i < wildFixedCount(); i++)
    if (wildFixedDex(i) == dex && n-- == 0) return i;
  return -1;
}
static int count(uint16_t area, const Pet &p, int want) {
  int n = 0;
  for (uint32_t r = 0; r < 10000; r++) if (wildFixedFor(area, p, r) == want) n++;
  return n;
}
static bool cellPoint(int i, int16_t &x, int16_t &y) {
  for (int16_t py = 240; py < 466; py += 4)
    for (int16_t px = 0; px < 466; px += 4)
      if (btlCellIndexAt(px, py) == i) { x = px + 8; y = py + 8; return true; }
  return false;
}
// One Explore in `area`, as the EXPLORE button does it.
static bool explore(uint8_t region, uint16_t area) {
  battleOpen = false; exploreOpen = true; exploreRegion = region;
  exploreAreaPick[region] = (uint16_t)(area + 1);
  return startWildBattle(region, false);
}
// Catch what is in front of us with a Master Ball from the battle bag.
static void catchIt() {
  for (ItemKey k = 1; k < ITEM_COUNT; k++) while (bag.count(k)) bag.consume(k, bag.count(k));
  bag.add(IT_MASTERBALL, 1);
  for (int t = 0; t < 10 && btlMsgCount; t++) onTap(233, 233);
  int16_t bx, by, cx, cy;
  if (!cellPoint(1, bx, by) || !cellPoint(0, cx, cy)) return;
  onTap(bx, by);
  onTap(cx, cy);
  for (int t = 0; t < 40 && battleOpen; t++) onTap(233, 233);
}
// An epoch on the given weekday (0 = Sunday), noon.
static uint32_t onWeekday(int wd) {
  uint32_t d = 20000;
  while ((d + 4) % 7 != (uint32_t)wd) d++;
  return d * 86400 + 12 * 3600;
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
  pet.lastSeenEpoch = onWeekday(1);   // a Monday

  int snor1 = fixedOf(143), snor2 = fixedOf(143, 1), el1 = fixedOf(101), el2 = fixedOf(101, 1);
  int sudo = fixedOf(185), lap = fixedOf(131), gya = fixedOf(130), kec = fixedOf(352);
  int r12 = areaIn(0, "ROUTES 12-15"), r16 = areaIn(0, "ROUTES 16-18"), plant = areaIn(0, "POWER PLANT");
  int union2 = areaIn(1, "UNION CAVE B2F"), rage = areaIn(1, "LAKE OF RAGE");
  ck(snor1 >= 0 && snor2 >= 0 && el1 >= 0 && el2 >= 0 && sudo >= 0 && lap >= 0 && gya >= 0 && kec >= 0,
     "every fixed one-off is in WILD_FIXED");
  ck(wildFixedArea(snor1) == (uint16_t)r12 && wildFixedArea(snor2) == (uint16_t)r16 &&
     wildFixedArea(el1) == (uint16_t)plant && wildFixedArea(el2) == (uint16_t)plant &&
     wildFixedArea(lap) == (uint16_t)union2 && wildFixedArea(gya) == (uint16_t)rage,
     "each stands where the game puts it");
  ck(wildFixedLevel(snor1) == 30 && wildFixedLevel(el1) == 34 && wildFixedLevel(sudo) == 20 &&
     wildFixedLevel(lap) == 20 && wildFixedLevel(gya) == 30 && wildFixedLevel(kec) == 30,
     "at the levels the games' scripts fight them at");
  ck(wildFixedShiny(gya) && !wildFixedShiny(lap) && !wildFixedShiny(snor1),
     "only the Lake of Rage's Gyarados is always shiny");

  // ---- the egg pool keeps them -----------------------------------------------
  {
    bool homed = false;
    for (uint8_t i = 0; i < wildFixedCount(); i++) if (wildIsExploreLegend(wildFixedDex(i))) homed = true;
    ck(!homed, "no fixed one-off counts as an Explore legendary");
    Pet e; bool lapras = false, kecleon = false;
    for (int i = 0; i < 4000 && !(lapras && kecleon); i++) {
      if (e.rollInRegion(0, R_RARO) == 131) lapras = true;
      if (e.rollInRegion(2, R_COMUN) == 352) kecleon = true;
    }
    ck(lapras && kecleon, "Lapras and Kecleon still hatch from eggs");
  }

  // ---- the rules ---------------------------------------------------------------
  {
    Pet p; p.lastSeenEpoch = onWeekday(1);
    for (uint8_t b = 0; b < 3; b++) p.winBadge(0, b, false);
    ck(!wildFixedLive(snor1, p) && count(r12, p, snor1) == 0, "Snorlax waits for its gate");
    p.winBadge(0, 3, false);
    ck(wildFixedLive(snor1, p) && count(r12, p, snor1) == 10000,
       "past it, the first Explore on Routes 12-15 meets Snorlax for certain");
    ck(count(r16, p, snor2) == 10000 && count(r16, p, snor1) == 0,
       "the other Snorlax is Routes 16-18's, and only there");
    p.setFixedMet(snor1);
    int rep = count(r12, p, snor1);
    ck(rep > 200 && rep < 400, "missed, it comes back at the legendaries' rate");
    ck(count(r16, p, snor2) == 10000, "and the Route 16 one is still due");
    p.setFixedCaught(snor1);
    ck(count(r12, p, snor1) == 0 && p.fixedMet(snor1), "caught, it never comes back");
  }
  {   // the weekday: Lapras is there on Fridays only
    Pet p;
    for (uint8_t b = 0; b < 4; b++) p.winBadge(1, b, false);
    p.lastSeenEpoch = onWeekday(4);
    ck(wildWeekday(p) == 4 && count(union2, p, lap) == 0, "no Lapras on a Thursday");
    p.lastSeenEpoch = onWeekday(5);
    ck(count(union2, p, lap) == 10000, "a Friday meets it for certain");
    p.setFixedMet(lap);
    int rep = count(union2, p, lap);
    ck(rep > 200 && rep < 400, "and missed, it comes back on Fridays");
    p.lastSeenEpoch = onWeekday(6);
    ck(count(union2, p, lap) == 0, "but not on Saturday");
    p.lastSeenEpoch = 0;
    ck(wildWeekday(p) < 0 && count(union2, p, lap) == 0, "and never without a clock");
  }

  // ---- the callers: the Power Plant, its legendary first ------------------------
  for (uint8_t b = 0; b < 5; b++) pet.winBadge(0, b, false);
  {
    char want[32];
    snprintf(want, sizeof(want), "* %s *", wildAreaName(plant));
    ck(!strcmp(areaMarkedName(plant, true), want), "the Power Plant is starred: Zapdos is due");
    ck(!strcmp(areaMarkedName(plant, false), wildAreaName(plant)), "(a locked row gives nothing away)");
    ck(explore(0, plant) && wildDex == 145 && wildLegend >= 0 && wildFixed < 0,
       "the first Explore there meets Zapdos -- a legendary wins");
    snprintf(want, sizeof(want), "! %s !", wildAreaName(plant));
    ck(!strcmp(areaMarkedName(plant, true), want), "then the row reads '!': an Electrode is due");
    ck(explore(0, plant) && wildFixed == el1 && wildDex == 101 && wildLvl == 34 && wildLegend < 0,
       "the next Explore meets the first Electrode, at L34");
    ck(pet.fixedMet(el1), "and the meeting is spent as the fight starts");
    {
      Pet reloaded; reloaded.begin();
      ck(reloaded.fixedMet(el1), "already in the save");
    }
    ck(!strcmp(areaMarkedName(plant, true), want), "the second Electrode is still due");
    ck(explore(0, plant) && wildFixed == el2, "and is met next");
    ck(!strcmp(areaMarkedName(plant, true), wildAreaName(plant)),
       "with both met the marker goes, though neither is caught");

    // Missed: they come back now and then. Catch one when it does.
    int seen = 0, tries = 0;
    while (tries++ < 3000) {
      if (explore(0, plant) && wildFixed >= 0) { seen++; if (seen == 1) break; }
    }
    ck(seen == 1 && wildFixed >= 0, "a missed Electrode turns up again");
    int8_t which = wildFixed;
    catchIt();
    ck(wildCaught && pet.fixedCaught((uint8_t)which), "caught with a ball, and recorded");
    {
      Pet reloaded; reloaded.begin();
      ck(reloaded.fixedCaught((uint8_t)which), "the catch is in the save");
    }
    int again = 0, other = 0;
    for (int i = 0; i < 1500; i++)
      if (explore(0, plant) && wildFixed >= 0) { if (wildFixed == which) again++; else other++; }
    ck(again == 0 && other > 10, "the caught one never returns; the other still does");
  }

  // ---- the red Gyarados: shiny every time ----------------------------------------
  for (uint8_t b = 0; b < 4; b++) pet.winBadge(1, b, false);
  {
    char want[32];
    snprintf(want, sizeof(want), "! %s !", wildAreaName(rage));
    ck(!strcmp(areaMarkedName(rage, true), want), "the Lake of Rage reads '!'");
    ck(explore(1, rage) && wildFixed == gya && wildDex == 130 && wildShiny,
       "the first Explore there meets the red Gyarados, shiny");
    bool repeat = false, plain = true;
    for (int i = 0; i < 3000 && !repeat; i++)
      if (explore(1, rage) && wildFixed == gya) { repeat = true; plain = !wildShiny; }
    ck(repeat && !plain, "and its repeat is shiny too");
  }

  // ---- the save, through each half ------------------------------------------------
  {
    Pet a; a.begin();
    a.setFixedMet(kec); a.setFixedCaught(sudo);
    a.saveNow();
    ck(a.saveHealthy() && nvs().count("fixd"), "the save lands, legacy key too");
    std::vector<uint8_t> legacy = nvs()["fixd"];
    nvs().erase("fixd");
    Pet b; b.begin();
    ck(b.fixedMet(kec) && !b.fixedCaught(kec) && b.fixedCaught(sudo) && !b.fixedMet(lap),
       "the checkpoint alone carries the bits, with the legacy key deleted");
    nvs()["fixd"] = legacy;
    nvs().erase("plyA"); nvs().erase("plyB");
    Pet c; c.begin();
    ck(c.fixedMet(kec) && c.fixedCaught(sudo) && !c.fixedMet(lap),
       "and so does the legacy key, with the checkpoint deleted");
    nvs().erase("fixd");
    Pet d; d.begin();
    ck(!d.fixedMet(kec) && !d.fixedCaught(sudo), "an older save reads as none met");
  }

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
