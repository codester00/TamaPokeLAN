// Explore chooses its own installed region and routes that choice into the
// real wild encounter builder without changing the egg region -- and, since
// v3.39, an AREA within the region, chosen from a paged list and gated by the
// badges that open it. Everything is driven through onTap(), so the screen is
// tested along with the rules it is supposed to ask.
#include <cstring>
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include <cstdio>

uint32_t g_seed = 0xE7A0;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void onTap(int16_t x, int16_t y);
bool startWildBattle(uint8_t region, bool hard);
int8_t exploreRegionHit(int16_t x, int16_t y);
int uiTapFinger();
extern Pet pet;
extern bool exploreOpen;
extern uint8_t exploreRegion;
extern int16_t wildDex;
extern uint8_t wildLvl;
extern bool battleOpen, areaListOpen;
extern uint8_t areaPage; extern uint16_t exploreAreaPick[];
extern const char *const SCREEN_NAME[];
uint8_t uiCurrentScreen();
void areaRowRect(int i, int *x, int *y, int *w, int *h);
void exploreAreaRect(int *x, int *y, int *w, int *h);
void exploreBtnRect(int *x, int *y, int *w, int *h);
uint16_t exploreAreaOf(uint8_t region);
void areaStatusText(uint16_t a, char *buf, size_t n);
int exploreHour();
uint8_t currentExploreRegion();
void exploreStepRegion(int8_t step);
void onRim(int dir);
#include "wild.h"
#include "trainers.h"
#include "i18n.h"
#include "noart.h"

static int bad = 0;
// Art kept for a chosen few species, standing in for art not yet ingested.
static bool gKeepArt[DEX_COUNT + 1];
static bool keptArt(int16_t d) { return d > 0 && d <= DEX_COUNT && gKeepArt[d] && speciesHasArt(d); }
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

static bool measureRegionControl(int8_t direction, int16_t &left, int16_t &top,
                                 int16_t &right, int16_t &bottom) {
  left = top = 466;
  right = bottom = -1;
  for (int16_t py = 0; py < 466; py++)
    for (int16_t px = 0; px < 466; px++)
      if (exploreRegionHit(px, py) == direction) {
        if (px < left) left = px;
        if (px > right) right = px;
        if (py < top) top = py;
        if (py > bottom) bottom = py;
      }
  return right >= left && bottom >= top;
}

int main() {
  setup();
  pet.setRegion(0);
  pet.dbgHatchAs(6, false);
  gRegionArt = 0xFFFF;
  exploreOpen = true;
  exploreRegion = 0;

    int16_t rightL, rightT, rightR, rightB, leftL, leftT, leftR, leftB;
    ck(measureRegionControl(1, rightL, rightT, rightR, rightB),
      "the next-region control has a hit area");
    ck(measureRegionControl(-1, leftL, leftT, leftR, leftB),
      "the previous-region control has a hit area");
    ck(rightR - rightL + 1 >= uiTapFinger() && rightB - rightT + 1 >= uiTapFinger(),
      "the next-region control is finger-sized in both dimensions");
    ck(leftR - leftL + 1 >= uiTapFinger() && leftB - leftT + 1 >= uiTapFinger(),
      "the previous-region control is finger-sized in both dimensions");
    int16_t rightX = (rightL + rightR) / 2, rightY = (rightT + rightB) / 2;
    int16_t leftX = (leftL + leftR) / 2, leftY = (leftT + leftB) / 2;
  onTap(rightX, rightY);
  ck(exploreRegion == 1, "the Explore selector advances to Johto");
  ck(pet.region == 0, "changing Explore does not change the egg region");
  onTap(leftX, leftY);
  ck(exploreRegion == 0, "the Explore selector can move back to Kanto");

  gRegionArt = (uint16_t)((1u << 0) | (1u << 2));
  exploreRegion = 0;
  onTap(rightX, rightY);
  ck(exploreRegion == 2, "the selector skips a region whose pack is missing");

  // Every real region has areas since v3.49 (Paldea, the last). The selected
  // region, not the egg region, decides where an encounter comes from: from
  // the area Explore looks in there. Checked against that area's own picks --
  // an area may hold another region's species (Johto's Route 29 has Pidgey).
  bool allAreas = true;
  for (uint8_t r = 0; r < REGION_COUNT; r++)
    if (r != REGION_ALL && !wildRegionHasAreas(r)) allAreas = false;
  ck(allAreas, "every region has areas");
  const uint8_t far = REGION_COUNT - 2;          // the last real region, never the egg's here
  ck(far != pet.region, "(the region tried is not the egg region)");
  gRegionArt = 0xFFFF;
  battleOpen = false; exploreOpen = true;
  ck(startWildBattle(far, false), "an installed selected region starts an encounter");
  {
    uint16_t at = exploreAreaOf(far);
    bool fromIt = false;
    for (uint32_t r = 0; r < 20000 && !fromIt; r++)
      if (wildAreaPick(at, wildTimeBand(exploreHour()), pet, r) == wildDex) fromIt = true;
    ck(at != WILD_NO_AREA && fromIt, "the encounter comes from the selected region's area, not the egg region");
  }

  // ---------------------------------------------------------------- areas
  battleOpen = false;
  exploreOpen = true;
  exploreRegion = 0;
  pet.badges = pet.badgesHard = 0;             // ...and no Kanto badges here
  for (int r = 0; r < REGION_COUNT; r++) exploreAreaPick[r] = 0;
  auto on = [](const char *n) { return !strcmp(SCREEN_NAME[uiCurrentScreen()], n); };
  auto tapRect = [](void (*rect)(int *, int *, int *, int *)) {
    int x, y, w, h; rect(&x, &y, &w, &h); onTap(x + w / 2, y + h / 2);
  };
  auto tapRow = [](int i) {
    int x, y, w, h; areaRowRect(i, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2);
  };
  uint16_t first = wildAreaFirst(0);

  ck(exploreAreaOf(0) != 0xFF && wildAreaGate(exploreAreaOf(0)) == 0,
     "with no badges Explore looks in a gate-0 area");
  ck(on("explore"), "the test starts on Explore");
  tapRect(exploreAreaRect);
  ck(on("area") && areaListOpen, "tapping the area row opens the area list");
  ck(areaPage == (exploreAreaOf(0) - first) / 3, "and the list opens on the page of the current area");

  // Row 1 of page 1 is area 4 (Route 3, gate 1): locked with no badges.
  areaPage = 1;
  uint16_t before = exploreAreaOf(0);
  tapRow(1);
  ck(areaListOpen && exploreAreaOf(0) == before,
     "a locked row refuses the tap: the list stays and the choice is unchanged");
  ck(!wildAreaUnlocked((uint16_t)(first + 4), pet), "(that row really was locked)");

  // An open row is chosen, and the list closes back onto Explore.
  areaPage = 1;
  tapRow(0);                                   // area 3, VIRIDIAN FOREST, gate 0
  ck(!areaListOpen && on("explore"), "an open row closes the list");
  ck(exploreAreaOf(0) == first + 3, "and becomes the area Explore looks in");

  // The encounter comes from that area, at its band (or a floor above it).
  bool inArea = true, inBand = true, hardBand = true;
  for (int i = 0; i < 60; i++) {
    battleOpen = false;
    exploreOpen = true;
    bool hard = i & 1;
    if (!startWildBattle(0, hard)) { inArea = false; break; }
    uint16_t a = exploreAreaOf(0);
    bool found = false;
    for (uint32_t roll = 0; roll < 10000 && !found; roll += 37)
      if (wildAreaPick(a, 7, pet, roll) == wildDex) found = true;
    if (!found) inArea = false;
    uint8_t floor = wildAreaLevel(a, wildDex, 0);   // what roll 0 gives, floor applied
    uint8_t lo = wildAreaLvMin(a) > floor ? wildAreaLvMin(a) : floor;
    uint8_t hi = wildAreaLvMax(a) > floor ? wildAreaLvMax(a) : floor;
    bool ok = wildLvl >= lo && wildLvl <= hi;
    if (!ok) { if (hard) hardBand = false; else inBand = false; }
  }
  ck(inArea, "every encounter is a species of the chosen area");
  ck(inBand, "at a level inside that area's band");
  ck(hardBand, "and HARD uses the same band -- it changes the AI, not the levels");

  // Badges move the default forward, so three badges do not open on Route 1.
  for (int r = 0; r < REGION_COUNT; r++) exploreAreaPick[r] = 0;
  for (uint8_t b = 0; b < 3; b++) pet.winBadge(0, b, false);
  ck(wildAreaGate(exploreAreaOf(0)) == 3, "with three badges Explore opens on a gate-3 area");
  areaPage = 1;
  battleOpen = false; exploreOpen = true;
  tapRect(exploreAreaRect);
  areaPage = 1;
  tapRow(1);                                   // Route 3 again: open now
  ck(!areaListOpen && exploreAreaOf(0) == first + 4, "and the row that was locked can be chosen");

  // The list pages on the rim, and swipes back out to Explore.
  tapRect(exploreAreaRect);
  uint8_t p0 = areaPage;
  onRim(1);
  ck(areaListOpen && areaPage == p0 + 1, "the area list pages on the rim");

  // Every other region's area row opens its own list, as Kanto's does.
  bool lists = true;
  for (uint8_t r = 1; r < REGION_COUNT; r++) {
    if (r == REGION_ALL) continue;
    areaListOpen = false; battleOpen = false; exploreOpen = true; exploreRegion = r;
    tapRect(exploreAreaRect);
    if (!areaListOpen || exploreAreaOf(r) == WILD_NO_AREA) lists = false;
  }
  areaListOpen = false;
  ck(lists, "every region's area row opens its own list");

  // ALL is switched off (EXPLORE_ALLOW_ALL 0). The selector must visit every
  // installed region and never ALL -- the first half proves the second is not
  // passing by the selector being stuck.
  battleOpen = false; exploreOpen = true;
  gRegionArt = 0xFFFF;
  exploreRegion = 0;
  bool sawAll = false, visited[REGION_COUNT] = { false };
  for (int i = 0; i < 2 * REGION_COUNT; i++) {
    exploreStepRegion(1);
    uint8_t r = currentExploreRegion();
    visited[r] = true;
    if (r == REGION_ALL) sawAll = true;
  }
  bool everyReal = true;
  for (int r = 0; r < REGION_COUNT; r++) if (r != REGION_ALL && !visited[r]) everyReal = false;
  ck(everyReal, "the selector reaches every installed region");
  ck(!sawAll, "and never ALL, while it is switched off");
  ck(!startWildBattle(REGION_ALL, false), "nor can an encounter be started in ALL");

  // The best rod's hidden gate, through the real encounter builder (CLAUDE.md
  // trap 1: test the caller). Route 22's fishing catch waits for the rod's
  // badge; the species only the rod finds are found by comparing what the
  // area offers a player with every badge against this one, not named.
  {
    uint16_t r22 = (uint16_t)(first + 2);
    ck(!strcmp(wildAreaName(r22), "ROUTE 22"), "(the third Kanto area is Route 22)");
    exploreAreaPick[0] = (uint16_t)(r22 + 1);
    Pet full;
    for (uint8_t t = 0; t < TRAINER_COUNT; t++) full.winBadge(0, t, false);
    bool rodOnly[DEX_COUNT + 1] = { false };
    int nRod = 0;
    for (uint32_t r = 0; r < 20000; r++) {   // past any area total: every roll comes up
      int16_t d = wildAreaPick(r22, wildTimeBand(-1), full, r);
      if (d && !rodOnly[d]) {
        bool early = false;
        for (uint32_t q = 0; q < 20000 && !early; q++)
          if (wildAreaPick(r22, wildTimeBand(-1), pet, q) == d) early = true;
        if (!early) { rodOnly[d] = true; nRod++; }
      }
    }
    ck(nRod > 0 && !pet.hasBadge(0, 3, false), "with three badges, Route 22 holds some creatures back");
    bool leaked = false, came = false, live = true;
    for (int i = 0; i < 300; i++) {
      battleOpen = false; exploreOpen = true; exploreRegion = 0;
      if (!startWildBattle(0, i & 1)) { live = false; break; }
      if (rodOnly[wildDex]) leaked = true;
    }
    ck(live, "the area still has encounters before the rod");
    ck(!leaked, "and Explore never meets a creature only the best rod finds");
    pet.winBadge(0, 3, false);   // Erika's: FireRed's Super Rod
    for (int i = 0; i < 600 && !came; i++) {
      battleOpen = false; exploreOpen = true; exploreRegion = 0;
      if (startWildBattle(0, i & 1) && rodOnly[wildDex]) came = true;
    }
    ck(came, "until the rod's badge is won -- then they come");
  }

  // NO VALID SPRITES through the screen (v3.45): an open area whose creatures
  // cannot be drawn says so on its row, refuses the tap, starts no fight, and
  // is not where Explore opens -- then comes back the moment the art does.
  {
    static uint16_t dead; dead = (uint16_t)(first + 3);    // Viridian Forest
    wildHasArt = [](int16_t d) { return regionOfDex(d) != 0 && speciesHasArt(d); };  // Kanto's art gone
    char sub[40];
    areaStatusText(dead, sub, sizeof(sub));
    ck(!strcmp(sub, T(S_AREA_NO_SPRITES)), "an area with nothing drawable reads NO VALID SPRITES");
    areaListOpen = false; battleOpen = false; exploreOpen = true; exploreRegion = 0;
    for (int r = 0; r < REGION_COUNT; r++) exploreAreaPick[r] = 0;
    tapRect(exploreAreaRect);
    areaPage = 1;
    tapRow(0);                                   // Viridian Forest's row
    ck(areaListOpen && exploreAreaOf(0) != dead, "its row refuses the tap");
    areaListOpen = false;
    exploreAreaPick[0] = (uint16_t)(dead + 1);
    battleOpen = false; exploreOpen = true;
    ck(!startWildBattle(0, false), "and Explore starts no fight there");
    wildHasArt = speciesHasArt;                  // the art is ingested
    areaStatusText(dead, sub, sizeof(sub));
    ck(strcmp(sub, T(S_AREA_NO_SPRITES)) != 0, "with the art back the row shows its levels again");
    battleOpen = false; exploreOpen = true;
    ck(startWildBattle(0, false), "and the fight starts");
    // A greyed area is never Explore's default while a live one is open: keep
    // art only for Viridian Forest's creatures (found by sampling it), so the
    // further areas this player has opened go dark.
    for (int d = 0; d <= DEX_COUNT; d++) gKeepArt[d] = false;
    for (uint32_t r = 0; r < 20000; r++) gKeepArt[wildAreaPick(dead, wildTimeBand(-1), pet, r)] = true;
    wildHasArt = keptArt;
    exploreAreaPick[0] = 0;
    uint16_t def = exploreAreaOf(0);
    bool furtherDead = false;
    for (uint8_t k = 0; k < wildAreaCount(0); k++) {
      uint16_t a = (uint16_t)(first + k);
      if (wildAreaUnlocked(a, pet) && wildAreaGate(a) > wildAreaGate(def) &&
          !wildAreaHasLive(a, wildTimeBand(exploreHour()), pet)) furtherDead = true;
    }
    printf("      default %s (gate %u, live %d); a further open area is grey: %d\n", wildAreaName(def),
           wildAreaGate(def), wildAreaHasLive(def, wildTimeBand(exploreHour()), pet), furtherDead);
    ck(wildAreaHasLive(def, wildTimeBand(exploreHour()), pet) && furtherDead,
       "Explore opens on a live area, passing over further ones that are grey");
    wildHasArt = speciesHasArt;
  }


  // Today's weather on the area row (v3.48): a Galar Wild Area zone names it
  // after its levels, through the same status line the list draws; no clock,
  // or an area without weather, says nothing about it.
  {
    uint16_t gfirst = wildAreaFirst(7), west = WILD_NO_AREA;
    for (uint8_t k = 0; k < wildAreaCount(7); k++)
      if (!strcmp(wildAreaName((uint16_t)(gfirst + k)), "W. LAKE AXEWELL")) west = (uint16_t)(gfirst + k);
    ck(west != WILD_NO_AREA, "Galar's West Lake Axewell is found");
    uint32_t keep = pet.lastSeenEpoch;
    char sub[40], lv[24];
    snprintf(lv, sizeof(lv), T(S_AREA_LV_FMT), wildAreaLvMin(west), wildAreaLvMax(west));
    pet.lastSeenEpoch = 0;
    areaStatusText(west, sub, sizeof(sub));
    ck(!strcmp(sub, lv), "with no clock set the row shows only its levels");
    bool named = true, seen[9] = {false};
    for (uint32_t d = 0; d < 120; d++) {
      pet.lastSeenEpoch = (20000u + d) * 86400u + 43200u;
      int8_t wx = wildAreaWeather(west, pet);
      char want[40];
      snprintf(want, sizeof(want), "%s %s", lv, wx >= 0 ? T((StrId)(S_WX_CLEAR + wx)) : "?");
      areaStatusText(west, sub, sizeof(sub));
      if (wx < 0 || strcmp(sub, want)) named = false; else seen[wx] = true;
      if (strlen(sub) > 20) named = false;   // fits the row at text size 2
    }
    int kinds = 0; for (int w = 0; w < 9; w++) kinds += seen[w];
    ck(named, "with a clock it shows its levels and today's weather, in 20 characters or fewer");
    ck(kinds > 5, "and the weather it shows changes from day to day");
    areaStatusText(first, sub, sizeof(sub));   // Kanto's Route 1
    snprintf(lv, sizeof(lv), T(S_AREA_LV_FMT), wildAreaLvMin(first), wildAreaLvMax(first));
    ck(!strcmp(sub, lv), "an area without weather shows only its levels, clock or not");
    pet.lastSeenEpoch = keep;
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}