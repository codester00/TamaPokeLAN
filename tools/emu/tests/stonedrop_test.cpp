// Evolution stones found in the wild (v3.84, plan § E5): a win drops a stone
// only in an area where the games leave one -- an item ball or a hidden item
// on its maps -- about one win in ten there, never anywhere else, and only
// the stones that area holds. Every stone the four decompiled games place has
// an area; the Ice Stone and the Black Augurite are the Mart's only.
//
// The table through wildAreaStones(); the drops through the sketch's own
// wildGrantDrops(), after the sketch's own startWildBattle() chose the area.
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
extern Pet pet;
extern Inventory bag;
extern bool exploreOpen, battleOpen, btlHard;
extern uint8_t exploreRegion, btlMsgCount;
extern uint16_t exploreAreaPick[];
extern uint16_t wildAreaNow;
extern ItemKey wildDrops[4];
extern uint8_t wildDropN;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

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
    bool every = true, mart = true, safari = false, decomp = true;
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
          if (wildAreaRegion(a) > 3) decomp = false;
        }
      }
      if (s <= 8 && !areas) every = false;          // Fire .. Dawn: placed in the games
      if (s >= 9 && areas) mart = false;            // Ice, the Black Augurite
    }
    ck(every, "every stone FireRed, Crystal, Emerald and Platinum leave on a map has an area");
    ck(mart, "the Ice Stone and the Black Augurite are found nowhere: the Mart sells them");
    ck(!safari, "no stone in a Safari area (an encounter there is never won)");
    ck(decomp, "only the decompiled regions have stones (Unova onward: the Mart)");
    uint8_t st[EVO_STONE_COUNT];
    uint16_t moon = wildAreaByName(0, "MT. MOON");
    ck(wildAreaStones(moon, st, EVO_STONE_COUNT) == 1 && st[0] == 4, "Mt. Moon holds the Moon Stone");
    ck(wildAreaStones(wildAreaByName(3, "SOLACEON RUINS"), st, EVO_STONE_COUNT) == 3,
       "Solaceon Ruins' three dead ends: Fire, Water and Thunder");
    ck(wildAreaStones(wildAreaByName(0, "ROUTE 1"), st, EVO_STONE_COUNT) == 0, "Route 1 holds none");
  }

  // ---- the drops, in the area the fight is in ----------------------------------------
  {
    ck(explore(0, "MT. MOON"), "a fight in Mt. Moon remembers its area");
    Tally t = wins(6000, false);
    double rate = (double)t.withStone / t.wins;
    bool onlyMoon = true;
    for (uint8_t s = 0; s < EVO_STONE_COUNT; s++) if (s != 4 && t.per[s]) onlyMoon = false;
    printf("      Mt. Moon: a stone in %.1f%% of wins\n", 100 * rate);
    ck(rate > 0.06 && rate < 0.14, "about one win in ten finds a stone there");
    ck(onlyMoon, "and only its own, the Moon Stone");

    ck(explore(3, "SOLACEON RUINS"), "a fight in Solaceon Ruins");
    Tally h = wins(6000, true);
    ck(h.per[0] && h.per[1] && h.per[2] && h.dup == 0,
       "all three of its stones come, never the same twice in one win");

    ck(explore(0, "ROUTE 1"), "a fight on Route 1");
    Tally r = wins(6000, true);
    ck(r.withStone == 0, "where the games leave none, no stone ever drops");
  }

  printf(bad ? "FAILED %d\n" : "OK\n", bad);
  return bad ? 1 : 0;
}
