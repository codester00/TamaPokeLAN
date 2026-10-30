// Evolution stones found in the wild (v3.84, plan § E5; v3.85): a win drops a
// stone only in an area where the games leave one -- an item ball or a hidden
// item, from the decomps for Kanto to Sinnoh and Bulbapedia for Unova onward --
// one win in 25 there on either difficulty, never anywhere else, and only the
// stones that area holds. A Safari area's come with a catch. Every stone has
// an area but the Black Augurite, which is the Mart's only.
//
// The table through wildAreaStones(); the drops through the sketch's own
// wildGrantDrops(), after the sketch's own startWildBattle() chose the area,
// and a Safari catch through the screen.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "inventory.h"
#include "wild.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x570E;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
bool startWildBattle(uint8_t region, bool hard);
void wildGrantDrops();
void onTap(int16_t x, int16_t y);
int btlCellIndexAt(int16_t x, int16_t y);
extern Pet pet;
extern Inventory bag;
extern bool exploreOpen, battleOpen, btlHard, btlOver, btlSafari, wildCaught;
extern uint8_t exploreRegion, btlMsgCount;
extern uint16_t exploreAreaPick[];
extern uint16_t wildAreaNow;
extern ItemKey wildDrops[4];
extern uint8_t wildDropN;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static bool cellPoint(int i, int16_t &x, int16_t &y) {
  for (int16_t py = 240; py < 466; py += 4)
    for (int16_t px = 0; px < 466; px += 4)
      if (btlCellIndexAt(px, py) == i) { x = px + 8; y = py + 8; return true; }
  return false;
}
static void drain() { for (int t = 0; t < 20 && btlMsgCount; t++) onTap(233, 233); }
static bool isStone(ItemKey k) { return k >= IT_FIRESTONE && k <= IT_BLACKAUGURITE; }

struct Tally { int wins = 0, withStone = 0, dup = 0; int per[EVO_STONE_COUNT] = {}; };
// `n` wins in the area the last fight was in, as the battle hands them out.
static Tally wins(int n, bool hard) {
  Tally t;
  btlHard = hard;
  for (int i = 0; i < n; i++) {
    btlMsgCount = 0;
    wildGrantDrops();
    bool any = false;
    for (uint8_t a = 0; a < wildDropN; a++) {
      if (!isStone(wildDrops[a])) continue;
      any = true;
      t.per[wildDrops[a] - IT_FIRESTONE]++;
      for (uint8_t b = 0; b < a; b++) if (wildDrops[b] == wildDrops[a]) t.dup++;
    }
    t.wins++;
    if (any) t.withStone++;
  }
  btlHard = false;
  return t;
}
static bool explore(uint8_t region, const char *name) {
  uint16_t a = wildAreaByName(region, name);
  battleOpen = false; exploreOpen = true; exploreRegion = region;
  exploreAreaPick[region] = (uint16_t)(a + 1);
  return startWildBattle(region, false) && wildAreaNow == a;
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
  for (uint8_t r = 0; r < 4; r++)
    for (uint8_t b = 0; b < 8; b++) pet.winBadge(r, b, false);

  // ---- the table -------------------------------------------------------------------
  {
    bool every = true, mart = true, safari = false, late = false;
    for (uint8_t s = 0; s < EVO_STONE_COUNT; s++) {
      int areas = 0;
      for (uint8_t rg = 0; rg < GYM_REGIONS; rg++)
      for (uint8_t k = 0; k < wildAreaCount(rg); k++) {
        uint16_t a = wildAreaFirst(rg) + k;
        uint8_t st[EVO_STONE_COUNT];
        uint8_t n = wildAreaStones(a, st, EVO_STONE_COUNT);
        for (uint8_t i = 0; i < n; i++) if (st[i] == s) {
          areas++;
          if (wildAreaMode(a) == WILD_MODE_SAFARI) safari = true;
          if (wildAreaRegion(a) > 3) late = true;
        }
      }
      if (s <= 9 && !areas) every = false;          // Fire .. Ice
      if (s == 10 && areas) mart = false;           // the Black Augurite
    }
    ck(every, "every stone the games leave on a map has an area, the Ice Stone too");
    ck(mart, "the Black Augurite is found nowhere: the Mart sells it");
    ck(safari, "a Safari area holds its stones (they come with a catch)");
    ck(late, "Unova onward have stones (Bulbapedia)");
    uint8_t st[EVO_STONE_COUNT];
    uint16_t moon = wildAreaByName(0, "MT. MOON");
    ck(wildAreaStones(moon, st, EVO_STONE_COUNT) == 1 && st[0] == 4, "Mt. Moon holds the Moon Stone");
    ck(wildAreaStones(wildAreaByName(3, "SOLACEON RUINS"), st, EVO_STONE_COUNT) == 3,
       "Solaceon Ruins' three dead ends: Fire, Water and Thunder");
    ck(wildAreaStones(wildAreaByName(0, "ROUTE 1"), st, EVO_STONE_COUNT) == 0, "Route 1 holds none");
    ck(wildAreaStones(wildAreaByName(4, "ROUTE 6"), st, EVO_STONE_COUNT) == 2, "Unova's Route 6: the Moon and Shiny Stones");
    ck(wildAreaStones(wildAreaByName(8, "GLASEADO MTN"), st, EVO_STONE_COUNT) == 1 && st[0] == 9,
       "Paldea's Glaseado Mountain: the Ice Stone");
  }

  // ---- the drops, in the area the fight is in ----------------------------------------
  {
    ck(explore(0, "MT. MOON"), "a fight in Mt. Moon remembers its area");
    Tally t = wins(8000, false), h = wins(8000, true);
    double rate = (double)t.withStone / t.wins, hrate = (double)h.withStone / h.wins;
    bool onlyMoon = true;
    for (uint8_t s = 0; s < EVO_STONE_COUNT; s++) if (s != 4 && (t.per[s] || h.per[s])) onlyMoon = false;
    printf("      Mt. Moon: a stone in %.1f%% of easy wins, %.1f%% of hard\n", 100 * rate, 100 * hrate);
    ck(rate > 0.03 && rate < 0.05 && hrate > 0.03 && hrate < 0.05,
       "one win in 25 finds a stone there, on either difficulty");
    ck(onlyMoon, "and only its own, the Moon Stone");

    ck(explore(3, "SOLACEON RUINS"), "a fight in Solaceon Ruins");
    Tally r3 = wins(6000, true);
    ck(r3.per[0] && r3.per[1] && r3.per[2] && r3.dup == 0,
       "all three of its stones come, never the same twice in one win");

    ck(explore(0, "ROUTE 1"), "a fight on Route 1");
    Tally r = wins(6000, true);
    ck(r.withStone == 0, "where the games leave none, no stone ever drops");
  }

  // ---- the Safari Zone: a catch is its win -----------------------------------------
  {
    int catches = 0, leaf = 0, other = 0, fled = 0;
    for (int e = 0; e < 1500 && leaf == 0; e++) {
      for (ItemKey k = 1; k < ITEM_COUNT; k++) while (bag.count(k)) bag.consume(k, bag.count(k));
      if (!explore(0, "SAFARI ZONE") || !btlSafari) break;
      drain();
      for (int t = 0; t < 40 && battleOpen && !btlOver; t++) {
        int16_t x, y;
        if (!cellPoint(0, x, y)) break;
        onTap(x, y);                       // BALL
        if (btlOver) break;
        drain();
      }
      if (wildCaught) {
        catches++;
        if (bag.count(IT_LEAFSTONE)) leaf++;
        for (ItemKey k = IT_FIRESTONE; k <= IT_BLACKAUGURITE; k++)
          if (k != IT_LEAFSTONE && bag.count(k)) other++;
      } else fled++;
      for (int t = 0; t < 40 && battleOpen; t++) onTap(233, 233);
      battleOpen = false;
    }
    printf("      Safari Zone: %d catches (%d got away) before a Leaf Stone\n", catches, fled);
    ck(leaf == 1 && other == 0, "a catch in the Safari Zone can bring its Leaf Stone, and only that");
  }

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
