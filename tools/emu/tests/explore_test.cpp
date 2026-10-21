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
uint8_t currentExploreRegion();
void exploreStepRegion(int8_t step);
void onRim(int dir);
#include "wild.h"

static int bad = 0;
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

  gRegionArt = 0xFFFF;
  ck(startWildBattle(1, false), "an installed selected region starts an encounter");
  ck(wildDex >= REGIONS[1].lo && wildDex <= REGIONS[1].hi,
     "the encounter comes from the selected region, not the egg region");

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
      if (wildAreaPick(a, 7, roll) == wildDex) found = true;
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

  // A region without areas yet: the row cannot open a list, encounters still work.
  areaListOpen = false;
  exploreRegion = 1;
  tapRect(exploreAreaRect);
  ck(!areaListOpen, "a region without areas has no list to open");
  battleOpen = false; exploreOpen = true;
  ck(startWildBattle(1, true), "and it still has encounters, from the whole region");

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

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}