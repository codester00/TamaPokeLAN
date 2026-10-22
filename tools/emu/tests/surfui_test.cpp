// Surfing Pikachu's secret and screen (v3.54). The SUMMER BEACH HOUSE row is in
// Kanto's area list only while all four of the owner's conditions hold -- and
// each one, taken away alone, hides it again (a guard phrased "X never happens"
// needs its mechanism shown engaging). Then a run is played through the real
// screen: it pays coins, joy and bond once, and the best score is saved.
//
// GCSHOT=<dir> writes a PPM of the ride and the Goal for looking at by eye.
#include <cstring>
#include <cstdlib>
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "moves.h"
#include "gamecorner.h"
#include "surf.h"
#include "wild.h"
#include <cstdio>

uint32_t g_seed = 0x5A1F;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
void onSwipeV(int dir);
void areaRowRect(int i, int *x, int *y, int *w, int *h);
void openAreaList();
int areaListEntry(uint8_t region, int k, bool *isHall);
static int hallRowOf(uint8_t region, uint8_t hall) {
  bool isHall;
  for (int k = 0, e; (e = areaListEntry(region, k, &isHall)) >= 0; k++)
    if (isHall && e == hall) return k;
  return -1;
}
uint8_t areaHallCount(uint8_t region);
void surfFrame(uint32_t now);
void hallBack();
extern Pet pet;
extern bool exploreOpen, areaListOpen;
extern uint8_t exploreRegion, areaPage, hallOpen, hallMode;
extern SurfGame surfRun;
extern uint32_t surfLast;
extern Arduino_Canvas *gfx;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static void shot(const char *name) {
  const char *dir = getenv("GCSHOT");
  if (!dir) return;
  char path[256];
  snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6 466 466 255\n");
  const uint16_t *fb = gfx->buffer();
  for (int i = 0; i < 466 * 466; i++) {
    uint16_t c = fb[i];
    unsigned char px[3] = { (unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2),
                            (unsigned char)((c & 31) << 3) };
    fwrite(px, 1, 3, f);
  }
  fclose(f);
}

static uint8_t mv(const char *n) {
  for (uint16_t i = 1; i < MOVE_COUNT; i++) if (!strcmp(MOVE_TBL[i].name, n)) return (uint8_t)i;
  return 0;
}

int main() {
  setup();
  uint8_t surf = mv("SURF");
  uint16_t sea = wildAreaByName(0, SURF_AREA_NAME);
  ck(sea != WILD_NO_AREA, "SEA ROUTES 19-21 exists");
  // a species that lives there, to take out of the Pokedex
  int16_t local = wildAreaPick(sea, 7, pet, 0);
  ck(local > 0, "(and has a species to register)");

  auto setAll = [&]() {
    pet.dbgHatchAs(25, false);                     // PIKACHU
    pet.moves[0] = surf;
    for (uint8_t b = 0; b < 8; b++) pet.winBadge(0, b, false);   // the routes are open
    memset(pet.dexReg, 0xFF, sizeof(pet.dexReg));  // everything registered
  };
  auto clearReg = [&](int16_t d) { pet.dexReg[(d - 1) >> 3] &= (uint8_t)~(1 << ((d - 1) & 7)); };

  // --- 1. all four conditions: the beach house is the second row
  setAll();
  ck(surfSecretOpen(pet) && areaHallCount(0) == 2, "PIKACHU + SURF + the sea routes open + all registered: the row appears");
  ck(areaHallCount(1) == 1, "and only in Kanto");

  // --- 2. each taken away alone hides it
  setAll(); pet.dbgHatchAs(26, false); pet.moves[0] = surf;
  ck(!surfSecretOpen(pet) && areaHallCount(0) == 1, "a RAICHU that knows SURF: hidden");
  setAll(); for (int i = 0; i < MOVE_SLOTS; i++) pet.moves[i] = mv("THUNDERBOLT");
  ck(!surfSecretOpen(pet) && areaHallCount(0) == 1, "a PIKACHU without SURF: hidden");
  setAll(); clearReg(local);
  ck(!surfSecretOpen(pet) && areaHallCount(0) == 1, "one species of the sea routes unregistered: hidden");
  setAll(); pet.badges = pet.badgesHard = 0;
  ck(!wildAreaUnlocked(sea, pet) && !surfSecretOpen(pet) && areaHallCount(0) == 1, "the sea routes still locked: hidden");

  // --- 3. a run through the screen
  setAll();
  pet.coins = 0; pet.surfHi = 0;
  exploreOpen = true; exploreRegion = 0;
  openAreaList();
  int beachRow = hallRowOf(0, HALL_BEACH);
  {
    bool isHall;
    int before = beachRow > 0 ? areaListEntry(0, beachRow - 1, &isHall) : -1;
    ck(before == (int)sea && !isHall, "the beach house sits right after SEA ROUTES 19-21");
  }
  areaPage = (uint8_t)(beachRow / 3);
  render();
  shot("surf_list");
  int x, y, w, h; areaRowRect(beachRow % 3, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2);
  ck(hallOpen == HALL_BEACH + 1 && hallMode == 4, "tapping it starts the ride");
  uint8_t joy0 = pet.joy, bond0 = pet.bond;
  pet.joy = 10; joy0 = 10;
  uint32_t t = surfLast;
  for (int i = 0; i < 400; i++) { t += 85; surfFrame(t); }
  render();
  shot("surf_ride");
  ck(!surfRun.over(), "(mid-run)");
  for (int i = 0; i < 800 && !surfRun.over(); i++) { t += 85; surfFrame(t); }
  ck(surfRun.over(), "the ride ends at the Goal");
  render();
  shot("surf_goal");
  uint32_t sc = surfRun.score;
  ck(sc > 0 && pet.coins == surfCoins(sc), "the run pays its coins");
  ck(pet.surfHi == sc, "and sets the best score");
  ck(pet.joy > joy0 && pet.bond >= bond0, "and joy and bond");
  uint16_t c1 = pet.coins;
  surfFrame(t + 85); surfFrame(t + 170);
  ck(pet.coins == c1, "it pays once");
  onTap(233, 233);
  ck(!hallOpen && areaListOpen, "a tap on the Goal goes back to the list");

  // --- 4. the best score is saved
  pet.renameTrainer("ASH");
  { Pet r; r.begin(); ck(r.surfHi == sc, "the best score survives a reload"); }

  // --- 5. leaving mid-run keeps what was earned, and a worse run is not a best
  areaPage = (uint8_t)(beachRow / 3);
  areaRowRect(beachRow % 3, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2);
  t = surfLast;
  for (int i = 0; i < 60; i++) { t += 85; surfFrame(t); }
  uint32_t part = surfRun.score;
  uint16_t c2 = pet.coins;
  onSwipeV(1);
  ck(!hallOpen && pet.coins == c2 + surfCoins(part), "backing out mid-ride pays what was earned");
  ck(pet.surfHi == sc, "and a shorter run does not lower the best");

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
