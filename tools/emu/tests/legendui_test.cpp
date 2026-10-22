// Explore's legendaries through the screen (v3.40): choose Articuno's home from
// the area list, tap EXPLORE, meet it, catch it with a ball from the battle bag
// -- all through onTap() -- and prove the sketch's own hooks recorded both the
// meeting and the catch, in the save. legend_test covers the rules themselves;
// this covers the CALLERS (CLAUDE.md trap 1: test the caller, not just the rule).
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "inventory.h"
#include "wild.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x1E6E;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void onTap(int16_t x, int16_t y);
bool startWildBattle(uint8_t region, bool hard);
int btlCellIndexAt(int16_t x, int16_t y);
void areaRowRect(int i, int *x, int *y, int *w, int *h);
void areaListPos(uint16_t area, uint8_t *page, uint8_t *row);
void exploreAreaRect(int *x, int *y, int *w, int *h);
void exploreBtnRect(int *x, int *y, int *w, int *h);
uint16_t exploreAreaOf(uint8_t region);
extern Pet pet;
extern Party party;
extern Inventory bag;
extern bool exploreOpen, battleOpen, areaListOpen, btlOver, wildCaught;
extern uint8_t exploreRegion, areaPage, btlMsgCount, wildLvl;
extern int16_t wildDex;
extern int8_t wildLegend;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static bool cellPoint(int i, int16_t &x, int16_t &y) {
  for (int16_t py = 240; py < 466; py += 4)
    for (int16_t px = 0; px < 466; px += 4)
      if (btlCellIndexAt(px, py) == i) { x = px + 8; y = py + 8; return true; }
  return false;
}
static void tapRect(void (*rect)(int *, int *, int *, int *)) {
  int x, y, w, h; rect(&x, &y, &w, &h); onTap(x + w / 2, y + h / 2);
}
static int legendOf(int16_t dex) {
  for (uint8_t i = 0; i < wildLegendCount(); i++) if (wildLegendDex(i) == dex) return i;
  return -1;
}
// Choose a Kanto area through the list, the way a player does.
static void chooseArea(uint16_t area) {
  battleOpen = false; exploreOpen = true; exploreRegion = 0;
  tapRect(exploreAreaRect);
  uint8_t pg, row;
  areaListPos(area, &pg, &row);      // the firmware's answer: the halls come first
  areaPage = pg;
  int x, y, w, h; areaRowRect(row, &x, &y, &w, &h);
  onTap(x + w / 2, y + h / 2);
}

int main() {
  setup();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(6, false);
  pet.ageMinutes = 59UL * MINUTES_PER_LEVEL;   // level 60: strong enough to survive the throw
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  gRegionArt = 0xFFFF;

  int art = legendOf(144), zap = legendOf(145);
  uint16_t seafoam = wildLegendHome(art), plant = wildLegendHome(zap);
  for (uint8_t b = 0; b < 5; b++) pet.winBadge(0, b, false);   // the Soul Badge opens Seafoam

  chooseArea(seafoam);
  ck(!areaListOpen && exploreAreaOf(0) == seafoam, "Seafoam Islands is chosen through the list");
  ck(!pet.legendMet(art), "(Articuno has not been met)");
  tapRect(exploreBtnRect);
  ck(battleOpen && wildDex == 144 && wildLegend == art, "the first Explore there meets Articuno");
  ck(wildLvl == wildLegendLevel(art), "at the level the game fights it at");
  ck(pet.legendMet(art), "and the meeting is spent as the fight starts");
  {
    Pet reloaded; reloaded.begin();
    ck(reloaded.legendMet(art), "already in the save, before the fight is over");
  }

  // Catch it: empty the bag, one Master Ball, BAG then the ball.
  for (ItemKey k = 1; k < ITEM_COUNT; k++) while (bag.count(k)) bag.consume(k, bag.count(k));
  bag.add(IT_MASTERBALL, 1);
  for (int t = 0; t < 10 && btlMsgCount; t++) onTap(233, 233);   // "A wild ARTICUNO appeared!"
  int16_t bx, by, cx, cy;
  ck(cellPoint(1, bx, by) && cellPoint(0, cx, cy), "(the battle menu's cells were found)");
  onTap(bx, by);                                  // BAG
  onTap(cx, cy);                                  // the only item: the Master Ball
  ck(wildCaught && btlOver, "the Master Ball catches it");
  for (int t = 0; t < 40 && battleOpen; t++) onTap(233, 233);
  ck(!battleOpen, "(the fight closed)");
  ck(pet.legendCaught(art), "and the catch is recorded");
  bool held = false;
  for (int i = 0; i < PARTY_SLOTS; i++) if (party.slots[i].dex == 144) held = true;
  for (int i = 0; i < BOX_SLOTS; i++) if (party.box[i].dex == 144) held = true;
  ck(held, "Articuno is in the party or the box");
  {
    Pet reloaded; reloaded.begin();
    ck(reloaded.legendCaught(art), "the catch is in the save");
  }

  // Caught: Seafoam Islands is ordinary from now on, and says so.
  bool again = false;
  for (int i = 0; i < 300; i++) {
    battleOpen = false; exploreOpen = true;
    if (startWildBattle(0, false) && wildDex == 144) again = true;
  }
  ck(!again, "a caught legendary never comes back");
  ck(!wildAreaHasLegend(seafoam, pet), "and its home loses the star");

  // Hard changes the AI, not the legendary's level.
  chooseArea(plant);
  battleOpen = false; exploreOpen = true;
  ck(startWildBattle(0, true) && wildDex == 145 && wildLvl == wildLegendLevel(zap),
     "on hard, Zapdos is met at its own level too");

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
